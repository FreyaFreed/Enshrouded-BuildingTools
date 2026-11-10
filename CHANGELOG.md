# Changelog

Each `## x.y.z` section is used as the release notes on GitHub and as the file
changelog on Nexus Mods, so keep it short and written for players.

## 0.31.2-props2 (fork test build)

- **Turn, tilt and roll now turn the object.** props1 turned the surface direction under
  the cursor instead of the object, so the object did not turn, and with a nudge set the
  turns moved it around instead. The turns and the nudge now go on the object's own
  transform, after the game worked it out (its placement check). "Nudge along the
  object's axes" now follows the object as it is turned; in props1 it followed a fixed
  direction.
- The nudge comes after the game's placement check, so the ghost no longer turns red
  when nudged into a wall, and the object can be placed there.
- The log shows the rotation as well on each click.

## 0.31.2-props1 (fork test build)

- **Exact rotation and nudge for objects** (an object in hand: chair, torch, plant...).
  Turn, tilt and roll in degrees on top of the game's own rotation, and a nudge in cm
  along the object's own axes or the world's. Works in a world you host and **also when
  you join a dedicated server**: the server places an object where your game's ghost is,
  so the server needs nothing new. Keys while an object is in hand: numpad 4 / 6 turn,
  8 / 2 tilt, 7 / 9 roll, arrows nudge X and Z, numpad + / - nudge up and down, numpad 5
  resets; one press is 15 degrees or 5 cm (both settable), with Ctrl 1. The same values,
  the steps and the axes are in the Object settings window (Caps Lock wheel). The
  settings stay until you reset them, and the text under the crosshair shows them.
## 0.33.0

- **Shroudforge**: a second download, "Building Tools for Shroudforge", for players who use
  the Shroudforge mod loader. It installs the mod in Shroudforge's `mods` folder, so
  Shroudforge keeps its own `dinput8.dll`, lists Building Tools in its Modloader and loads
  it. Note: Shroudforge's F9 and F10 are also Building Tools' Replace style and Free
  placement keys; change one of them in either menu.
- If Building Tools gets loaded twice (for example by Shroudforge and by an old copy in the
  game folder), the second copy stays off and `BuildingTools.log` says where it is.

## 0.32.2

- **Menu with other mods**: some players had no in-game menu ("menu: not available" in
  the log) because another mod changes the game's screen switching code that the menu
  opens through. The mod now finds that code even when it has been changed.

## 0.32.1

- Flying: clicking the side of a block now aims at that block (the preview box and Area
  delete / Replace sometimes took the block below or above it).
- Flying: Area delete no longer stops working after switching from a big shape to a small
  one (it kept treating the hammer as holding the big shape until it was put away).
- The hoe, seeds and other tools no longer mark Area corners (their clicks counted as
  building clicks, and single player could think it was on a remote server).

## 0.32.0

- **Blueprints** (issue #49): save part of a building to a file and build it again anywhere,
  also in another world. Blueprint is a new mode next to Prefab and Area (the mode on the
  tools wheel; Options tab: Blueprints on / off; keys in the Keys tab), with two actions on
  the tools wheel:
  - **Save**: click a block and a window like Exact mode's opens. Choose where the clicked
    block sits in the box (now also front middle and back middle), move that point by
    blocks, set width, depth and height, type a name, and Save. The box is drawn beside the
    window; hold the right mouse button to look around. The blocks in the box are saved with
    their own materials, and the objects in it (doors, windows, chests, beds, stations,
    furniture, lights, decorations, railings...) with their position and rotation, to a
    file in the new `BuildingTools-Blueprints` folder next to `enshrouded.exe`.
  - **Place**: click and pick a blueprint. The next page shows the materials and objects it
    needs, by name, and what you have; its rotation (90 degree steps); Mirror; Allow partial
    build; Objects (build / leave out); and which point of it sits at the block you look at.
    Its ghost follows your aim (hold the right mouse button) until you press Place here;
    arrow rows move it by blocks right, away and up. Build places everything in one action:
    blocks first, every one with its own material, then the objects, each costing one of its
    item (creative building: free). With partial build on, every block your materials cover
    is built, material by material. Undo works as for areas.
  - **Mirror** builds the blueprint with left and right swapped. The game has no mirrored
    models, so objects are turned to fit; with Mirror on, the window lists every kind of
    object with As is / Turned around for any that comes out facing the wrong way (kept for
    next time).
  Good to know: blocks are saved per cell of the shape in your hand (hold the small 0.5 m
  cube for full detail; a cell at least half full counts as a full block). Save inside a
  base: the objects are found through the game's comfort check, which runs there. Doors,
  windows and other wirable objects join the base's logic network after the world is loaded
  again. Blueprints do not work on a dedicated server yet.
- Exact mode: two more reference points, front middle and back middle.

## 0.31.2

- **Works with GreeneyeDS mods** (Find - Base Storage Search, The Worst Map Mod). They hook
  the game's HUD drawing function that Building Tools' menu, tools wheel, HUD rows and
  on-screen messages use, which hid it from the mod: the menu said "not available" and
  keys like F7 gave no message on screen. The mod now finds that function behind the other
  mod's hook and runs next to it.
- **Tools wheel with Find**: Find also hooks a drawing function the wheel uses; the wheel now
  draws through that hook.
- **Works with XHL Planting Assistant.** Its planting stayed off ("live body/ABI mismatch")
  because the object undo patch changed the game's object placement code before Planting
  Assistant checked it. That patch is now written only when you first hold an object with
  Object lines on, after Planting Assistant has started.
- Planting seedlings (or placing any object) with Area mode on no longer marks Area corners.
- With XHL Planting Assistant (planting on), seeds and seedlings are left to it: Object lines
  (line, curve, circle...) still work for plants when it is not there, and for all other objects.

## 0.31.1

- **Higher frame rate.** The mod's checks before it reads game memory were far slower than
  expected and cost about 2 ms every frame with the hammer or a material in hand, plus 3 to
  4.5 ms with the magic chests of all bases on (about 100 FPS dropped to about 60). The mod
  now remembers memory it has already checked, so its share is below 0.1 ms a frame.
- `Profile=1` under `[Debug]` writes the frame rate and the time each part of the mod takes
  to BuildingTools.log every 10 seconds (off by default), for reports about FPS.

## 0.31.0

- **Undo for objects** (#51). The game's undo now also takes back objects you placed
  (chairs, torches, plants...): one object, or a whole line, curve or circle in one press,
  and gives them back. Press Y with the object still in hand (`ObjectUndo` on the Keys
  tab), or the game's undo with the hammer. It uses the game's own undo and its own
  "pick up" removal. A line of more than 30 objects can undo its first 30 (the game's
  undo holds 32 entries). On while Object lines is on; worlds you host, not yet on a
  dedicated server.
- **Smoother object lines**: the preview of a long line no longer costs many frames
  (it is worked out again only when something moves, and reads the ground in fewer steps).

## 0.30.0

- **Object placement** (#18). With an object in hand (a chair, torch, plant...), the tools
  wheel picks how the next clicks place it: Single (as the game), Line (2 clicks), Curve
  (start, end, then a point it bends through), Circle (centre, then a point on it), Arch
  (its two ends, a half circle standing up) or Standing circle (centre, then a point on
  it). The line and a ring for each object are drawn over the world while you aim; the
  last click places all of them and charges them at once.
- **Object settings** (on the wheel): number of objects (2 to 200), random spacing, random
  rotation and random height, the reference plane (the ground under each object, level
  at the first click, or a slope from click to click; objects on walls keep the line's
  height) and Turn with the line, so each object faces along the line or, on arches and
  standing circles, round the centre. In Single mode the random rotation and height apply
  to every object you place.
- **Object lines** can be switched off in the menu's Options tab (`Objects=` in `[Tools]`);
  then an object in hand works exactly as in the game.
- Works in any world you host (also alone); not yet when you join someone else's server
  or on a dedicated server. The game has no undo for objects, so a placed line is taken
  back by picking the objects up.

## 0.29.1

- **Large areas build again.** A big area (about 850 shapes and more, like a large roof)
  was refused with "not enough material" even with plenty in the backpack. The game
  refuses a payment with that many cost lines, so the mod now pays each material in one
  line.

## 0.29.0

- **Exact mode** (#46). A third choice for Area corners (two corners, base + height,
  exact), on the tools wheel and its key. Click once (left builds or replaces, right
  removes) and a window opens next to the area: pick where the clicked block sits (the
  middle of the bottom or a bottom corner, front and left as you look) and set width, depth
  and height as shapes past the clicked one (0 = only it, negative = the other way) with
  the arrows or the -10 / +10 buttons. The fill and its options and the game's material
  picker are in the window, and the 3D preview follows every change. Hold the right mouse
  button to hide the window, look around and walk; let go and it is back. Build (Enter)
  or Cancel (Esc). Works while flying; the last sizes are kept.
- **Options tab: pick your tools.** It now switches whole tools on or off: Building Tools
  itself, Replace, Area mode, Area shapes (roof, round and diagonal fills), Free placement,
  Creative building and Magic chests of all bases (plus the language). A tool that is off
  does nothing: its keys are the game's again and it leaves the building box hints and the
  tools wheel. The tools' own settings stay on their keys and the wheel.
- The old info panel window at the bottom of the screen is gone (it still showed at game
  start and exit); the building box rows and the floating text show everything.
- The shed roof preview now always matches what is built.
- The Keys tab is wider, so long (translated) action names are no longer under the keys.

## 0.28.1

- **Area clicks only while building.** With Area mode (and Replace) on, attacking or
  mining with a weapon or tool no longer marks area corners, and the area box no longer
  shows while you play. Corners are taken only with the hammer or a building material in
  hand. This mostly hit games where the mod thought it was on a server (single player
  while not holding the hammer).
- The crash log names the module of addresses outside the game and the mod.

## 0.28.0

- **Conical and dome roofs.** Two new roof shapes after shed (Shift+R). They are round:
  they cover the circle (square area) or ellipse (rectangular area) that the round fill
  makes over the same area, so they sit on top of round walls and towers. Conical has
  straight sides up to a point (45 degrees over a flat circle); dome is half a sphere,
  steep at the edge and flat on top. Thickness 1 to 3; overhang carries a cone's slope on
  down and gives a dome a flat brim. 3D preview of the exact blocks, works for build,
  replace and remove, and on dedicated servers (update the server too).

## 0.27.0

- **Diagonal walls in Area mode.** A new area fill, **diagonal**. Standing: a wall from your
  first corner to your second, seen from above, as high as the area (mark a wall one shape
  thin and you get a diagonal brace in it). Lying: a ramp from your first corner up or down
  to the far side, along the area's longer side; over a flat area it rises at 45 degrees,
  over a 3D area it runs exactly corner to corner. The steps share faces, so there are no
  gaps.
- **R** turns it: standing to the other diagonal, lying to fall instead of rise or to run
  along the shorter side. Standing / lying shares the round fill's key (unassigned at
  first), and is in the Options tab and on the tools wheel. Thickness 1 to 3, a quick key
  for the fill, 3D preview of the exact blocks. Works for build, replace and remove, and
  on dedicated servers (update the server too).

## 0.26.0

- **Round walls in Area mode** (#17). A new area fill, **round**: a full circle, a half or a
  quarter, solid or hollow. Your two corners bound the shape, so a square area gives a
  circle (or half / quarter of one) and a rectangle gives an ellipse. **Shift+R** picks
  full, half or quarter, **R** turns a half or quarter four ways (a quarter starts as a
  curve from your first corner to your second). Both keys work only with the round or roof
  fill on.
- **Hollow** builds a round wall with no gaps, 1 to 3 shapes thick; a half or quarter
  stays open on its straight sides.
- **Standing or lying.** Standing rounds follow the ground plan (towers, round rooms); mark
  a wall one shape thin and the round stands in it (arches). Lying turns the area into a
  tube along its longer side: pipes, and tunnels from a half.
- In the menu's Options tab and on the tools wheel (now up to 12 options); quick keys for
  the round fill, solid / hollow and standing / lying, unassigned at first. The 3D preview
  shows exactly the blocks that will be built. Works for build, replace and remove, and on
  dedicated servers (update the server too).

## 0.25.0

- **Roofs in Area mode** (#19). A new area fill, **roof**, with five shapes: gable (two
  slopes), hip (four slopes), gambrel (barn roof), mansard and shed (one slope).
  **Shift+R** picks the shape, **R** turns the roof. Both keys work only while the roof
  fill is on, so R stays free for the game otherwise.
- Mark a flat area, for example the top of your walls, and the roof covers it, about one
  shape up for every shape in from the edge, like the game's roof blocks. Mark a 3D area
  instead and the box height becomes the roof height, for steeper or flatter roofs.
- **Roof overhang** (none to 3 shapes) carries the slope past the area's edge.
- **Thickness** 1 to 3 shapes for hollow rooms, walls and roofs.
- All of it is in the menu's Options tab (which now scrolls) and on the tools wheel.
  Thickness and overhang also have their own keys, unassigned at first (set them in the
  Keys tab). The 3D preview shows the result before you click. Gable ends stay open. Works for build, replace and remove, and on
  dedicated servers (update the server too).

## 0.24.1

- **Works with Enshrouded Patch #1** (1.0.1.0). The patch moved game code the mod hooks,
  so 0.24.0 turned itself off ("game version not supported"). Everything is back: area
  building, Replace, fill delete, the menu, the tools wheel, the 3D preview, the game's
  own sounds and the dedicated server support.
- Update the server too: the patched `enshrouded_server.exe` needs this version.
- This version needs the patched game; keep 0.24.0 for the original 1.0 client.

## 0.24.0

- **3D area preview.** While you set up an area, the mod draws its own see-through box
  over it, in the game's building-ghost style: a soft pulsing fill, a 0.5 m grid and bright
  edges. Parts hidden behind terrain or blocks stay visible, drawn dimmer with dashed edges,
  so you can tell front from back. Your own character never hides it, and the box turns
  red while you stand inside it.
- Inside the box you see **the blocks the action will touch**, as one shaded solid: every
  block for a solid fill, the shell for a hollow room, the walls for walls only. Blue
  builds, amber replaces, pink-red removes. Delete and Replace (overlap or connected
  style) show only the blocks that are already there.
- It replaces the edge highlights. If the box can't be drawn on your PC, the edge
  highlights come back by themselves. Turn it off with `[Area] DrawBox=0` in
  `BuildingTools.ini`; `DrawBoxBlocks=0` hides the blocks and keeps the box.

## 0.23.0

- Fix: **the area preview was one block off** when one corner was on the ground and the
  other on a wall. The area built was right; the preview and the size readout now match it.
- New option **base-area box preview**: `[Area] Preview=2` in `BuildingTools.ini` shows
  the area being set up with the box the game draws for a flame altar's base area
  (`Preview=3` shows it together with the edge highlights). Like the game's own, it
  lights up where the box meets blocks and terrain.

## 0.22.0

- **Tools wheel.** Hold Caps Lock with the hammer or a building material in hand: a
  blurred wheel in the middle of the screen, drawn with the game's own wheel. Move the mouse towards an option and let go
  of the key (or click it to change several things in one go; right click closes).
  It shows only what applies right now: mode, Replace, Free placement and the menu
  always, Replace style while Replace is on, Area fill and Clear corners in Area mode.
  The camera stays still and clicks don't build while it is open.
- **Quick keys** (a new section of the Keys tab, which now scrolls): Replace while a
  key is held (back as it was when you let go), and keys that set one option directly: Prefab or Area mode, Replace on or
  off, each replace style, each area fill, creative building on / off. They have no key
  until you set one.
- Creative building can be switched in the menu (Options tab) in single player.
- Middle mouse and the mouse side buttons (Mouse 4 / 5) can be bound to any action.

## 0.21.0

- **Dedicated servers.** Put `dinput8.dll` next to `enshrouded_server.exe` as well as
  in your game folder. Area mode, Replace, the fill bucket, connected delete and area
  remove then work on the server. Your game sends your Building Tools settings to the
  server with its normal input, so each player builds with their own settings, and
  players without the mod build as usual. The server's `BuildingTools.ini` decides
  creative building, chests of all bases and the shape limits.
- The area corners and guide lines show in your game while you play on a server, and
  "Area sent to the server: N shapes" confirms the final corner. This also works with
  the free camera flown far from your character.

## 0.20.3

- Fix: **Replace, the fill bucket and the area preview far from your character with the
  free camera** (Z + Space). Flying the camera away from the character, Replace failed
  with "could not read the aimed block" and the blue area preview disappeared: far from
  the character the game stops updating the cursor the mod reads. The mod now works out
  the aimed block from your crosshair itself, so all of these work however far the
  camera is.

## 0.20.2

- Fix: **works together with XHL Vein Mining again.** Building Tools rerouted every
  call of the game's item payment, including the one that gives you the ore you mine.
  Vein Mining checks that call before it starts, saw it changed and stayed off. Building
  Tools now only reroutes the building systems' own calls, so both mods run at the same
  time (load them through the XHL `dinput8.dll` and `dinput8-chain.ini`, see the README).

## 0.20.1

- Fix: **Area remove and connected delete on building pieces** (foundations, stairs and
  other placed pieces). The game removes a piece as one object, so a right click deleted
  the whole clicked piece, the area reset, and the refund could count pieces that stayed.
  Now Area remove and connected delete treat pieces like blocks: corners work, and the
  blocks of the area (or the touching blocks of the clicked material) are removed and
  refunded. In Prefab mode without connected delete, a right click still removes the
  whole piece as in the game.
- **A building piece in the hammer works as in the game:** Replace and the fill bucket
  leave it alone (Area mode still repeats it).
- **The fill bucket works with the small shapes** (the first row of the shape picker).
  With a bigger shape selected, connected replace repaints one whole shape and connected
  delete removes one whole shape, as the full-shape style does. Replace with a bigger
  shape repaints the part the new shape covers.
- Fix: the mod's rows in the building box no longer disappear while the hammer aims at
  the sky.
- A one-shape Replace you can't pay for now says "Not enough material" instead of doing
  nothing.

## 0.20.0

- New: **the mod speaks your language.** Menu, building box rows, floating text, info bar
  and messages follow the game's language, in all 15 languages of the game's language
  menu. English and Polish are checked; the other translations are drafts and may contain
  mistakes, sorry! Corrections are very welcome on Nexus or GitHub (see TRANSLATING.md).
- The translations are plain text files in the new `BuildingTools-Languages` folder next
  to the mod, so anyone can fix a word and see it in the game.
- New option **Language** (Options tab, or `Language=` in the ini): follow the game, or
  pick one by hand.
- Key names and distances use the language's own form (for example `Strg`, `4,0 m`).
- Polish translation checked and corrected by Aleksander.

## 0.19.0

- New: **connected delete.** With Replace on and the style set to connected (fill
  bucket), a right click removes every touching block of the clicked material, up to
  FillLimit. Other materials in the same spots stay. Works on blocks and roofs, and on
  terrain while a terrain material is selected (the hammer's own rule). The material is
  refunded with the normal pop-up; undo covers the first 30 or so shapes, as Area remove.
- New: **Replace works on terrain.** With a terrain material selected, Replace (all three
  styles) repaints ground too, for example dirt into stone, keeping its shape. The fill
  bucket spreads over terrain as well. The old terrain is refunded by how much of it was
  there.
- New: **the fill bucket follows roofs diagonally.** For roof materials, connected replace
  and connected delete also spread to diagonal neighbours (side to side, up and down and
  across), so a whole sloped roof is caught in one click.

## 0.18.0

- New: **Building Tools menu in the game's own UI.** Press Home (also while holding the
  hammer) for a full-screen menu that looks and works like the game's menus: blurred
  world, the game's tab bar (Q / E), mouse cursor, and Esc to close. The character and
  camera stay still while it is open.
- **Keys tab:** click a key, then press the new one. Shift, Ctrl and Alt combinations
  work (for example Shift+K). Esc cancels, Delete clears. Saved to BuildingTools.ini at
  once; a key taken from another action is removed there.
- **Options tab:** mode, Replace, replace style, area fill and free placement, as rows
  like the game's settings. The F-keys keep working.
- The menu key itself can be changed in the Keys tab or with `Menu=` in the ini.
- New: **the mod's keys in the game's building box** (bottom right, under "Undo Build
  Action"), with the game's own key icons: Area mode, Replace and Free placement with the
  game's on/off switch, plus fill, replace style, clear corners, wheel distance and the
  menu key while they apply. `HudRows=0` in the ini turns them off.
- New: **info as floating text** below the screen centre, drawn like the game's snapping
  hint (blurred, soft backdrop): area state, size and block count, and the mod's short
  messages. The old info bar at the bottom only shows up when the building box is not
  there (for example with `HudRows=0`).

## 0.17.4

- Fix: Replace often said "could not read the aimed block, try again", above all while
  flying (Airborne Architect) and on the first clicks after loading. The game skips its
  own placement preview while the ghost is blocked, and the mod only learned where you
  aim from that preview. It now reads your aim every frame.

## 0.17.3

- New: **partial area and connected actions.** When an area build, area replace or
  connected replace costs more than your backpack and chests hold, it now does as many
  shapes as you can pay for (bottom layer first; nearest blocks first for connected
  replace) instead of nothing. The banner says "Placed N of M shapes (not enough
  material)". Replace refunds and undo cover exactly the shapes that were done.
- Fix: such an action used to fail without a word (often right after the "FillLimit"
  message). If not even one shape can be paid, the banner now says "Not enough
  material for N shapes (needs X items)".
- Fix: very big area and connected actions (thousands of shapes) could take longer to
  price than the mod waited for the game to write them, and then only one shape or
  nothing was changed. The wait now starts when pricing is done.
- Fix: connected replace with a big shape said "that is already your material" when
  the aimed spot also held blocks of the new material (for example next to blocks
  already repainted). It now picks the clicked material from the other blocks there.
- The FillLimit message now reads "only the first N shapes (FillLimit)": the shapes
  found up to the limit are still replaced.

## 0.17.2

- Fix (magic chests of all bases): crafting at a bench in a base without any chests of
  its own (or outside every base) only used the backpack, although the costs showed
  the materials of the other bases. It now uses the chests of every base there too.

## 0.17.1

- Faster and lighter: the info panel no longer re-creates its font, looks up the game
  window and asks Windows for key names twenty times a second; hooks share one small
  block of memory instead of 64 KB each; a few unused game lookups at start-up are gone.
- Fix: connected replace (fill bucket) with a very large `FillLimit` could freeze the
  game for a long time; its search table is now big enough for the maximum limit.
- Fix: the mod no longer refuses to start when one unused piece of game code is not
  found (after a game update).
- The info panel shows the key you set for the free placement wheel (not always Q).
- Cleaner log: debug lines from earlier versions removed.

## 0.17.0

- New: **magic chests of all bases** (`GlobalMagicChests=1` in the new `[Chests]`
  section of `BuildingTools.ini`, off by default). Building and crafting take
  materials from the magic chests of every flame altar area, not only the one you
  stand in, also outside a base. The material picker, the red "missing materials"
  ghost and the crafting menus count them too. Order: backpack, the chests of the
  base you are in, then the other bases. The loot deposit stays local.
- Chests of a base the game has not loaded (far away) may not count.
- `BuildingTools.ini` gets the new `[Chests]` section on the first start; your
  settings are kept.

## 0.16.0

- New: **F5 turns the whole mod off and on** (`ToggleMod` in the ini). Off is the
  vanilla game: Prefab mode, Replace, free placement and creative building are off
  and the other mod keys do nothing until you press it again.
- New: **connected replace (fill bucket)**. F9 now cycles full shape / overlap only /
  connected. With Replace on in Prefab mode, click a block: every touching block of
  the same material is repainted with your current material, in steps of the
  selected shape (0.5 m cube = block-exact), up to `FillLimit` shapes (default 500).
  Other materials are never touched; costs, refunds and undo work as for Replace.
- Fix: Replace with a terrain material as the new material removed the blocks
  instead of repainting them; they now become that terrain.
- `BuildingTools.ini` gets the new options (`ToggleMod`, `FillLimit`) on the first
  start; your settings are kept. If F5 clashes with another key, change `ToggleMod`.

## 0.15.0

- New: **creative building** (`CreativeBuilding=1` in the `[Creative]` section of
  `BuildingTools.ini`). Blocks, roofs, terrain and prefabs cost nothing, removing
  and undoing give nothing back, so the inventory is never touched. Props, chests
  and crafting stay normal.
- In creative building the material picker lists every block, roof, terrain and
  overgrowth material in its "backpack and magic chests" order, also materials
  you have none of. Favourites, last used, amount and item sets stay as they are.
  A material type you have nothing of can be built with right away.
- Works next to Creative Mode and other mods that hook the same game functions:
  the payment function and the dismantle dependency copy are no longer patched
  at their entry.
- Installing next to other mods that ship a `dinput8.dll` (XHL Native Core
  `dinput8-chain.ini`) is described in the README.

## 0.14.1

- The mouse hook is installed only while free placement is on and the game has
  focus (fixes cursor delay and hover/click issues some players saw).

## 0.14.0

- Area preview: the edges of the area being set up light up with the hammer's
  block highlight (on block surfaces; `Preview=0` turns it off).

## 0.13.4

- Works when another mod has already hooked the same game functions
  (fixes "PayCost not found").

## 0.13.3

- Replace with larger shapes in flight mode no longer reports "no live aim".

## 0.13.2

- First public release: Replace, Area mode (solid, hollow room, walls), free
  placement, game sounds, info bar, refund pop-up and undo of Replace.
