# Quoridor Game (C + Optional raylib UI)

A C implementation of a Quoridor-style game with:
- full terminal gameplay, and
- an optional graphical UI built with `raylib`.
The project is structured to match the required phases described in `main.pdf`:
1) board/map creation and ASCII rendering,
2) full two-player game logic,
3) player-vs-computer mode,
4) binary save/load,
5) magic box (talisman/reward) effects.
6) simple 4-player local mode (PvP4).

## Project Scope (Mapped to `main.pdf`)

### Phase 1 - Map and Board Rendering
- Reads map data from a text file (`program map.txt` mode).
- Parses board size, player positions, wall counts, and wall coordinates.
- Prints an ASCII board with coordinates.
- Validates invalid map entries and reports clear errors.

### Phase 2 - Two-Player Rules
- Turn-based gameplay between Player 1 and Player 2.
- Legal movement rules:
- Adjacent orthogonal move.
- Mandatory jump over opponent when directly adjacent and open behind.
- Diagonal side-step when direct jump is blocked.
- Legal wall placement rules:
- Horizontal (`H`) or Vertical (`V`) only.
- No overlap or crossing conflicts.
- Placement rejected if it blocks all paths for any player.
- Winner detection when a player reaches the target row.

### Bonus Option 1 - Simple 4-Player Local Mode
- New mode at startup: `3 = PvP4`.
- Four human players take turns on one board.
- Start sides:
- `P1`: bottom, goal is top row.
- `P2`: top, goal is bottom row.
- `P3`: left, goal is right column.
- `P4`: right, goal is left column.
- Movement is intentionally simple in this mode: one-step orthogonal moves only.

### Phase 3 - Player vs Computer
- Mode selection: PvP or PvC at game start.
- Computer evaluates valid moves/walls and chooses a better action (distance/impact based).
- AI still respects all legal move and wall constraints.
- All standard game rules are still enforced for AI turns.

### Phase 4 - Save and Reload
- Save command available during gameplay (`save [filename]`).
- Load command available during gameplay (`load [filename]`).
- Startup prompt allows loading an existing saved game or starting a new one.
- Uses a binary save format with version and header validation.
- Current save writer uses version `2` (supports 2-player and 4-player state).
- Loader supports both version `1` (older saves) and version `2`.
- Includes robust corruption checks and metadata validation.

### Phase 5 - Magic Box Effects
- Every turn, one random player receives a random magic effect.
- Effects are categorized as `talisman` or `reward`.
- Implemented effects:
- `remove_all_walls`: clears all placed walls from board.
- `decrease_walls`: decreases target player's remaining walls (2/3/5).
- `block_turns`: blocks target player for 1 or 2 turns.
- `increase_walls`: increases target player's remaining walls (2/3/5).
- `steal_walls`: transfers 1 or 2 walls from another player to target (in 4-player mode, from the current richest opponent).
- Effects are applied immediately.

## Data Structures and Algorithms
- Uses `struct` and `enum` throughout game state modeling, per project requirements.
- Uses graph traversal (BFS-style path check) to validate that wall placement does not eliminate all legal paths.
- Uses `rand()` for random behavior and seeds once with `srand(time(NULL))`.

## Build Requirements
- A C compiler:
- MSVC (`cl`) on Windows, or
- GCC/MinGW.
- Terminal (VS Code terminal, PowerShell, Command Prompt, etc.).
- Optional for graphics mode: `raylib` headers/libs and build flag `USE_RAYLIB`.

## Build and Run

### MSVC (Developer PowerShell / Developer Command Prompt)
```bat
cl /W4 /std:c11 main.c game.c io.c save.c ai.c rayui.c /Fe:main.exe
main.exe
```

### MSVC + raylib (graphical mode)
```bat
cl /W4 /std:c11 /DUSE_RAYLIB main.c game.c io.c save.c ai.c rayui.c /I"path\to\raylib\include" /Fe:main.exe /link /LIBPATH:"path\to\raylib\lib" raylib.lib winmm.lib gdi32.lib user32.lib shell32.lib
main.exe
```

### GCC / MinGW
```bash
gcc -std=c11 -Wall -Wextra -O2 main.c game.c io.c save.c ai.c rayui.c -o quoridor.exe
./quoridor.exe
```

### GCC / MinGW + raylib (graphical mode)
```bash
gcc -std=c11 -Wall -Wextra -O2 -DUSE_RAYLIB main.c game.c io.c save.c ai.c rayui.c -lraylib -lopengl32 -lgdi32 -lwinmm -o quoridor.exe
./quoridor.exe
```

### Map-Render Mode (Phase 1 style input)
```bash
main.exe input.txt
```
or
```bash
quoridor.exe input.txt
```

## Runtime Flow
- On startup, program asks for a save filename.
- Press Enter to start a new game.
- Enter a filename to load and continue from previous state.
- For a new game, set:
- mode (PvP/PvC/PvP4),
- board size,
- walls per player,
- player names.
- In `PvC`, Player 2 name is set to `COMPUTER`.
- In `PvP4`, all 4 player names are requested.
- If built with `USE_RAYLIB`, game runs in graphical mode by default.
- CLI flags:
- `--console`: force terminal mode.
- `--gui`: force graphical mode (prints warning if raylib is not enabled in build).

## In-Game Commands
- `move r c` or `r c`: move to `(row, col)`.
- `wall r c H|V` or `r c H|V`: place wall at `(row, col)`.
- `save [filename]`: save current game state.
- `load [filename]`: load a saved game state.
- `quit` or `exit`: terminate game.

## Graphical Controls (raylib mode)
- `M`: move mode.
- `W`: wall mode.
- `H` / `V`: horizontal/vertical wall direction.
- `Right Click`: toggle wall direction.
- `Left Click`: apply action on board.
- `F2`: toggle Magic Box effects on/off.
- `F5`: open save prompt (type filename, Enter to save).
- `F9`: open load prompt (type filename, Enter to load).
- `Ctrl+S`: quick-save using last used filename.
- `Ctrl+L`: quick-load using last used filename.
- At game end: save prompt opens automatically; choose filename or press `Esc` to skip.
- `Esc`: exit window.

## Board Symbols
- `1`: Player 1.
- `2`: Player 2 (or computer in PvC).
- `3`: Player 3 (PvP4).
- `4`: Player 4 (PvP4).
- `.`: empty cell.
- `|`: vertical wall segment.
- `---`: horizontal wall segment.

## Win Condition
- Player 1 wins by reaching row `0`.
- Player 2 wins by reaching row `size - 1`.
- In 4-player mode:
- Player 3 wins by reaching column `size - 1`.
- Player 4 wins by reaching column `0`.

## Map File Format (`program map.txt`)

Text format:
```text
N
p1_row p1_col
p2_row p2_col
k1
row col H|V   (k1 lines)
k2
row col H|V   (k2 lines)
```

Where:
- `N` is board size (`2..50`).
- Coordinates are zero-based.
- `H` is horizontal wall, `V` is vertical wall.
- For compatibility with different map writers, wall lines can also be read as `H|V row col`.

Example:
```text
10
0 1
7 3
1
1 1 H
1
5 5 V
```

## Save File Details
- Binary save with:
- magic header (`QDR1`),
- format version (`v2` for newly saved files),
- full board and gameplay state,
- player names,
- wall/block arrays.
- Backward-compatible loading for old `v1` saves.
- Save path priority:
- `%USERPROFILE%\Documents\save_games\<filename>`
- fallback: current working directory.

## Repository Structure
- `main.c`: program entry, setup, CLI options, console/game-mode dispatch.
- `game.c`, `game.h`: board model, rules, path checks, magic effects.
- `io.c`, `io.h`: input parsing and board/status rendering.
- `ai.c`, `ai.h`: computer decision logic.
- `save.c`, `save.h`: binary serialization/deserialization.
- `rayui.c`, `rayui.h`: optional raylib graphical loop and rendering.
- `input.txt`: sample map input.
- `main.pdf`: project specification.

## Notes
- Output and build artifacts (`.exe`, `.obj`, etc.) are ignored via `.gitignore`.
- Project is split across multiple source files to keep logic modular and maintainable.

## Academic Attribution
This project was jointly developed by **Ali Madadi** and **Sina Ziaei** for **Ferdowsi University of Mashhad** under the supervision of **Professor Mostafa Nouri Bayegi**.
