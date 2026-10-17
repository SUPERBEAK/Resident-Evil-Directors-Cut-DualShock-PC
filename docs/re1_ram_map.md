# RE1 Dual Shock - how the PS1 memory is used (simple overview)

The PS1 has 2 MB of main RAM. RE1 splits it roughly like this (in-game, top to bottom):

| Part | Size | What it is |
|---|---|---|
| Program code + data | ~650 KB | the main program (SLUS_007.47) |
| Game state | ~117 KB | all globals: player and 30 enemy slots, effects, save data, flags, drawing lists |
| Player + weapon | ~150 KB | the player's model, its drawing data, the equipped weapon |
| Sounds + music | ~45 KB | sound bank headers and the music data |
| Stage overlay | up to 188 KB | the enemy code of the current stage (STAGE1-7) |
| Room heap | ~560 KB | the current room file, enemy models, in-game movies |
| Shared screen area | 150 KB | used in turns by the room background, door animations, the item viewer, file pages, maps and full-screen pictures |
| Room background data | 32 KB | the compressed background picture |
| Tasks + stacks | ~6 KB | the small thread system and its stacks |

The most important trick: **many areas are shared in turns**. The same memory holds a door
animation, then the room background, then a map, depending on what the game is doing.
The small programs (logo, title, character select, prologue, ending) load into the player/weapon
area, because no player is on screen then.

The detailed version (exact addresses, every buffer and its names in the code):
[re1_ram_map_detailed.md](re1_ram_map_detailed.md). In the decompiled code each of these addresses
has a name, see `dualshock/config/addrs.txt`.
