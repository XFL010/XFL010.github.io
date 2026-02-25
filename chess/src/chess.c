/*
 * chess.c — Simple chess engine with material-based evaluation
 *
 * Usage: ./chess <fen> <moves> <timeout>
 *
 * Takes three command-line arguments:
 *   fen     — the current board position in Forsyth-Edwards Notation
 *   moves   — space-separated list of legal moves in algebraic notation
 *   timeout — seconds available to decide (not used in 1-ply search)
 *
 * Prints the 0-based index of the chosen move to stdout.
 *
 * The engine parses the FEN to understand the board, simulates each
 * legal move, evaluates the resulting position using material and
 * basic positional heuristics, and picks the move with the best score.
 *
 * Compilation:
 *   gcc -O2 -Wall -Wextra -Werror -pedantic -o chess chess.c
 */

#include <stdio.h>   /* printf, fprintf, sscanf */
#include <stdlib.h>  /* atoi */
#include <string.h>  /* strlen, strcmp, strchr, strncpy, memcpy */
#include <ctype.h>   /* isupper, islower, toupper, tolower */

/* Board dimensions */
#define ROWS 8
#define COLS 8

/* Maximum number of legal moves we expect to receive */
#define MAX_MOVES 256

/* Maximum length of a single move string (e.g. "Qxd8+") */
#define MAX_MOVE_LEN 16

/* Piece-value table used for material evaluation (centipawns) */
#define VAL_PAWN   100
#define VAL_KNIGHT 320
#define VAL_BISHOP 330
#define VAL_ROOK   500
#define VAL_QUEEN  900
#define VAL_KING   20000

/* -------------------------------------------------------------------------
 * piece_value — Returns the centipawn value of a piece character.
 *
 * Uppercase = white, lowercase = black.  Returns a positive value for
 * white pieces and negative for black pieces.
 * ---------------------------------------------------------------------- */
static int piece_value(char piece)
{
    switch (piece) {
        case 'P': return  VAL_PAWN;
        case 'N': return  VAL_KNIGHT;
        case 'B': return  VAL_BISHOP;
        case 'R': return  VAL_ROOK;
        case 'Q': return  VAL_QUEEN;
        case 'K': return  VAL_KING;
        case 'p': return -VAL_PAWN;
        case 'n': return -VAL_KNIGHT;
        case 'b': return -VAL_BISHOP;
        case 'r': return -VAL_ROOK;
        case 'q': return -VAL_QUEEN;
        case 'k': return -VAL_KING;
        default:  return 0;
    }
}

/* -------------------------------------------------------------------------
 * parse_fen — Decodes a FEN string into an 8×8 board array.
 *
 * board[row][col] is set to the piece character ('P','n','K', etc.)
 * or '.' for empty squares.  Row 0 is rank 8 (top of the board).
 *
 * Also extracts the side to move: 'w' or 'b'.
 *
 * Returns 1 on success, 0 on failure.
 * ---------------------------------------------------------------------- */
static int parse_fen(const char *fen, char board[ROWS][COLS], char *side)
{
    int row = 0;  /* current row being filled (0 = rank 8) */
    int col = 0;  /* current column in that row */
    const char *p = fen; /* read cursor */

    /* Initialise the board to empty */
    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            board[row][col] = '.';
        }
    }

    row = 0;
    col = 0;

    /* Parse the piece-placement section (before the first space) */
    while (*p != '\0' && *p != ' ') {
        if (*p == '/') {
            row++; /* move to the next rank */
            col = 0;
        } else if (*p >= '1' && *p <= '8') {
            col += (*p - '0'); /* skip empty squares */
        } else {
            /* Must be a piece letter */
            if (row < ROWS && col < COLS) {
                board[row][col] = *p;
            }
            col++;
        }
        p++;
    }

    /* Parse the active colour (w or b) */
    if (*p == ' ') {
        p++;
    }
    *side = *p; /* 'w' or 'b' */

    return 1; /* success */
}

/* -------------------------------------------------------------------------
 * evaluate — Static evaluation of a board position.
 *
 * Returns a score in centipawns from white's perspective:
 *   positive = white is better, negative = black is better.
 *
 * Considers:
 *   - Material balance (piece values)
 *   - Central pawn bonus (pawns on e4/d4/e5/d5)
 *   - Knight/bishop activity (centre proximity)
 * ---------------------------------------------------------------------- */
static int evaluate(char board[ROWS][COLS])
{
    int score = 0;  /* accumulated evaluation */
    int row, col;   /* loop counters */
    char piece;     /* piece at the current square */

    /* Piece-square bonus: small reward for occupying central squares */
    /* Indexed by [row][col]; higher values near the centre */
    static const int centre_bonus[ROWS][COLS] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  1,  2,  2,  1,  0,  0},
        { 0,  0,  2,  3,  3,  2,  0,  0},
        { 0,  0,  2,  3,  3,  2,  0,  0},
        { 0,  0,  1,  2,  2,  1,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0}
    };

    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            piece = board[row][col];
            if (piece == '.') {
                continue; /* empty square */
            }

            /* Add material value */
            score += piece_value(piece);

            /* Add positional bonus for knights and bishops */
            if (piece == 'N' || piece == 'B') {
                score += centre_bonus[row][col] * 5; /* small positional bonus */
            } else if (piece == 'n' || piece == 'b') {
                score -= centre_bonus[row][col] * 5;
            }

            /* Pawn advancement bonus: reward pawns closer to promotion */
            if (piece == 'P') {
                score += (7 - row) * 5; /* white pawns gain value as they advance */
            } else if (piece == 'p') {
                score -= row * 5;       /* black pawns gain value as they advance */
            }
        }
    }

    return score;
}

/* -------------------------------------------------------------------------
 * find_piece — Locates a piece on the board for move application.
 *
 * Given a piece character, a target square (dest_row, dest_col), and
 * optional source hints (src_row_hint, src_col_hint: -1 if unknown),
 * finds the source square of the piece that can reach the destination.
 *
 * For pawns, knights, bishops, rooks, and queens, checks standard
 * movement patterns.  Returns 1 if found, 0 otherwise.
 * ---------------------------------------------------------------------- */
static int find_piece(char board[ROWS][COLS], char piece,
                      int dest_row, int dest_col,
                      int src_row_hint, int src_col_hint,
                      int *out_row, int *out_col)
{
    int r, c;   /* candidate source square */
    int dr, dc; /* direction deltas for sliding pieces */
    int i;      /* loop counter */

    /* Knight move offsets */
    static const int knight_dr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int knight_dc[] = {-1, 1, -2, 2, -2, 2, -1, 1};

    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            /* Must match the piece type */
            if (board[r][c] != piece) {
                continue;
            }

            /* Apply hints if given */
            if (src_row_hint >= 0 && r != src_row_hint) {
                continue;
            }
            if (src_col_hint >= 0 && c != src_col_hint) {
                continue;
            }

            /* Check if this piece can actually reach the destination */
            switch (toupper((unsigned char)piece)) {
                case 'P': {
                    /* White pawn movement */
                    if (isupper((unsigned char)piece)) {
                        /* Non-capture: move forward (row decreases for white) */
                        if (c == dest_col && board[dest_row][dest_col] == '.') {
                            if (r - 1 == dest_row) { goto found; }
                            /* Two-square advance from starting rank */
                            if (r == 6 && dest_row == 4 && board[5][c] == '.') {
                                goto found;
                            }
                        }
                        /* Capture: diagonal forward */
                        if (r - 1 == dest_row &&
                            (c - 1 == dest_col || c + 1 == dest_col)) {
                            goto found; /* captures and en-passant */
                        }
                    } else {
                        /* Black pawn movement (row increases) */
                        if (c == dest_col && board[dest_row][dest_col] == '.') {
                            if (r + 1 == dest_row) { goto found; }
                            if (r == 1 && dest_row == 3 && board[2][c] == '.') {
                                goto found;
                            }
                        }
                        if (r + 1 == dest_row &&
                            (c - 1 == dest_col || c + 1 == dest_col)) {
                            goto found;
                        }
                    }
                    break;
                }

                case 'N': {
                    /* Knight: check all 8 L-shaped offsets */
                    for (i = 0; i < 8; i++) {
                        if (r + knight_dr[i] == dest_row &&
                            c + knight_dc[i] == dest_col) {
                            goto found;
                        }
                    }
                    break;
                }

                case 'B': {
                    /* Bishop: check four diagonal directions */
                    for (dr = -1; dr <= 1; dr += 2) {
                        for (dc = -1; dc <= 1; dc += 2) {
                            int sr = r + dr, sc = c + dc;
                            while (sr >= 0 && sr < ROWS &&
                                   sc >= 0 && sc < COLS) {
                                if (sr == dest_row && sc == dest_col) {
                                    goto found;
                                }
                                if (board[sr][sc] != '.') {
                                    break; /* blocked by another piece */
                                }
                                sr += dr;
                                sc += dc;
                            }
                        }
                    }
                    break;
                }

                case 'R': {
                    /* Rook: check four orthogonal directions */
                    static const int rdr[] = {-1, 1, 0, 0};
                    static const int rdc[] = {0, 0, -1, 1};
                    for (i = 0; i < 4; i++) {
                        int sr = r + rdr[i], sc = c + rdc[i];
                        while (sr >= 0 && sr < ROWS &&
                               sc >= 0 && sc < COLS) {
                            if (sr == dest_row && sc == dest_col) {
                                goto found;
                            }
                            if (board[sr][sc] != '.') {
                                break;
                            }
                            sr += rdr[i];
                            sc += rdc[i];
                        }
                    }
                    break;
                }

                case 'Q': {
                    /* Queen: combination of rook + bishop */
                    static const int qdr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
                    static const int qdc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
                    for (i = 0; i < 8; i++) {
                        int sr = r + qdr[i], sc = c + qdc[i];
                        while (sr >= 0 && sr < ROWS &&
                               sc >= 0 && sc < COLS) {
                            if (sr == dest_row && sc == dest_col) {
                                goto found;
                            }
                            if (board[sr][sc] != '.') {
                                break;
                            }
                            sr += qdr[i];
                            sc += qdc[i];
                        }
                    }
                    break;
                }

                case 'K': {
                    /* King: one square in any direction */
                    if (abs(r - dest_row) <= 1 && abs(c - dest_col) <= 1) {
                        goto found;
                    }
                    break;
                }
            }
        }
    }

    return 0; /* piece not found */

found:
    *out_row = r;
    *out_col = c;
    return 1; /* success */
}

/* -------------------------------------------------------------------------
 * apply_move — Applies an algebraic notation move to the board.
 *
 * Modifies 'board' in place.  Handles:
 *   - Pawn moves (e4, exd5)
 *   - Piece moves (Nf3, Bb5, Qd1)
 *   - Captures (Bxe5, exd5)
 *   - Castling (O-O, O-O-O)
 *   - Promotions (e8=Q)
 *   - Disambiguation (Nbd2, R1a3)
 *   - Check/mate suffixes (+, #) are ignored
 *
 * Returns 1 on success, 0 if the move could not be applied.
 * ---------------------------------------------------------------------- */
static int apply_move(char board[ROWS][COLS], const char *move, char side)
{
    int dest_row, dest_col;     /* destination square */
    int src_row, src_col;       /* source square (found by search) */
    int src_row_hint = -1;      /* disambiguation: known source row */
    int src_col_hint = -1;      /* disambiguation: known source column */
    char piece;                 /* piece being moved */
    char promote_to = '\0';     /* promotion piece if any */
    const char *p = move;       /* read cursor into the move string */
    int len;                    /* length of the move string */

    len = (int)strlen(move);

    /* ---- Handle castling ---- */
    if (strcmp(move, "O-O") == 0 || strcmp(move, "O-O+") == 0 ||
        strcmp(move, "O-O#") == 0) {
        /* Kingside castling */
        int row = (side == 'w') ? 7 : 0; /* rank 1 for white, rank 8 for black */
        board[row][4] = '.';              /* king leaves e1/e8 */
        board[row][7] = '.';              /* rook leaves h1/h8 */
        board[row][6] = (side == 'w') ? 'K' : 'k'; /* king to g1/g8 */
        board[row][5] = (side == 'w') ? 'R' : 'r'; /* rook to f1/f8 */
        return 1;
    }
    if (strcmp(move, "O-O-O") == 0 || strcmp(move, "O-O-O+") == 0 ||
        strcmp(move, "O-O-O#") == 0) {
        /* Queenside castling */
        int row = (side == 'w') ? 7 : 0;
        board[row][4] = '.';              /* king leaves e1/e8 */
        board[row][0] = '.';              /* rook leaves a1/a8 */
        board[row][2] = (side == 'w') ? 'K' : 'k'; /* king to c1/c8 */
        board[row][3] = (side == 'w') ? 'R' : 'r'; /* rook to d1/d8 */
        return 1;
    }

    /* ---- Strip check/mate suffixes ---- */
    /* We work with a local copy to avoid modifying the input */
    {
        char buf[MAX_MOVE_LEN];
        int  blen;
        strncpy(buf, move, MAX_MOVE_LEN - 1);
        buf[MAX_MOVE_LEN - 1] = '\0';
        blen = (int)strlen(buf);
        while (blen > 0 && (buf[blen - 1] == '+' || buf[blen - 1] == '#')) {
            buf[--blen] = '\0'; /* remove trailing + or # */
        }
        p = buf;
        len = blen;

        /* ---- Check for promotion (e.g. "e8=Q") ---- */
        if (len >= 4 && buf[len - 2] == '=') {
            promote_to = buf[len - 1]; /* the promotion piece letter */
            buf[len - 2] = '\0';       /* remove the "=X" suffix */
            len -= 2;
        }

        /* ---- Determine the piece type ---- */
        if (isupper((unsigned char)*p) &&
            *p != 'O' &&
            (*p == 'N' || *p == 'B' || *p == 'R' || *p == 'Q' || *p == 'K')) {
            /* Named piece (N, B, R, Q, K) */
            piece = (side == 'w') ? *p : (char)tolower((unsigned char)*p);
            p++;
            len--;
        } else {
            /* Pawn move */
            piece = (side == 'w') ? 'P' : 'p';
        }

        /* ---- Extract destination square (always the last two characters) ---- */
        if (len < 2) {
            return 0; /* move string too short */
        }

        dest_col = buf[strlen(buf) - 2] - 'a'; /* file: a=0 .. h=7 */
        dest_row = '8' - buf[strlen(buf) - 1];  /* rank: 8=0 .. 1=7 */

        if (dest_row < 0 || dest_row >= ROWS ||
            dest_col < 0 || dest_col >= COLS) {
            return 0; /* invalid destination */
        }

        /* ---- Parse disambiguation and capture markers ---- */
        /* Whatever is between the piece letter and the destination square */
        {
            const char *mid = p;
            const char *dest_start = buf + strlen(buf) - 2;
            while (mid < dest_start) {
                if (*mid == 'x') {
                    /* capture marker — skip */
                } else if (*mid >= 'a' && *mid <= 'h') {
                    src_col_hint = *mid - 'a'; /* file disambiguation */
                } else if (*mid >= '1' && *mid <= '8') {
                    src_row_hint = '8' - *mid;  /* rank disambiguation */
                }
                mid++;
            }
        }

        /* ---- Find the source square ---- */
        if (!find_piece(board, piece, dest_row, dest_col,
                        src_row_hint, src_col_hint,
                        &src_row, &src_col)) {
            return 0; /* could not locate the piece */
        }

        /* ---- Handle en passant for pawn captures ---- */
        if (toupper((unsigned char)piece) == 'P' &&
            src_col != dest_col &&
            board[dest_row][dest_col] == '.') {
            /* Pawn moved diagonally to an empty square → en passant */
            board[src_row][dest_col] = '.'; /* remove the captured pawn */
        }

        /* ---- Perform the move ---- */
        board[src_row][src_col] = '.'; /* clear the source square */

        if (promote_to != '\0') {
            /* Place the promotion piece with the correct colour */
            board[dest_row][dest_col] = (side == 'w')
                ? (char)toupper((unsigned char)promote_to)
                : (char)tolower((unsigned char)promote_to);
        } else {
            board[dest_row][dest_col] = piece; /* place the piece on its destination */
        }
    }

    return 1; /* success */
}

/* -------------------------------------------------------------------------
 * choose_move — Selects the best move from the given list.
 *
 * Parameters:
 *   fen     — current board position in FEN notation
 *   moves   — space-separated list of legal moves
 *   timeout — seconds available (unused in 1-ply search)
 *
 * Returns the 0-based index of the chosen move.
 *
 * Strategy: for each legal move, apply it to a copy of the board,
 * evaluate the resulting position, and pick the move that yields the
 * best score for the side to move.
 * ---------------------------------------------------------------------- */
int choose_move(char *fen, char *moves, int timeout)
{
    char board[ROWS][COLS];         /* board parsed from FEN */
    char trial[ROWS][COLS];         /* temporary board for trying moves */
    char side;                      /* 'w' or 'b' */
    char move_list[MAX_MOVES][MAX_MOVE_LEN]; /* individual move strings */
    int  num_moves = 0;             /* total number of moves parsed */
    int  best_idx  = 0;             /* index of the best move so far */
    int  best_score;                /* score of the best move so far */
    int  score;                     /* score of the current candidate move */
    int  i;                         /* loop counter */
    char *tok;                      /* tokeniser pointer */
    char moves_copy[4096];          /* mutable copy of the moves string */

    (void)timeout; /* unused in this simple 1-ply search */

    /* Parse the FEN into our board representation */
    parse_fen(fen, board, &side);

    /* Split the moves string into individual move tokens */
    strncpy(moves_copy, moves, sizeof(moves_copy) - 1);
    moves_copy[sizeof(moves_copy) - 1] = '\0';
    tok = strtok(moves_copy, " ");
    while (tok != NULL && num_moves < MAX_MOVES) {
        strncpy(move_list[num_moves], tok, MAX_MOVE_LEN - 1);
        move_list[num_moves][MAX_MOVE_LEN - 1] = '\0';
        num_moves++;
        tok = strtok(NULL, " ");
    }

    if (num_moves == 0) {
        return 0; /* no moves available */
    }

    /* Initialise with the worst possible score */
    best_score = (side == 'w') ? -9999999 : 9999999;

    /* Evaluate each candidate move */
    for (i = 0; i < num_moves; i++) {
        /* Make a copy of the board to try this move on */
        memcpy(trial, board, sizeof(trial));

        /* Apply the move to the trial board */
        if (!apply_move(trial, move_list[i], side)) {
            continue; /* could not apply — skip this move */
        }

        /* Evaluate the resulting position */
        score = evaluate(trial);

        /* Pick the move that is best for the side to move */
        if (side == 'w') {
            if (score > best_score) {
                best_score = score; /* white wants to maximise */
                best_idx   = i;
            }
        } else {
            if (score < best_score) {
                best_score = score; /* black wants to minimise */
                best_idx   = i;
            }
        }
    }

    return best_idx;
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(int argc, char *argv[])
{
    int result; /* index of the chosen move */

    /* ------------------------------------------------------------------ */
    /* 1. Validate command-line arguments                                   */
    /* ------------------------------------------------------------------ */

    if (argc != 4) {
        /* Three arguments required: fen, moves, timeout */
        fprintf(stderr, "Usage: %s <fen> <moves> <timeout>\n", argv[0]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* 2. Choose the best move using 1-ply evaluation                       */
    /* ------------------------------------------------------------------ */

    result = choose_move(argv[1], argv[2], atoi(argv[3]));

    /* ------------------------------------------------------------------ */
    /* 3. Print the chosen move index and exit                              */
    /* ------------------------------------------------------------------ */

    printf("%d\n", result);

    return 0; /* success */
}
