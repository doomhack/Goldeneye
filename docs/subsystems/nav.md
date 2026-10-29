# Navigation

Where things can stand and how guards get from one place to another.

| File | What it holds |
| --- | --- |
| `src/game/nav/stan.c` | The floor tiles ("stan"): loading them, and finding tiles and floor heights |
| `src/game/nav/stanwalk.c` | Every collision and line-of-sight test, as walks over the tiles |
| `src/game/nav/geometry.c` | The 2D geometry on the floor plane that the walks and the props' collision use |
| `src/game/nav/waypoint.c` | The path graph guards follow, and route finding |

## Floor tiles (stan)

Each level has a stan file: a flat list of convex polygons ("tiles") covering everywhere a
character can stand, sorted by room. The files are built from C in `assets/obseg/stan`.
`stanLoadFile` takes the loaded file as it is: it fixes up the pointers at its start, and the
tiles are used in place, starting at `g_StanTiles`. It then records the first tile of each
room (`g_StanRoomFirstTile`) and each room's bounding box (`g_StanRoomBounds`); the rooms
without a model of their own (`bgRoomCalcBB`) use those bounds too.

A tile (`StandTile`, in `stan.h`) has:

* a name, packed into `id`: a letter (p or q), a number, a letter and a digit, such as
  `p123a4`. Pads in the setup file name the tile they stand on, and `stanResolvePadTile`
  turns the name into a pointer at level load;
* its room;
* a special type (`enum StanSpecial`: normal, crouch, ladder) and a colour that lights
  characters standing on it (`stanGetTileColour`);
* up to 15 points (9 in the game's files), anticlockwise seen from above. Each point is the
  start of an edge, and has a link: the tile on the other side, or a wall
  (`stanLinkedTile` returns NULL). Every tile has room for at least 3 points
  (`stanNextTile`);
* three of its points picked as a triangle that covers most of the tile (`tri0` to
  `tri2`), used for the quick "is this point on the tile" tests (`stanIsNearlyInsideTri`)
  and to work out the floor height at a point (`stanGetFloorY`).

Points are 16-bit integers. `g_StanScale` (set per level by `stanLoadFile`) converts world
coordinates to tile coordinates; every function takes and returns world coordinates.

`stanFindTileBelowPos` finds the floor below a point by searching the tiles of every room
whose bounds contain it. It is slow, and used when an object has no tile yet.
`stanFindNearestLegalPos` finds the nearest spot, on any tile, where a circle of a given
size fits. `stanSearchOutward` searches the tiles out from one tile, nearest first, for one
that passes a test (the guards use it to find a tile with a waypoint).

## Walks

There is no collision against walls as such. Everything that moves keeps the tile it is
standing on, and every test is a walk across the tile graph from there (`stanwalk.c`):

* `walkLine` follows a straight line from tile to tile, crossing each edge the line
  passes through. It stops when the line ends (success) or meets a wall edge (a hit).
  * `stanWalkLine` is just that, and updates the tile.
  * `stanGetRoomsBetween` collects the rooms the line passes through, with fast paths for
    "same room" and "rooms that share a portal".
  * `stanTestLineUnobstructed` also checks props in those rooms, filtered by collision
    type (`cdtypes`), and can ignore props the line passes over or under.
    `stanLineFindDoor` returns the nearest prop of the given types on the line (Rare's
    debug message calls it `stanLineDoor`).
* `walkCircle` visits every tile a circle overlaps, spreading through edges the circle
  touches. It reports a hit when the circle touches a wall edge, and gives up (with
  `STAN_COLLISION_TRAVERSAL_LIMIT`) once it has found 41 tiles. A `CircleWalk` can see each
  tile, turn linked edges into walls, and keep going past walls.
  * `stanCircleLegal` asks whether a circle fits where it is.
  * `stanCircleRooms` collects the rooms it touches.
  * `stanTestVolume` adds props, like the line test.
  * `stanCircleGetSpecial` finds crouch and ladder tiles under and beside Bond (a crouch
    tile beside him counts as a wall to the walk; Bond squats to go on). `stanGetLadderQuad`
    then gives the ladder's corners.
  * `stanTestCircleMaxHeight` treats tile edges higher than a given height as walls too.

A hit is saved in `g_StanHit`: the tile and edge, or as a line when it was a prop, and the
prop. `stanGetCollisionEdge` returns it, and Bond and the guards use it to slide along walls
(with `geoCircleSweepFrac`).

## Geometry

`geometry.c` has the floor-plane maths: distances, which side of a line a point is on,
whether two segments cross, where a line meets an edge, and how far a circle can move
before it touches an edge. The props' collision (`propobj.c`) uses them as well as the
walks.

Collision and so the whole game depend on these giving the same results to the bit as
Rare's: the attract-mode demos replay recorded input, and a guard a fraction of a unit off
changes everything after. The rewrites keep every float operation in Rare's order, and
`tools/hosttest/geotest.c` checks the results are right.

## The path graph

Guards don't path over tiles. The setup file has a separate graph:

* waypoints, each on a pad, each with a list of neighbouring waypoints;
* waypoint groups, each with a list of its waypoints and neighbouring groups.

`waypointFindRoute` finds a route in two steps:

1. A breadth-first search over the groups numbers each group by its distance from the start,
   and marks the shortest route back from the end.
2. Following the route from group to group, it finds the two waypoints that join each pair
   of groups and does the same search among the waypoints inside each group.

Both searches keep their numbers in the `dist` field of the graph itself, so only one search
can run at a time. `waypointFindNextStepToward` picks a step for a guard who should look
like he is searching: a random one that doesn't go nearer if there is one.
`waypointsValidate` checks the graph at level load (links go both ways, linked groups
connect, every waypoint in exactly one group, each group's waypoints link up) and stops
with an error screen if it is broken.

## Traps

* `geoCircleSweepFrac` is only right for what the game uses it for, sliding along a wall it
  has hit: moving away from the edge it gives 0, and near an edge's end it sometimes picks
  the wrong end.
* `stanCircleRooms` reports a hit only for two or more wall edges, or one if the walk
  finished: Rare counted the end of the walk as a hit.
