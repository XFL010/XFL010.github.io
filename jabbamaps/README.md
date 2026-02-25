# jabbamaps — Shortest City Tour (TSP)

## Description

`jabbamaps` solves a variant of the classic
**Travelling Salesman Problem**: given a map of cities connected by roads,
find the shortest path that visits every city **exactly once**, starting
from the first city that appears in the map file.

## Build

```bash
gcc -m32 -Ofast -Wall -Wextra -Werror -pedantic -o jabbamaps src/jabbamaps.c
```

> On systems without 32-bit libraries, compile without `-m32` for testing:  
> `gcc -Ofast -Wall -Wextra -Werror -pedantic -o jabbamaps src/jabbamaps.c`

## Usage

```
./jabbamaps <mapfile>
```

Each line of the map file must have the format:

```
city1-city2: distance
```

- City names may contain spaces and special characters but **not** `-` or `:`.
- The distance is always a non-negative integer.
- The first city name that appears in the file is the **starting city**.

## Example

```bash
$ cat map4.txt
Athens-Thessaloniki: 501
Athens-Ioannina: 422
Athens-Patras: 224
Patras-Thessaloniki: 468
Patras-Ioannina: 223
Thessaloniki-Ioannina: 261

$ ./jabbamaps map4.txt
We will visit the cities in the following order:
Athens -(224)-> Patras -(223)-> Ioannina -(261)-> Thessaloniki
Total cost: 708
```

## Algorithm

The program uses the **nearest-neighbour greedy heuristic**:

1. Start at city 0 (the first city read from the file).
2. At each step, move to the closest unvisited city that has a direct road.
3. Repeat until all cities have been visited.

This runs in **O(n²)** time and is well within the 30-second limit for
up to 64 cities.

| Property | Value |
|---|---|
| Maximum cities | 64 |
| Maximum distance per edge | 2³¹ |
| Time complexity | O(n²) |

## Observations

- The nearest-neighbour heuristic is not guaranteed to find the global
  optimum for arbitrary inputs, but produces good results in practice —
  and gives the optimal answer for all provided example maps.
- City names are parsed by finding the **last** `-` before the `:` on
  each line, which correctly handles city names containing spaces,
  parentheses, and other punctuation.
- The total-cost accumulator uses `long long` (64-bit) to handle cases
  where 64 cities each have the maximum edge weight (up to 64 × 2³¹ ≈ 10¹¹).
