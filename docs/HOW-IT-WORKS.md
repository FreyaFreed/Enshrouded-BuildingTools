# How the mod works

This is the design overview. Exact addresses and the research history are in
[reverse-engineering-notes.md](reverse-engineering-notes.md). All addresses are RVAs in the
2026-10-20 1.0 client unless noted; the 0.24.1 section of the notes lists what moved in
Patch #1 (1.0.1.0); Hotfix #4 (1.0.1.1) and Hotfix #5 (1.0.1.2) needed no changes.

## Loading

Windows loads `dinput8.dll` from the game folder before the system one, so the mod is
a *proxy DLL*. It exports `DirectInput8Create`, which loads the real
`System32\dinput8.dll` and forwards the call. On load it starts:

- an **init thread**, which waits for the game to unpack, finds all code by byte signatures
  and installs the hooks. If any required signature is missing, the mod does nothing and
  logs why.
- an **input thread** for the hotkeys and a **mouse hook thread** for Q + wheel and the
  tools wheel.

No C runtime is linked. The DLL has its own entry point, `memcpy`/`memset` and a stack probe.
Hooks are 5–10 byte inline jumps to a relay within ±2 GB of the target, plus a
trampoline holding the stolen instructions; relays, trampolines and the replace blit
share one 64 KB block next to the game image. If another mod already put a jump at a
function's entry, the hook chains to that jump. Two functions that Creative Mode also
finds by their first bytes (`PayCost` and 9bc1a0) are not patched at all: the mod
retargets the `call` instructions that reach them instead, so both mods work whichever
starts first. The play-state UI (the menu's entry point) is reached the same way, through
its one call; Find and The Worst Map Mod hook its first bytes, so when its signature does
not match the mod looks for it with the 18-byte prologue left out and calls whatever the
call reached before.

The DLL also works when loaded by another loader (for example as `BuildingTools.dll`
listed in the XHL Native Core's `dinput8-chain.ini`): everything starts from `DllMain`,
and the ini and log are kept next to `enshrouded.exe`.

## The game's building pipeline

Enshrouded's engine is an ECS. Building runs in the `player_building_blueprints`
(placing) and `player_building_dismantle` (removing) systems. In single player both run
in the in-process **server world** and in one or more **client prediction worlds**.

For one click the server does:

1. **Dry run.** `PlaceShape` (48a660) or `RemoveShape` (48b180) measures the shape through
   the voxel blit with a "count only" flag and appends cost entries to the undo step's
   cost list.
2. **Payment.** `PayCost` (48eeb0) charges (or, for removal, refunds) the list via the
   player's inventory transaction.
3. **Commit.** `PlaceCommit` (487270) or `RemoveCommit` (487af0) writes the voxels and adds
   one undo record to the current undo step.

Clients receive the result as voxel data from the server. That is why the mod acts only
in the server world: clients only lend their live aim cursor and skip their prediction.

## Area mode

`place_hook` (on `PlaceShape`) and `remove_hook` (on `RemoveShape`) turn clicks into
corners. Corner clicks build and remove nothing, and the following commit is suppressed.
On the last corner, the area is planned (`core::plan_fill`):

- Corners are stored as **shape centres** from the shape's bounding box. The cursor
  position depends on which face was clicked, so it can't be used directly.
- Shapes are laid exactly one shape size apart, starting at corner 1, with
  `round(distance / size) + 1` shapes per axis.
- Fill modes keep the boundary cells (hollow) or only the side boundaries (walls).
- Thickness `t` keeps cells within `t` of the boundary (hollow, walls).
- Roof fill (`core::plan_area`, `core::roof_column`): each column's distance `d` from the
  eave (gable / gambrel: across the shorter side, turned: the longer; hip / mansard: to the
  nearest edge; shed: from one side) gives its top via a profile (linear; gambrel and
  mansard steep then flat) scaled so the ridge distance `D` reaches the roof height. A flat
  area grows upward to `D + 1` shapes (about 45 degrees); a 3D area keeps its height. A
  column holds the shapes from just above the previous column's top up to its own, `t`
  deeper, so the steps touch. Overhang widens the plan on every side and extends the
  profile to negative `d`, so the plan also grows downward (`roofBase`). The preview shader
  (`cells.vert`) repeats the same rule from bits packed into the push constants.
- Cone and dome roofs (`core::roof_round_span`): the outline is the full round fill's ellipse
  of the plan (overhang included). With `r` = the column centre's distance from the middle,
  1 on the area's ellipse, the top is `h * (1 - r)` (cone) or `h * sqrt(1 - r^2)` (dome; a flat
  brim past `r = 1`), rounded; `h` as for the hip roof. A column runs from just above its
  lowest 4-neighbour's top to its own top, `t` deeper. The preview computes the cells on the
  CPU (bits). On a server they travel as the roof fill with thickness field 3 (no other roof
  sends it) and roof pose `(shape - cone) * 3 + thickness - 1`.
- Round fill (`core::round_inside`, `core::round_keep`): a cell of the round's plane is in
  when its centre is inside the ellipse that fills the box (half: centre on the middle of one
  side, quarter: on a corner, the ellipse twice as big). Hollow keeps inside cells within one
  diagonal step plus `t - 1` straight steps of an outside cell, so the wall is face-connected
  (no diagonal gaps); past a half's or quarter's straight sides the ellipse continues, so those
  sides stay open. The plane is the ground plan, the vertical plane of an area one cell thin,
  or (lying) the cross-section across the longer horizontal side; the shape repeats along the
  remaining axis. The preview computes the same cells on the CPU and uploads them as bits.
  On a dedicated server the round settings travel as the roof fill with roof poses 10..15.
- Diagonal fill (`core::diag_keep`): a 4-connected digital line from one corner of a plane
  of the box to the opposite one. Cell (u, v) in box space is kept when
  `f = u * dv - v * du` lies in `[-W/2, W - W/2)`, `W = t * (du + dv)`, so consecutive cells
  share faces and `t` widens it by whole steps. The plane is the ground plan (standing), the
  plane of an area one cell thin, or (lying) the upright plane along the longer horizontal
  side; a flat lying area grows upward to a 45 degree ramp. It repeats along the remaining
  axis. On a server it travels as round values 18..23 (no round uses them).

Every position is dry-run priced in the game's own cost list, so the normal payment step
charges the total. The single commit is then replaced by one commit per position.

**Undo budget.** An undo step holds at most 32 records, and the record pool and snapshot
memory are small (about 30 small blocks in total). When they are full, the game drops
the oldest undo steps, even the one being written, which corrupts the pool. The mod
therefore records undo only while the current step's own records clearly fit. The rest
is committed without undo, and the step's cost list is trimmed so undo refunds exactly
what it takes back. A hook on the "drop undo step" function (492480) detects any loss.

**Preview.** The client cursor's hover function (28e2f0) runs every frame. When the
hammer aims at an existing block, it highlights that block's voxels with one looped VFX
per voxel. The mod starts its own set of the same highlight, using the same start,
stop and time functions on the same thread, along the 12 edges of the area being set
up. Up to 256 voxels are used, thinned evenly. The effect is drawn on block surfaces,
so edges in open air stay invisible. The game's own blue ghost block is a tinted model
render, which the mod does not reuse.

The live corner is the live cursor position plus corner 1's offset from cursor to shape
centre. Where the cursor sits inside the shape depends on its rotation (it follows the
aimed face), so the offset is turned into the shape's frame with corner 1's rotation and
back out with the live cursor's.

**Drawn 3D box** (`DrawBox=1`, default; `src/vkdraw.h`). The game renders with Vulkan and
gets `vkGetInstanceProcAddr` with GetProcAddress, so the mod patches that one import and
hands out wrappers for the few Vulkan functions it needs. At each present it records its
own small command buffer (own render pass, pipelines, fence and semaphore per swapchain
image) that draws into the image being presented; the submit waits on the game's present
semaphores and the present waits on the mod's. The camera is the client cursor system's
(position and rotation each game frame), predicted a quarter frame ahead because the frame
on screen is drawn from slightly newer data (`DrawBoxDelay100=-25`). The game's scene depth
(the depth image of its last world pass with an HDR colour target) is sampled, so parts
behind the world are drawn dimmer; points near the player's character (a capsule below the
camera's look-at point) never count as in front. The blocks inside are one instanced draw
(one instance per grid cell; only faces between a kept and a left-out cell are drawn). For
delete and overlap replace the client world's own "is this blueprint blocked" query (the
hover function's call at +0x2ff) is run over the area in 16-voxel regions, 1.5 ms per frame,
and one bit per cell goes to the shader through a small host-visible buffer. If the GPU
offers fewer than 144 bytes of push constants or anything fails, nothing is drawn and the
edge highlights come back. Client only: the server never loads these hooks.

**Base-area box** (`Preview=2`). The renderer of the build-zone borders reads the client
world's BuildZoneVisualizationState (world+0xffd0; 0xffc0 before Patch #1) through a getter once per frame; the
flame altar cursor fills its preview box there (min/max in metres, valid flag). The mod
redirects that one getter call and returns a copy of the state with the area's box, so
the game's state is never written.

**Partial actions.** Before an area or connected action is paid, the mod checks whether
the whole cost list is affordable, using the material counts of the picker's lists
(`updateInventoryStocks`, noted every frame: backpack plus magic chests, all bases with
GlobalMagicChests, exactly what the charge takes from). If not, it finds
the largest number of shapes, in planned order, whose costs are, cuts the cost list and
the queued shapes to them, and for Replace rebuilds the refund from a per-shape log of
repainted old-material voxels, so refunds and undo match the shapes actually written.

## Replace

Placement normally never touches existing building voxels. Materials 0x80–0xff are
building materials, 1–0x7f terrain, 0 air. The mod patches the existing-material checks in
the voxel blit callback (a81350) with `src/cave.S`. In Replace mode an existing building
voxel of another material is counted and handed back to the point where the game picks
the write path by the new material: a building material takes the block path (material
byte, density byte 0), a terrain material the terrain path (material and density byte).
The terrain path sees the old block as density 0, so it always writes.
An existing terrain voxel (material 1–0x7f, density > 0) that the removal dry run allows
is repainted too: a new building material takes the block path; a new terrain material
keeps the voxel's density and is priced by it (full shape then raises the density to the
shape's, as the game's terrain placement does). The cave tallies terrain in density units
(255 = a full voxel) and the refund turns them into density / 255, the unit the game's
removal uses. Materials the game would not let you remove at that spot are left alone: they
are found by a removal dry run, which feeds the `allow[]` table.

The dry run runs with the patch active, so the game prices exactly what will be written.
The counted old voxels go through the game's refund builder (48ae10), and the resulting
entries are paid back after the charge.

**Refund pop-up.** Items added by an inventory transaction produce `PickupEvent`s (the
"+N item" pop-up) only if the transaction has an event writer and flag bit 0. The
placement system has no such writer. The mod records the dismantle system's writer per
world (from the system dependency block copied by 9bc1a0) and plugs it into the
placement transaction before refunding.

**Undo.** The game's undo refunds the step's cost list (the new material) and restores
the old voxels. The mod remembers, per undo step, the refund it paid for the shapes that
undo covers. When undo refunds that step, the mod first takes the old material back. If
it is no longer in the inventory, the undo is refused.

## Connected replace (fill bucket)

With Replace style "connected" in Prefab mode, the click starts a breadth-first walk
over a grid of cells the size of the selected shape, from the aimed shape along the six
world axes. Each new cell is probed with a removal dry run (`RemoveShape` with refunds
suppressed), which reports the volume of every material it would remove there; a cell
joins when it contains the clicked material (the building material with the most volume
in the first cell). The walk stops at `FillLimit` cells. The found cells are then priced
and committed like an Area replace in overlap style, with the blit's allow table holding
only the clicked material, so other materials in the same cells are left alone.
The clicked material can be terrain too, when the removal dry run reports it (a terrain
material is selected). When the clicked material is a roof material (`isRoofBlock` in the
game's material config, looked up the way the removal table builder does), the walk
also takes the 20 diagonal neighbours, so it follows sloped roofs.

## Connected delete

With the same style, a right click starts the same walk from the dismantle system's
dry run (`RemoveShape`). Removal builds a 256-entry "may remove" table per material
(48f1c0) both in the dry run and in `RemoveCommit`, and the removal blit leaves every
voxel whose entry is 0. The mod redirects both calls; while it removes connected cells
on its thread, every material but the clicked one is cleared from the table. Every
found cell is then dry-run in the dismantle's own context (so the game builds the
refund for the clicked material only) and committed with one `RemoveCommit` per cell,
with the same undo budget as Area remove.

## Magic chests of all bases

With `GlobalMagicChests=1`, building and crafting use the magic chests of every base
(flame altar area), not only the one the player stands in.

The game keeps every base in a `PlayerBases` system (one in the server world, one in
the client world): the number of bases at +0x24 and up to 16 records of 0x130 bytes from
+0x30. A record starts with the list of the base's magic chest entity ids and holds the
base's area box. Three places in the game use "the chests of the base the player is in",
and the mod widens each of them. All three are reached through redirected `call`
instructions; the game functions themselves are not patched.

1. **Payments and crafting (server).** An inventory transaction (building costs,
   crafting, station queues) takes from the backpack and then from the chests that
   `ChestList` (468660) returns for the player's position. The mod returns the chests of
   every base instead, the current base first, so local chests are used before distant
   ones. Outside every base, where the game would answer "not in a base", it returns all
   bases' chests too. Two callers keep the normal list:
   - the **loot deposit** (`area_deposit_trigger`), which would otherwise put loot into
     chests of other bases;
   - **client worlds** (the UI's per-recipe checks ask several thousand times per
     second). Their counts come from the client stock map below, so widening them would
     only cost time; in testing it dropped the game to 2 FPS with a crafting bench open.
   If the deposit's call site is not found in a game version, only the building systems'
   payments are widened (a thread flag set in `pay_hook`) and crafting stays local.
   One client-world change is needed: the crafting menu chooses "backpack only" when
   its chest list is empty (a base without chests of its own, or no base), and
   "backpack and chests" otherwise. In that case the mod hands it one chest of another
   base, so the menu takes its counts from the client stock map (all bases) and lets
   the craft go to the server, which then uses every base's chests.
2. **Client stock map (picker, ghost, crafting menu).** The client's
   `cache_crafting_stock` system adds up the current base's chests into one stock map,
   40 chests per frame, through the iterator `ClientStock` (4680b0). The material picker,
   the red "missing materials" ghost and the crafting menu read that map. For the length
   of each iterator call the mod points the base's chest list at a list of every base's
   chests and restores it afterwards. The list starts with the base's own chests, and the
   pointer is swapped in before the count and out after it, so another thread reading the
   record never sees an id past the end. The list is rebuilt only when a new pass starts
   or the player changes base. Outside every base it uses the first base and a point
   inside that base's area, so the map is built for the player anywhere.
3. **Server cursor check.** Before a click turns into a building action, the server
   (`server_cursor_no_prediction`) runs the same missing-materials check as the client
   (2865e0). It counts the material in a per-base stock map (`StockCount`, 469610), and
   those server maps do not hold what the chests contain (they read 0 in testing even
   for materials in the local chests). For a material that is only in another base, the
   server therefore dropped the click: the animation played, nothing was built or paid.
   With the option on, the server's count answers "plenty" and the payment (1) decides:
   it takes from every chest, or refuses the placement when there is not enough
   anywhere. The client's check is unchanged and still turns the ghost red when you are
   short. Server and client worlds are told apart by the client world's system, which the
   mod learns in (2); until then the check is left alone.

Chests whose entity is not loaded are skipped by the game itself (the entity lookup
fails), so a base far away may only count once the game has it in memory.

## Object undo

The game can undo a placed object: an undo record of type 1 (entity id, dismantle method)
makes its undo remove the object with the same function that picking an object up uses
(it destroys the entity and plays the pick-up effect), after refunding the step's cost
list. The game switches this off in three places, and the mod opens all three while
Object lines is on:

1. The place-object system starts an undo step only for a few item kinds. A small patch
   at that test also starts it for every object (single placements, and the click that
   ends a line; never for a click that only marks a point, whose step would refund
   something that was not placed).
2. PlaceProp adds the undo record only when a check says so, and that check always says
   no. The mod redirects that one call.
3. The server cursor tells the client whether undo is possible with the same item test.
   The mod redirects that call too, so an object in hand also gets "can undo".

With an object in hand the game never reads its undo key: only the hammer's controls
produce the BuildingUndo input. The mod reads its own key (default Y, `ObjectUndo`) and,
in the client's action code, sets that input for one call of the game's undo-key
function. The game drops an undo request while the place animation still plays, so the
press is sent again every 0.3 s for up to 1.5 s until the undo's refund runs. Lines use
the existing undo budget (one record per object, at most 30 per step).

## Exact rotation and nudge of objects

`props.h`. The place-object system takes an object's position and rotation from the
player's place cursor: the transform the player's game sends with its input every frame
(`ClientPlayerInputData` +0x2f8, the server's `PlayerInput` +0x300; for some modes the
aim at +0x330 / +0x338). That cursor is the ghost `client_cursor` works out each frame.
So a dedicated server places an object wherever the client's ghost is, and the feature
needs nothing on the server.

- **Ghost.** `client_cursor` first gets the spot and the surface normal from the cursor
  transform (the one Object lines reads; its +0x18 is the normal, not a rotation), then
  the placement check (2994d0) turns them and the game's own turn into the object's
  transform (position, rotation quaternion) and stores it, once for the place cursor and
  once for the aim. `prop_check_hook` (both calls) puts the turns and the nudge on top of
  what it stored: rotation = the game's rotation × turn (about the object's y) × tilt (x)
  × roll (z), so the game's own turning still works and the mod's turns follow the
  object; the nudge in cm along the object's axes as the game holds it (before the mod's
  turns, so the turns turn the object where it is), or the world's. The game then draws
  and sends the moved ghost. The check does not store a transform every frame (an invalid
  ghost): the mod keeps the game's last one and puts the turns and the nudge on it again,
  so a key pressed meanwhile shows at once.
- **Keys.** The nudge arrows follow the camera: right / left and away / closer step the
  axis of the nudge's frame (the object's or the world's) that points most that way,
  level (`core::prop_nudge_axis`); numpad + / - the one most up. The placement check (red ghost) has already run on the
  game's spot, so a nudge can push an object into a wall.
- **Input.** `input_encode_hook` compares the place cursor and the aim that go out with
  the last 8 ghosts. Still a ghost from before the move: the mod moves it for that packet
  (and puts it back after the encoder ran). The result is logged on each click.
- **Single player.** The place hook does the same for the click's place info, in case the
  in-process server got the ghost from before the move.

The values are plain settings (`[Objects]` Turn, Tilt, Roll, NudgeX/Y/Z,
`NudgeWorldAxes`), changed by keys while an object is in hand (they stay the game's
otherwise) or in the Object settings window; keys write the ini 2 s after the last press.

## Dedicated servers

`enshrouded_server.exe` loads the same DLL (`g_dedicated`, from the exe name). It has
the server-side building code, so every server hook installs; the UI, input and preview
hooks are skipped. The server never sees a player's keys, so the client sends them:

- **Client.** `input_encode_hook` wraps the encoder of `ClientPlayerInputData` (client
  3a5640, sent every frame). After the original runs, the mod's state is packed into
  the lowest 16 bits of the int64 camera position x/y/z (`core::net_pack`: tag, on,
  area, replace, style, fill, corner count, a 3-bit click counter, a check word). That
  moves the camera by under 0.02 mm and is sent losslessly. While a building item is in
  hand the encoder is also told to send the secondary (aim) cursor transform, which the
  game otherwise sends only while the right button is held, so Replace has an aim.
- **Server.** The decoded input is the PlayerInput component: camera at +0x28, primary
  cursor at +0x300, aim at +0x338. `NetScope` loads the clicking player's settings and
  area corners into the mod's globals for one hook call and saves the corners back.
  The dismantle context holds PlayerInput at +0x58; the blueprint system fills that
  field only after `PlaceShape`, so `place_hook` takes PlayerInput from the caller's
  frame (the cursor component it read the transform from, rbp-0x58 = xf-0x118).
  A player whose camera bits fail the check (no mod) or with the mod off builds vanilla.
- **Corners in step.** The client counts its own corner clicks and mirrors the corners
  for its HUD and guide lines; the server uses the client's count and click counter
  (`core::net_corner_index`) to skip repeated clicks and restart when it missed one.
- **Remote or local.** The client mirrors corners only when no in-process server world
  builds (`remote_session`). The server world reads the building cursor only while the
  hammer or a material is in hand, so the choice is made only while the game draws its
  building box (`building_now`: no server world read for a second of building = remote)
  and kept when the player switches to a weapon or tool. Mirrored clicks also need the
  building box, so attack and mining clicks never count as corners.

## Off switch

`ToggleMod` (F5) sets the mod back to vanilla: Prefab mode, Replace, free placement and
creative building off, corners cleared. All hooks stay installed but pass everything
through, and the other mod keys are ignored until the key is pressed again.

## Free placement

The place cursor transform is computed in 28bc80 from the aim ray. With free placement
on and a building shape selected, a post-hook replaces the position with
`camera + forward × distance` (camera position and rotation from the cursor system),
snapped to the 0.5 m grid, with the last valid rotation, and marks the transform valid.
The game then shows its normal ghost there and places at it.

## Creative building

With `CreativeBuilding=1`:

- **Payment.** `PayCost` is reached through redirected `call` instructions, so the mod
  knows which game system is paying (the return address). For the calls of the
  blueprint (place), dismantle (shape and prefab) and undo systems it returns success
  without touching the inventory; every other caller (props, crafting, chests) pays
  normally. Replace's own refund of the old material is skipped too.
- **Preview.** The client's "missing materials" check (red ghost, Build greyed out) is
  answered with "enough" for building shapes and prefabs.
- **Material picker.** The picker's lists are rebuilt every frame by
  `updateInventoryStocks` (4076b0) from the backpack, the magic chests and the
  favourites. While the hammer is out, the mod adds every item of the terrain, block,
  roof and overgrowth item groups (count 0) to the list on screen, in its "backpack and
  magic chests" order only, just before the game finalises it. The ids are collected
  once; each frame only missing entries are appended.
- **Empty types.** When the material chosen last is not in a list, the game picks the
  selected entry and sends it as the new choice, but refuses an entry with count 0
  (406020). The mod accepts it, so a type with nothing in the inventory gets a material.

## Menu and HUD (game UI)

The game's UI is immediate mode: every frame the HUD code calls widget functions
(beginWindow, beginBox, text, buttons...) and the engine lays them out. The mod calls the
same functions, found by signatures, so everything looks like the game.

- **Menu.** The HUD has a screen byte (HudScreenType at hud+0x1cd8); 52 is unused in 1.0. A
  hook on the play UI (the function the game calls for the HUD each frame) opens and closes
  it with the game's toggleHudScreen, and the screen dispatcher's call for 52 is redirected
  to the mod's menu. The window is fullscreen and modal (input stays in the menu), with the
  game's blur, tab bar (menuTabBar with the tab list and tab widget redirected while the
  mod's menu draws it), setting rows (settingOption), buttons and hint bar. Key rebinding is
  captured by the input thread; the ini is rewritten at once.
- **Building box rows.** The building HUD box function draws its key rows with a row
  function (action + binding + label). The mod redirects the two row calls to copy their
  parameters and the call that finishes the box to add its own rows first, drawn with the
  game's bindingHint. Bindings are built from the mod's keys (USB HID codes, modifiers as
  extra inputs), so the game shows its own key icons. On/off rows are built like the
  game's "first person view" row: key row, stretcher, the game's switch.
- **Floating text.** After the play UI, the mod opens a fullscreen window that takes no
  input and draws a centred box below the crosshair like the game's snapping hint: blur
  behind, the soft nine-slice frame, body text.

- **Exact window.** In exact mode the first area click only notes corner 1 and the facing
  (camera forward and right snapped to world x / z) and opens screen 52, which then draws
  the exact window instead of the menu: a full-height panel at the left with its own
  backdrop and no blur, so the drawn box stays visible beside it (the box is clipped to the
  right of the panel). `core::exact_corners` turns reference + sizes into the two corners.
  While the right button is held the mod toggles back to the screen it came from (the game
  has the camera and movement), and back to 52 on release; Material toggles screen 30
  (BlueprintMultiToolMaterialCatalog) and returns when it closes. Build fixes the corners,
  closes the window and the input thread sends a real mouse click (left build / replace,
  right remove; sent again up to twice if the game drops it); the click hooks substitute
  the fixed corners. While that click is on its way the cursor read keeps the ghost valid
  in the air, else the game drops clicks while flying. On a dedicated server the client
  sends two clicks and patches their cursors to the corners.
- **Render camera.** Menu screens pause the cursor system, so the drawn box would lose its
  camera behind the exact window. A hook on the frame render function (13fe20 on
  1.0.1.1) finds the camera in its arguments at run time (arg 2 + 0x8d0 on 1.0.1.1) while
  the cursor camera is live, and feeds the box from it while the cursor camera is paused.
  The detour is an asm thunk that saves rcx..r9 and xmm0..3 and jumps to the original with
  the stack untouched (the function reads eight arguments; a C detour forwarding six
  broke the pickaxe's aim circle).
- **Tools in use.** The Options tab switches tools on or off (ini [Tools]). `act_allowed`
  gates every action, so keys of tools that are off are not read (they stay the game's),
  their wheel entries and hint rows are left out, and switching a tool off puts its state
  back to the game's (Prefab mode, Replace off, a plain fill, free placement off).

## Sounds

UI sounds come from the game's UI bundle (`FbUiBundle.sounds`). They are played through
the game's UI sound function (a429f0) from a hook on the HUD building update (b25350),
because that function is not thread safe. (The separate info panel window of the early
versions was removed in 0.29.0; the game UI shows everything.)

## Frame time

Before reading game memory the mod checks it is committed and readable (`readable()`,
VirtualQuery). In the game's process each VirtualQuery can take up to a millisecond (it
works out the size of the whole region, and the heaps are large), which cost 2 ms a frame
with an item in hand and 3-4 ms more with the magic chests of all bases before 0.31.1.
Pages found readable are now cached for 3 seconds (1024 slots, page number and tick in one
word), so almost every check is a cache hit. `[Debug] Profile=1` times every per-frame
hook with QueryPerformanceCounter, and the game's own function called from it separately
("game:" entries), and logs the time per frame every 10 seconds from the Vulkan present hook.

## Files written

`BuildingTools.ini` holds settings, written with comments on first start. `BuildingTools.log`
records every action, with the previous session kept as `BuildingTools.prev.log`. A crash
handler logs fatal exceptions as `game+offset` / `mod+offset` with a stack trace, which
is how the bugs during development were found.
