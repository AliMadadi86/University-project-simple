# Simple Quoridor (for easier presentation)

This folder is a simpler version of the project.
It does not change the original files.

## Features kept
- Phase 1 map render from text file
- Two-player game rules (move + wall + path check)
- PvC mode with simple random-valid AI
- Binary save/load
- Magic box effects each turn

## Features removed to stay simple
- 4-player mode
- Advanced AI scoring logic
- Multi-version/complex save format

## Build (MSVC)
```bat
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /std:c11 simple_version\main.c simple_version\game.c simple_version\io.c simple_version\save.c /Fe:simple_version\simple_main.exe
```

## Run
```bat
simple_version\simple_main.exe
```

Map-only mode:
```bat
simple_version\simple_main.exe simple_version\input.txt
```

## Commands
- `move r c` or `r c`
- `wall r c H|V` or `r c H|V`
- `save [file]`
- `load [file]`
- `quit`
