# future — Simple Moving Average Predictor

## Description

`future` reads a sequence of floating-point numbers from a text file and
computes their **Simple Moving Average (SMA)** — the arithmetic mean of
the last *N* values, where *N* is called the *window*.

This is a widely used technique in time-series analysis and algorithmic
trading to smooth out short-term noise and identify trends.

## Build

```bash
gcc -Os -Wall -Wextra -Werror -pedantic -o future src/future.c
```

## Usage

```
./future <filename> [--window N (default: 50)]
```

| Argument | Meaning |
|---|---|
| `<filename>` | Path to a text file containing whitespace-separated numbers |
| `--window N` | Size of the sliding window (default: 50) |

## Examples

```bash
$ cat values.txt
9 7 7 1 4 4 4 38 8 4

$ ./future values.txt --window 10
8.60

$ ./future values.txt --window 3
16.67

$ ./future values.txt --window 1
4.00

$ ./future values.txt --window 0
Window too small!

$ ./future values.txt --window 12
Window too large!

$ ./future values.txt --window 1000000000000
Failed to allocate window memory
```

## How it works

The program uses a **circular buffer** of size *W* (the window).  
As each number is read from the file it is written to `buf[pos % W]`,
overwriting the oldest entry once the buffer is full.  
After the entire file has been read, `buf` contains exactly the last *W*
values regardless of the total file size, using only O(W) memory.  
The SMA is then the sum of those *W* values divided by *W*.

## Observations

- The window is validated *before* opening the file (`< 1` → error), so
  astronomically large window values (e.g. 10¹²) fail at the `malloc`
  call rather than requiring a full file read.
- Numbers in the file may appear on the same line or on different lines;
  `fscanf` treats all whitespace (spaces, tabs, newlines) as separators.
- The result is printed with exactly two decimal places (`%.2f`).
