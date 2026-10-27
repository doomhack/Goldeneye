# Menus

Everything the player picks from before a mission starts: the logos and intro, the file
folders, mission and difficulty select, the briefing, the debrief, multiplayer setup and
the cheat menu. The in-game watch menu (`options.c`) and the multiplayer pause menu
(`mpmenu.c`) are in the same folder but will be mapped in a later step.

| File | What it holds |
| --- | --- |
| `src/game/menu/front.c` | The front end: every menu's handlers, the menu switch (`frontTick`, `frontRender`, `frontSetMenu`), the choices the rest of the game reads (`g_FrontStage`, `g_FrontDifficulty`, the multiplayer setup), and the cast screen |
| `src/game/menu/title.c` | The intro: Rare logo, gun barrel, Bond shooting at the camera (`title*`), and the gradient behind the folder screens |
| `src/game/menu/cheat.c` | Cheat codes typed with the buttons, the table of every cheat (`g_CheatInfo`), and what turning each one on or off does |
| `src/game/menu/mp_weapon.c` | The multiplayer weapon sets and which one is picked (`mpWeaponSet*`) |

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

Cheats won by beating target times are listed in the cheat menu (`g_CheatMenuList`,
`frontIsCheatUnlocked`) and switched on there (`g_CheatActivated`); `lvTick` turns them on
in the first frame of a mission (`cheatActivate`).

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
* `cheatResetText` and `cheatResetPlayerDisplay` are debug-only and never built.
