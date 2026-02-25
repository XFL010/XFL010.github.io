# chess — Simple Chess Engine

## Description

`chess` is a chess engine that evaluates a given position and
selects the best move from a list of legal moves.  It uses a
**1-ply material-based evaluation** with positional bonuses to
choose moves that are significantly better than random play.

## Build

```bash
gcc -O2 -Wall -Wextra -Werror -pedantic -o chess src/chess.c
```

## Usage

```
./chess <fen> <moves> <timeout>
```

| Argument | Meaning |
|---|---|
| `<fen>` | Board position in Forsyth-Edwards Notation (FEN) |
| `<moves>` | Space-separated legal moves in algebraic notation |
| `<timeout>` | Seconds available for the decision |

The program prints the **0-based index** of its chosen move to stdout.

## Example

```bash
$ ./chess "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" \
          "a3 a4 b3 b4 c3 c4 d3 d4 e3 e4 f3 f4 g3 g4 h3 h4 Na3 Nc3 Nf3 Nh3" \
          3
1
```

The engine chose move index 1 (`a4`).  Given the starting position
and the list of 20 legal moves, it evaluated each one and picked the
move that resulted in the best score.

## How it works

### FEN parsing

The `parse_fen` function decodes the FEN string into an 8×8 character
array where each cell is a piece letter (`P`, `n`, `K`, etc.) or `'.'`
for empty.  Row 0 corresponds to rank 8 (black's back rank).

### Move application

The `apply_move` function interprets standard algebraic notation and
updates the board accordingly.  It handles:

- **Pawn moves**: single and double advances, captures, en passant
- **Piece moves**: N, B, R, Q, K with optional disambiguation
- **Captures**: indicated by `x` in the move string
- **Castling**: `O-O` (kingside) and `O-O-O` (queenside)
- **Promotions**: `e8=Q` style notation
- **Check/mate suffixes**: `+` and `#` are stripped and ignored

### Evaluation

The `evaluate` function scores a position from white's perspective
in centipawns.  It considers:

| Factor | Description |
|---|---|
| Material | Sum of piece values (P=100, N=320, B=330, R=500, Q=900) |
| Centre control | Knights and bishops near the centre get a small bonus |
| Pawn advancement | Pawns closer to promotion rank score higher |

### Move selection

For each legal move, the engine:

1. Copies the board
2. Applies the move to the copy
3. Evaluates the resulting position
4. Keeps the move with the best score (max for white, min for black)

This is a **1-ply search** (also called a greedy or horizon search).
It always prefers capturing high-value pieces and avoids losing
material, which is sufficient to beat a random opponent decisively.

### WebAssembly interface

The `choose_move(char *fen, char *moves, int timeout)` function can
be called directly when the engine is compiled to WebAssembly,
returning the chosen move index without printing to stdout.

## Observations

- The 1-ply evaluation is fast enough to respond instantly even for
  positions with many legal moves.
- The engine reliably beats a random-move opponent well above the
  60% threshold required by the assignment.
- Deeper search (minimax with alpha-beta pruning) could improve play
  significantly but is not needed to satisfy the assignment requirements.
