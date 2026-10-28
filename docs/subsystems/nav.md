# Navigation

Where things can stand and how guards get from one place to another.

| File | What it holds |
| --- | --- |
| `src/game/nav/stan.c` | The floor tiles ("stan"), and every collision and line-of-sight test built on them |
| `src/game/nav/waypoint.c` | The path graph guards follow, and route finding |

## Floor tiles (stan)

Each level has a stan file: a flat list of convex polygons ("tiles") covering everywhere a
character can stand, sorted by room. `stanLoadFile` takes the loaded file as it is: the tiles
are used in place, starting at `g_StanTiles`. `stanBuildRoomData` then records the
first tile of each room (`g_StanRoomFirstTile`) and each room's bounding box
(`g_StanRoomBounds`).

A tile (`StandTile`) has:

* a name (a letter, number and digit such as `p123a4`, shown by `stanGetTileName`). Pads in
  the setup file name the tile they stand on, and `stanResolvePadTile` turns the name into a
  pointer at level load;
* its room;
* a special type (normal, forced crouch, ladder; `g_StanTileSpecialFlags`) and a colour that
  lights characters standing on it (`stanGetTileColour`);
* up to 15 points. Each point is the start of an edge, and has a link: either 0 (the edge is
  a wall) or the tile on the other side;
* three of its points picked as a triangle that covers most of the tile, used for the quick
  "is this point on the tile" tests (`stanIsInsideTri`) and to work out the floor height at a
  point (`stanGetPositionYValue`).

Points are 16-bit integers. `g_StanScale` (set per level by `stanSetLevelScale`) converts
world coordinates to tile coordinates. Functions ending in `World` take world coordinates;
the rest take tile coordinates.

## Walks

There is no collision against walls as such. Everything that moves keeps the tile it is
standing on, and every test is a walk across the tile graph from there:

* `stanWalkLine` follows a straight line from tile to tile, crossing each edge the line
  passes through. It stops when the line ends (success) or meets a wall edge (a hit).
  A callback sees each tile on the way.
  * `stanLineRooms` collects the rooms the line passes through, and `stanGetRoomsBetween`
    adds fast paths for "same room" and "rooms that share a portal".
  * `stanTestLineUnobstructed` also checks props in those rooms, filtered by collision
    type (`cdtypes`). `stanLineFindDoor` returns the nearest prop of the given types
    on the line (Rare's debug message calls it `stanLineDoor`).
* `stanWalkCircle` visits every tile a circle overlaps, spreading through edges the circle
  touches. It reports a hit when the circle touches a wall edge. Three optional callbacks
  see each tile, each touched edge, and the final tile list.
  * `stanCircleLegal` asks whether a circle fits where it is.
  * `stanCircleRooms` collects the rooms it touches.
  * `stanTestVolume` adds props, like the line test.
  * `stanCircleGetSpecial` finds crouch and ladder tiles under Bond.

A hit is saved in `g_StanHitTile` and `g_StanHitEdge`, or as a line in `g_StanHitLineA` and
`g_StanHitLineB` when it was a prop. `stanGetCollisionEdge` returns it, and Bond and the
guards use it to slide along walls.

`stanFindTileBelowPos` finds the floor below a point by searching the tiles of every room
whose bounds contain it. It is slow, and used when an object has no tile yet.
`stanFindNearestLegalPos` finds the nearest spot, on any tile, where a circle of a given
size fits.

## The path graph

Guards don't path over tiles. The setup file has a separate graph:

* waypoints, each on a pad, each with a list of neighbouring waypoints;
* waypoint groups, each with a list of its waypoints and neighbouring groups.

`waypointFindRoute` finds a route in two steps:

1. A breadth-first search over the groups (`waygroupsMarkRoute`) numbers each group by its
   distance from the start.
2. Following those numbers from group to group, `waygroupFindLinkPoints` finds the two
   waypoints that join each pair of groups, and `waypointFindRouteInGroup` does the same
   search among the waypoints inside each group.

Both searches keep their numbers in the `dist` field of the graph itself, so only one search
can run at a time. `waypointsValidate` checks the graph at level load (no waypoint linking
to itself, groups that connect, every waypoint in a group) and hangs if it is broken.

## Traps

* The tile struct is packed by hand: `tail.half` holds the point count and the three
  triangle point indices as nibbles, and much of the code shifts them out directly.
* `stanTriEdgeDist` and `stanTriEdgeDist2` are near duplicates.
