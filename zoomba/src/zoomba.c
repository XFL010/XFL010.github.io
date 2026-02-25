/*
 * zoomba.c — Shortest path for a Roomba-like robot on a grid
 *
 * Usage: reads from stdin
 *
 * Input format:
 *   Line 1:  N               (grid size, NxN)
 *   Line 2:  sx sy tx ty     (start and target coordinates)
 *   Lines 3..N+2:  N digits per line (0 = free, 1 = obstacle)
 *
 * Finds the shortest path from (sx, sy) to (tx, ty) avoiding obstacles,
 * then prints the path as a sequence of moves: U (up), D (down),
 * L (left), R (right).  Prints "0" if no path exists.
 *
 * Uses Breadth-First Search (BFS) which guarantees the shortest path
 * on an unweighted grid.
 *
 * Compilation:
 *   gcc -O3 -Wall -Wextra -Werror -pedantic -o zoomba zoomba.c
 */

#include <stdio.h>   /* printf, scanf */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset */

/* Maximum grid dimension the program supports */
#define MAX_N 10000

/* Direction vectors: up, down, left, right */
static const int dx[] = {-1, 1, 0, 0};
static const int dy[] = {0, 0, -1, 1};

/* Characters corresponding to each direction (U/D/L/R) */
static const char dir_char[] = {'U', 'D', 'L', 'R'};

/* -------------------------------------------------------------------------
 * BFS queue node: stores a grid coordinate as a single integer index.
 * For an NxN grid, cell (r,c) is stored as r*N + c.
 * ---------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * find_path — BFS shortest path from (sx,sy) to (tx,ty) on the grid.
 *
 * Parameters:
 *   n     — grid dimension (NxN)
 *   room  — 1-D array of size n*n; room[r*n+c] is 0 (free) or 1 (wall)
 *   sx,sy — starting row and column
 *   tx,ty — target row and column
 *
 * Prints the path as a string of U/D/L/R characters, or "0" if
 * no path exists.
 * ---------------------------------------------------------------------- */
static void find_path(int n, int *room,
                      int sx, int sy, int tx, int ty)
{
    int *came_from;   /* for each cell, stores the index of its predecessor   */
    int *direction;   /* for each cell, stores which direction we arrived from */
    int *queue;       /* BFS queue of cell indices (r*n + c)                  */
    int  head, tail;  /* queue front and back pointers                        */
    int  start_idx;   /* linear index of the start cell                       */
    int  target_idx;  /* linear index of the target cell                      */
    int  cur;         /* current cell being expanded                          */
    int  cr, cc;      /* row and column of current cell                       */
    int  nr, nc;      /* row and column of neighbour cell                     */
    int  ni;          /* linear index of neighbour cell                       */
    int  i;           /* direction loop counter                               */
    char *path;       /* reconstructed path string                            */
    int   path_len;   /* length of the reconstructed path                     */
    int   idx;        /* index into the path string during backtracking       */

    start_idx  = sx * n + sy; /* convert 2D start to 1D index */
    target_idx = tx * n + ty; /* convert 2D target to 1D index */

    /* Handle the trivial case where start equals target */
    if (start_idx == target_idx) {
        printf("\n"); /* empty path — already at the goal */
        return;
    }

    /* Allocate BFS data structures */
    came_from = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    direction = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    queue     = (int *)malloc((size_t)n * (size_t)n * sizeof(int));

    if (came_from == NULL || direction == NULL || queue == NULL) {
        printf("0\n"); /* memory allocation failed */
        free(came_from);
        free(direction);
        free(queue);
        return;
    }

    /* Initialise came_from to -1 (unvisited) for every cell */
    memset(came_from, -1, (size_t)n * (size_t)n * sizeof(int));

    /* Mark the start cell as visited (its own predecessor) */
    came_from[start_idx] = start_idx;

    /* Initialise the BFS queue with the start cell */
    head = 0;
    tail = 0;
    queue[tail++] = start_idx;

    /* ---- BFS main loop ---- */
    while (head < tail) {
        cur = queue[head++]; /* dequeue the front cell */

        cr = cur / n; /* extract row from linear index */
        cc = cur % n; /* extract column from linear index */

        /* Try all four cardinal directions */
        for (i = 0; i < 4; i++) {
            nr = cr + dx[i]; /* neighbour row */
            nc = cc + dy[i]; /* neighbour column */

            /* Check bounds */
            if (nr < 0 || nr >= n || nc < 0 || nc >= n) {
                continue; /* out of the grid */
            }

            ni = nr * n + nc; /* linear index of the neighbour */

            /* Skip walls and already-visited cells */
            if (room[ni] != 0 || came_from[ni] != -1) {
                continue;
            }

            came_from[ni] = cur; /* record where we came from */
            direction[ni] = i;   /* record which direction led here */

            /* Check if we have reached the target */
            if (ni == target_idx) {
                /* Reconstruct the path by backtracking from target to start */
                path_len = 0;
                idx = ni;
                while (idx != start_idx) {
                    path_len++;          /* count the steps */
                    idx = came_from[idx]; /* move to predecessor */
                }

                /* Allocate a string for the path (+1 for NUL) */
                path = (char *)malloc((size_t)path_len + 1);
                if (path == NULL) {
                    printf("0\n");
                    free(came_from);
                    free(direction);
                    free(queue);
                    return;
                }

                path[path_len] = '\0'; /* NUL-terminate */

                /* Fill in the path characters backwards */
                idx = ni;
                while (idx != start_idx) {
                    path_len--;
                    path[path_len] = dir_char[direction[idx]];
                    idx = came_from[idx];
                }

                printf("%s\n", path); /* print the path */

                free(path);
                free(came_from);
                free(direction);
                free(queue);
                return;
            }

            queue[tail++] = ni; /* enqueue the neighbour for later expansion */
        }
    }

    /* BFS exhausted all reachable cells without finding the target */
    printf("0\n");

    free(came_from);
    free(direction);
    free(queue);
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    int  n;                   /* grid dimension */
    int  sx, sy, tx, ty;     /* start and target coordinates */
    int *room;               /* 1-D array storing the NxN grid */
    int  i, j;               /* loop counters for row and column */
    int  digit;              /* single cell value (0 or 1) */

    /* ------------------------------------------------------------------ */
    /* 1. Read the grid size                                               */
    /* ------------------------------------------------------------------ */

    if (scanf("%d", &n) != 1) {
        return 1; /* failed to read N */
    }

    if (n <= 0 || n > MAX_N) {
        return 1; /* grid size out of supported range */
    }

    /* ------------------------------------------------------------------ */
    /* 2. Read start and target coordinates                                */
    /* ------------------------------------------------------------------ */

    if (scanf("%d %d %d %d", &sx, &sy, &tx, &ty) != 4) {
        return 1; /* failed to read coordinates */
    }

    /* Validate that coordinates are within the grid */
    if (sx < 0 || sx >= n || sy < 0 || sy >= n ||
        tx < 0 || tx >= n || ty < 0 || ty >= n) {
        return 1; /* coordinate out of bounds */
    }

    /* ------------------------------------------------------------------ */
    /* 3. Allocate the grid as a 1-D array (row-major order)               */
    /* ------------------------------------------------------------------ */

    room = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (room == NULL) {
        return 1; /* allocation failed */
    }

    /* ------------------------------------------------------------------ */
    /* 4. Read the grid values (each cell is a single digit: 0 or 1)       */
    /* ------------------------------------------------------------------ */

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (scanf("%1d", &digit) != 1) {
                free(room);
                return 1; /* failed to read a cell */
            }
            room[i * n + j] = digit; /* store in row-major order */
        }
    }

    /* ------------------------------------------------------------------ */
    /* 5. Validate that start and target cells are not obstacles            */
    /* ------------------------------------------------------------------ */

    if (room[sx * n + sy] != 0 || room[tx * n + ty] != 0) {
        free(room);
        return 1; /* start or target is blocked */
    }

    /* ------------------------------------------------------------------ */
    /* 6. Run BFS to find and print the shortest path                      */
    /* ------------------------------------------------------------------ */

    find_path(n, room, sx, sy, tx, ty);

    /* ------------------------------------------------------------------ */
    /* 7. Clean up and exit                                                */
    /* ------------------------------------------------------------------ */

    free(room); /* release the grid memory */
    return 0;   /* success */
}
