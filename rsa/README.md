# rsa — RSA Encryption & Decryption

## Description

`rsa` encrypts and decrypts integer messages using the
**RSA algorithm** — one of the most widely used public-key
cryptosystems.  The same mathematical operations that this program
performs run every time you connect to a website over HTTPS.

## Build

```bash
gcc -O3 -Wall -Wextra -Werror -pedantic -o rsa src/rsa.c
```

## Usage

```
./rsa enc|dec <pub_exp> <priv_exp> <prime1> <prime2>
```

The message is read from **standard input** as a single integer.

| Argument | Meaning |
|---|---|
| `enc` or `dec` | Encrypt or decrypt the message |
| `<pub_exp>` | Public exponent *e* |
| `<priv_exp>` | Private exponent *d* |
| `<prime1>` | First prime *p* |
| `<prime2>` | Second prime *q* |

The RSA modulus is computed as *N = p × q*.

## Examples

```bash
# Encrypt the message 42
$ echo 42 | ./rsa enc 257 257 173 193
6990

# Decrypt it back
$ echo 6990 | ./rsa dec 257 257 173 193
42

# Roundtrip with larger keys
$ echo 43434343 | ./rsa enc 65537 2278459553 62971 38609 | \
    ./rsa dec 65537 2278459553 62971 38609
43434343

# Error: missing arguments
$ ./rsa
Usage: ./rsa enc|dec <exp_exp> <priv_exp> <prime1> <prime2>

# Error: bad operation
$ ./rsa pop 1 2 3 4
First argument must be 'enc' or 'dec'

# Error: negative numbers
$ ./rsa enc 1 2 -3 4
Negative numbers are not allowed

# Error: non-prime parameters
$ ./rsa enc 1 2 3 4
p and q must be prime

# Error: e not coprime with phi(N)
$ ./rsa enc 3 6 17 19
e is not coprime with phi(N)

# Error: invalid key relationship
$ ./rsa enc 5 6 17 19
e * d mod phi(N) is not 1

# Error: message too large
$ echo 500 | ./rsa enc 5 173 17 19
Message is larger than N
```

## How it works

### Input validation

The program checks all RSA constraints before performing any
computation:

1. All parameters must be **positive** integers.
2. *p* and *q* must be **prime** (tested by trial division up to √n).
3. *e* must be **coprime** with φ(N) = (p−1)(q−1).
4. *e × d* mod φ(N) must equal **1** (modular inverse).
5. The message *m* must be **smaller than** *N*.

### Encryption / Decryption

Both operations are a single modular exponentiation:

| Operation | Formula |
|---|---|
| Encrypt | *c = m^e mod N* |
| Decrypt | *m = c^d mod N* |

### Modular exponentiation (repeated squaring)

Naïve computation of *m^e* for large *e* would be impossibly slow.
The `mod_pow` function uses **repeated squaring**: it examines each
bit of the exponent from least to most significant, squaring the
base at each step and multiplying into the result only when the
current bit is 1.  This reduces the number of multiplications from
*e* to O(log *e*).

## Observations

- Parameters may be up to 10^18 (64-bit signed integers).
- Primality testing uses an optimised 6k±1 trial division, which
  is efficient for values up to ~10^18.
- The GCD is computed iteratively using the Euclidean algorithm
  (the same algorithm from the companion `gcd` exercise).
