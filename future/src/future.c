/*
 * future.c — Simple Moving Average (SMA) predictor
 *
 * Usage: ./future <filename> [--window N (default: 50)]
 *
 * Reads a sequence of floating-point numbers from a file and prints
 * their Simple Moving Average using the last N values (the window).
 *
 * Compilation:
 *   gcc -Os -Wall -Wextra -Werror -pedantic -o future future.c
 */

#include <stdio.h>   /* printf, fprintf, fopen, fclose, fscanf, rewind */
#include <stdlib.h>  /* malloc, free, strtoll, exit */
#include <string.h>  /* strcmp */

/* Default window size when --window is not provided */
#define DEFAULT_WINDOW 50LL

int main(int argc, char *argv[])
{
    long long window;   /* number of values to include in the average */
    const char *filename; /* path to the data file */
    FILE *fp;           /* file handle for the data file */
    double *buf;        /* circular buffer holding the last 'window' values */
    double val;         /* single value read from the file */
    long long count;    /* total number of values read from the file */
    long long pos;      /* current write position in the circular buffer */
    double sum;         /* sum of the values in the circular buffer */
    long long i;        /* loop counter */

    /* ------------------------------------------------------------------ */
    /* 1. Parse command-line arguments                                      */
    /* ------------------------------------------------------------------ */

    /* Need at least one argument: the filename */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> [--window N (default: 50)]\n",
                argv[0]);
        return 1;
    }

    filename = argv[1]; /* second token on the command line is the filename */
    window   = DEFAULT_WINDOW; /* start with the default window size */

    if (argc == 4) {
        /* User supplied --window N: verify the flag name is correct */
        if (strcmp(argv[2], "--window") != 0) {
            fprintf(stderr,
                    "Usage: %s <filename> [--window N (default: 50)]\n",
                    argv[0]);
            return 1;
        }

        {
            char *endptr; /* points past the last digit converted */
            /* Convert the window argument to a 64-bit integer */
            window = strtoll(argv[3], &endptr, 10);
            /* If endptr did not advance, or there are leftover chars, fail */
            if (endptr == argv[3] || *endptr != '\0') {
                fprintf(stderr,
                        "Usage: %s <filename> [--window N (default: 50)]\n",
                        argv[0]);
                return 1;
            }
        }
    } else if (argc != 2) {
        /* Any other argument count (3, 5, …) is invalid */
        fprintf(stderr, "Usage: %s <filename> [--window N (default: 50)]\n",
                argv[0]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 2. Validate the window size before touching the file                 */
    /* ------------------------------------------------------------------ */

    /* A window of 0 or negative makes no sense */
    if (window < 1) {
        fprintf(stderr, "Window too small!\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 3. Allocate a circular buffer large enough for 'window' doubles      */
    /* ------------------------------------------------------------------ */

    /*
     * We cast window to size_t for the allocation.
     * If window is astronomically large (e.g. 10^12), malloc will fail and
     * we report it before even opening the file.
     */
    buf = (double *)malloc((size_t)window * sizeof(double));
    if (buf == NULL) {
        /* malloc returns NULL when it cannot satisfy the request */
        fprintf(stderr, "Failed to allocate window memory\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 4. Open the data file                                                */
    /* ------------------------------------------------------------------ */

    fp = fopen(filename, "r"); /* open for reading (text mode) */
    if (fp == NULL) {
        /* fopen returns NULL if the file does not exist or is unreadable */
        fprintf(stderr, "Cannot open file: %s\n", filename);
        free(buf);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 5. Read all values into the circular buffer                          */
    /* ------------------------------------------------------------------ */

    /*
     * A circular buffer of size W overwrites its oldest entry once full.
     * After reading all N values, buf[] contains the last W values
     * (in some cyclic order), regardless of how large N is.
     * This avoids storing the entire file in memory.
     */
    count = 0; /* how many numbers we have read so far */
    pos   = 0; /* next slot to write in the buffer     */

    while (fscanf(fp, "%lf", &val) == 1) {
        buf[pos % window] = val; /* wrap around when pos reaches 'window' */
        pos++;                   /* advance the write position            */
        count++;                 /* increment total-read counter          */
    }

    fclose(fp); /* we are done reading; close the file handle */

    /* ------------------------------------------------------------------ */
    /* 6. Validate that the window is not larger than the data set          */
    /* ------------------------------------------------------------------ */

    if (window > count) {
        /* We cannot average more values than exist in the file */
        fprintf(stderr, "Window too large!\n");
        free(buf);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 7. Compute the SMA as the arithmetic mean of the buffer contents     */
    /* ------------------------------------------------------------------ */

    sum = 0.0;
    for (i = 0; i < window; i++) {
        sum += buf[i]; /* add every slot in the circular buffer */
    }

    /* Divide by the window size to get the average */
    printf("%.2f\n", sum / (double)window);

    /* ------------------------------------------------------------------ */
    /* 8. Clean up and exit                                                 */
    /* ------------------------------------------------------------------ */

    free(buf); /* release the buffer allocated in step 3 */
    return 0;  /* exit code 0 signals success */
}
