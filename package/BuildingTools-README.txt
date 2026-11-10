Enshrouded Building Tools 0.30.0
================================

Files in the game folder
  dinput8.dll           the mod (Windows loads it with the game)
  BuildingTools.ini     settings, created on first start
  BuildingTools.log     what the mod did (previous session: BuildingTools.prev.log)
  BuildingTools-Languages\   the translations, one text file per language

Dedicated server
  Copy dinput8.dll (and BuildingTools-Languages) next to enshrouded_server.exe as
  well. Your game sends your Building Tools settings with its input, so each player
  builds with their own settings; players without the mod build as usual. The
  server's BuildingTools.ini sets creative building, chests of all bases and the
  shape limits. Games hosted by a player don't support per-player settings yet.

Keys (with the Construction Hammer)
  Caps Lock (hold)  Tools wheel: point the mouse at an option, let go to pick it
  Home  Building Tools menu: change keys and options in the game's own UI
  F5  whole mod off / on (off = vanilla game, other mod keys do nothing)
  F6  switch Prefab mode (vanilla) <-> Area mode
  F7  Replace on/off (works in both modes)
  F8  clear area corners
  F9  Replace style: full shape -> overlap only -> connected (fill bucket)
  F10 Free placement on/off: the ghost hovers in front of you (snapped to the
      0.5 m building grid) instead of sticking to the surface you aim at
      Hold Q + mouse wheel (or Page Up / Page Down): farther / closer, 0.5 m steps
  F11 Area fill: solid -> hollow room -> walls only -> roof -> round -> diagonal
  Shift+R  next roof / round shape   R  turn it (roof, round or diagonal fill)

Prefab mode
  Vanilla placement. With Replace on, the shape replaces the blocks you aim at
  with your current material (aim at the block itself, not the ghost). This also
  works with a free camera flown far away from your character.
  Replace styles (F9):
    full shape   - blocks inside the shape get the new material and the rest of
                   the shape is built as well
    overlap only - only what already exists inside the shape is repainted; its
                   exact form is kept and nothing new is built
    connected    - fill bucket: click a block and every touching block of the
                   same material gets your material, nothing else changes. It
                   spreads in steps of the selected shape, so a 0.5 m cube
                   follows the blocks exactly and a bigger cube is faster but
                   coarser. Stops at FillLimit shapes ([Limits] in the ini,
                   default 500). Prefab mode only (in Area mode it works like
                   overlap only). The clicked block's material is the one that
                   spreads. Roof materials also spread diagonally (side to
                   side, up and down and across), so sloped roofs are
                   followed.
                   Right click deletes instead: every touching block of the
                   clicked material is removed and refunded, other materials
                   in the same spots stay.
                   The fill bucket works with the small shapes (first row of
                   the shape picker); with a bigger shape it replaces or
                   deletes one whole shape.
  Any material can be the new one, also a terrain material (the blocks then
  become that terrain). The ground is repainted (and with connected, deleted)
  only while a terrain material is selected, the same rule the hammer uses
  for removing ground; it keeps its shape and is refunded by how much of it
  was there.

Area mode (2 corners)
  Click two opposite corners; the current shape is repeated over the area
  between them, shapes side by side (exactly one shape size apart), starting at
  corner 1.
  Left click  = build corners (with Replace on: replace corners)
  Right click = remove corners
    line : two ends of the line
    floor: two opposite corners at the same height
    wall : two opposite corners in the same vertical plane
    box  : bottom corner and the opposite top corner
  After corner 1 the banner shows live what you would get if you clicked now
  (line / plane / box, shapes along each side, height, total), and a
  see-through 3D box is drawn over the area. Parts behind terrain or blocks
  are drawn dimmer with dashed edges; the box turns red while you stand in
  it. Inside it, the blocks the action will touch show as one shaded solid
  (blue build, amber replace, pink-red remove; delete and overlap/connected
  replace show only blocks that are already there). [Area] DrawBox=0 turns
  the box off, DrawBoxBlocks=0 hides the blocks.
  Without the box (DrawBox=0, or if it can't be drawn on your PC) the edges
  of the area light up with the hammer's block highlight instead, on block
  surfaces only. Turn that off with [Area] Preview=0. Preview=2 shows the
  area with the flame altar's base-area box, Preview=3 shows both. Corner clicks
  build or remove nothing. Switching between left and right click starts over.
  Build charges per shape as usual; Replace works as in Prefab mode (F9 style);
  Remove refunds like normal removal.
  Building pieces (foundations, stairs and other placed pieces) are removed
  as blocks too, in Area mode and with connected delete. In Prefab mode a
  right click removes the whole piece, as in the game. With a building piece
  in the hammer, Replace and the fill bucket are off (vanilla game).
  Fill (F11): solid fills the whole box; hollow room builds only the outer
  shell (walls, floor and ceiling, one shape thick); walls only leaves out the
  floor and ceiling. Floors, walls and lines come out complete in every mode.
  Roof: shapes gable (two slopes), hip (four slopes), gambrel (barn: steep
  below, flatter above), mansard (the same on four sides) and shed (one
  slope); Shift+R cycles them. Mark a flat area (for example the top of your
  walls) and the roof covers it, about one shape up for each shape in from
  the edge, like the game's roof blocks. Mark a 3D area instead and the box
  height is the roof height (steeper or flatter slopes). R turns the roof
  (ridge along the long or short side; a shed rises toward any of the four
  sides). Roof overhang 0-3 shapes carries the slope past the area's edge.
  Thickness 1-3 shapes for hollow rooms, walls and roofs. All of these are
  also on the tools wheel and in the exact window; thickness and
  overhang can get their own keys in the Keys tab. The 3D preview shows the
  roof first. Gable ends are left open.
  Conical and dome roofs (after shed in the Shift+R cycle) are round: they
  cover the circle (square area) or ellipse (rectangular area) that a full
  round fill of the same area makes, so they sit on round walls. Conical:
  straight sides up to a point (45 degrees over a flat circle); dome: half a
  sphere, steep at the edge, flat on top. Overhang carries a cone's slope on
  down and gives a dome a flat brim. They don't turn.
  Round: a full circle, a half or a quarter (Shift+R), filling the box of
  your two corners: a square gives a circle, a rectangle an ellipse. R turns
  a half or quarter four ways; a quarter starts as a curve from corner 1 to
  corner 2, a half with its flat side on the longer side. Solid, or hollow:
  a round wall without gaps, 1-3 shapes thick, open on straight sides.
  Standing: round in the ground plan (an area one shape thin stands the round
  up in that wall: arches). Lying: a tube along the area's longer side
  (pipes; a half makes a tunnel). Solid / hollow and standing / lying are on
  the tools wheel and in the exact window, and can get their own keys.
  Diagonal: standing, a wall from corner 1 to corner 2 seen from above, as
  high as the area (in an area one shape thin: a diagonal brace in that
  wall); R turns it to the other diagonal. Lying: a ramp from corner 1 up or
  down to the far side along the longer side, 45 degrees over a flat area;
  R turns it 4 ways (falling, along the shorter side). 1-3 shapes thick, no
  gaps on the steps. Standing / lying shares the round fill's key.
  The fill applies to build, replace and remove.
  Area corners (tools wheel, or a key in the Keys tab): two corners -> two
  base corners + a third click for the height -> exact.

Exact mode (Area corners: exact)
  Click once (left = build or replace, right = remove) and a window opens next
  to the area. Pick where the clicked block sits in the structure (the middle
  of the bottom or one of its four bottom corners; front and left as you look)
  and enter width, depth and height as shapes past the clicked one: 0 = only
  that shape, negative = the other way. Arrows step by 1, the -10 / +10
  buttons by 10. The fill and its options are in the window too, Material
  opens the game's material picker, and the 3D preview follows as you change
  things. Hold the right mouse button to hide the window, look around and
  walk; let go and it comes back. Build (or Enter) builds, Cancel (or Esc)
  goes back. Works while flying. The last sizes are kept.
  MaxShapes in the ini limits the size (default 2000 shapes).

Undo after Replace
  Undo puts the old blocks back, returns the new material and takes back the
  old material Replace gave you. If you no longer have that old material, the
  undo is refused (banner message) so nothing is duplicated.

Undo after area actions
  The game's undo memory is small (about 30 small blocks). An area action is one
  undo press; for big areas it covers only the first shapes ("undo covers N" in
  the banner) and gives back exactly their materials. Remove the rest with an
  area remove.

Object placement (an object in hand)
  Hold an object (chair, torch, plant, anything that is not a building material)
  and hold Caps Lock: the wheel shows Objects (the mode), Object settings, Clear
  points and the menu.
    Single           as the game (with random rotation / height if set)
    Line             click the start, then the end
    Curve            start, end, then a point the line bends through
    Circle           the centre, then a point on the circle
    Arch             its two ends: a half circle standing up between them
    Standing circle  the centre, then a point on it (stands upright)
  While you aim, the line and a ring for each object are drawn over the world.
  The clicks before the last only mark points; the last click places all
  objects and charges them at once (not enough: nothing is placed).
  Object settings: number of objects (2-200), random spacing (% of the gap),
  random rotation (degrees), random height (cm), New random values, the
  reference plane (Ground = the ground under each object, objects on walls keep
  the line's height; Level = the first click's height; Slope = straight from
  click to click) and Turn with the line (each object faces along the line; on
  arches and standing circles it turns round the centre).
  Works in a world you host (also alone); not yet on a server you join or a
  dedicated server. Undo: press Y with the object still in hand (key ObjectUndo)
  or the game's undo with the hammer; it takes back the last object or the whole
  line and gives them back (a line of more than 30 objects: its first 30). Saved
  in [Objects] of the ini; keys ObjectMode / ObjectSettings / ObjectUndo.

Exact rotation and nudge (an object in hand)
  On top of the game's own rotation, also when you join a server:
    Numpad 6 / 4     turn about the object's up axis + / -
    Numpad 8 / 2     tilt about its X axis + / -
    Numpad 9 / 7     roll about its Z axis + / -
    Right / Left     nudge right / left (as you look)
    Up / Down        nudge away / closer
    Numpad + / -     nudge up / down
    Numpad 5         reset rotation and nudge
  One press turns 15 degrees or nudges 5 cm; with Ctrl held, 1. Held keys repeat.
  The ghost shows the result, the text under the crosshair the values. They stay
  until you reset them (also for the next objects). Object settings (Caps Lock
  wheel) has the same values to set by number, the key steps, and whether the
  nudge follows the object's axes (turned with it) or the world's. The arrows
  step the axis that points most that way from the camera; the turns turn the
  object where it is, nudged or not. Saved in
  [Objects] of the ini (Turn, Tilt, Roll, NudgeX/Y/Z, NudgeWorldAxes, TurnStep,
  NudgeStep); keys ObjectTurnPlus ... ObjectResetRotation in [Keys].

Tools wheel (hold Caps Lock)
  With the hammer or a building material in hand: a blurred wheel in the middle of the screen, drawn with the game's own wheel.
  Move the mouse towards an option and let go of the key to pick it. A left
  click picks the option and keeps the wheel open (change several things at
  once); a right click closes it without changes. The camera stays still and
  clicks don't build while it is open. Only what applies right now is shown:
  mode, Replace, free placement and the menu always; replace style while
  Replace is on; area fill, area corners and clear corners in Area mode.
  Tools switched off in the Options tab are not shown.
  RadialMenu= in the ini (or the Keys tab) changes the key; 0 turns it off.

Menu (Home)
  A full-screen menu like the game's own, also while holding the hammer. Switch
  tabs with Q / E or a click; Esc or Home closes it.
  Keys tab: click a key, then press the new one. Shift, Ctrl and Alt
  combinations work (Shift+K). Esc cancels, Delete clears. Saved at once.
  The middle mouse button and Mouse 4 / 5 can be bound too.
  Quick keys (section of the Keys tab, scroll down): Replace while held (Replace is on while you hold the key and
  back as it was when you let go), and keys that set one option directly
  (Prefab or Area mode, Replace on or off, each replace style, each area fill,
  creative building on / off). They have no key until you set one.
  Options tab: which tools you use. Building Tools (the whole mod), Replace,
  Area mode, Area shapes (roof, round and diagonal fills), Free placement,
  Object lines,
  Creative building (single player; on a server its ini decides), Magic
  chests of all bases, and the language. A tool that is off does nothing:
  its keys are the game's again and it leaves the building box hints and the
  tools wheel. Saved under [Tools] in the ini. The tools' own settings (mode,
  replace style, fill...) are on their keys and the tools wheel.

Building box and floating text
  While you build, the game's building box (bottom right) lists the mod's keys
  with the game's key icons: Area mode, Replace and Free placement with an
  on/off switch, then fill, replace style, clear corners, the wheel distance and
  the menu key while they apply. The area state, size and block count and the
  mod's short messages show as floating text below the crosshair.
  HudRows=0 under [Interface] turns both off.

Sounds
  Sounds are the game's own UI sounds (Sound=1); Sound=2 uses Windows sounds,
  Sound=0 turns them off. When Replace gives old material back, the game's
  item pop-up shows it (RefundPopup=0 turns that off).

Changing keys and settings
  All keys and options are in BuildingTools.ini in the game folder (this
  version rewrites an older ini once so it lists every option; your values are
  kept). Close the game, open the file with Notepad, change a number, save.
  Easier: change keys and options in the menu (Home), which saves the ini.
  Keys are Windows key codes: F1..F12 = 112..123, A..Z = 65..90, 0..9 = 48..57,
  Page Up 33, Page Down 34, Insert 45, Delete 46, Home 36, End 35,
  Numpad 0..9 = 96..105, Caps Lock 20, middle mouse 4, Mouse 4 = 5,
  Mouse 5 = 6. 0 turns a key off. Add 4096 for Shift, 8192 for
  Ctrl, 16384 for Alt (Shift+K = 4096 + 75 = 4171). Example: to use F4 for Area mode,
  set ToggleAreaMode=115. The comments in the file explain each option.
  Delete the ini to get all defaults back.
  Low FPS? Profile=1 under [Debug] writes the frame rate and the time each
  part of the mod takes to BuildingTools.log every 10 seconds; attach the log
  to a report.

Language
  The mod's texts follow the game's language (Settings > Gameplay > Language, or
  Steam's language when the game uses the system language). Options tab in the
  menu > Language picks one by hand; Language= under [Interface] in the ini does
  the same (auto, en, de, fr, it, ja, ko, pl, pt-BR, ru, es, th, tr, uk, zh-CN,
  zh-TW). The translations are text files in the BuildingTools-Languages folder.
  English and Polish are checked; the others are drafts and may contain
  mistakes, sorry! Corrections are very welcome: edit the file, test it in the
  game (it is read when the language is set and when the menu opens), and send
  it in a Nexus comment or on GitHub:
  https://github.com/AleksanderWojtal-CC/Enshrouded-BuildingTools
  Language=test shows every text longer and in [brackets ~~], to check that
  long translations fit.

Turning the mod off
  F5 (ToggleMod in the ini) switches the whole mod off: Prefab mode, Replace,
  free placement and creative building turn off and the other mod keys do
  nothing (also from the Options tab). The game then plays exactly as without the
  mod. Press F5 again to turn it back on (the banner says which).

Creative building
  Set CreativeBuilding=1 in the [Creative] section of BuildingTools.ini (close
  the game first). Blocks, roofs, terrain and prefabs then cost nothing, and
  removing or undoing gives nothing back, so your inventory is never touched.
  Props, chests and crafting stay normal. The info bar shows "creative".
  Material picker: in its "backpack and magic chests" order it lists every
  block, roof, terrain and overgrowth material, also ones you have none of
  (count 0). Favourites, last used, amount and item sets show the normal lists.
  A type you have nothing of (e.g. no roof at all) can be built right away.
  Blocks built in creative are refunded like any other block if you remove
  them later with creative off.

Magic chests of all bases
  Set GlobalMagicChests=1 in the [Chests] section of BuildingTools.ini (close
  the game first). Building and crafting then take materials from the magic
  chests of all your bases (flame altar areas): first your backpack, then the
  chests of the base you stand in, then the other bases. This also works
  outside a base. The material picker, the red "missing materials" ghost and
  the crafting menus count the chests of all bases. The loot deposit still
  only puts loot into the chests of the base you are in.
  If there is not enough of a material anywhere, the block is simply not
  placed, as without the mod. F5 (mod off) switches this off too.
  The game only sees chests in parts of the world it has loaded; chests of a
  base far away may not count until you have been near it. Off by default.

Uninstall
  Close the game and delete dinput8.dll and the BuildingTools-Languages folder
  (the ini and log can go too).

With other mods that ship a dinput8.dll (Creative Mode, XHL mods)
  Only one file can be called dinput8.dll. Use the XHL Native Core (the
  dinput8.dll from any XHL mod, e.g. XHL Creative Mode Fix) as dinput8.dll and
  let it load the others:
  1. Rename this mod's dinput8.dll to BuildingTools.dll.
  2. If you use Creative Mode: rename its dinput8.dll to CreativeModeLoader.dll
     (do this before the XHL dinput8.dll overwrites it).
  3. Put the XHL dinput8.dll in the game folder.
  4. Create dinput8-chain.ini in the game folder:
       [Chain]
       CreativeMode=CreativeModeLoader.dll
       BuildingTools=BuildingTools.dll
     (leave out the CreativeMode line if you don't use it).
  XHL-Native-Plugin-Host.log shows what it loaded; BuildingTools.log should end
  with "ready".
  XHL mods with their own folder under mods\ (e.g. XHL Vein Mining) are loaded
  by the XHL dinput8.dll as before; Building Tools runs next to them.

With GreeneyeDS mods (Find, The Worst Map Mod, Quick Deposit, Continue)
  Nothing to set up: they load through their own dbgcore.dll, next to
  Building Tools' dinput8.dll.

With XHL Planting Assistant
  While its planting is on (its config file), seeds and seedlings are planted
  by it; Object lines still work for all other objects.
