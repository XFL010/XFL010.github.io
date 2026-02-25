/*
 * jabbamaps.c — Travelling Salesman Problem (nearest-neighbour heuristic)
 *
 * Usage: ./jabbamaps <mapfile>
 *
 * Reads a map file where every line has the format
 *   city1-city2: distance
 * and finds the minimum-cost Hamiltonian path starting from the first
 * city that appears in the file, visiting every city exactly once.
 * Uses the nearest-neighbour greedy strategy: at each step go to the
 * closest unvisited city.
 *
 * Compilation:
 *   gcc -m32 -Ofast -Wall -Wextra -Werror -pedantic -o jabbamaps jabbamaps.c
 */

#include <stdio.h>   /* printf, fprintf, fopen, fclose, fgets */
#include <stdlib.h>  /* atoi, exit */
#include <string.h>  /* strcmp, strncpy, strchr, strlen */
#include <limits.h>  /* LLONG_MAX */

/* Maximum number of cities the program supports */
#define MAX_CITIES 64

/* Maximum number of characters in a city name (including NUL terminator) */
#define MAX_NAME 256

/* Sentinel value meaning "no direct road between these two cities" */
#define NO_ROAD (-1)

/* -------------------------------------------------------------------------
 * Data structures
 * ---------------------------------------------------------------------- */

/* City names stored in the order they were first encountered */
static char city_names[MAX_CITIES][MAX_NAME];

/* Number of cities added so far */
static int num_cities = 0;

/*
 * Distance matrix.
 * dist[i][j] holds the road distance between city i and city j,
 * or NO_ROAD if there is no direct connection.
 * Distances are stored as long long to accommodate values up to 2^31.
 */
static long long dist[MAX_CITIES][MAX_CITIES];

/* -------------------------------------------------------------------------
 * Helper: find_or_add_city
 *
 * Searches city_names[] for 'name'.  If found, returns its index.
 * If not found, registers the name and returns the new index.
 * Returns -1 if the city table is already full.
 * ---------------------------------------------------------------------- */
static int find_or_add_city(const char *name)
{
    int i; /* loop index */

    /* Linear search through the names already registered */
    for (i = 0; i < num_cities; i++) {
        if (strcmp(city_names[i], name) == 0) {
            return i; /* found an existing entry */
        }
    }

    /* Not found — add a new entry if there is room */
    if (num_cities >= MAX_CITIES) {
        return -1; /* table is full */
    }

    /* Copy the name (at most MAX_NAME-1 characters) and NUL-terminate */
    strncpy(city_names[num_cities], name, MAX_NAME - 1);
    city_names[num_cities][MAX_NAME - 1] = '\0';

    return num_cities++; /* return the new index and increment the counter */
}

/* -------------------------------------------------------------------------
 * Helper: trim_spaces
 *
 * Removes leading and trailing ASCII space/tab/CR/LF characters from
 * the NUL-terminated string pointed to by 's', in place.
 * ---------------------------------------------------------------------- */
static void trim_spaces(char *s)
{
    int len;   /* current string length */
    char *p;   /* pointer used to skip leading whitespace */

    if (s == NULL || *s == '\0') {
        return; /* nothing to trim */
    }

    /* Remove trailing whitespace by overwriting with NUL */
    len = (int)strlen(s);
    while (len > 0 &&
           (s[len - 1] == ' '  || s[len - 1] == '\t' ||
            s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }

    /* Remove leading whitespace by shifting the string left */
    p = s;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (p != s) {
        /* memmove is safe for overlapping regions */
        memmove(s, p, (size_t)(strlen(p) + 1));
    }
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(int argc, char *argv[])
{
    FILE       *fp;                      /* handle for the map file        */
    char        line[1024];              /* one line read from the file    */
    char        city1[MAX_NAME];         /* name of the first city on a line  */
    char        city2[MAX_NAME];         /* name of the second city on a line */
    char       *colon;                   /* pointer to ':' in the line     */
    char       *dash;                    /* pointer to the last '-' before ':' */
    char       *p;                       /* general-purpose pointer        */
    int         c1, c2;                  /* city indices                   */
    int         len1, len2;              /* lengths of extracted name substrings */
    long long   d;                       /* distance read from the line    */
    int         i, j;                    /* loop counters                  */

    /* Nearest-neighbour algorithm variables */
    int         visited[MAX_CITIES];     /* 1 if city i has already been visited */
    int         path[MAX_CITIES];        /* city indices in visit order    */
    long long   edge_costs[MAX_CITIES];  /* cost of each edge in the path  */
    int         curr;                    /* current city index             */
    int         next;                    /* next city to visit             */
    long long   best_dist;              /* shortest distance to an unvisited city */
    long long   total_cost;             /* accumulated path cost          */
    int         step;                    /* current step in building the path */

    /* ------------------------------------------------------------------ */
    /* 1. Validate command-line arguments                                   */
    /* ------------------------------------------------------------------ */

    if (argc != 2) {
        /* Exactly one argument (the map filename) is required */
        fprintf(stderr, "Usage: %s <mapfile>\n", argv[0]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 2. Initialise the distance matrix                                    */
    /* ------------------------------------------------------------------ */

    for (i = 0; i < MAX_CITIES; i++) {
        for (j = 0; j < MAX_CITIES; j++) {
            dist[i][j] = NO_ROAD; /* no connection by default */
        }
    }

    /* ------------------------------------------------------------------ */
    /* 3. Open and parse the map file                                       */
    /* ------------------------------------------------------------------ */

    fp = fopen(argv[1], "r"); /* open for reading */
    if (fp == NULL) {
        fprintf(stderr, "Cannot open file: %s\n", argv[1]);
        return 1;
    }

    while (fgets(line, (int)sizeof(line), fp) != NULL) {

        /* Skip blank lines */
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }

        /* Locate the ':' that separates the city pair from the distance */
        colon = strchr(line, ':');
        if (colon == NULL) {
            continue; /* malformed line — skip silently */
        }

        /*
         * Locate the LAST '-' that appears before the ':'.
         * City names may contain spaces, parentheses, etc., but must not
         * contain '-' or ':', so the last '-' before ':' is always the
         * separator between city1 and city2.
         */
        dash = NULL;
        for (p = line; p < colon; p++) {
            if (*p == '-') {
                dash = p; /* keep updating — we want the last one */
            }
        }
        if (dash == NULL) {
            continue; /* no '-' found before ':' — skip the line */
        }

        /* Extract city1: everything from the start of the line up to dash */
        len1 = (int)(dash - line);
        if (len1 <= 0 || len1 >= MAX_NAME) {
            continue; /* name too short or too long */
        }
        strncpy(city1, line, (size_t)len1);
        city1[len1] = '\0';
        trim_spaces(city1); /* remove any surrounding whitespace */

        /* Extract city2: everything from dash+1 to colon */
        len2 = (int)(colon - dash - 1);
        if (len2 <= 0 || len2 >= MAX_NAME) {
            continue;
        }
        strncpy(city2, dash + 1, (size_t)len2);
        city2[len2] = '\0';
        trim_spaces(city2);

        /* Extract the distance: the integer after ':' (atoi skips spaces) */
        d = (long long)atoi(colon + 1);

        /* Register both cities and record their mutual distance */
        c1 = find_or_add_city(city1);
        c2 = find_or_add_city(city2);

        if (c1 == -1 || c2 == -1) {
            /* City table is full — cannot add more cities */
            fprintf(stderr, "Too many cities (maximum %d)\n", MAX_CITIES);
            fclose(fp);
            return 1;
        }

        dist[c1][c2] = d; /* road is bidirectional */
        dist[c2][c1] = d;
    }

    fclose(fp); /* done reading the map */

    /* ------------------------------------------------------------------ */
    /* 4. Check that we actually read some cities                           */
    /* ------------------------------------------------------------------ */

    if (num_cities < 1) {
        fprintf(stderr, "No cities found in %s\n", argv[1]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 5. Nearest-neighbour greedy TSP                                      */
    /*                                                                      */
    /* Starting from city 0 (the first city encountered in the file),      */
    /* at each step move to the closest city not yet visited.               */
    /* ------------------------------------------------------------------ */

    /* Initialise: no cities visited, starting at city 0 */
    for (i = 0; i < num_cities; i++) {
        visited[i] = 0; /* 0 = not yet visited */
    }

    path[0]    = 0; /* start at the first city */
    visited[0] = 1; /* mark it as visited      */
    total_cost = 0; /* accumulated travel cost */

    for (step = 1; step < num_cities; step++) {
        curr      = path[step - 1]; /* city we are currently at */
        next      = -1;             /* best candidate so far    */
        best_dist = LLONG_MAX;      /* shortest distance found  */

        /* Scan all cities for the nearest unvisited one */
        for (j = 0; j < num_cities; j++) {
            if (!visited[j] &&              /* must not have been visited */
                dist[curr][j] != NO_ROAD && /* must have a direct road    */
                dist[curr][j] < best_dist) {
                best_dist = dist[curr][j];
                next      = j;
            }
        }

        if (next == -1) {
            /* The graph is not fully connected — no path from curr */
            fprintf(stderr,
                    "No road from %s to any unvisited city\n",
                    city_names[curr]);
            return 1;
        }

        path[step]          = next;      /* record the city we chose */
        edge_costs[step - 1] = best_dist; /* record the edge cost     */
        visited[next]        = 1;         /* mark it as visited       */
        total_cost          += best_dist; /* add to the running total */
    }

    /* ------------------------------------------------------------------ */
    /* 6. Print the result                                                  */
    /* ------------------------------------------------------------------ */

    printf("We will visit the cities in the following order:\n");

    /* Print the first city without a preceding arrow */
    printf("%s", city_names[path[0]]);

    /* Print each subsequent city together with the edge that leads to it */
    for (i = 1; i < num_cities; i++) {
        printf(" -(%lld)-> %s", edge_costs[i - 1], city_names[path[i]]);
    }

    printf("\n"); /* end the city-list line */

    printf("Total cost: %lld\n", total_cost); /* print the total distance */

    return 0; /* success */
}
