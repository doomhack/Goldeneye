# Menus

Everything the player picks from: before a mission (the logos and intro, the file
folders, mission and difficulty select, the briefing, the debrief, multiplayer setup and
the cheat menu) and during one (the watch menu in solo and co-op, the pause menu in
multiplayer).

| File | What it holds |
| --- | --- |
| `src/game/menu/front.c` | The front end: every menu's handlers, the menu switch (`frontTick`, `frontRender`, `frontSetMenu`), the choices the rest of the game reads (`g_FrontStage`, `g_FrontDifficulty`, the multiplayer setup), and the cast screen |
| `src/game/menu/title.c` | The intro: Rare logo, gun barrel, Bond shooting at the camera (`title*`), and the gradient behind the folder screens |
| `src/game/menu/cheat.c` | Cheat codes typed with the buttons, the table of every cheat (`g_CheatInfo`), and what turning each one on or off does |
| `src/game/menu/mp_weapon.c` | The multiplayer weapon sets and which one is picked (`mpWeaponSet*`) |
| `src/game/menu/watchmenu.c` | The watch menu's pages (`watchMenu*`) and the settings they change (`options*`, `g_Options`) |
| `src/game/menu/watchmodel.c` | The 3D models the watch draws (the held item, the controller on the controls page) and where each item sits on the inventory page (`invGetWatch*`, `itemGetWatch*`) |
| `src/game/menu/mpmenu.c` | The multiplayer pause menu, end-of-game awards, and the multiplayer pause state (`mpMenu*`, `g_MpPaused`) |

## How the front end runs

The front end is a level of its own, `LEVELID_TITLE`. While it is loaded, `lvTick` calls
`frontTick` each frame and `lvRender` calls `frontRender` (see [level](level.md)).
`frontInit` sets it up when the title level loads: it allocates the buffer the logo and
folder models are loaded into (`g_FrontModelBuffer`) and the hi-res frame buffer
(`g_FrontFrameBuffer`).

`g_FrontMenu` is the menu on screen, one of the `MENU_*` values. Each menu has four
handlers, named after it:

| Handler | When it runs |
| --- | --- |
| `front<Menu>Enter` | Once, when the menu opens: loads models, sets the cursor, starts music |
| `front<Menu>Tick` | Every frame: reads the controller and decides where to go next |
| `front<Menu>Render` | Every frame: builds the display list |
| `front<Menu>Leave` | Once, when the menu closes: frees what Enter loaded |

`frontSetMenu(menu, viaBlank)` changes menu. With `viaBlank` false the new menu opens on
the next frame (`g_FrontEnterMenu`). With it true, the old menu's Leave runs and the
screen goes black for at least 4 frames (the `Blank` menu, `g_FrontNextMenu`) before the
new menu opens; this is also where the screen switches between 320x240 (logos, intro, a
running mission) and 440x330 (the folder menus), waiting for the right frame buffer to come
round (`g_FrontHiResPending`, `g_FrontLowResPending`).

## The menus

| Menu | Name in code | Goes to |
| --- | --- | --- |
| Legal screen | `Legal` | Nintendo logo, or the file select once the intro has been seen |
| Nintendo logo, Rare logo, gun barrel, GoldenEye logo | `Nintendo`, `Rare`, `Gunbarrel`, `Logo` | the next logo; after the first boot, a button skips to the file select |
| Cast screen | `Cast` | after the GoldenEye logo sits idle, and as the credits after Cuba |
| File select (four folders, copy and erase) | `FileSelect` | mode select; back to the legal screen after 30 seconds idle |
| Mode select (mission, co-op, counter-op, multiplayer, cheats) | `ModeSelect` | mission select, co-op setup, multiplayer options, cheat menu |
| Co-op / counter-op setup | `CoopSetup` | mission select |
| Mission select | `MissionSelect` | difficulty |
| Difficulty | `Difficulty` | briefing, or the 007 options |
| 007 options (enemy reaction, health, accuracy, damage sliders) | `Options007` | briefing |
| Briefing | `Briefing` | run the mission |
| Running a mission | `RunStage` | debrief, multiplayer options, the cast (after Cuba), or the legal screen after a demo |
| Debrief (objective status) | `Debrief` | statistics |
| Statistics (time, target time, new cheat) | `Stats` | the next mission's briefing (straight into Cuba after Cradle), mission select, or the same briefing again |
| Multiplayer options and its sub-menus | `MpOptions`, `MpScenario`, `MpTeams`, `MpChar`, `MpHandicap`, `MpControls`, `MpStage` | run the game |
| Cheat menu | `Cheat` | mode select |
| No controller | `NoController` | stays there |

The folder menus share a layout: a cursor moved with the stick (`frontMoveCursor`,
`g_FrontCursorX/Y`) and three tabs down the right edge, "start", "next" and "back"
(`g_FrontTab*`). A menu's Tick checks whether the cursor is on a tab and A or Z was
pressed.

## What the rest of the game reads

* **Solo:** `g_FrontStage` and `g_FrontDifficulty`, handed to the level code when the
  mission starts (`frontRunStageEnter`); `g_FrontMission` is the entry in the mission table
  (`g_FrontMissions`), which also names the briefing file; the 007 sliders
  (`g_Front007*`).
* **After a mission:** `g_FrontMissionAborted` and `g_FrontBondKilled` decide what the
  debrief shows, and `frontIsMissionSuccess` whether the mission counts as done.
* **Multiplayer:** player count, scenario, stage, game length, characters, handicaps,
  control styles and aim settings, all `g_Mp*`, read through `frontGetMp*`.
  `frontSetMpScenario` sets which options a scenario allows (`g_MpCanPick*`).
* **Co-op:** the partner's outfit (`frontCoopGetPartner*`).
* `g_FrontGameMode`: intro, solo, multiplayer or cheats.

## Title intro

`title.c` draws the intro that runs inside the `Rare` and `Gunbarrel` menus.
`g_TitleStage` steps through it: the Rare logo (`titleRenderRareIntro`), then the gun
barrel sliding across (`titleRenderGunbarrel`), Bond turning and shooting
(`titleRenderBond`), the blood from [hud](hud.md) running down, and a fade to black.
`titleIsRareIntroDone` and `titleIsGunbarrelDone` tell the menus when to move on. Its
matrices are double-buffered (`g_TitleMtxIndex` flips each frame).

## Cheats

A cheat code is a sequence of single button presses. `cheatTick` runs each frame, on the
menus and in a game, and records each press in the current player's input buffer;
`cheatCheckInput` then looks for a cheat in `g_CheatInfo` whose sequence matches the end
of the buffer and which applies here (menus, solo game or multiplayer game, by its
`maskfield`). `cheatTurnOn` and `cheatTurnOff` do the work for each cheat: give weapons,
change health, turn on DK mode (`cheatSetDkMode`), and so on.

Line mode, turbo mode and the position display are plain switches the rest of the game
reads: `g_CheatLineMode` (boss.c draws the coverage values over the frame),
`g_CheatTurboMode` (Bond runs fast) and `g_CheatShowPos` (Bond's room, position and
heading on screen). They used to live in the debug menu's flags.

Cheats won by beating target times are listed in the cheat menu (`g_CheatMenuList`,
`frontIsCheatUnlocked`) and switched on there (`g_CheatActivated`); `lvTick` turns them on
in the first frame of a mission (`cheatActivate`).

## The watch menu

In solo and co-op, Start raises Bond's watch (the animation is in [player](player.md)).
`watchMenuOpen` resets the menu when the watch comes up, `watchMenuTick` reads the
controller while it is fully up, and `watchMenuRender` draws the current page onto the
watch face. Left and right move between five pages (`g_WatchMenuPage`); each has its own
Tick and Render:

| Page | Tick / Render | What it does |
| --- | --- | --- |
| Mission status | `watchMenuStatusTick` / `watchMenuRenderStatus` | objectives, the held weapon, and abort mission (asks to confirm, `g_WatchMenuAbortAsk`) |
| Inventory | `watchMenuInventoryTick` / `watchMenuRenderInventory` | a scrolling list of what Bond carries; A equips it (`watchMenuEquipSelected`) |
| Control | `watchMenuControlTick` / `watchMenuRenderControl` | the control styles, on a spinning 3D controller; dual styles need two controllers |
| Options | `watchMenuOptionsTick` / `watchMenuRenderOptions` | music and effects volume sliders, then a row per entry in `g_Options` |
| Briefing | `watchMenuBriefingTick` / `watchMenuRenderBriefing` | background, M, Q, Moneypenny and objectives pages |

A row is picked with A (`g_WatchMenuSelected`) and then changed with left and right. The
stick is read through small helpers that only fire once per push (`watchMenuStickPushed*`,
`watchMenuStickTapped*`). Changing page beeps and sometimes flashes static
(`watchMenuStatic`).

`g_Options` holds the settings: look up/down, auto-aim, aim control, sight and ammo on
screen, look ahead, screen size, ratio, hi-res, split screen, fps counter and draw
distance, read through `optionsGet*`. The control style is kept per player in the player
struct. In co-op, `optionsCoopUsePlayer` (called when the current player changes) swaps
the first seven settings so each player has their own; the rest are shared. The save file
stores the settings with each folder (see [save](save.md)).

## The multiplayer pause menu

In multiplayer each player has their own menu, opened with Start (`g_CurrentPlayer->
mpmenumode`): pause, kills, losses, scores, exit (with a confirm), and the awards at the
end of a game. `mpMenuTick` runs from `lvTick`, `mpMenuRender` from `lvRender`.

Pausing stops the game for everyone: `mpMenuPause` sets `g_MpPaused` and locks the
controls, and only the player who paused (`g_MpPausedBy`) can unpause. `g_MpStopPlay` and
`g_MpGameOver` are set when the time or score limit is reached; `mpMenuCanPlayerAct` says
whether the current player can still move and shoot. `mpMenuCalculateAwards` works out the
end-of-game awards ("most deadly", "longest innings"...) from each player's stats.

## Traps

* `menu_update` and `maybe_prev_menu` were the old names of `g_FrontNextMenu` and
  `g_FrontEnterMenu`: "update" and "interface" in the old handler names meant Leave and
  Tick.
* `frontRunStageTick` lives in `replay/ramromreplay.c`: it returns whether a demo was
  playing.
* `frontIsCheatUnlocked` hangs forever on a cheat it doesn't know.
* `frontSetBriefingPage` assigns inside an `if` and does nothing else.
* The Expansion Pak check in `frontLegalTick` is behind `#if` flags that are never set,
  and compares the address of `osGetMemSize` rather than calling it. The real check is
  `bossCheckExpansionPak` in `boss.c`, before the front end runs.
* `titleRenderRareLogo` only clears the screen: the Rare logo itself isn't drawn there.
* `g_CheatPlayerMask` is written when a cheat is turned on but never read.
* `watchMenuReset` is defined with a stage argument but `lvStageLoad` calls it with none,
  so it has no prototype in `watchmenu.h`.
* `watchMenuDebugNudge`, `watchMenuDebugNudgeGfx`, `watchMenuUnusedTick`,
  `watchMenuRenderInventoryOld`, `watchMenuRenderNothing`, `watchMenuIdentity` and
  `watchMenuNull` are leftovers from development.
* `g_Options` entries are indexed by `PLAYER_OPTION_*`, but the watch's options rows are
  numbered by `GAME_OPTIONS_INDEX_*`, which starts with the two volume sliders.
