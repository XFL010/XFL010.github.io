/*
 * jason.c — JSON extractor and AI chatbot front-end.
 *
 * Two modes of operation:
 *
 *   --extract <file>
 *       Read a JSON file and print the value of
 *       json.choices[0].message.content to stdout.
 *       Prints "Not an accepted JSON!" to stderr and exits with code 1
 *       if the file is not a valid JSON in the expected shape.
 *
 *   --bot
 *       Repeatedly prompt the user for a question, send it to the AI
 *       service via neurolib, parse the JSON response, and print the
 *       answer.  Stops when the user sends EOF (Ctrl-D).
 *
 * Compilation:
 *   gcc -Wall -Wextra -Werror -pedantic -c neurolib.c
 *   gcc -Wall -Wextra -Werror -pedantic -c jason.c
 *   gcc -o jason neurolib.o jason.o -lssl -lcrypto
 */

#include <stdio.h>   /* printf, fprintf, fgetc, fgets, stdin, stdout     */
#include <stdlib.h>  /* malloc, free, exit                               */
#include <string.h>  /* strcmp, strstr, strchr, strlen, strdup           */

#include "neurolib.h" /* neuro_ask                                       */

/* Maximum size of a JSON file we will read into memory (1 MB) */
#define MAX_JSON_SIZE (1024 * 1024)

/* Maximum length of a single line of user input */
#define MAX_INPUT_LEN 4096

/* -------------------------------------------------------------------------
 * extract_content
 *
 * Navigates a JSON string looking for:
 *   .choices  →  array  →  first element  →  .message  →  .content
 *
 * Returns a heap-allocated, NUL-terminated, unescaped string containing
 * the value of that field, or NULL if the path cannot be found or the
 * JSON is malformed.  The caller must free() the result.
 * ---------------------------------------------------------------------- */
static char *extract_content(const char *json)
{
    const char *p;        /* read cursor inside json                       */
    const char *str_end;  /* marks the closing '"' of the content value    */
    char       *out;      /* output buffer (unescaped content)             */
    size_t      i;        /* write index into out                          */
    size_t      cap;      /* allocated capacity of out                     */

    /* ---- Step 1: find the "choices" key ---- */
    p = strstr(json, "\"choices\"");
    if (p == NULL) {
        return NULL; /* "choices" key is absent */
    }
    p += strlen("\"choices\""); /* move past the key itself */

    /* Skip optional whitespace and the colon */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; }
    if (*p != ':') { return NULL; } /* expected ':' after key */
    p++;

    /* ---- Step 2: find the opening '[' of the choices array ---- */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; }
    if (*p != '[') { return NULL; } /* "choices" value is not an array */
    p++;

    /* ---- Step 3: find the opening '{' of the first array element ---- */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; }
    if (*p != '{') { return NULL; } /* first element is not an object */
    p++;

    /* ---- Step 4: find the "message" key inside that object ---- */
    p = strstr(p, "\"message\"");
    if (p == NULL) { return NULL; }
    p += strlen("\"message\"");

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; }
    if (*p != ':') { return NULL; }
    p++;

    /* ---- Step 5: find the opening '{' of the message object ---- */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; }
    if (*p != '{') { return NULL; }
    p++;

    /* ---- Step 6: find the "content" key inside message ---- */
    p = strstr(p, "\"content\"");
    if (p == NULL) { return NULL; }
    p += strlen("\"content\"");

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; }
    if (*p != ':') { return NULL; }
    p++;

    /* ---- Step 7: find the opening '"' of the content string ---- */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; }
    if (*p != '"') { return NULL; } /* content value is not a string */
    p++; /* move past the opening quote */

    /* ---- Step 8: decode the JSON string into a plain C string ---- */

    /*
     * We do not know the final length, so start with a reasonable
     * buffer and grow if needed.  The decoded string can only be
     * shorter than or equal to the encoded one, so strlen(p) is safe.
     */
    cap = strlen(p) + 1;
    out = (char *)malloc(cap);
    if (out == NULL) { return NULL; }

    i = 0;
    while (*p != '\0') {
        if (*p == '"') {
            break; /* unescaped '"' marks the end of the JSON string */
        }

        if (*p == '\\') {
            /* Escape sequence — interpret the next character */
            p++;
            switch (*p) {
                case 'n':  out[i++] = '\n'; break; /* newline            */
                case 't':  out[i++] = '\t'; break; /* horizontal tab     */
                case 'r':  out[i++] = '\r'; break; /* carriage return    */
                case '"':  out[i++] = '"';  break; /* double quote       */
                case '\\': out[i++] = '\\'; break; /* backslash          */
                case '/':  out[i++] = '/';  break; /* forward slash      */
                case 'b':  out[i++] = '\b'; break; /* backspace          */
                case 'f':  out[i++] = '\f'; break; /* form feed          */
                default:
                    /* Unknown escape — keep both characters literally */
                    out[i++] = '\\';
                    out[i++] = *p;
                    break;
            }
        } else {
            out[i++] = *p; /* ordinary character — copy as-is */
        }

        p++;

        /* Safety check: ensure we have not gone past the closing '"' */
        if (i >= cap - 1) {
            free(out);
            return NULL; /* unexpected end of buffer */
        }
    }

    /* Verify we stopped at a closing '"', not at '\0' */
    str_end = p;
    if (*str_end != '"') {
        free(out);
        return NULL; /* string was not properly closed */
    }

    out[i] = '\0'; /* NUL-terminate the decoded string */
    return out;
}

/* -------------------------------------------------------------------------
 * read_file
 *
 * Reads the entire contents of 'filename' into a heap-allocated buffer.
 * Returns a NUL-terminated string on success, or NULL on failure.
 * Caller must free().
 * ---------------------------------------------------------------------- */
static char *read_file(const char *filename)
{
    FILE   *fp;    /* file handle                                        */
    char   *buf;   /* destination buffer                                 */
    size_t  n;     /* bytes actually read                                */

    fp = fopen(filename, "r"); /* open in text mode for reading */
    if (fp == NULL) {
        return NULL; /* file not found or permission denied */
    }

    buf = (char *)malloc(MAX_JSON_SIZE + 1);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }

    n = fread(buf, 1, MAX_JSON_SIZE, fp); /* read up to MAX_JSON_SIZE bytes */
    fclose(fp);

    buf[n] = '\0'; /* NUL-terminate regardless of how many bytes were read */
    return buf;
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(int argc, char *argv[])
{
    /* ------------------------------------------------------------------ */
    /* 1. Parse the mode flag                                               */
    /* ------------------------------------------------------------------ */

    if (argc < 2) {
        /* No mode flag given at all */
        fprintf(stderr, "Usage: %s [--extract <file> | --bot]\n", argv[0]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* MODE A: --extract <filename>                                         */
    /* ------------------------------------------------------------------ */

    if (strcmp(argv[1], "--extract") == 0) {

        char *json;     /* raw JSON text loaded from the file  */
        char *content;  /* decoded choices[0].message.content  */

        if (argc != 3) {
            /* --extract requires exactly one additional argument */
            fprintf(stderr, "Usage: %s --extract <file>\n", argv[0]);
            return 1;
        }

        /* Load the JSON file into memory */
        json = read_file(argv[2]);
        if (json == NULL) {
            fprintf(stderr, "Cannot open file: %s\n", argv[2]);
            return 1;
        }

        /* Extract choices[0].message.content from the JSON */
        content = extract_content(json);
        free(json); /* no longer needed once parsed */

        if (content == NULL) {
            /* The file exists but is not in the expected JSON shape */
            fprintf(stderr, "Not an accepted JSON!\n");
            return 1;
        }

        /* Print the decoded content to stdout */
        printf("%s\n", content);
        free(content); /* release the decoded string */

        return 0; /* success */
    }

    /* ------------------------------------------------------------------ */
    /* MODE B: --bot (interactive chatbot)                                  */
    /* ------------------------------------------------------------------ */

    if (strcmp(argv[1], "--bot") == 0) {

        char  input[MAX_INPUT_LEN]; /* line of text typed by the user  */
        char *json_response;        /* raw JSON returned by neuro_ask  */
        char *answer;               /* decoded content field           */
        int   len;                  /* length of the input string      */

        if (argc != 2) {
            /* --bot takes no additional arguments */
            fprintf(stderr, "Usage: %s --bot\n", argv[0]);
            return 1;
        }

        /*
         * Conversation loop: keep prompting until the user sends EOF
         * (Ctrl-D on most systems).
         */
        while (1) {
            /* Display the prompt and flush immediately so it appears   */
            /* before fgets() blocks waiting for input.                 */
            printf("> What would you like to know? ");
            fflush(stdout);

            /* Read one line of user input */
            if (fgets(input, (int)sizeof(input), stdin) == NULL) {
                /* EOF or read error — end the conversation */
                printf("Terminating\n");
                break;
            }

            /* Strip the trailing newline that fgets leaves in the buffer */
            len = (int)strlen(input);
            if (len > 0 && input[len - 1] == '\n') {
                input[len - 1] = '\0';
                len--;
            }

            /* Ignore empty lines */
            if (len == 0) {
                continue;
            }

            /* Send the question to the AI service */
            json_response = neuro_ask(input);
            if (json_response == NULL) {
                fprintf(stderr, "Error: failed to get a response.\n");
                continue;
            }

            /* Extract the assistant's answer from the JSON */
            answer = extract_content(json_response);
            free(json_response); /* raw JSON no longer needed */

            if (answer == NULL) {
                fprintf(stderr, "Error: could not parse the response.\n");
                continue;
            }

            /* Print the answer */
            printf("%s\n", answer);
            free(answer); /* release the decoded answer string */
        }

        return 0; /* success */
    }

    /* ------------------------------------------------------------------ */
    /* Unknown flag                                                          */
    /* ------------------------------------------------------------------ */

    fprintf(stderr, "Usage: %s [--extract <file> | --bot]\n", argv[0]);
    return 1;
}
