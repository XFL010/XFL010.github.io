# zoomba — Shortest Path for a Grid Robot

## Description

`zoomba` finds the **shortest path** for a Roomba-like cleaning robot
on an N×N grid.  Given a starting position, a target position, and a
map of obstacles, it prints the optimal sequence of moves (U/D/L/R)
to reach the target — or `0` if the target is unreachable.

## Build

```bash
gcc -O3 -Wall -Wextra -Werror -pedantic -o zoomba src/zoomba.c
```

## Usage

```
./zoomba < input.txt
```

Input is read from **standard input** in the following format:

```
N
sx sy tx ty
<N lines of N digits each>
```

| Field | Meaning |
|---|---|
| `N` | Grid size (N×N), max 10000 |
| `sx sy` | Starting row and column (0-indexed) |
| `tx ty` | Target row and column (0-indexed) |
| Grid digits | `0` = free cell, `1` = obstacle |

## Examples

```bash
# Simple 5×5 grid
$ echo "5
0 0 4 4
00000
01010
00000
01010
00000" | ./zoomba
DDDDRRRR

# 3×3 grid (blocked)
$ echo "3
0 0 2 2
010
111
010" | ./zoomba
0

# 3×3 grid (open)
$ echo "3
0 0 2 2
000
000
000" | ./zoomba
DDRR
```

## How it works

### BFS (Breadth-First Search)

The program uses **BFS** to explore the grid level by level, starting
from the Zoomba's initial position.  BFS guarantees that the first
time a cell is reached, it is reached via a **shortest path**.

1. Enqueue the start cell and mark it as visited.
2. Dequeue the front cell; try all four neighbours (U/D/L/R).
3. For each valid, unvisited, obstacle-free neighbour, record its
   predecessor and the direction taken, then enqueue it.
4. Stop as soon as the target cell is reached.

### Path reconstruction

Once the target is found, the path is reconstructed by following
the `came_from` chain backwards from target to start, collecting
the direction letters in reverse.  The resulting string is then
printed forwards.

### Memory layout

The N×N grid is stored as a 1-D array in row-major order:
`room[r * N + c]`.  The BFS queue, predecessor array, and direction
array are all 1-D arrays of size N².

## Observations

- BFS runs in **O(N²)** time and space, which handles the maximum
  grid size of 10,000 × 10,000 within the time and memory limits.
- The directions U/D/L/R correspond to row decrements/increments
  and column decrements/increments respectively.
- If multiple shortest paths exist, BFS returns one depending on
  the order of neighbour exploration (up, down, left, right).
