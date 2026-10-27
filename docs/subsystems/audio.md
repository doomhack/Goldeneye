# Audio (level music)

Which music plays during a level and when it changes. The music player itself, which
plays a track on one of three channels, is `music.c` at the top of `src/`; the sound
effects player is `snd.c`. This folder decides what those play.

| File | What it holds |
| --- | --- |
| `src/game/audio/levelmusic.c` | Each level's tracks (`g_LevelMusic`), the music state machine (`levelMusicSetState`), the music slots the AI scripts use, and stopping the tank's engine sound at level end |

## A level's tracks

`g_LevelMusic` gives each solo level up to three tracks:

| Track | Played on | What it is |
| --- | --- | --- |
| Main (`levelMusicGetTrack`) | channel 1 | The level's music. A level not in the table (the multiplayer levels) gets a random one from `g_LevelMusicRandom` |
| X track (`levelMusicGetXTrack`) | channel 2 | The tense version, played while the AI wants it (alarms, fights) |
| Ambient (`levelMusicGetAmbientTrack`) | channel 3 | Background sound under the music; only Surface 2 has one (wind) |

## The music state

`g_LevelMusicState` is one of the `MUSICSTATE_*` values, and `levelMusicSetState` does
whatever it takes to go from the old state to the new one: start or stop channels, fade
the X track in or out.

| State | What plays |
| --- | --- |
| `MUSICSTATE_SILENT` | nothing |
| `MUSICSTATE_LEVEL` | the main track |
| `MUSICSTATE_XTRACK` | the main track and the X track |
| `MUSICSTATE_WATCH` | the watch menu music (the game is paused) |
| `MUSICSTATE_LEVEL_AMBIENT`, `MUSICSTATE_XTRACK_AMBIENT` | as above, with the ambient track |
| `MUSICSTATE_MP_DEATH` | the death music in multiplayer, until the dead players' death screens have finished |

`levelMusicStart` runs when a level loads and starts the main track. Opening the watch
calls `levelMusicPause`, which remembers the state and switches to the watch music, and
closing it calls `levelMusicResume`. `levelMusicStop` runs when the level unloads.

## Music slots

The AI scripts can ask for the X track with a command that names a slot (0 to 3), how
long to keep it going after the reason stops, and how long at most
(`levelMusicSlotStart`, `levelMusicSlotStop`). `levelMusicTick` runs each frame from
`lvTick`: it counts each slot down, and switches to the X track while any slot is active
and back to the main track when none is.

## Traps

* The file was called `mp_music.c` but is used in solo too; the music state functions
  were `set_missionstate` and friends.
* The slot timers were called "minutes" and "seconds" (`g_LevelMusicSlotLinger`,
  `g_LevelMusicSlotTotal`): both are in frames.
* The original US code did nothing when a level started from `MUSICSTATE_SILENT` going
  to silent again; PAL stops all three channels. The US build here takes the PAL path
  (`BACKPORT_R1`).
* Solo death music isn't part of the state machine: `bondview2.c` stops the slots, sets
  the state to silent and plays the death track on channel 1 itself.
