# Guards

Guards and the other characters (Natalya, Trevelyan, scientists, civilians). In the code a
character is a "chr", and its record is a `ChrRecord`.

| File | What it holds |
| --- | --- |
| `src/game/chr/chr.c` | Chr slots, the per-frame chr tick (`chrTick`), moving with collision, drawing, shot hit tests on the body, hearing |
| `src/game/chr/chraction.c` | Actions: what a guard is physically doing (standing, walking a patrol, attacking, dying) and the code that starts and ticks each one; sight, navigation round obstacles, firing |
| `src/game/chr/chrai.c` | The AI script interpreter (`ai`) and the AI sound slots. The commands themselves are in `src/aicommands.def` |
| `src/game/chr/chraidata.c` | The global AI scripts, shared by every level |
| `src/game/chr/body.c` | Bodies and heads: loading them and building a chr's model, random heads |

The random numbers that dent shot objects (`chrObjRandomGetNext`) are a second stream of
the game's generator, in `src/random.c`.

Guards are created at level load by `setupGuard` in `prop/setup.c`.

## Slots

`chrsAllocateSlots` allocates one `ChrRecord` per guard in the setup file, plus 10 for
guards spawned during play (`chrSpawnAtPad`, `chrSpawnAtChr`). A slot is free when its
`model` is NULL. Each chr also has a prop (`PROP_TYPE_CHR`) and a model built from a body
and a head (`bodyCreateModel`).

## Each frame

A guard is a prop, so it is ticked by the prop loop (see [props](props.md)). `chrTick` runs
for each player's view, but only the first pass advances the guard:

1. `chrActionTick` counts down `sleep`. When it runs out, the guard's AI script runs
   (`ai`), then the tick for the current action (`chrTickStand`, `chrTickPatrol`,
   `chrTickAttack`...).
2. The animation advances. The model's root motion moves the guard through
   `chrRootMoveCallback`, which keeps it on the floor tiles and slides it along walls
   (`chrMoveWithCollision`).
3. The guard's rooms are updated (`chrDetectRooms`) and, if it is on screen, it is drawn
   joint by joint (`chrRender`).

AI scripts that are not attached to a guard (ID 0x1000 and up in the setup file, used for
level events) run on dummy chrs in `g_BgChrs`, ticked once per frame by `bgChrsTick`.

## AI scripts

An AI list is a byte stream of commands. The commands are defined once, in
`src/aicommands.def`, as `_AI_CMD` blocks giving each command's name, parameters and code;
`chrai.c` includes the file twice, once to build `ai`, the interpreter, and once to build
`aiCommandSize`. About half the commands are conditions that jump to a label
(`aiFindLabel`); the others start actions, set flags and timers, or change the level.

A script normally ends each pass with a command that yields, and resumes from
`aioffset` the next time `sleep` runs out. Scripts are either the level's own (in the
setup file) or global (`g_GlobalAILists` in `chraidata.c`), found by ID with
`ailistFindById`.

Objects can run scripts too: vehicles and aircraft run theirs from `objTick`.

## Actions

`actiontype` says what a guard is doing. Each action has a function that starts it and a
tick function, and the action's state lives in a union in the `ChrRecord`
(`act_stand`, `act_attack`, `act_gopos`...).

| Action | Started by | What it does |
| --- | --- | --- |
| `ACT_STAND` | `chrStand`, `chrStandImmediate` | Idle, optionally turning to face something |
| `ACT_KNEEL` | `chrKneelChooseAnimation` | Kneel |
| `ACT_ANIM` | `chrStartAnim` | Play an animation picked by the AI script |
| `ACT_PATROL` | `chrStartPatrol` | Walk a patrol path from the setup file |
| `ACT_GOPOS` | `chrGoToPos` | Walk or run to a position along the waypoint graph |
| `ACT_RUNPOS` | `chrRunToPos` | Run straight to a nearby position |
| `ACT_ATTACK`, `ATTACKWALK`, `ATTACKROLL` | `chrAttackStand`, `chrAttackKneel`, `chrInitActAttackWalk`, `chrInitActAttackRoll` | Aim and fire, standing, kneeling, walking or rolling |
| `ACT_SIDESTEP`, `ACT_JUMPOUT` | `chrSidestep`, `chrJumpOut` | Dodge sideways |
| `ACT_PREARGH`, `ACT_ARGH` | `chrTakeShot`, `chrReactToHit` | Hit reaction |
| `ACT_DIE`, `ACT_DEAD` | `chrReactToHit` | Dying, then lying dead and fading |
| `ACT_SURPRISED`, `ACT_SURRENDER` | `chrStartSurprised`, `chrTrySurrender` | Spotted Bond, or giving up |
| `ACT_STARTALARM`, `ACT_THROWGRENADE` | `chrStartAlarmChooseAnimation`, `chrThrowGrenade` | Run to an alarm, throw a grenade |
| `ACT_BONDINTRO`, `ACT_BONDDIE` | | Bond's model in the intro and death cutscenes |

The AI commands use the `chrTry*` functions (`chrTryAttackStand`, `chrTryStartPatrol`...),
which check the guard is still alive and able before starting the action.

## Moving

`chrGoToPos` plans a route over the waypoint graph (`waypointFindRoute`, see
[nav](nav.md)) and walks it waypoint by waypoint. Each tick `chrNavTick` moves towards the
next waypoint and, if something is in the way, `chrNavTryObstacle` looks for a way round
on either side. When the route passes only through rooms nobody can see, the guard skips
the walk animation and slides along the route (`chrGoPosTickMagic`), which is cheaper.

## Seeing, hearing and being shot

* Sight: `chrCanSeePos` checks the angle and the line of sight over the floor tiles;
  `chrCheckCanSeeTarget` also records when the guard last saw its target
  (`lastseetarget60`).
* Hearing: gunfire calls `chrCheckGuardsHeardSound` with a noise level, and every guard in
  range hears it.
* Being shot: `chrTestHit` finds which part of the body a bullet hits, using the joint list
  (see [models](models.md)). `chrTakeShot` applies the damage (armour first, then health,
  scaled by the hit part), plays the hurt sound and reacts with `chrReactToHit`, which
  picks a flinch, stagger or death animation. Nearby guards are told
  (`chrAlertOthersOfInjury`).

## Co-op and counter-op

`coopChrSelectTarget` picks which player a guard is after before its tick, and
`chrActionTick` makes that player current while the guard runs. Guards then aim at,
see and chase that player through the usual "current player" functions. The co-op and
counter-op functions are prefixed `coop` and `counterOp`.

## Traps

* Much of the guard code still says "Bond" or "player" where it means "the guard's
  target", because in solo they were the same. In co-op the current player is switched
  to the guard's target, so the code works, but the names mislead.
* `ai` keeps its one-word decomp name for now: renaming it would also hit every
  `ai` variable in `aicommands.def`.
* Many names come from Perfect Dark, where the functions match (`chrStand`,
  `chrGoPosConsiderRestart`, `chrNavTryObstacle`). PD's code is a good reference here.
* `chrGetGuard007SpeedRating` and friends scale guard speed and reactions by difficulty.
