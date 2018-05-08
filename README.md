# tinycavestory
Press a, d, 1, or space keys to do things in the demo.

![screenshot 1](/screenshots/tinycavestory.png?raw=true)

Little demo of platforming and sprite batching via [tinyheaders](https://github.com/RandyGaul/tinyheaders) and Cave Story art. Made to demonstrate [tinyheaders](https://github.com/RandyGaul/tinyheaders) and provide a good example reference of use cases. The demo features a silly platforming. Notice there are some problems with the character controller! Namely the character will slowly slide down slopes on his own, and can launch himself into the air simply by pressing a or d and running over bumps. Typically these problems are non-trivial to solve, and require special game-specific code to handle them. Here's an example of some of the platforming problems:

![screenshot 4](/screenshots/tinycavestory_slide.gif?raw=true)

Tiled from [mapeditor.org](https://www.mapeditor.org/) was used to craft the tile map. Similarly, Tiled was used to setup a mapping from tile ids to shapes. Explanation [on tigsource forums here](https://forums.tigsource.com/index.php?topic=46289.820msg1378707#msg1378707).

The below image shows what the collision shapes look like in Tiled. Each index maps to a tile in the Cavestory tileset. By convention, the indices line up to define the shape for each different kind of tile. This is preferred over Tiled's built-in collision editor because it A) saves memory and map load time, and B) can be easily visualized.

![screenshot 2](/screenshots/collision_tiled.png?raw=true)

The level itself can be editted while the demo is running. The demo will detect edits and reload the level dynamically. Here's an example (Tiled editor on the left, and the demo on the right):

![screenshot 3](/screenshots/tinycavestory_live_edit.gif?raw=true)

Some more features planned to be added:
* Some jump sound effects and looping music with tinysound.h
* A little bit of text rendering using rasterized font from Cave Story (with unreleased tinyfont.h)
* Implementing some animation for the Quote instead of using a single image (with unreleased tinyanim.h)
