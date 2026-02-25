/*
 * neurolib.c — Implementation of the AI query library.
 *
 * When OPENAI_API_KEY is set:
 *   Opens a TLS connection to api.openai.com:443, sends an HTTP POST
 *   to /v1/chat/completions with the user's prompt, and returns the
 *   raw JSON response body.
 *
 * When OPENAI_API_KEY is not set:
 *   Returns a pre-canned JSON string that looks exactly like a real
 *   OpenAI response, so the rest of the program works without a key.
 */

/* POSIX extensions (needed for getaddrinfo, etc.) */
#define _POSIX_C_SOURCE 200112L

#include "neurolib.h"

#include <stdio.h>      /* snprintf, fprintf                            */
#include <stdlib.h>     /* malloc, free, getenv, realloc                */
#include <string.h>     /* strlen, strcpy, strstr, memset               */

/* POSIX networking */
#include <sys/types.h>  /* type definitions required by socket headers  */
#include <sys/socket.h> /* socket, connect                              */
#include <netdb.h>      /* getaddrinfo, freeaddrinfo                    */
#include <unistd.h>     /* close                                        */

/* OpenSSL TLS */
#include <openssl/ssl.h>     /* SSL_CTX, SSL, SSL_connect, ...          */
#include <openssl/err.h>     /* ERR_clear_error                         */

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

#define API_HOST  "api.openai.com"       /* hostname of the OpenAI API    */
#define API_PORT  "443"                  /* HTTPS port                    */
#define API_PATH  "/v1/chat/completions" /* REST endpoint for chat        */
#define API_MODEL "gpt-4o-mini"          /* cheap, fast chat model        */

/* Maximum size for the response we will buffer (2 MB) */
#define RESP_BUF_SIZE (2 * 1024 * 1024)

/* -------------------------------------------------------------------------
 * Mock responses (used when no API key is available)
 * ---------------------------------------------------------------------- */

/*
 * Each entry is the "content" field of a fake assistant reply.
 * They are cycled through in order so successive questions get
 * different answers.
 */
static const char *mock_contents[] = {
    "I'd answer that, but I don't want to ruin the surprise.",
    "I could tell you, but then I'd have to awkwardly dance away without explaining why.",
    "That's classified. If I told you, I'd have to forget I said it.",
    "My sources are unreliable, but my confidence is sky high.",
    "Great question! Unfortunately, the answer is beyond mortal understanding.",
    NULL /* sentinel */
};

/* Index of the next mock response to return (cycles through the list) */
static int mock_index = 0;

/*
 * build_mock_json — Wraps a plain-text content string in a JSON envelope
 * that matches the OpenAI chat-completion response schema.
 * Returns a heap-allocated string; caller must free().
 */
static char *build_mock_json(const char *content)
{
    /* Template: { "choices": [ { "message": { "content": "..." } } ] } */
    const char *fmt =
        "{\"choices\":[{\"message\":{\"content\":\"%s\"}}]}";
    size_t len;  /* total bytes needed for the formatted string */
    char  *json; /* output buffer */

    len  = strlen(fmt) + strlen(content) + 1;
    json = (char *)malloc(len);
    if (json == NULL) {
        return NULL; /* allocation failed */
    }

    snprintf(json, len, fmt, content); /* fill in the content */
    return json;
}

/* -------------------------------------------------------------------------
 * HTTPS helper functions (used only when an API key is present)
 * ---------------------------------------------------------------------- */

/*
 * tcp_connect — Opens a TCP socket to host:port.
 * Returns the socket file descriptor on success, or -1 on failure.
 */
static int tcp_connect(const char *host, const char *port)
{
    struct addrinfo hints; /* criteria for address selection    */
    struct addrinfo *res;  /* linked list of results            */
    struct addrinfo *rp;   /* iterator over the result list     */
    int sock;              /* file descriptor of the new socket */
    int rv;                /* return value of getaddrinfo       */

    memset(&hints, 0, sizeof(hints)); /* zero all fields first */
    hints.ai_family   = AF_UNSPEC;   /* accept IPv4 or IPv6   */
    hints.ai_socktype = SOCK_STREAM; /* TCP stream socket      */

    rv = getaddrinfo(host, port, &hints, &res);
    if (rv != 0) {
        return -1; /* DNS lookup or port resolution failed */
    }

    sock = -1;
    /* Try each address returned until one connects successfully */
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) {
            continue; /* socket() failed for this address — try next */
        }
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; /* connected successfully */
        }
        close(sock); /* connect failed — close and try next address */
        sock = -1;
    }

    freeaddrinfo(res); /* free the address list regardless of outcome */
    return sock;       /* -1 if no address worked */
}

/*
 * read_all_ssl — Reads every byte from the SSL connection until the
 * peer closes it, storing the data in a dynamically grown buffer.
 * Returns a heap-allocated NUL-terminated string, or NULL on error.
 * Caller must free().
 */
static char *read_all_ssl(SSL *ssl)
{
    char   chunk[4096]; /* temporary read buffer                    */
    char  *buf;         /* accumulation buffer                      */
    size_t total;       /* bytes stored in buf so far               */
    int    n;           /* bytes returned by SSL_read in one call   */

    buf   = (char *)malloc(RESP_BUF_SIZE);
    if (buf == NULL) {
        return NULL;
    }
    total = 0;

    /* Read chunks until SSL_read returns 0 (clean shutdown) or an error */
    while ((n = SSL_read(ssl, chunk, (int)sizeof(chunk) - 1)) > 0) {
        if (total + (size_t)n >= RESP_BUF_SIZE - 1) {
            break; /* response is larger than our buffer — stop early */
        }
        memcpy(buf + total, chunk, (size_t)n); /* append chunk to buf */
        total += (size_t)n;
    }

    buf[total] = '\0'; /* NUL-terminate the accumulated data */
    return buf;
}

/*
 * extract_http_body — Given a full HTTP response (headers + body),
 * returns a pointer to the start of the body (past the blank line).
 * The pointer is into the same buffer — do not free it separately.
 * Returns NULL if the header separator is not found.
 */
static char *extract_http_body(char *response)
{
    char *sep; /* pointer to the blank-line separator "\r\n\r\n" */

    sep = strstr(response, "\r\n\r\n");
    if (sep == NULL) {
        return NULL; /* malformed HTTP response */
    }
    return sep + 4; /* skip past the separator to reach the body */
}

/*
 * make_request_body — Builds the JSON payload for the OpenAI API call.
 * Returns a heap-allocated string; caller must free().
 */
static char *make_request_body(const char *prompt)
{
    /*
     * Minimal chat-completion request:
     * { "model": "...", "messages": [ { "role": "user", "content": "..." } ] }
     *
     * NOTE: for simplicity we do not escape the prompt string here.
     * A production library would JSON-encode every special character.
     */
    const char *fmt =
        "{\"model\":\"%s\","
        "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}";
    size_t len;
    char  *body;

    len  = strlen(fmt) + strlen(API_MODEL) + strlen(prompt) + 1;
    body = (char *)malloc(len);
    if (body == NULL) {
        return NULL;
    }
    snprintf(body, len, fmt, API_MODEL, prompt);
    return body;
}

/*
 * real_api_call — Sends the prompt to the OpenAI REST API over HTTPS
 * and returns the JSON response body as a heap-allocated string.
 * Returns NULL on any network or TLS error.
 */
static char *real_api_call(const char *api_key, const char *prompt)
{
    int      sock;        /* TCP socket file descriptor              */
    SSL_CTX *ctx;         /* TLS context (holds CA certs, settings)  */
    SSL     *ssl;         /* per-connection TLS state                */
    char    *body;        /* JSON request body                       */
    char    *headers;     /* HTTP request header block               */
    char    *raw;         /* full HTTP response (headers + body)     */
    char    *json_body;   /* pointer into raw, past the headers      */
    char    *result;      /* final heap-allocated string to return   */
    size_t   hdr_len;     /* length of the headers buffer            */

    /* Step 1: open a TCP connection to api.openai.com:443 */
    sock = tcp_connect(API_HOST, API_PORT);
    if (sock == -1) {
        return NULL; /* network unreachable or DNS failure */
    }

    /* Step 2: create a TLS context and wrap the socket */
    ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL) {
        close(sock);
        return NULL;
    }

    /* Load the system's default CA certificate bundle for verification */
    SSL_CTX_set_default_verify_paths(ctx);

    ssl = SSL_new(ctx);    /* allocate a new TLS connection object    */
    if (ssl == NULL) {
        SSL_CTX_free(ctx);
        close(sock);
        return NULL;
    }

    SSL_set_fd(ssl, sock); /* bind the TLS layer to our TCP socket    */

    /* SNI (Server Name Indication) lets the server pick the right cert */
    SSL_set_tlsext_host_name(ssl, API_HOST);

    /* Perform the TLS handshake */
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return NULL;
    }

    /* Step 3: build the JSON request body */
    body = make_request_body(prompt);
    if (body == NULL) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return NULL;
    }

    /* Step 4: build the HTTP/1.1 POST request headers */
    hdr_len  = 512 + strlen(API_PATH) + strlen(API_HOST) +
               strlen(api_key) + strlen(body);
    headers  = (char *)malloc(hdr_len);
    if (headers == NULL) {
        free(body);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return NULL;
    }

    snprintf(headers, hdr_len,
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Authorization: Bearer %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             API_PATH, API_HOST, api_key, strlen(body), body);

    free(body); /* body is now embedded in headers; no longer needed */

    /* Step 5: send the request over TLS */
    SSL_write(ssl, headers, (int)strlen(headers));
    free(headers);

    /* Step 6: read the entire HTTP response into a buffer */
    raw = read_all_ssl(ssl);

    /* Step 7: tear down TLS and close the socket */
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);

    if (raw == NULL) {
        return NULL; /* read failed */
    }

    /* Step 8: skip past the HTTP headers to find the JSON body */
    json_body = extract_http_body(raw);
    if (json_body == NULL) {
        free(raw);
        return NULL;
    }

    /* Copy the body into its own allocation so we can free raw */
    {
        size_t body_len = strlen(json_body) + 1; /* +1 for NUL terminator */
        result = (char *)malloc(body_len);
        if (result != NULL) {
            memcpy(result, json_body, body_len);
        }
    }
    free(raw); /* free the full response buffer */
    return result;
}

/* =========================================================================
 * Public function
 * ====================================================================== */

char *neuro_ask(const char *prompt)
{
    const char *api_key; /* value of the OPENAI_API_KEY environment variable */
    int         idx;     /* index into the mock_contents array               */

    /* Check whether the caller has set an API key */
    api_key = getenv("OPENAI_API_KEY");

    if (api_key == NULL || api_key[0] == '\0') {
        /*
         * No API key — return the next mock response from the list,
         * cycling back to the first one after the last.
         */
        idx = mock_index;

        /* Count how many mock responses we have */
        while (mock_contents[mock_index] != NULL) {
            mock_index++;
        }
        /* mock_index now equals the total number of entries */
        mock_index = (idx + 1) % mock_index; /* advance, wrapping around */

        /* Restore mock_index for the count we need */
        return build_mock_json(mock_contents[idx]);
    }

    /* API key is present — make a real HTTPS request */
    return real_api_call(api_key, prompt);
}
