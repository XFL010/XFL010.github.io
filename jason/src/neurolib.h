/*
 * neurolib.h — Public interface for the AI query library.
 *
 * This library sends a text question to an AI service (OpenAI) over HTTPS
 * and returns the raw JSON response body.  If the OPENAI_API_KEY
 * environment variable is not set it returns a pre-canned JSON response
 * so that the program still behaves sensibly without a real API key.
 *
 * Compilation (together with jason.c):
 *   gcc -Wall -Wextra -Werror -pedantic -c neurolib.c
 *   gcc -Wall -Wextra -Werror -pedantic -c jason.c
 *   gcc -o jason neurolib.o jason.o -lssl -lcrypto
 */

#ifndef NEUROLIB_H
#define NEUROLIB_H

/*
 * neuro_ask — Query the AI service with a natural-language question.
 *
 * Parameters:
 *   prompt  NUL-terminated string containing the user's question.
 *
 * Returns:
 *   A dynamically allocated NUL-terminated string containing the raw
 *   JSON response from the service, or NULL if the request fails.
 *   The CALLER is responsible for calling free() on the returned pointer.
 *
 * Environment:
 *   OPENAI_API_KEY — when set, real requests are sent to api.openai.com.
 *                    When not set, a built-in mock response is returned.
 */
char *neuro_ask(const char *prompt);

#endif /* NEUROLIB_H */
