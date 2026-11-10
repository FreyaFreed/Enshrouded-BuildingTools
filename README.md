# Enshrouded Building Tools

A mod for **Enshrouded 1.0, Patch #1, Hotfix #4 and Hotfix #5** (single player and dedicated servers) that extends the Construction Hammer:

- **Replace**: repaint existing blocks with your current material, as a full shape,
  only where the shape overlaps existing blocks, or **connected** (fill bucket: every
  touching block of the clicked material, up to a limit; roofs also diagonally). Old material is refunded, with the game's item pop-up.
  Works with a free camera far from your character too. Undo restores everything, including taking the refund back. With a terrain material
  selected it repaints the ground too (for example dirt into stone).
- **Connected delete**: with the fill bucket style, a right click removes every touching
  block (or terrain) of the clicked material and leaves other materials alone. The fill
  bucket uses the small shapes (first row); bigger shapes replace or delete one whole
  shape, and building pieces in the hammer work as in the game.
- **Area mode**: click two opposite corners and the selected shape is repeated over a
  line, floor, wall or box. You can build, replace or remove the area in one action.
  Area remove and connected delete cut building pieces (foundations...) like blocks.
  Fills: solid, hollow room (walls + floor + ceiling), walls only, roof, round or diagonal. Roof shapes:
  gable, hip, gambrel, mansard, shed, conical and dome (Shift+R cycles them, R turns the roof;
  conical and dome cover the area's circle or ellipse, so they sit on round walls). Over a
  flat area a roof climbs one shape up per shape in from the edge; over a 3D area the box
  height is the roof height. Rooms, walls and roofs can be 1 to 3 shapes thick, and roofs
  can overhang the area by up to 3 shapes. Round fills: a full circle, half or quarter
  (ellipses in a rectangle), solid or a hollow round wall, standing (towers, arches in a thin
  wall) or lying (tubes and tunnels). Diagonal fills: a wall from corner 1 to corner 2
  seen from above, or lying, a ramp from corner to corner (R turns both). A live size readout and a drawn 3D preview show
  what the next click will make.
- **Exact mode** (Area corners: exact): click once and a window opens next to the area.
  Choose where the clicked block sits (middle or a bottom corner) and enter width, depth
  and height as shapes past it (negative grows the other way), with the fill, its options
  and the material picker in the window. The 3D preview follows as you type; hold the right
  mouse button to hide the window and look around. Build or Cancel.
- **Free placement**: the block ghost hovers in front of you, snapped to the 0.5 m
  building grid, instead of sticking to surfaces. Hold Q and turn the mouse wheel to set
  the distance.
- **Creative building** (optional, `CreativeBuilding=1` in `BuildingTools.ini`): blocks,
  roofs, terrain and prefabs cost nothing and removing or undoing gives nothing back, so
  the inventory is never touched. The material picker's "backpack and magic chests"
  order lists every material, also ones you have none of; favourites and the other
  orders stay normal. Props, crafting and chests work as normal.
- **Magic chests of all bases** (optional, `GlobalMagicChests=1` in `BuildingTools.ini`):
  building and crafting take materials from the magic chests of every flame altar area,
  not only the one you stand in, also outside a base. The material picker, the red
  "missing materials" ghost and the crafting menus count them too. Chests of the base you
  are in are used first; the loot deposit still only fills local chests.
- **Object placement**: with an object (chair, torch, plant...) in hand, the tools wheel
  picks Single, Line, Curve, Circle, Arch or Standing circle. The line and a ring for each
  object are drawn over the world; the last click places them all, evenly spaced, with
  optional random spacing, rotation and height, on the ground under each object, level
  or along a slope, turned with the line if you like. In worlds you host. Undo (Y with
  the object in hand, or the hammer's undo) takes back the last object or the whole line.
- **Exact rotation and nudge of objects**: turn, tilt and roll an object in hand by exact
  degrees on top of the game's rotation, and nudge it by cm along its own axes or the
  world's (numpad and arrow keys, or the Object settings window). Also on a server you join.
- **Tools wheel** (hold Caps Lock): a blurred wheel drawn with the game's own wheel;
  point the mouse at an option and let go. It shows only what applies right now
  (Replace style while Replace is on, fill and corners in Area mode).
- **Pick your tools**: the menu's Options tab switches whole tools on or off (Replace, Area
  mode, Area shapes, Free placement, Object lines, Creative building, Magic chests of all bases). A tool
  that is off gives its keys back to the game and leaves the hints and the wheel.
- **Menu in the game's own UI** (Home): change keys (also Shift / Ctrl / Alt combinations)
  and options in a full-screen menu with the game's tabs. While you build, the mod's keys
  sit in the game's building box with the game's key icons and on/off switches, and the
  area size and messages show as floating text below the crosshair.
  Everything is also in `BuildingTools.ini`.
- **In your language**: the mod's texts follow the game's language (all 15 languages of
  the game's language menu). English and Polish are checked; the other translations are drafts that may contain
  mistakes; corrections are very welcome, see [TRANSLATING.md](TRANSLATING.md).

Materials are charged and refunded through the game's own building code, and edits
replicate like vanilla building.

## Install

1. Download `EnshroudedBuildingTools-<version>.zip` from the
   [Releases](../../releases) page.
2. Extract it into the game folder, next to `enshrouded.exe`
   (Steam: right click Enshrouded → Manage → Browse local files).
3. Start the game. The banner "Building Tools <version> ready" confirms it loaded.

**On a dedicated server**, also extract the same zip next to `enshrouded_server.exe`
and restart the server; every player who wants the tools installs the mod in their game
as above. See [Compatibility](#compatibility).

The zip also holds the `BuildingTools-Languages` folder with the translations; keep it
next to `dinput8.dll` (without it the mod shows English).

Uninstall by deleting `dinput8.dll` and `BuildingTools-Languages` from the game folder. The full key list and
options are in [`package/BuildingTools-README.txt`](package/BuildingTools-README.txt),
which is also shipped in the zip.

| Key | Action |
|---|---|
| Caps Lock (hold) | Tools wheel: point at an option, let go (or click) to pick it |
| Home | Building Tools menu: change keys and options in the game's own UI |
| F6 | Prefab mode (vanilla) / Area mode |
| F7 | Replace on/off |
| F8 | Clear area corners |
| F9 | Replace style: full shape / overlap only / connected (fill bucket) |
| F10 | Free placement on/off (Q + wheel: distance) |
| F11 | Area fill: solid / hollow room / walls only / roof / round / diagonal |
| Shift+R | Next roof or round shape (only in Area mode with the roof or round fill) |
| R | Turn the roof, round shape or diagonal (only with that fill) |
| F5 | Whole mod off / on (off = vanilla game) |

All keys can be changed in the menu (Keys tab), also to Shift, Ctrl or Alt combinations
and to the middle or side mouse buttons. The Keys tab scrolls; its Quick keys section adds Replace while held and keys
that set one option directly (Prefab or Area mode, Replace on or off, each replace style,
each area fill, creative building). Thickness, roof overhang, round solid / hollow and
round / diagonal standing / lying have their own keys too,
unassigned at first.

What changed in each version: [CHANGELOG.md](CHANGELOG.md).

## Build

```sh
sudo apt install clang lld mingw-w64-x86-64-dev libvulkan-dev zip   # Ubuntu/Debian
./build.sh            # -> build/dinput8.dll
./build.sh test       # + unit tests
./build.sh package    # + dist/EnshroudedBuildingTools-<version>.zip
```

See [docs/BUILDING.md](docs/BUILDING.md) for Windows (MSYS2), the test options and the
release process. [docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md) explains the design;
[docs/reverse-engineering-notes.md](docs/reverse-engineering-notes.md) is the raw
research log with all game addresses.

## Compatibility

The mod is built against Enshrouded Patch #1 (1.0.1.0, 2026-11-02) and works unchanged with
Hotfix #4 (1.0.1.1, 2026-11-03; checked with 0.28.1) and Hotfix #5 (1.0.1.2, 2026-11-04; checked with 0.30.0); 0.24.0 and older only work with the original 1.0 client. It finds the game code by
byte signatures. If an update changes that code, the mod turns itself off safely, and
`BuildingTools.log` says which signature was not found.

**Dedicated servers:** put the same `dinput8.dll` (and `BuildingTools-Languages`) next
to `enshrouded_server.exe` too. Each player's game sends that player's Building Tools
settings to the server with its input, so everyone builds with their own Area/Replace
settings, and players without the mod build as usual. The server's `BuildingTools.ini`
sets creative building, chests of all bases and the shape limits for everyone. In a game
hosted by a player (not a dedicated server), per-player settings are not supported yet.

**Other mods with a `dinput8.dll`** (Creative Mode, XHL mods): only one file can have
that name. Use the XHL Native Core (the `dinput8.dll` from an XHL mod) and chain the
others with a `dinput8-chain.ini` in the game folder, after renaming this mod's DLL to
`BuildingTools.dll` and Creative Mode's to `CreativeModeLoader.dll`:

```ini
[Chain]
CreativeMode=CreativeModeLoader.dll
BuildingTools=BuildingTools.dll
```

Building Tools redirects the calls to the two functions Creative Mode also hooks
(`PayCost`, `9bc1a0`) at their call sites and leaves their first bytes alone, so the two mods
can start in either order. Only the building systems' own `PayCost` calls are redirected:
XHL Vein Mining checks that the pickaxe's reward call still goes to `PayCost` and stays off
otherwise.

**GreeneyeDS mods** (Find, The Worst Map Mod, Quick Deposit, Continue) load with their own
`dbgcore.dll` and need no chain entry. Find and The Worst Map Mod hook the start of the
game's play-state UI function, which the menu, the tools wheel and the on-screen messages
draw in; since 0.31.2 Building Tools finds that function by the rest of its code and goes
through their hook, so the order in which the mods start does not matter. The tools wheel's
draw function is found the same way.

**XHL Planting Assistant** compares the game's object placement function with the game file
when it starts and accepts redirected calls there, not other changes. The object undo patch
(a jump in that function) is therefore written only when an object is first held with Object
lines on.

## License

MIT, see [LICENSE](LICENSE). This project is not affiliated with Keen Games.
