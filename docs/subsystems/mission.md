# Mission

A mission's objectives and briefing text: reading them from the setup file, working out
each objective's status, and telling the player when one changes. The AI scripts that
drive most of a mission (setting stage flags, opening doors) are in
[guards](guards.md).

| File | What it holds |
| --- | --- |
| `src/game/mission/mission.c` | `g_Mission`: mission-wide flags AI scripts set (exit on a button press, the player's controls taken away, invincible, explosions round the player, the tank blowing up on death) |
| `src/game/mission/objective.c` | The lists built from the setup file, objective status (`objectiveCheckStatus`), the "Objective A: completed" messages, the checks for entering a room, depositing an item and taking a photo, and the briefing text for the watch |

## From the setup file

When `setupCreateProps` (see [props](props.md)) reads a level's setup file, it hands these
entries to the mission code. Each is kept as a linked list of pointers into the setup
data, newest first:

| Setup entry | Added by | List |
| --- | --- | --- |
| A tag: a number that names an object, so other entries can refer to it | `tagAdd` | `g_Tags` (`tagFind`, `objFindByTagId`) |
| Briefing text for one watch page (overview, M, Q, Moneypenny) | `briefingTextAdd` | `g_BriefingTexts` (`briefingGetText`) |
| The start of an objective, followed by its criteria | `objectiveAdd` | `g_Objectives[letter]` |
| "Enter room" criterion | `objectiveAddRoomEntered` | `g_RoomEnteredCriteria` |
| "Deposit item in room" criterion | `objectiveAddDeposit` | `g_DepositCriteria` |
| "Photograph object" criterion | `objectiveAddPicture` | `g_PictureCriteria` |

`objectivesReset` clears all of it at level load.

## Objective status

An objective is a list of criteria in the setup file, ended by `PROPDEF_OBJECTIVE_END`.
`objectiveCheckStatus` walks the list each time it is asked, and the objective is
complete only when every criterion is: an object destroyed, an AI stage flag set (or not
set, for a fail condition), an object collected or deposited, a photo taken, a room
entered. Any failed criterion fails the objective. Collect and photograph criteria fail
if their object is destroyed.

The room, deposit and photo criteria can't be checked from the setup data alone, so the
game sets a flag in them when it happens: `objectivesCheckRoomEntered` from the player's
move, `objectivesCheckDeposit` when a thrown item lands, and `objectivesCheckPicture` when
the camera is used.

Objectives have a difficulty: ones above the current difficulty are skipped, and the
letters shown on the watch count only those that apply.

## Messages and the end of a mission

`objectivesTickStatusMessages` runs each frame from the HUD. It compares each objective's
status with last frame's (`g_ObjectiveStatuses`) and shows "Objective A: completed" (or
incomplete, or failed) at the bottom of the screen when one changes; in co-op, on both
players' screens. It only runs in solo and co-op, not multiplayer. When the player leaves
the level, `bossReturnTitleStage` (in `boss.c`) checks the statuses one last time, turns
the messages off (`objectivesDisableStatusDisplay`), and if `objectivesAllComplete` says
every objective for this difficulty is done, moves on to the end-of-mission screens.
`objectivesFinish` marks objectives with a certain flag complete when the level unloads.

## Traps

* `g_ObjectiveLastIndex` is the highest objective letter in use, not the count:
  `objectiveGetCount` adds one.
* `objectiveCheckStatus` also has a second name, `objectiveGetStatus_WEAK`, which the AI
  interpreter calls with an extra argument; it was a trick to match the original code.
* The flag `objectivesFinish` tests (`unkD & 1` in `struct objective_entry`) isn't named
  yet.
