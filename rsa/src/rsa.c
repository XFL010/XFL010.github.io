/*
 * rsa.c — RSA encryption and decryption tool
 *
 * Usage: ./rsa enc|dec <pub_exp> <priv_exp> <prime1> <prime2>
 *
 * Reads a single integer message from stdin and encrypts or decrypts it
 * using the RSA algorithm.
 *
 *   Encryption:  c = m^e mod N   (where N = p * q)
 *   Decryption:  m = c^d mod N
 *
 * The program validates all RSA constraints before proceeding:
 *   - All parameters must be positive
 *   - p and q must be prime
 *   - e must be coprime with phi(N)
 *   - e * d mod phi(N) must equal 1
 *   - The message must be smaller than N
 *
 * Uses modular exponentiation (repeated squaring) for efficiency.
 *
 * Compilation:
 *   gcc -O3 -Wall -Wextra -Werror -pedantic -o rsa rsa.c
 */

#include <stdio.h>   /* printf, fprintf, scanf */
#include <stdlib.h>  /* strtoll, exit */
#include <string.h>  /* strcmp */

/* -------------------------------------------------------------------------
 * is_prime — Primality test by trial division.
 *
 * Returns 1 if 'n' is prime, 0 otherwise.
 * Works for all values in the 64-bit positive range.
 * Optimises by testing 2 and 3 first, then only 6k ± 1.
 * ---------------------------------------------------------------------- */
static int is_prime(long long n)
{
    long long i; /* trial divisor */

    if (n < 2) {
        return 0; /* 0 and 1 are not prime */
    }
    if (n < 4) {
        return 1; /* 2 and 3 are prime */
    }
    if (n % 2 == 0 || n % 3 == 0) {
        return 0; /* divisible by 2 or 3 → composite */
    }

    /*
     * Every prime > 3 can be written as 6k ± 1.
     * Test divisors of that form up to √n.
     * i*i <= n avoids computing the square root explicitly.
     */
    for (i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return 0; /* found a divisor → composite */
        }
    }

    return 1; /* no divisor found → prime */
}

/* -------------------------------------------------------------------------
 * gcd — Greatest Common Divisor via the Euclidean algorithm.
 *
 * Returns gcd(a, b).  Both inputs must be non-negative.
 * ---------------------------------------------------------------------- */
static long long gcd(long long a, long long b)
{
    long long temp; /* holds the remainder during the swap */

    while (b != 0) {
        temp = b;       /* save divisor */
        b    = a % b;   /* remainder becomes the new divisor */
        a    = temp;     /* old divisor becomes the new dividend */
    }

    return a; /* when b reaches 0, a holds the GCD */
}

/* -------------------------------------------------------------------------
 * mod_pow — Modular exponentiation by repeated squaring.
 *
 * Computes (base^exp) mod mod efficiently in O(log exp) multiplications.
 * All intermediate products are kept below mod^2 by reducing after each
 * multiplication, which fits in a 64-bit unsigned long long as long as
 * mod < 2^32.
 *
 * Uses unsigned long long internally to avoid signed overflow when
 * multiplying two values that are each less than mod.
 * ---------------------------------------------------------------------- */
static long long mod_pow(long long base, long long exp, long long mod)
{
    unsigned long long result; /* accumulated result */
    unsigned long long b;     /* working copy of the base */
    long long e;              /* working copy of the exponent */

    if (mod == 1) {
        return 0; /* anything mod 1 is 0 */
    }

    result = 1;
    b      = (unsigned long long)(base % mod); /* reduce base first */
    e      = exp;

    /*
     * Repeated squaring: examine each bit of the exponent from LSB to MSB.
     * If the current bit is 1, multiply result by the current power of base.
     * Then square the base for the next bit position.
     */
    while (e > 0) {
        if (e % 2 == 1) {
            result = (result * b) % (unsigned long long)mod; /* odd bit → multiply */
        }
        e = e / 2;                                             /* shift exponent right */
        b = (b * b) % (unsigned long long)mod;                 /* square the base */
    }

    return (long long)result;
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(int argc, char *argv[])
{
    long long e, d, p, q;   /* RSA parameters from command line */
    long long m;             /* message read from stdin */
    long long n_val;         /* N = p * q (the RSA modulus) */
    long long phi;           /* phi(N) = (p - 1) * (q - 1) */
    long long result;        /* encrypted or decrypted output */
    char *endptr;            /* used by strtoll for validation */

    /* ------------------------------------------------------------------ */
    /* 1. Validate argument count                                          */
    /* ------------------------------------------------------------------ */

    if (argc != 6) {
        /* Exactly 5 arguments required: op, e, d, p, q */
        fprintf(stderr,
                "Usage: %s enc|dec <exp_exp> <priv_exp> <prime1> <prime2>\n",
                argv[0]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 2. Check the operation flag                                         */
    /* ------------------------------------------------------------------ */

    if (strcmp(argv[1], "enc") != 0 && strcmp(argv[1], "dec") != 0) {
        /* First argument must be either "enc" or "dec" */
        fprintf(stderr, "First argument must be 'enc' or 'dec'\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 3. Parse numeric arguments from the command line                     */
    /* ------------------------------------------------------------------ */

    e = strtoll(argv[2], &endptr, 10); /* public exponent */
    if (*endptr != '\0') {
        fprintf(stderr,
                "Usage: %s enc|dec <exp_exp> <priv_exp> <prime1> <prime2>\n",
                argv[0]);
        return 1;
    }

    d = strtoll(argv[3], &endptr, 10); /* private exponent */
    if (*endptr != '\0') {
        fprintf(stderr,
                "Usage: %s enc|dec <exp_exp> <priv_exp> <prime1> <prime2>\n",
                argv[0]);
        return 1;
    }

    p = strtoll(argv[4], &endptr, 10); /* first prime */
    if (*endptr != '\0') {
        fprintf(stderr,
                "Usage: %s enc|dec <exp_exp> <priv_exp> <prime1> <prime2>\n",
                argv[0]);
        return 1;
    }

    q = strtoll(argv[5], &endptr, 10); /* second prime */
    if (*endptr != '\0') {
        fprintf(stderr,
                "Usage: %s enc|dec <exp_exp> <priv_exp> <prime1> <prime2>\n",
                argv[0]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 4. All parameters must be positive                                  */
    /* ------------------------------------------------------------------ */

    if (e <= 0 || d <= 0 || p <= 0 || q <= 0) {
        fprintf(stderr, "Negative numbers are not allowed\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 5. p and q must be prime numbers                                    */
    /* ------------------------------------------------------------------ */

    if (!is_prime(p) || !is_prime(q)) {
        fprintf(stderr, "p and q must be prime\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 6. Compute N = p * q and phi(N) = (p-1) * (q-1)                    */
    /* ------------------------------------------------------------------ */

    n_val = p * q;                /* the RSA modulus */
    phi   = (p - 1) * (q - 1);   /* Euler's totient of N */

    /* ------------------------------------------------------------------ */
    /* 7. e must be coprime with phi(N)                                    */
    /* ------------------------------------------------------------------ */

    if (gcd(e, phi) != 1) {
        /* e shares a factor with phi(N) → RSA keys are invalid */
        fprintf(stderr, "e is not coprime with phi(N)\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 8. e * d mod phi(N) must equal 1 (modular inverse relationship)     */
    /* ------------------------------------------------------------------ */

    if ((e * d) % phi != 1) {
        /* d is not the modular inverse of e under phi(N) */
        fprintf(stderr, "e * d mod phi(N) is not 1\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 9. Read the message from standard input                             */
    /* ------------------------------------------------------------------ */

    if (scanf("%lld", &m) != 1) {
        /* Could not read an integer from stdin */
        fprintf(stderr, "Failed to read message\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 10. The message must be positive and smaller than N                  */
    /* ------------------------------------------------------------------ */

    if (m < 0) {
        fprintf(stderr, "Negative numbers are not allowed\n");
        return 1;
    }

    if (m >= n_val) {
        /* RSA can only encrypt messages in the range [0, N-1] */
        fprintf(stderr, "Message is larger than N\n");
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 11. Perform encryption or decryption                                */
    /* ------------------------------------------------------------------ */

    if (strcmp(argv[1], "enc") == 0) {
        /* Encrypt: c = m^e mod N */
        result = mod_pow(m, e, n_val);
    } else {
        /* Decrypt: m = c^d mod N */
        result = mod_pow(m, d, n_val);
    }

    /* ------------------------------------------------------------------ */
    /* 12. Print the result and exit                                        */
    /* ------------------------------------------------------------------ */

    printf("%lld\n", result);

    return 0; /* success */
}
