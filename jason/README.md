# jason — JSON Extractor & AI Chatbot

## Description

`jason` is a two-mode command-line tool that works with OpenAI-style
JSON responses.

| Mode | Flag | What it does |
|---|---|---|
| Extraction | `--extract <file>` | Reads a JSON file and prints `choices[0].message.content` |
| Chatbot | `--bot` | Interactively queries an AI service and prints its answers |

## Build

```bash
gcc -Wall -Wextra -Werror -pedantic -c src/neurolib.c
gcc -Wall -Wextra -Werror -pedantic -c src/jason.c
gcc -o jason neurolib.o jason.o -lssl -lcrypto
```

## Usage

### Extraction mode

```bash
./jason --extract response.json
```

Reads `response.json`, finds the path `choices[0].message.content`, and
prints the decoded string to stdout.  Prints `Not an accepted JSON!` to
stderr and exits with code 1 if the file is not valid or does not
contain the expected path.

### Chatbot mode

```bash
export OPENAI_API_KEY=sk-...   # optional — see below
./jason --bot
```

Repeatedly asks:

```
> What would you like to know?
```

and prints the AI's answer until the user sends **EOF** (Ctrl-D).

If `OPENAI_API_KEY` is **not** set, the program responds with built-in
humorous placeholder answers so the program can be demonstrated without
a paid API key.

## Examples

```bash
# Extraction
$ ./jason --extract json/1.json
Why do programmers always mix up Halloween and Christmas?
Because Oct 31 == Dec 25!
$ echo $?
0

$ ./jason --extract json/2.json 2> err
$ echo $?
1
$ cat err
Not an accepted JSON!

# Chatbot (no API key — mock responses)
$ ./jason --bot
> What would you like to know? What is the last digit of pi?
I'd answer that, but I don't want to ruin the surprise.
> What would you like to know? ^D
Terminating

# Chatbot (with a real API key)
$ export OPENAI_API_KEY=sk-...
$ ./jason --bot
> What would you like to know? What is the last digit of pi?
Pi is an irrational number and has no last digit...
> What would you like to know? ^D
Terminating
```

## How it works

### JSON extraction (`jason.c`)

The `extract_content()` function navigates the JSON tree by searching
for the literal key strings in order:

```
"choices"  →  [  →  {  →  "message"  →  {  →  "content"  →  "…"
```

It then decodes JSON escape sequences (`\n`, `\"`, `\\`, etc.) into
their corresponding characters before printing.

### AI communication (`neurolib.c`)

`neuro_ask(prompt)` is the single public function of the library:

- **Without an API key**: returns one of five pre-written funny
  answers, cycling through them on successive calls.
- **With `OPENAI_API_KEY`**: opens a TCP socket to `api.openai.com:443`,
  wraps it in a TLS session using OpenSSL, sends an HTTP/1.1 POST
  request to `/v1/chat/completions`, reads the response, and returns
  the JSON body.

## Observations

- JSON files can be up to 1 MB; a fixed 2 MB buffer is used for the
  HTTP response.
- The program has no memory leaks: every `malloc` is paired with a
  matching `free` before each return path.
- The JSON parser is intentionally minimal — it targets only the
  specific response shape produced by the OpenAI chat-completions API
  rather than attempting to be a general-purpose JSON library.
