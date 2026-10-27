# World

The level's fixed geometry, called the background or "bg": rooms and the portals between
them, working out which rooms can be seen, loading and drawing their models, bullets
hitting walls, the fog, the sky, animated water and the light fixtures that can be shot
out. Things that move (doors, guards, weapons) are props; see [props](props.md).

| File | What it holds |
| --- | --- |
| `src/game/world/bg.c` | Rooms and portals: loading the level's bg file (`bgLoad`), finding the visible rooms each frame (`bgFindVisibleRooms`), the visibility command list, loading and freeing room models, drawing the rooms (`bgLevelRender`), rays against room geometry (`bgTestBulletHitBackground`), the portal helpers (`bgGetPortal*`) and the display list colour patching (`bgApplyDynamicCCRMLUT`) |
| `src/game/world/roommtx.c` | A cache of per-room model matrices (`roomMtx*`): each room is drawn with its own matrix that places it relative to the camera's origin room |
| `src/game/world/bgfog.c` | The level's environment (`fog*`): fog distances and colour, the sky colour, the water image, and switching to an alternative environment |
| `src/game/world/sky.c` | The sky and cloud layer, and the water plane on levels with water (`skyRender`) |
| `src/game/world/water.c` | Scrolling the water texture (`waterTick`) and the render state for drawing it (`waterSetupGdl`) |
| `src/game/world/lightfixture.c` | Light fixtures that can be shot out: darkening their vertices and throwing glass |

## The bg file

Each level has a bg file (`bg_*_all_p.seg`), loaded once by `bgLoad` at level start.
It holds:

* the room table (`g_BgRoomFiles`): each room's position and where
  its compressed model data is in the file;
* the portals (`g_BgPortals`): each joins two rooms (`connectedRoom1`, `connectedRoom2`)
  and has a polygon (`offset_portal`);
* the visibility commands (`g_BgVisCommands`), a small program run each frame, described
  below;
* a table of one float per room (`g_BgRoomValues`, read by `bgGetRoomValue`), purpose
  unknown.

Room data is in bg units. `g_BgLevelScale` is the level's scale from the level table:
world units times the scale give bg units, and `g_BgLevelScaleInv` goes back again
(`bgGetLevelScale`, `bgGetLevelScaleInv`).

`g_BgRoomInfo[room]` is the run-time state of each room: whether its model is loaded and
where, its vertex bounds, whether it was drawn this frame, and its slot in the room matrix
cache (`mtxIndex`).

## Finding the visible rooms

For each player, the level loop calls `bgFindVisibleRooms`:

1. Work out the camera's room. It starts from the room of the player's floor tile (or the
   camera's tile in the intro, `playerGetRoom`, see [nav](nav.md)), then follows the line
   from `playerGetPos` to `playerGetPos3` through any portals it crosses, so that a camera
   above or below the tile's room ends up in the right one. Doorway-style portals are
   skipped here (`bgPortalIsVertical`).
2. `bgDetermineVisibleRooms` runs the visibility commands (`bgRunVisCommands`), then walks
   out from the camera's room through each portal (`bgTraversePortal`). A portal is
   projected to a box on screen (`bgGetPortalScreenBox`); if that box overlaps the screen
   box of the room it was reached from, the room behind it is visible, and the walk
   carries on from there with the smaller box. Each visible room is added with its
   screen box by `bgAddVisibleRoom` into `g_BgRoomsDrawn`.
3. Cradle has no useful portals, so every room on screen is drawn there.

Rooms not drawn for a few frames have their models freed (`bgRoomsTickUnload`). Loading
is spread out: at most three room models load per frame in normal play
(`g_RoomLoadBudget`), but any number during the intro and cutscenes.

### The visibility commands

Each level's bg file carries a small stack-machine program (`VISOP_*`, parsed by
`bgParseVisCommands`) that patches the portal results where they are wrong or too slow:
force a room visible, hide a room or a range, make a room visible only if seen through
a given portal, and preload rooms before they are seen, with `IF`/`ENDIF` and logic
operators on a stack (`bgStack*`).

## Drawing the rooms

`bgLevelRender` sets the lights and the bg segment, then `bgRenderVisibleRooms` draws the
visible rooms and the props in them together, room by room, each scissored to the
room's screen box. Rooms are sorted by how many portals deep they were found:

1. Nearest first: for each room, the opaque parts of its props (`propsRenderPass`, see
   [props](props.md)), the room's primary (opaque) display list, then the props flagged
   to draw after the room (`PROPFLAG_RENDERPOSTBG`).
2. The wall scorch marks and bullet impacts.
3. Farthest first: each room's secondary (translucent) display list, then its props'
   translucent pass.

Each room is drawn with its own matrix from `roomMtxLoad`, which scales from bg units to world
units and moves the room relative to the camera's origin room (see `camSetRoomOrigin` in
[player](player.md)). The cache keeps one matrix per room per origin room, and frees
those not used for two frames (`roomMtxTick`).

When a room model loads, `bgApplyDynamicCCRMLUT` rewrites its colour combiner and render
mode commands for fog and for some special surfaces.

## Rays and collision

`bgTestBulletHitBackground` tests a bullet's path against a room's triangles, and
`bgFindRoomsAlongSegment` finds the rooms a line passes through via the portals it
crosses (`bgSegmentCrossesPortal`). Walking collision uses the floor tiles, not the room
geometry; see [nav](nav.md).

## Fog and environment

`fogLoadLevelEnvironment` picks the level's `EnvironmentRecord` from `fog_tables` at level
start: the draw distance, near and far fog distances, fog colour, sky colour, and which
cloud and water images to use. A record's id is the level number, plus 100 per player in
multiplayer (each player count has its own record), or plus 900 for the intro cameras.
The record after it is the alternative environment: an AI script can switch to it
(`AI_SwitchSky`, `fogSwitchToSolosky2`), and toxic gas blends towards it.
`fogSetRenderFogColor` puts the fog into the display list; props use
`fogGetPropDistColor` so they fade into the fog the same way.

## Sky and water

`skyRender` draws first in the frame, before the rooms. On levels without clouds it just
clears the view to the environment colour. Otherwise it draws the cloud layer as a
textured plane high above the level and, below the horizon, either a water plane (on
levels with water) or a flat fill, working out where the planes cross the screen edges
(`skyIsScreenCornerInSky`, `skyIsCornerInWater`, `skyCalculateEdgeVertex`). `skyTick`
scrolls the clouds (`g_SkyCloudOffset`), and `waterTick` scrolls the water texture's two
layers and pulses their blend.

## Light fixtures

`lightfixture.c` has a long comment at the top on how it works: shooting a light darkens
its fixture's vertices to a quarter brightness and throws glass shards, and the list of
darkened vertices is kept so a room reloaded later is darkened again.

## Traps

* "bg" means the level geometry, not a background image.
* `g_BgLevelScale` and its inverse are easy to swap: multiply world positions by the
  scale to get bg units, and bg positions by the inverse to get world units.
* `bgPortalIsVertical` is about the portal's plane: a doorway is vertical.
* `bgGetLevelUnknownFloatScaled`, `bgAddVisibleRoomGroup` and `bgApplyPrimaryFogLUTEntries`
  are never called.
* `g_BgRoomInfo[].field_35` is set when the visibility commands preload a room, and keeps
  the room loaded for the rest of the level.
* `roomMtxSetOriginRoom` sets `curRoomIndex` on the player, which is the origin room for
  the room matrices, not necessarily the room the player is standing in.
