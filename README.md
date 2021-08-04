# "Non-Euclidean" Field-of-view prototype

What the project is

## Installation

Requires NCURSES.
Compile with any C compiler; something like:
```
cc -lncurses nonEuclidFOV.c
```

## Usage

Run the resulting executable.

The avatar is the red tile; move with the **arrow keys**. **Ctrl-C** to exit.

To change demo worlds, edit the code. Uncomment lines 67-91, comment out lines 93-121, change lines 20 and 21 to reflect the world's new dimensions, and recompile.

The demo world currently uncommented (demo world #2) is a single L-shaped room with portals in two corners, demonstrating rotation in the rendered tiles. Demo word #1 is an "impossible corridor" that is longer on one side than the other.
