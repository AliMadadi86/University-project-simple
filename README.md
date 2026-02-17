# University Project (Simple Version)

This repository contains a simplified C implementation of Quoridor for course presentation.

## Features kept
- Phase 1 map render from text file
- Two-player game rules (move + wall + path check)
- PvC mode with simple random-valid AI
- Binary save/load
- Magic box effects each turn (4 effects)

## Features removed to stay simple
- 4-player mode
- Advanced AI scoring logic
- Multi-version/complex save format

## Build (MSVC)
```bat
build_simple_msvc.cmd
```

Manual build:
```bat
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /std:c11 main.c game.c io.c save.c /Fe:simple_main.exe
```

## Run
```bat
simple_main.exe
```

Map-only mode:
```bat
simple_main.exe input.txt
```

## Commands
- `move r c` or `r c`
- `wall r c H|V` or `r c H|V`
- `save [file]`
- `load [file]`
- `quit`
