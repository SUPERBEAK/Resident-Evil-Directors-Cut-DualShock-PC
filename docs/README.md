# Documentation

How to set up and build the project is in the main [README](../README.md). Topics with two documents have
a short overview and a `_detailed` version.

## Working on the decompilation

| Document | What it covers |
|---|---|
| [decomp_guide.md](decomp_guide.md) | Start here: the tools in `dualshock/tools/`, naming conventions, the folder layout |
| [matching_guide.md](matching_guide.md) | How to write C that GCC 2.6.3 turns into exactly the original code: toolchain, Capcom's coding idioms, matching rules, pitfalls |
| [code_notes.md](code_notes.md) | How the source is organized and named, and why some odd-looking forms must stay for the byte match |
| [tools.md](tools.md) / [tools_detailed.md](tools_detailed.md) | The installed tools, their versions, and the scripts in `tools/` |

## The game

| Document | What it covers |
|---|---|
| [game_notes.md](game_notes.md) | How the game works, as found while decompiling it: stages, rooms, entities, player, enemies, menus, items |
| [re1_ram_map.md](re1_ram_map.md) / [re1_ram_map_detailed.md](re1_ram_map_detailed.md) | What lives where in the PS1's 2 MB of RAM |
| [psyq_sdk.md](psyq_sdk.md) / [psyq_sdk_detailed.md](psyq_sdk_detailed.md) | Which Sony PsyQ library versions the game uses, and where they sit |
| [disc_contents.md](disc_contents.md) / [disc_contents_detailed.md](disc_contents_detailed.md) | What's on the Dual Shock disc: folders, file types, audio and movie formats |

## Background

| Document | What it covers |
|---|---|
| [project_history.md](project_history.md) | How the decompilation got to its current state, phase by phase |
