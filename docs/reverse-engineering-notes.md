# Enshrouded building mod: reverse-engineering notes

Target: `enshrouded.exe` 1.0 client, PE timestamp 2026-10-20 14:47:40 UTC,
SizeOfImage `0x2A1E000`, image base `0x140000000`. All addresses below are RVAs.
Single player only (the client runs its own internal server).

## Tooling (cloud workspace)
- Capstone 5.0.6 built from source, Python bindings in `pylib/`.
- `pe.py` PE loader + `.pdata` function table + chained-unwind `real_start()`.
- `index.py` full linear sweep of `.text` -> `xidx.pkl` (rip-relative refs, call/jmp targets).
- `refl.py` dumps the engine's reflection data -> `types.txt` (12,646 types with field
  offsets; enums decoded).
- `systems.txt` 861 ECS systems: `name fn=<rva> desc=<rva>`.

## Engine facts
- Positions are int64 fixed point, 1 m = 2^32 (scale constant 2.3283064e-10).
- Cursor/aim transform is 0x38 bytes: int64 x,y,z; float4 rotation (+0x18); float (+0x28).
  Copied from the player's cursor component at +0x300 (or +0x338 when the player is
  in a special state 5/8, flag at +0x1b1).
- Reflection descriptor: +00 name, +20 full name, +40 u32 size, +48 u32 count,
  +4c kind, +50 u64 hash, +58 members (structs, stride 0x30) / +60 (enums, stride 0x28).

## Building pipeline (server side, runs in single player)
| What | RVA | Notes |
|---|---|---|
| ECS system `player_building_blueprints` | `2d3270` | Handles shape placement/removal |
| ECS system `player_building_dismantle` | `2d3c80` | |
| ECS system `player_building_place_prop` | `2d4fd0` | |
| ECS system `player_building_undo` | `2d5b20` | |
| Request event `keen::actor::PlaceVoxelObjectEvent` | type id `0xde627227` | +0x60 `subtract` (remove mode), +0x61 `consume` |
| Get cursor transform | `218720` | `(out, playerA, playerB)` |
| Get player undo storage | `4926d0` | used by every building action |
| Begin undo step | `4919a0` | |
| **Place one shape** | **`48a660`** | single caller (`2d3762`). Args: rcx=undo list, rdx=ctx(0x130 struct), r8=player building state (blueprint id at +0), r9=player data (material id byte at +0x454), [rsp+20]=&transform |
| Blueprint lookup by id | `e72bc0` | |
| **Voxel write (blit)** | **`a85300`** | shared by placement paths; returns changed volume |
| Material id -> item | `e75240` | |
| Record cost/undo entry | `490060` | |
| Pay/refund materials | `48eeb0` | mode byte: 0 = check/consume, 1/2 = refund at position |
| Send BuildingPlaceEvent | `487270` | |
| No-build-zone overlap test | `e8cc70` | `(zones, &aabb)` |
| Script `terraforming` node | `3e5ef0` | switch on `TerraformingType`: Add `3d38d0`, Flatten `3dbbb0` |

Enums: `TerraformingType` Remove=0 Add=1 Flatten=2 FlattenHorizontal=3 OvergrowthAdd=4
OvergrowthRemove=5 DecayAdd=6 DecayRemove=7.

Shapes: `keen::VoxelBlueprintItem` { itemId, uint3 size @+4, BlobArray<uint8> data @+0x10,
bool isDataCompressed @+0x18 }.

## Mod design (proposed)
Hook `48a660`. On a real click, call the original once per target position:
- line / plane / box: positions between first and second click, stepped by the shape size.
- brush size N: N x N x N grid of the current shape.
- remove variants: same, with the event's `subtract` flag.
- replace: subtract then add with the current material at the same position.
Cost and undo then go through the game's own code (one undo step per click).

## Open
- Voxel material read (needed for replace-connected / flood fill and "replace only
  this material"). Candidates: the dismantle refund path (`48b4c0`), or runtime tracing
  of `BlueprintMaterialSelectFromCrosshair`.
- Confirm in game that calling `48a660` repeatedly within one event is safe (cost
  accumulation, undo grouping, client prediction/visual sync).
- Shroudtopia loader on this build; pattern signatures for every RVA above.

## Findings from in-game test 1 (v0.1.0)
- The blueprint system runs once per world per click: server (has
  `g38_building::UndoStorage`, cost list = undo step [rbp+0x220] + 0x110) and 1–2
  client prediction worlds (cost list = stack local rbp+0x430, aimed cursor stale or 0).
- Cursor component: +0x300 = place cursor (ghost; next to surface for 0.5/1/2 m
  blocks), +0x338 = aimed cursor (the block you look at). Large prefabs: both equal.
- Placement context +0x40 must be a TerraformingLootMemory (0x808 bytes) for the
  removal function; the placement system leaves it null -> crash when removal hits blocks.
- Cost entries are 0x18 bytes, item id at +0x10, count at +0x14. `48eeb0(mode, list,
  consume)` charges (1) or refunds (0) a whole list; mode byte 0 = inventory.

## v0.2.0 changes
Server-only actions; prediction suppressed while a mod mode is on; loot-memory stand-in
for removals; refunds via a hook on 48eeb0; corner clicks place nothing.

## Findings from in-game test 2 (v0.2.0)
- Server ServerCursor +0x338 is NOT live aim: it holds the previous action's position.
  Live aim only exists in client worlds' cursor component (+0x338).
- Server-only edits are not shown on the player's screen (only the last click's
  region updates); the client world must perform the same edit itself, as vanilla does.

## v0.3.0 changes
Mod action runs in every world (server + clients) with per-world corner state keyed by
ctx[0]; clients record place->aim, server looks aim up by identical place cursor
(1.5 s window). Third corner axis-locked (core::lock_third).

## Findings from test 3 (v0.3.0) + static analysis
- Crash: client cost list is an 8-entry inline array with no allocator
  ("No allocator set in BaseDynamicArray") -> never append >8 entries in client worlds.
- Client worlds run the system several times per click (input replay), so per-world
  click counting is wrong.
- KEY: 48a660 (PlaceShape) and 48b180 (RemoveShape) are DRY RUNS: blit callback
  a81080 checks params+0x30 ("count only") which they set to 1. Real writes happen
  after payment: 487270 = PlaceCommit(ctx, Commit*, shapeId), 487af0 =
  RemoveCommit(ctx, Commit*, shapeId). Commit = {float pos[3] (cursor pos, m),
  float rot[4] @0xc, u32 player entity @0x1c, u8 flag @0x20}.
- 48a050 replays building action records by type (+0x4c): 2=place(487270),
  4=remove(487af0), shape id @+0x48 -> this is how server writes reach clients.
- Cost entry from PlaceShape = {pdata+4 (16 B), u32 pdata+0, u32 e75240(ctx+0xf8, pdata[0x454])}.

## v0.5.0 design
Server only. place_hook prices every shape (removal dry runs -> refund buffer;
placement dry runs, synthesised entry when replace hits full voxels) and queues
positions; place_commit_hook (487270) turns the single commit into RemoveCommit +
PlaceCommit per queued position. Clients only lend live aim (g_clientCC) and return 0.

## Test 4 (v0.5.0): crash in RemoveCommit with the placement context
- 487af0 -> 48bff0 / 490100 read ctx+0x120 unconditionally. In the dismantle ctx it is a
  system dependency (likely ecs::ColliderSystem, which only dismantle/undo declare);
  in the placement ctx +0x120 is a non-pointer value.
- Blit add path (a81080, params+0xa0 != 0) only raises density: full voxels are never
  overwritten, so Replace needs a real RemoveCommit.
- 48f1c0 builds the 256-entry per-material "may write" table via 48f070.

## v0.6.0
Hook RemoveShape (48b180) to capture dismantle ctx fields +0x40/+0x48/+0x120 from the
game's own server-side dismantle (keyed by ctx+0x30 voxel world). RemoveCommit runs on a
copy of the placement ctx with those patched in. Replace refuses until captured.
Vectored exception handler logs crash address + stack return addresses.

## Test 5 (v0.6.0)
Crash logged by the handler: game+0x9b7e30 (movzx eax,[rcx+0xb8], rcx=0) <- 490540
(reads ctx+0xc8) <- 487af0+0x6ba, after the removal itself. 48bff0 returns false when
ctx+0x120 == 0 (aborts the removal), so both are real dismantle-only dependencies.
v0.6.1: capture the full dismantle ctx (0x158 bytes); RemoveCommit uses it with
stack-pointing fields and per-click fields (+0x30,+0xe8..+0x108,+0x130..+0x140,+0x148..+0x157)
from the placement ctx.

## Test 6 (v0.6.1)
Same crash: 490540 = send BuildingTearDownEvent (type 0x474baeed, 0x48 bytes) via event
writer at ctx+0xc8; null in both captured dismantle ctx and placement ctx at that point.
It is the last step of RemoveCommit (effects only). v0.6.2 hooks 490540 and skips it when
ctx+0xc8 is null.

## Test 7 (v0.6.2): prefab Replace WORKS
Removal + placement + refund work repeatedly. New material was not charged: e75240 is
items(registry, material, float volume in xmm2) = ceil(volume / (blocksPerLootItemExchangeRate*8));
the synthesised entry passed no volume. v0.6.3 hooks 48ae10 (refund builder, gets the
per-material removed volume table {float*, count}) to sum removed volume during the mod's
own removal dry runs and prices the new material from it.
Open: the refund shows no inventory pop-up (vanilla dismantle sends a notify/UI event).

## Test 8 (v0.6.3): prefab Replace complete for full-shape style; costs correct.
Requests: overlap-only style; drop the "Replace ready" warm-up.

## v0.7.0: replace through a blit patch (src/cave.S)
- 48a050 is only called by player_building_undo, so clients receive building edits as
  voxel data from the server; a server-side blit change replicates.
- Removal blit (a84960) sets fully removed voxels to (material 0, density 0) = air.
- Patch: blit add path density check at a813ba (sig a813a7, +0x13, 11 bytes) jumps to a
  copied cave. Mode 0 = original code; 1 = overlap (solid voxels: new material, keep
  density; empty: skip); 2 = full (solid: max density; empty: fill). Skips voxels already
  of the new material; tallies old density per material in cave vols[256].
- Pricing = placement dry run with the cave mode set (game prices what is written);
  refund = 48ae10 called with vols/255 (same units as blit/removal: density/255).
- No RemoveCommit / dismantle capture / teardown hook in the replace path anymore.

## Tests 9-10 (v0.7.0/0.7.1): blit patch never reached
Diagnostics: cave hit counters 0; removal dry run showed removable volume but no
materials in 1..0x7f. => Voxel materials: 0 air, 1..0x7f terrain, 0x80..0xff BUILDING.
In a81080 the add loop skips existing materials >= 0x80 at a81350 (so placement never
touches building voxels, but overwrites terrain), and building materials (params+0xa0 == 0)
are written by the path at a81402 (material byte, 2nd byte 0, volume += 1.0 per voxel),
not the density path patched in 0.7.0.

## v0.7.2
Patch moved to a81350 (sig a81327, +0x29, 35 bytes = the three existing-material checks).
Mod mode: existing building voxel -> skip if same material or not in allow[] (materials
the removal dry run reports removable), else vols[m]++ and jump to the building write
path a81402; air/terrain -> overlap skips, full runs the original checks. Refund volumes
are voxel counts (1.0 each). 48f1c0 table hook dropped.

## Test 11 (v0.7.2): prefab Replace (both styles) WORKS.

## v0.8.0: Area mode (renamed from Structure)
- Build/replace: unchanged pipeline (dry run per shape, PlaceCommit per shape).
- Remove (RMB): hook RemoveShape 48b180 (server ctx = ctx+0xf0 undo step != 0) and
  RemoveCommit 487af0. Corner clicks return 0 and suppress the following commit (the
  dismantle system commits even when the dry run found nothing). Third corner: dry runs
  at every position in the dismantle's own ctx (refund entries go to the game's list,
  refunded by its own 48eeb0 call), then one RemoveCommit per position; commit position
  = click commit pos + (centre_i - click centre). Step = dry-run aabb extents.
- Corner state has an op (build/remove); switching op restarts corners.

## Test 12 (v0.8.0): Area build of 220 shapes crashed
Fatal "StaticQueue is full" in 491af0 (undo record add): an undo step holds 32 records,
one per PlaceCommit/RemoveCommit (only when ctx+0x151 == 0 and ctx+0xf0 != 0). Undo
storage (4926d0) keeps 5 steps (ring at +0xa00/+0xa08, stride 0x200); 4919a0(alloc =
ctx+0x30, storage = ctx+0xe8) begins a step (cost list at +0x110, inline cap 8; flag
+0x1f8 = consume flag used by undo's 48eeb0). Undo (2d5b20) refunds the step's cost list
then replays its records.
v0.8.1: commits in chunks of 30; chunk k>0 gets its own step (up to 5 total) with its
cost entries merged per item and moved there; later chunks commit with ctx+0xf0 = null
(no undo, entries dropped). ctx+0xf0 restored afterwards.

## Test 13 (v0.8.1): 3x3x3 build/remove fine; 392-shape build crashed in the mod
undo_end read a cleared cost list: the first step had been dropped. 491af0 also drops
oldest steps (492480) whenever the record pool (+0xa10, used +0xa28, cap +0xa38) or the
snapshot memory (+0xa50) is full; it can drop the current action's steps.
v0.8.2: hook 492480; if it drops one of the action's steps, the rest of the action
commits without undo records. Chunk costs are moved when the chunk's step begins.
Banner shows the live area size (client cursor +0x300 build, +0x338 replace/remove).

## Test 14 (v0.8.2): crash in record pool alloc (87b6d0 <- 491be9) after many area removes
Logs: the game dropped the action's first step after ~30-33 records every time, i.e. the
undo record pool/memory holds only ~32 small records in total. When the dropped step is
the one being written, 491af0 still writes its record into it -> orphaned records ->
corrupted pool free list. Chunking into several steps is useless with that budget.
Pool +0xa10: used u32 +0x18 (a28), cap u32 +0x28 (a38). Snapshot memory +0xa50 (ring
allocator 87bbf0): cap +0x10 (a60), head +0x18 (a68, monotonic).
v0.9.0: one undo step per action; records only while own records < 30, < pool cap-2 and
own bytes + 2*max record < mem cap/2; rest committed with ctx+0xf0 = null; step costs
trimmed to the recorded shapes. 2-corner Area mode (ini [Area] Corners=2 default).
Planner lays shapes exactly one shape size apart from corner 1 (no stretching, no
forced extra shape), so different shapes sit side by side.

## v0.10.0 (polish)
- UI sounds: FbUiBundle (ctx = [rva 281e498], bundle = ctx+0xc8) .sounds @0x12060;
  play = a429f0(ctx, &SoundContainerReference copy) (not thread safe: hash-set insert).
  Played from a hook on the HUD building update b25350 (no stack args). Offsets:
  menu.highlightChange 12320, entryTrigger 12330, tabChange 12310, hud.buildActionFailed 12b70.
- Cursor query 287b10(q, sys, equippedItem, buildItem, a5, primary): built from the
  equipped item's CursorConfig (ItemInfo+0x104 equipment, +0x68 cursorConfig); q[0..2] =
  primaryActionConfig {hideWhenUnused, snapPositionToAddableVoxel, snapPositionToRemovableVoxel};
  q[3] mode dispatched in 28bc80 (1..5). Experiment: F10 clears q[1] for the primary query.
- Key-hint widget: FbUiLocaGameplayActionLabels (labels per input_mapping GameplayAction);
  adding custom entries needs new actions + loca text hooks (not done).

## Test 15 (v0.10.0): query byte q[1] already 0 for the hammer (dump 00 00 00 04 02 ...);
F10 had no effect. q[3]=4 = blueprint mode. 28bc80(out, sys, q, ray): ray[0]==0 -> hit path
28d9e0; no hit -> out valid only via forceClamp/restrictDistance path. out = {int64 pos[3],
float rot[4] @0x18, bool valid @0x28}. sys+0x640 camera pos, +0x658 camera quat (forward =
row 2 of 42880 matrix), +0x678 float distance.
v0.10.1: post-hook 28bc80: free placement writes pos = cam + fwd*dist snapped to 0.5 m,
rot = last valid rot, valid = 1. Panel bottom centre, font 24 px * height/1080 * PanelScale.

## Test 16 (v0.11.0): replace area missed the first layer when the two corner clicks hit
faces with different normals: the cursor position is not the shape centre and where it
sits depends on the cursor rotation (aim follows the surface normal). v0.12.0 stores
corners as shape box centres (shape_bounds) and converts planned centres back with the
template's (pos - centre) offset.
Ghost outline: camera pos/quat captured in 28bc80 hook (sys+0x640/+0x658); fovY/aspect from
ecs::ClientCamera {pos, quat, distance, fovY @2c, aspect @30, near, far}, found by a
memory search (ReadProcessMemory) for the current camera transform, verified over 750 ms.
Drawn as GDI lines on a colour-keyed topmost window. Camera basis = rows of 42880 matrix.

## Test 17 (v0.12.0): replace corner fix confirmed. Outline overlay never drew; user prefers
no overlay unless real in-game ghost blocks -> removed in v0.12.1.

## v0.13.0: refund pop-up
48eeb0 mode 0: mode+8 = inventory transaction C (init 21dea0, set up by 412170 from params).
Adds go through 40b4f0 into C's change list (+0xf0, count +0xf8, stride 0x34, type @+0x26:
0/5 = add). Commit 48be20 -> 408ed0 -> 4116f0 sends PickupEvent (0x7c31fc55, 0x30 bytes:
pos, itemId, count, by, from, slot, hideInUi) per added entry only if C+0xb8 (writer) != 0
and C+0xb0 bit 0. Mod sets bit 0 before its refund. Logs flags of vanilla refunds for
comparison. Ini rewritten with all options when older.

## Test 18 (v0.13.0): no pop-up. Placement transaction flags 0, writer 0; vanilla dismantle
transaction flags 0x25, writer 0x13bedbef0. Writer = dismantle system dependency (deps block
rbp+0x1f0 filled by 9bc1a0, +0x180 -> params+0xa0 -> C+0xb8). v0.13.1 hooks 9bc1a0, keeps the
writer per world (*sysctx) when called from the dismantle (ret 2d3cae), plugs it into the
placement transaction (C+0 = system ctx) and sets flag bit 0.

## Test 19 (v0.13.1): pop-up works. Undo of Replace duplicated: undo refunds the step's cost
list (new material) and restores the old voxels, the mod's refund of the old material stayed.
v0.13.2: per undo step (list ptr+data+count) remember the paid refund (prefab/full area) or
rebuild it from per-shape voxel snapshots for the recorded shapes (partial area undo); in
pay_hook, an undo refund of that list first charges those entries (refuse undo if missing).

## Test 20 (v0.13.2): "no live aim" with 2 m shapes in flight mode
The client world skips its placement prediction (so PlaceShape never runs there) when its
ghost is blocked, so neither the exact client-cursor match nor a click record existed.
v0.13.3 falls back to the client cursor component's live aim (+0x338) when it lies within
6 m of the server place cursor, and logs both client transforms when the lookup fails.

## v0.14.0: area preview
- The blue ghost block is a tinted model render (not VFX).
- Client cursor hover function 28e2f0 (called every frame from client_cursor) highlights the
  voxels of an aimed existing block: voxel list built by a8a290 only while hovering a block;
  per voxel a looped VFX guid {b0718612,4e1327b1,ca143cbf,9a608f41} started with
  29d3d0(fxSys = ctx+0x168, time = *9b9490(ctx[0]), &guid, &pos centre, &rot identity,
  &defaults 150ba30, &params) and stopped with 29d420(fxSys, &handle, time, &extra16).
  Voxel coords = floor(world m * 2); centre = v*0.5 + 0.25. ClientCursor keeps 256 slots.
- The mod runs its own 256 slots from a hook on 28e2f0 along the area's edges. The
  highlight is surface-projected (invisible in air). Hammer cursor VFX references tried
  (ItemInfo+0x1e4/+0x29c/+0x354/+0x38c): only equipment cursorVfx showed, flickery and near the
  player, with debris effects -> not used.

## v0.15.0: compatibility and creative building
- Creative Mode (Phoenix 1355) scans `building.payCosts` (48eeb0) and `ecs.queryBegin`
  (9bc1a0) by their first bytes; no other of its 168 patterns overlaps bytes the mod
  patches. Both are now reached through redirected E8 calls (PayCost: 12 calls, 9bc1a0:
  the dismantle call only).
- PayCost callers: 2d39ab blueprints (place), 2d46b0 dismantle shape (-> RemoveCommit),
  2d4b5a dismantle prefab (-> 488670), 2d4cc4 dismantle prop pickup, 2d5934/2d5991
  place prop, 2d6040 undo, 2c8e6f/2c8eca, 3d3b89, 3e1ef7, 3e700e other systems.
- 0.20.2: only the PayCost calls in the functions (.pdata chunks) of the four building
  calls are redirected (2d39ab, 2d46b0, 2d4b5a, 2d4cc4, 2d6040). XHL Vein Mining 1.1.1 finds
  the Terraform reward transfer (pattern at 3e6fe0, in 3e6c15), follows its E8 at +0x2e
  (3e700e -> PayCost) and requires the target inside the game image ("inventory Terraform
  reward transfer relationship changed" -> NOT APPLIED); our relay is outside it. Its other
  entries (remove 489bd0, terrain a86210, voxel a828c0, payout 49fd90, cursor_update
  28fc10, cursor_resolve 2949f0, terraform 3e5ef0, 121fb60/121faf0, 48b610) and body
  patterns do not overlap bytes this mod patches. XHL Native Core also redirects the tab
  bar's tab-list call bdb2a4 (as we do); each side chains to the other's relay.
- Missing-materials check 2865e0 (one caller 2975f2; true -> cursor flag 0x1000).
- `g38_items::updateInventoryStocks` 4076b0(stocks, ctx, onlyShown): 7 stock lists of
  0x648 bytes at stocks + type*0x648 ({data, count, cap, grow}, entries {itemId,
  slot u64 @+4, count u64 @+0x10} of 0x18), sort order byte +0x638 (0 spatial/backpack,
  1 last used, 2 amount, 3 curated/item sets, 4 favourites), selected index +0x628 and
  valid +0x630, shown type stocks+0x2bf8. Add 405140(list, &{id,count,0}, container,
  index, keep). Item groups: e823f0(ctx[0x68]), iterated with 3f070/3f270, group type
  byte 0x11..0x14 -> stock types 2..5 (terrain, default, roof, overgrowth), ids at
  group+8+off32, count +0xc. Finalise 4070c0 (call at +0xa76) sets the selected index
  from the chosen ids (405b90; types 2..5 read [ctx+8]+0xc8..0xd4).
- Removal refunds: building materials go through a per-material remainder
  (49fd70: acc += voxels; items = acc / rate), so a few 0.5 m blocks refund nothing.
- client_cycle_inventory_stock (2af5f0) -> 406020 (call 2afa47): chosen id not in the
  list -> selected/first entry, result 6 if its count is 0; result 0 + id -> 405270
  writes ClientPlayerInputData.buildingStockCycleAction (+0xa0 version, +0xa4..+0xb0).

## v0.16.0: off switch, connected replace, terrain as the new material
- Blit callback a81080, after the existing-material checks (a81373): [params+0xa0] != 0
  (terrain material placed) -> density path a81381..a81400 (density from the shape mask,
  existing terrain density from byte 1, building/air count as 0; writes material + density,
  volume += delta / xmm2); else block path a81402 (material, density byte 0, volume += 1).
  The cave used to jump straight to a81402, so a terrain material got density 0 = air;
  it now jumps to a81373.
- Connected replace probes cells with the mod's removal dry run (RemoveShape, refunds
  suppressed; per-material removed volume from the 48ae10 hook). In game: 500 cells with
  791 probes plus pricing took about 50 ms on the server thread.

## v0.19.0: connected delete, terrain replace
- 48f1c0(out[256], ctx, state) -> 48f070 builds the per-material "may affect" table:
  out[0] = 1; terrain 1..0x7f only when the shape state's material is terrain (or its
  item group is 0x11..0x13), split by the terrain material's flag +0x2c; building
  0x80..0xff by flags +0x6c / +0x14. Called in RemoveShape at 48b273 (table at rbp-0x30,
  passed to the blit as {ptr, 0x100} -> params +0x20/+0x28) and in RemoveCommit at
  487f24. The removal blit a84960 checks the table in both its terrain border pass
  (a84aa9) and main loop (a84bb4), so masking it removes only one material.
- e75970(registry = ctx+0xf8, id) returns the BuildingTerraformingConfig (0xb4 bytes) for a
  material id (terrain m, building m + 0x80 as a byte); +0x6c isRoofBlock, +0x14 hardness.
  Found at runtime through 48f1c0 + 0x144 -> 48f070, call at +0xe0.
- Blit a81080 at the cave site: r8 = voxel data (material, density bytes), rcx = rdx =
  voxel index, xmm2 = 255.0 and xmm1 = 1.0 for the whole call; [params+0x38] is the
  measured volume (float), [params+0x30] the count-only flag (dry run).

## v0.17.0: magic chests of all bases
- ECS systems by name: system descriptors in .rdata are {name, len, fn, ..., dependency
  arrays}. `cache_building_stock` (3fc450, server, deps ItemRegistry, PlayerBasesSystemServer,
  Inventory) and `cache_crafting_stock` (2af3b0, client, PlayerBasesSystemClient).
- PlayerBases system: base count +0x24, records of 0x130 from +0x30 (max 16): +0x00 chest
  ids (u32*), +0x08 count, +0x28/+0x30 a second id list, +0xf0 area box (min xyz, max at
  +0x100), +0x110 mask of sub-boxes (array of 0x20-byte boxes at [sys+0x1330]).
  Server: per-base stock maps at sys+0x1610+i*0x58 (staging +0x1b90). Client: one map at
  +0x1610 (staging +0x1668, chunk counters +0x16c8/+0x16cc, done flag +0x16c0).
  Map: {+8 bit set, +0x10 capacity, +0x28 u32 keys, +0x40 values {u16 count, ...}, +0x54 used}.
- 469060 BaseAt(out, sys, float pos[3]): out[0] = 0 found (index out[8]) or 0x1f.
- 468660 ChestList(out, list{ids,count,ids2,count2}, sysServer, sysClient, pos, player):
  status 0 / 0x1f no base / 0x16 permission (3b1cc0) / 0x12. Callers: 3f1ae0 (charge),
  3f1d70 (check), 2c1a80 (player_crafting), 2b3690 (player_ui, ~4000 calls/s with a
  bench open), 40bf70 (area_deposit_trigger).
- 3f1ae0 charge(C, list, ...): C+0xb1 chest order (0 backpack only, 1 backpack then
  chests, 2 chests first); per inventory 40a3d0(out, C, entity, item, count) -> 40a400
  (missing entity -> status 0x16, skipped). PayCost mode 0 -> 3f1ae0; modes 1/2 (client
  prediction, chosen by 9c5ee0 "is server world" in the blueprint system) -> 4664a0, which
  adjusts the client map at +0x1610 if the position is in a base. 3f20a0: C+0xb0 bit 0x40
  selects the map check (3f2800 via 4695f0) instead of the chest list.
- 4680b0 client iterator(out, sys, pos, player): throttled (+0x16d4 - +0x16d0), returns
  slices of <= 40 ids of the current base's two lists; at the end copies staging into
  +0x1610 and resets (46b150). Readers of the client map: 4695f0 (lea +0x1610) from
  updateInventoryStocks, player_ui, 3f2800; 469610 (missing check 2865e0, map at ctx+0xa0).
- Blueprint system (2d3270): handles PlaceVoxelObjectEvent (0xde627227), which the cursor
  systems only send when the missing-materials check passes. Then dry run, PayCost, and
  PlaceCommit (always after a successful payment), then 48be20(ctx, C, id, ok): applies
  the transaction (408ed0) only if the commit returned true, and sends event 0xbbaebaf7.
- 2865e0 runs in `client_cursor` and in the server's `server_cursor_no_prediction`
  (both through 297510). Its StockCount 469610(map, sys, item, pos) calls (+0x137, +0x2d7)
  read the server's per-base maps, which stayed 0 in testing, also for local materials:
  the server refused materials that were only in another base before paying.
- Tests (3 bases, 15 chests): picker counts and crafting from other bases worked first;
  building needed the server check (v0.17.0 answers 0xffff for the server's system).
  Widening player_ui's ChestList calls dropped the game to 2 FPS with a bench open;
  client worlds are excluded.
- v0.17.0 redirects all calls to 468660 (5), 4680b0 (1) and the two StockCount calls in
  2865e0. ChestList is widened for every caller except the deposit (40c379) and client
  worlds; the client iterator swaps the chosen base's list {ptr, count} for every base's
  chests for the length of the call.

## v0.17.1: review
- Unused lookups removed: WriteTable (48f1c0, its hook was never installed but the mod
  refused to start without it), TearDownEvent (490540), CursorQuery (287b10), BeginUndo.
- The E8 byte scan used to redirect every call of a function matches exactly the real
  calls on this build (checked against the capstone index: PayCost 12, Stocks 2,
  ChestList 5, ClientStock 1).
- Connected replace visits up to 6 * FillLimit + 1 cells; the visited table (2^16) could
  fill up for large limits and every lookup then walked the whole table. Now 2^18.

## v0.17.2: crafting in a base without chests
- Building in a base with no chests worked (server ChestList status 0, 0 local chests,
  widened to every base), crafting at a bench there did not: the menu (player_ui,
  2b3690) sets the transaction's chest order at 2b8c5d: 0 (backpack only) unless the
  ChestList result (2b89e6, client world) has chests ([rbp+0x4e8] or [rbp+0x4f8] != 0),
  then 2. The menu check 3f2030 -> 3f20a0 uses flag 0x40 (stock map path 3f2800), which
  with order 0 only counts the backpack (40a3d0) and never reaches the client map.
- 471290 (handle_crafting_events / factory_station) runs 3f2030 every frame for station
  queues with order 0 and flags 0x8; unrelated to the bench craft.
- v0.17.2: a client-world ChestList result with no chests gets one chest of another base
  (from the client stock's all-bases list), so the menu picks order 2.

## v0.17.3: player report (FillLimit 5000, aim errors with Airborne Architect)
- A failed PayCost of the server's building charge skips the commit silently
  (blueprint system 2d39b2 -> 2d3afd). With FillLimit 5000 the charge for thousands of
  shapes is easily more than the inventory holds; the "FillLimit" banner was the last
  thing shown. The mod now reports refused charges of its own multi-shape actions.
- The pending action's 3 s window started before pricing; pricing several thousand
  replace shapes (removal probe + dry run each) can take longer. Now set after pricing.
- Connected replace chose the material with the most volume in the first shape-sized
  cell, including the material being placed. Now the largest other building material.
- "could not read the aimed block" = aim_lookup failed: no client cursor component
  recorded yet (the client skips its placement prediction while its ghost is blocked,
  which is common when flying), or the client's live aim is more than 6 m from the
  server's place cursor. The log line after it says which; waiting for the player's log.
- Partial actions: 3f1d70(C, entity, count) is NOT an availability check: it asks whether
  inventory `entity` has room for `count` (the craft output), so it failed for every
  building material (first test: "0 of 500 affordable" with 128 in the inventory).
  The take path (40a400 -> 40f4a0 view -> 40a6e0 per slot) writes into the transaction's
  slot copies (4089b0) and its change list (412710 on C+0xf0), so trial charges cannot be
  rolled back. The mod uses the picker's counts instead (stock lists of types 2..5 after
  finalise 4070c0, entries {id, slot, u64 count @+0x10}), binary searches the largest
  affordable shape prefix (cost list prefix at g_pendCost[k]), truncates the list and the
  pending shapes, and rebuilds the Replace refund from a per-shape log of cave volume changes.

## v0.17.4: aim from the per-frame cursor read
- Player log (0.17.3): first Replace click of the session -> "aim lookup failed: no client
  cursor known yet". g_clientCC was only learned in client PlaceShape, which the client
  skips while its ghost is blocked (flying / Airborne Architect).
- Blueprint system 2d3270 loops over hammer players every frame: events (213b50), item
  check e7d430, then cursor read 218720(out = rbp+0xf8, player = [rbp-0x68], cursor
  component = [rbp-0x58]) at 2d340a (inside SIG_CURSOR_CALL, E8 at +15), before the
  PlaceVoxelObjectEvent loop. rbp+0x50 = undo storage, tested at 2d3687
  (48 83 7D 50 00, ServerCheck - 0x6b): non-null only in the server world.
- The mod redirects that one call and keeps the last 4 client cursor components;
  aim_lookup tries each (exact place match, then within 6 m).

## v0.18 (in progress): native in-game menu
- The game's UI is immediate mode: screens are code run every frame. Creative Mode 3.0
  (private "Flame" SDK) uses about 95 of these functions; used only as a map, every
  address below was located and checked in our exe.
- Play-state UI `c4fc40` ("doPlayStateUi()"), one caller `bc24ba`, no arguments used.
  hud = [[[ui ctx 281e498]+0xd0]]+0x38 (same ui ctx as the sounds); current
  `HudScreenType` byte at hud+0x1cd8 (1 InGame, 51 NoUi skips the UI, 0 Invalid).
  Opens window `PlayStateBaseWindow` (beginWindow a32b50(win, &StringView, 1, 2, -1, 0)),
  named box `doPlayStateUi()`, then dispatches on the screen at `c5028b`:
  52 TechGallery -> `cc6ad0` (call at `c50a9e`), 53 YcPlayground -> `ce77d0` (call at
  `c50a97`, used by Creative Mode), others below.
- `ecs::OpenHudScreenEvent {+8 playerEntityId, +0xc interactionHostId, +0x10 screen}` is the
  game's own way to open a screen (not used yet).
- TechGallery `cc6ad0` is a developer test screen ("Test1"), a complete minimal example:
  beginWindow a32b50(&win, &StringView{"Test1",5}, 0, 0, -1, 0); ctx = windowContext a3eb90(&win);
  beginRoot a301b0(&box, ctx); ... beginText b0e280(&text, &TextString, style, 0) with
  style = [ui ctx]+0xc8 (bundle) + 0x10a0c; endText b15a60; endBox a336a0; endWindow a330b0.
- StringView = {const char*, size_t}. TextString = {const char*, size_t, uint64 hash},
  hash = FNV-1a 64 of the text (named boxes use the same hash of their name).
- Test (dev1, writing the byte directly): screen 52 showed the test screen (red corner
  squares, check mark, "dies ist ein test"), the HUD disappeared, Home closed it, Esc went
  52 -> 2 (SystemMenu). No cursor and the character still walked: input/cursor are set
  on the screen transition, not per frame.
- `d60260` toggleHudScreen(ui ctx, screen): queues message type 1 on [ctx+0xd0]+0x33cc0
  and logs "[GameUi] toggleHudScreen to %k"; used by the game to open/close screens
  (85 callers). dev2 opens/closes screen 52 through it.
- `1205610` isMenuScreen(screen) (jump table): true for every menu-like screen, including
  TechGallery and YcPlayground. `12056b0` is true for screens 29..37 (multi-tool menus).
- Test (dev2, toggleHudScreen): cursor shows on screen 52 (isMenuScreen is true for it),
  but camera and walking still work and Esc goes to the system menu.
- Window input: beginWindow stores layer at window+0x7c, inputMask at +0x74, flags at
  +0x78. Flags bit 0 is modal: the UI system skips input for windows on lower layers
  while a modal window on a higher layer covers the same input mask (`118b8a0`,
  `11be190`). Every real menu opens its window with fullscreen=1 and flags=1 (layer
  9 for YcPlayground, 0x16 or 0xb for others); TechGallery uses fullscreen=0, layer 0,
  flags 0, so the world keeps the input. dev3 redirects the test screen's beginWindow call
  (screen fn + 0x73) to open it fullscreen, layer 9, modal.
- Other screen predicates: `1205720` true for 2, 3, 38, 39, 40, 49, 50; `1205830` true for
  29, 31-33, 36, 37; `1205690` true for 21, 22, 41.
- Test (dev3, modal window): mouse works, camera and walking stop, Esc and Home close it.
- Popup recipe (`c6a820`, reportSharedWorldPopup): beginWindow(fullscreen 1, layer 0x1b,
  mask -1, flags 1); beginRoot; root stretch (1,1) (`a3d000`); root background
  {0x80000000, 1.0} (ARGB, black at half alpha); beginBox panel; padding 90/60/90/60;
  spacing (0,30); selfAlignment 0x202 (centre); layout 0x102 (column); fancyBox(panel,
  params 0xb8: style 8, scale 1.0, rest 0); content; endBox x2; endWindow. Text styles
  in the UI bundle ([ui ctx]+0xc8): +0x11b94 (title), +0x10a0c (body), +0x11bec, +0x10abc.
  Node field writes: layout u16 @+0x6e, selfAlignment @+0x6c, stretch @+0x90,
  padding @+0xa8. dev4 draws the mod's own panel this way.
- Test (dev4): the panel draws with the game's frame and fonts; the root background
  dimming did not show (to look at later).
- settingOption (`ad0d70`, found by its call at `c44c59`; the function start has an
  identical twin): params 0xc48 bytes. Entries of 0x30 {u32 value, TextString label @+8}
  from 0, entry count u8 @0xc00 (max 64), row label TextString @0xc08, description
  TextString @0xc20, dword 0x10000 @0xc38 (arrows byte @0xc3a), dword 1 @0xc3c
  (interactive), byte 0 @0xc40. Returns true when the u8 value changed.
- settingHeader (`c8c6e0`)(TextString* title, float gap). dev5 draws the tool settings
  (mode, replace, style, fill, free placement) with these.
- Full-screen menus (HudMenu, `bd933c` chunk): blurBehind(ui ctx, node rect, {0xffffffff,
  1.0}) then a dark tint fillRect(`cf8a00`)(rect, colour from bundle+0xfa74), side images
  at bundle+0xd300, menuTabBar, drawHints. Node rect = node+0x114 (last layout).
- Layout/selfAlignment u16 values are alignments (low byte x, high byte y; 1 start,
  2 centre, 3 end). Direction comes from spacing: setSpacing (`11b3810`, [node+0xb8]=2)
  stacks vertically, setSpacingX (`11b3480`, [node+0xb8]=1) side by side.
- doButton (`cc8fb0`) params 0xa8 as the Deletion Confirm menu fills them: +4 dword
  0x10202, +8 word 0x100, +0xa warning byte, +0x10 qword 3 (style), +0x4c 10.0,
  +0x50 5.0, +0x54 1.0 (scale), +0x58 enabled, +0x78 TextString. Returns true on click.
- dev6: full-screen menu (blur, dark tint, menu at the left), Keys and Options tabs,
  key rebinding saved to the ini; opens from InGame and multi-tool screens 29-37.
- Test (dev6): rebinding and options work, Home works from the hammer (screen 29).
- menuTabBar (`bdaf40`)(menu, flag): the top tab bar of the full-screen menus (Q/E).
  Menus 3, 25, 27 always take the tab path. It fills a vector {data, count, capacity,
  grow fn} (inline room for 10) with menuTabList (`d11e90`, called at `bdb2a4`) and
  draws each tab with `cd0100` (called at `bdb719`; true = clicked, then `d67dd0`
  switches the game's screen). Tab 0x40 bytes: shortcut @+1 (0x8b none), label
  TextString @+0x18, active @+0x30, hover @+0x31, interactive @+0x32, opacity @+0x34.
  dev7 hooks both calls while it draws its own bar (menu 3) to show its pages.
- Test (dev7): tab bar looks and works like the game's; combos and columns work.
- Hint bar: HudMenu collects hints for the frame in a list at [ui ctx+0xd0]+0x32e18
  (16 x 0x48, count u8 @+0x480) and draws them with drawHints (`c2a9b0`)({list, count},
  0x303, padding {0,10,0,0}, 0, 0). makeHint (`b14b00`)(hint, UiActionId, enabled);
  custom label flag @+0x1b, label TextString @+0x20. UiActionId (types.txt
  keen::input_mapping::runtime::UiActionId): 0 Confirm, 1 Back, 10 NextTab,
  11 PreviousTab, 24 BindingReset, 25 BindingClear. dev8 draws a Back hint ("Close").

### Building HUD box rows (0.18.0-dev9)
- Building HUD box `b5d4e0` (named box at rbp+0x20). Key rows via `bc4e20`(u8 UiActionId, params 0x78)
  at b5dc6a and b5ddf2 ("Undo Build Action"); box finished by E8 at b5de4b (`11b3cf0`, rcx = &box),
  right after Creative Mode's hud.buildingAreaRows site (b5ddf7). bc4e20 looks the binding up
  (`e74570`(out 0xc4, inputMap = [[ui]+0xd0]+0x33cd0, action), 0x118 per action) and calls
  bindingHint `b2f8b0`(binding, params).
- Binding 0xc4: keyboard combos @0 and @0x30, bytes @0x60/0x61 (0x60 == 2: hold, progress), gamepad
  combos @0x62 / @0x92, bytes @0xc2/0xc3. Combo 0x30 = input (12 bytes) + up to 3 modifier inputs.
  Input: device u8 @0 (1 keyboard, 2 mouse, 6..8 gamepad), key u16 @2 = USB HID usage (SDL scancode:
  PageUp 0x4b, LeftControl 0xe0, LeftShift 0xe1, LeftAlt 0xe2; name parser `e83cd0`), mouse button
  u8 @4 (1 L, 2 R, 3 M, 4, 5). bindingHint skips the row if combo device is 0 and params+0x2c == 0.
- Row params 0x78: +0 combo index, +6 custom align flag (+4 u16), +0x28 flag, +0x2f = fill label from
  the action's name (bc4e20), +0x30 flag, +0x3c 1.0, +0x44 500.0, +0x48 hold progress (+0x4c set),
  +0x54 24.0, label TextString @0x58, colour {ARGB, intensity} @0x70.
- On/off switch `cf05d0`(bool* value, u8, u8) -> bool clicked; draws FbUiMenu.toggle textures
  (bundle +0xe860 menu, +0x1044 toggleOn/toggleOff). Not used: Aleksander prefers the bottom-left HUD mark and text On/Off options.
- Small on/off mark of the bottom-left HUD slots (c28190 @c284bb): box 21x21 (setSize 11b32e0), image ctx
  a3ddf0(), resolveImage a3eef0(ctx, &handle, texture), drawImage a3aab0(box, handle*, colour*);
  fancyToggleSmallOn bundle+0xfed4 (white), Off bundle+0xfec4 (colour qword bundle+0xd4d8).
- Building box title (b5d9a6 beginText): loca hud.building.buildTooltipAreaActions (bundle+0x41f0), empty
  in the game; dev11 replaces its string with mod messages and the area size.
- HUD toggle row `ccff30`(u8 action, bool on, TextString* label) ("first person view"): box, spacingX 10,
  stretch (1,0), key row bc4e20, stretcher `b1af90`(box, {1,1}), pill switch `ccfd00`(bool on), endBox.
  (The small mark at c28190 is the eye open/closed visibility icon.)
- Box title: the game uses it for messages like "Area protected by flame"; dev12 fills it only when empty.
- Tab widget cd0100 returns active || hovered (a41390 = point in node), not clicked: page switch needs our
  own click edge.
- Snapping cursor hint `cab2a0` (loca cursorHintSnappingActive/Ready/Off, bundle+0x4210/0x4220/0x4230):
  centred box (selfAlignment 0x202), offset (0, 60) (setOffset = node+0x70), blurBehind on the node rect,
  body text. dev13 draws the mod's floating text the same way in its own input-less window (layer 1),
  offset (0, 130). Box title override dropped (the game uses it for "Area protected by flame").
- Hint bars in game menus: drawHints pad {0,10,0,0} inside padded panels, {30,30,30,30} standalone.
- Soft rounded backdrop (snapping hint, cab59c): node rect at node+0x114 is {x, y, w, h}; rect expanded
  {x-8, y-4, w+16, h+8}; pass a3f2e0(uiCtx) / 119b2b0(pass) == 4 means draw; list 119b380(pass);
  blend 11ae450(list, 3) .. (list, 0); nine-slice 118bc50(list, rect, image u32, borders, radii[4]
  = min(w,h)/2, colour u64) with texture bundle+0xd3b0, borders bundle+0xd3c0. Used by dev14 at 63% alpha.

## v0.20: the game's language
- Reflection: `LanguageId` enum (35 values, 0 None, 1 En_Us, 10 Pl_Pl, 11 Ja_Jp, 20 Zh_Cn ...).
  The language menu (`FbUiLocaLanguages`) offers 15: English, German, French, Italian,
  Japanese, Korean, Polish, Portuguese (Brazil), Russian, Spanish, Thai, Turkish,
  Ukrainian, Simplified and Traditional Chinese.
- The choice is `g38_options::common::user::GameplaySettings.languageIdOverride` (+0x18).
  Saved as `"languageIdOverride": "Pl_Pl"` (enum name; "None" = system language) in
  `<Steam>\userdata\<account>\1203620\remote\user_settings.json` (Steam cloud). Machine
  settings (graphics, sound) are in `Saved Games\keen games\Enshrouded\local_settings.json`.
- With "None" the game uses the Steam language: `SteamAPI_SteamApps_v009()` +
  `SteamAPI_ISteamApps_GetCurrentGameLanguage` in steam_api64.dll ("polish", "schinese",
  "koreana", "brazilian", "latam" ...). The mod reads the json (newest user folder; Steam
  folder from steamclient64.dll's path), then Steam, at start and when its menu opens.
- Fonts: separate font groups `enshrouded_Japanese`, `_Korean`, `_SimplifiedChinese`,
  `_TraditionalChinese` (`UiFontLanguageGroupResource`). TextStrings are taken as UTF-8
  (checked with Polish in 0.20.0-dev1).

## v0.20.1: player report (area remove on foundations)
- The dismantle system (2d3c80) has two paths. Cursor target entity (cursor +0x78) null:
  blocks, RemoveShape at 2d465a with filter 0, PayCost 2d46b0, RemoveCommit 487af0.
  Entity set (placed building pieces such as foundations, 4x2x4 m): RemoveShape at
  2d4b04 with the entity id as filter (6th arg) and check 1, PayCost 2d4b5a, then
  488670(ctx, info, objectId), which removes the entity by id (9ad6f0 looks it up in a
  hash set) and never reaches RemoveCommit. These are the only two RemoveShape callers.
- So Area remove priced the area at the corner 2 click but the game removed only the
  clicked piece, and every corner click removed its piece (no commit to suppress).
- The path is chosen at 2d43de: rbx = flag [dep+0x80] == 1 ? [dep+0x78] : 0, dep =
  [rbp+0x1f8] (pointer at +8 of the 9bc1a0 dependency copy). v0.20.1 redirects the
  9b83c0 call at 2d43cf: in Area mode / connected delete it clears the flag after the
  call, so pieces take the block path (hammer shape, RemoveCommit); the flag is put back
  at the next RemoveShape or dismantle run. Fallback without it: RemoveShape with a
  non-null filter in those modes runs vanilla (one piece) with a banner.
- The dependency copy (deps_hook) did not give the state on the dismantle's thread, so
  the redirect goes through a thunk passing the caller's rbp ([rbp+0x1f8]) and rsi (the
  selected ItemInfo, from e7d430 at 2d3e5a).
- Selected item kind (ItemInfo, EquipmentSetup at +0x104): +0x440 voxelData.isBuildingVoxel,
  +0x458 voxelObject, +0x468 terrainVoxelObject. Piece = voxelObject && !isBuildingVoxel.
  The dismantle's block path needs terrain or (voxelObject && isBuildingVoxel). The shape
  state passed to PlaceShape / RemoveShape / ShapeBounds is this ItemInfo.

## v0.20.3: Replace far from the character (free camera)
- Player log (0.20.2): free camera tens of metres from the sitting character, Replace ->
  "aim lookup failed": client place (4384.5, 778.5, 2821.5), client aim 0.5 m below it
  (live and consistent), but the server's place cursor was (4401.5, 778.0, 2766.5), about
  57 m away; second client cursor all zero. Within some range of the character it works.
  The server cursor (server_cursor_no_prediction) is not where the player looks when the
  camera is far from the character, so neither the exact match nor the 6 m check held.
- v0.20.3: cursor_get_hook stamps each client cursor component with the last frame it
  was read. When the exact and near matches fail, aim_lookup takes the live aim of a
  client cursor read in the last 500 ms (single player: only the local player clicks),
  and the client's place cursor replaces the server's as the target (Area build corners).
  Logged as aim=client cursor (far) with a "client" transform line.
- dev1 test (Aleksander, free camera = game's Z + Space): worked near the character,
  failed from about half the distance: the client blueprint system stops its per-frame
  cursor read (frame cursor last read 9..28 s before the click) and the client cursor
  component keeps its last values (later all zero). 28bc80 still runs every frame with
  the live camera; its raw out is the ray hit (e.g. (3740.177, 843.000, 1428.830)) and
  the server's place cursor is exactly that floored to 0.5 m (3740.0, 843.0, 1428.5).
  The server cursor +0x338 is zero. The hover function 28e2f0 also still runs.
- dev3: 28bc80 keeps the raw hit, camera and view direction. query_cursor: place = cell
  of (hit - 0.05 m * fwd), aim = cell of (hit + 0.05 m * fwd) on the 0.5 m grid (the
  hit lies on the face's grid line). aim_lookup uses it ("ray hit") when that place
  equals the server's; big shapes take the server place (place == aim). The 6 m near
  fallback skips cursor components not read for 500 ms; the area preview and size
  readout use the ray-hit cursor while the client cursor is stale.
- dev3 test: middle distance worked (top faces); farthest failed on side faces: hit
  (3721.500, 844.739, 1389.555) looking +x, server place (3721.5, 844.5, 1389.5) with
  rot (0,0,0.707,0.707): the cursor position is a corner of the place cell that depends
  on the rotation, not the cell's minimum. dev4: check that the server place is a corner
  of the ray's place cell (0.25 m from its centre on every axis) and move the server
  place by (aimed cell - place cell); the area preview uses cell centres directly.
- dev4 test: works far away; one top-face click failed: hit (3720.826, 844.500, 1388.491)
  is 9 mm from the z grid line, so +-5 cm stepped on y and z. Another click near an edge
  "fit" but aimed one cell off in z. dev5 steps only on the axis whose grid line the hit
  is closest to.
- dev5 test: blocks fine far away; terrain failed: the hit is inside a voxel cell, not on
  a grid line (e.g. (3720.182, 844.734, 1388.196), server place (3720.0, 844.5, 1388.0) =
  that cell), so there was no face step. dev6: when the hit is more than 1 cm from every
  grid line, step one cell along the cursor's normal (rotation applied to +y; 28bc80 out
  +0x18), towards where the camera looks.
- Not changed: the dismantle path (Area remove, connected delete) still uses the server's
  aimed transform; untested far from the character.

## 0.21.0: dedicated server (issue #32)

- `enshrouded_server.exe` loads dinput8.dll; every server-side signature matches (PlaceShape
  223420, RemoveShape 223f40, CursorCall c06eb, PlaceCall c0a33, ServerCheck c09e2;
  blueprint fn c0560, dismantle fn c0f70). UI, free placement, preview: not found (expected).
- 0.20.3 on a server did nothing: the mod's settings live in the client process.
- Client->server: `ClientPlayerInputData` (0x3b8), encoder client 3a5640
  `(out, src, flags*, r9)`, server decoder 235060. camera.position int64 fixed point at
  +0x20, sent losslessly (schema code 9). cursorInput primary +0x2f8, secondary +0x330;
  digitalInput +0x3a8 (bit 0 MainhandAction, bit 4 SecondaryBuildingAction). flags bit0 =
  send primary, bit1 = send secondary (normally only while bit 4 is held). Steam P2P only,
  so no side channel.
- Server PlayerInput (0x5c0): fromClient at +8 -> camera +0x28, primary +0x300, aim +0x338.
  The "cursor component" the mod always read at +0x300/+0x338 is this component.
- Dismantle ctx+0x58 = PlayerInput. Blueprint ctx (rbp+0x130) is zeroed per item at
  c0791 and ctx+0x58 (rbp+0x188) is written only at c0cfc, after PlaceShape: dev1 found
  no player at place clicks, so area build and Replace ran vanilla while area remove
  worked. dev2 takes PlayerInput from rbp-0x58 (xf-0x118, the cursor read's r8).
- At the cursor read rbx = building item (`48 8B D8 48 85 C0 0F 84` at cc-0x49 in both exes).
- dev2 test (Aleksander, 2026-11-01): single player and dedicated server work.
- Free camera far away on a server (dev2): the client's blueprint cursor read stops, so
  the client stopped counting area clicks (server: every later click "repeat") and sent
  a stale secondary transform (src+0x330). dev3: "building item in hand" also when the
  hammer's ray (28bc80, q[3]==4) ran in the last 250 ms; while the cursor component is
  stale, src+0x330 is set for the encoder to the live place (src+0x2f8) moved by the
  ray-hit step (as aim_lookup's "ray hit") and restored after. Tested OK 2026-11-01.

## 0.22.0: tools wheel (issue #25)
- The game's wheels: `ds::WheelMenuType`, FbUiHUD.itemWheel (FbUiItemWheel, bundle+0xdb68:
  slot colours +0/+8, previewCenterColor +0x10, background +0x18, direction +0x28, dots
  +0x38/+0x48, ring +0x58/+0x68, blurredBackgroundMask +0x78, highlightColor +0x88).
  Sounds FbUiSounds.itemWheel (bundle+0x12a10 open, +0x12a20 close, +0x12a30 changeHighlight).
- Emote wheel `b93ad0` ("emoteWheel") calls the generic wheel `c04c50`(params): u8 kind
  (0 action bar, 1 emote, 2 other), +4 size 600, +8 draw centre, +0xc centre part 0.4,
  +0x10 start angle pi/2, +0x14 step, colours, +0x38 entries (FbUiEmoteEntry*, label is a loca
  tag). Its state lives in hud+0x1cda (2 = emote wheel open) and picks trigger emotes, so the
  mod draws its own wheel with the same calls instead.
- Draw calls (box = beginBox struct, rect = {x, y, w, h} from node+0x114): `118ca30`(box, rect,
  image u32 (0 = fill), colour u64); rotated `118d030`(box, rect, angle, image, colour);
  blend `11ae3e0`(box, mode). c04c50: blurBehind on the rect +-30, 25% black fill, blend 3 +
  the mask image, blend 0; centre disc; pointer turned to the slot angle (cos x, sin y); dot
  texture bundle+0xb9e8 24x24 per slot. Images through resolveImage(imageCtx, &h, texture).
- Mouse while open: the low-level mouse hook sums the moves (pt - GetCursorPos) and swallows
  moves, wheel turns and left/right clicks, so the camera stays still and nothing is built.
- Scrolling list (0.22.0-dev8), as the settings menu (`cc49d0`) draws its pages:
  params = GameUiScrollingLayoutParameterData (0x104) from `a324d0`; the settings menu sets
  enableScrollbar, centerChildren and useGlobalFocus (+0x2b/+0x2f/+0x32), hotkey mask |= 0x18,
  spacing 0, padding {0,10,0,0}, and the scroll bar (+0x44) textures from the UI bundle
  (+0xf954 range colour, +0xf914 range texture, +0xf8d4 slider texture), textured style,
  widths 24/24/24/20. beginScroll `a31640`(state 0x4e0, 1, &persistent float offset, 0,
  &params); node = state+8; scroll speed `d3aff0`(state) stored at state+0x464 (flag +0x468);
  endScroll `a33780`(state).

## 0.23.0: area preview shapes

- Debug draw is compiled out: the `draw_buildzones` system (32eeb0) is a bare `ret`.
- Build-zone borders: ZoneVisualizationSdfRenderingShaderData (flags: IsValid, DrawBuildZoneBorder
  = 2) with a preview box (previewBuildZoneBoundingSphere, previewBuildZone {halfSize, type},
  previewBuildZoneBorderColor). Source: client world singleton BuildZoneVisualizationState at
  world+0xffc0 (getter 3a4620 = `lea rax,[rcx+0xffc0]; ret`, one caller 856157 in the renderer):
  byte +0 draw borders, +0x10 min / +0x20 max (float4, world metres), byte +0x30 preview valid.
  `client_cursor_entity` (2919b0) fills the preview only for the flame altar (item hash
  0xebb93c94, size from the build zone balancing, x/z + 0.25). The SDF only shades surfaces
  near the box faces, so in open air nothing shows. 0.23.0 redirects the getter call (Preview=2).
- Live preview corner on a wall was 0.5 m off: corner 1's centre - cursor offset depends on the
  cursor rotation (ground: (+0.25,+0.25,+0.25); wall with rot (0.707,0,0,0.707): (+0.25,-0.25,+0.25)).
  Fixed by rotating the offset into the shape frame (corner 1 rot) and out with the live rot.
- Tried and dropped (dev builds): `client_cursor_entity`'s entity transform write (component lookup
  9b3cb0 at 292596, WorldTransform with float3 scale) is not the blue block ghost; overriding it
  showed nothing. Looped effects with a full transform: start 29d3a0(sys, time, &guid, &WorldTransform,
  &params), move 29cd90, float param 298f00(sys, handle, hash, f), vec3 param 298fc0, stop 29d420.
  All effect starts go through 29bbe0/29be20/29c160/29c2e0/29c410/29c520 (each calls 29b5f0).
  Starting a prefab-preview effect (9183293c-4fa853c0-d0ed4f84-2d8b98e1) at the area crashed in
  game+0x896580 (probably needs the prefab's resources loaded).
- VfxTerraformingCursorInstance {boxSize, sphereRadius, position, isBox, orientation, ring/fill
  alpha} is a see-through box/sphere render effect (RenderEffectId TerraformingCursor); which VFX
  asset uses it was not found.
- The renderer is Vulkan (vulkan-1.dll, DXGI swap chain interop): drawing the mod's own geometry
  would mean hooking it.

## 0.24.0: drawn 3D area box (Vulkan)

- Vulkan comes in through `GetProcAddress(vulkan-1, "vkGetInstanceProcAddr")` (the exe's only
  import of it): patching that IAT entry lets the mod wrap instance/device proc address lookups.
- Render facts: world at render resolution (DLSS, e.g. 2560x1440) with dynamic rendering, GPU
  driven (few draws); scene depth D32_SFLOAT, reversed Z (near 0.01 / far 10000), HDR colour
  R16G16B16A16_SFLOAT; UI layers 3968x2288 R8G8B8A8 composited into the swapchain
  (A2B10G10R10). Scene depth = depth attachment of the last vkCmdBeginRendering pass >= 640 px
  wide with an R16G16B16A16F colour target and SAMPLED usage; its layout from the last barrier.
- Camera: cursor system sys+0x640 int64[3] position (2^32 per metre), +0x658 quaternion,
  +0x678 distance to the look-at point (character), +0x680 aspect, +0x684 near, +0x688 far.
  Runs about twice per frame (identical entries). The render frame function 13d910 has a
  ClientCamera copy at arg3+0xad0 (fovY 1.1345 rad) but it lags about 3 frames; the cursor
  camera with -0.25 frame prediction and fov 60 matched best on screen.
- Blocked-voxel query a8a290 (called by the hover fn 28e2f0 at +0x2ff with
  rdx = [world], r8 = [world+8], world = ctx+0x130):
  `a8a290(list*, w0, w1, query*) -> byte` (1 = nothing in the way).
  List {data* +0, count +8, cap +0x10, grow fn +0x18 (null: entries past cap are dropped), +0x20 0};
  entries int32[3] voxel coords of taken voxels.
  Query (built by 48bd50 from a blueprint item): +0 data*, +8 count, +0x10 byte bit-packed,
  +0x14 size uint3, +0x20 pos int3, +0x2c rotated size, +0x38 orientation (axis index dwords
  +0x38/+0x3c/+0x40, "positive" flag bytes +0x44..+0x46), +0x48 ptr (unused by the query).
  Per voxel (a83ac0): blueprint voxel set and world voxel byte >= 0x80 -> taken.
  a8d280 (the other branch) is `xor al,al; ret`.


## 0.24.1: Patch #1 (1.0.1.0, build 1283531, 2026-11-02)

The patch rebuilt both executables. 14 of the client's 81 signatures stopped matching, and
the server lost 6 of its own; the mod refused to start ("RemoveCommit / RefundBuild /
HudUpdate / UiSound: not found"). How it was redone: the 1.0 and patched executables were
compared function by function (`.pdata` functions near the old address, instruction
sequences aligned with difflib, immediates and rip/rel32 displacements ignored), and the
reflection dumps (`refl.py`) of both were diffed.

What moved (patched client RVAs):

| What | 1.0 | Patch #1 | Change |
|---|---|---|---|
| RemoveCommit | 487af0 | 48ee40 | new prologue (`mov [rsp+18],rbx; push rbp...`), still 5 bytes stolen |
| RefundBuild | 48ae10 | 4920f0 | new prologue `mov rax,rsp; mov [rax+8],rcx`: **7 bytes stolen** |
| HUD building update | b25350 | b34c20 | new prologue `mov rax,rsp; push rbp; push rbx` |
| Dismantle system | 2d3c80 | 2d8da0 | frame shrank by 0x20: deps copy rbp+0x1d0, aim state [rbp+0x1d8] (read from the code now), pickup writer rbp+0x350 |
| Zone state getter | 3a4620 (world+0xffc0) | 3aa8a0 (world+0xffd0) | check accepts any `lea rax,[rcx+disp32]; ret` |
| UI ctx [+0xd0] state | +0x33cc0 / +0x31bdc / +0x32c90 | +0x33ce0 / +0x31bf4 / +0x32ca8 | only inside signatures |
| GameplayAction::Count | 0x8b | 0x8c | VoiceChatMute added at 125; the tab "no shortcut" value |

FbUiBundle grew from 0x13e88 to 0x14038 bytes (logos +0x180 onward, menu/sounds/styles
+0x1b0). Every bundle offset the mod uses was converted by field path:

| Field | 1.0 | Patch #1 |
|---|---|---|
| sounds (menu.tabChange / highlightChange / entryTrigger) | 0x12310 / 0x12320 / 0x12330 | 0x124c0 / 0x124d0 / 0x124e0 |
| sounds.itemWheel.open / close / changeHighlight | 0x12a10 / 0x12a20 / 0x12a30 | 0x12bc0 / 0x12bd0 / 0x12be0 |
| sounds.hud.buildActionFailed | 0x12b70 | 0x12d20 |
| namedStyles.defaultStyle / menu.header1 | 0x10a0c / 0x11b94 | 0x10bbc / 0x11d44 |
| menu.scrollbar slider / range / rangeTint | 0xf8d4 / 0xf914 / 0xf954 | 0xfa84 / 0xfac4 / 0xfb04 |
| menu.fancyToggleSmallOff / On | 0xfec4 / 0xfed4 | 0x10074 / 0x10084 |
| globalColors.attentionColor / warningColor | 0xd4c0 / 0xd4d8 | 0xd640 / 0xd658 |
| primitives.glowBoxLarge / border | 0xd3b0 / 0xd3c0 | 0xd530 / 0xd540 |
| hud.itemWheel previewCenterColor / background / direction / blurredBackgroundMask | 0xdb78 / 0xdb80 / 0xdb90 / 0xdbe0 | 0xdcf8 / 0xdd00 / 0xdd10 / 0xdd60 |
| icons.genericIcons.arrowRight | 0xb9e8 | 0xbb68 |

Unchanged (checked): PlaceShape, RemoveShape, ShapeBounds, PayCost, PopUndo, the cursor
transform (registers swapped only), HoverFx and its VFX calls, InputEncode, the blit check,
the material picker functions, the magic chest functions, every UI building block, the
building/item/cursor reflection types (EquipmentSetup, cursor components, base records).
PlaceCommit was changed inside (its stack layout), but its arguments and Commit layout are
the same. The patched dedicated server matches the same signatures (dismantle aim state
also at rbp+0x1d8).

## Hotfix #4 (1.0.1.1, build 1285601, 2026-11-03): no mod change

0.28.1 works unchanged. All 81 client signatures still match once, and every signature
the patched server matched before still matches once (same RVAs except BlitCheck a38b37 ->
a38bb7 and MaterialItem 933330 -> 9333c0; the blit patch bytes around it are the same).

Client RVAs: the building code (PlaceShape .. PickCall, InputEncode, cursor/hover/dismantle)
did not move. From about 0x9c0000 up code moved -0x20 (HudUpdate b34c00, UiSound b3643a,
SystemDeps 9c7aa0, ZoneVisCall 860150, the UI building blocks and menu), the 0x11a..0x11d
UI primitives moved -0xda0, and the data the UI sound and hover code point at moved -0x1000
(UI sound context 283f4a8, hover spawn defaults 1528a88). The mod reads all of those from
the code; only tests/test_core.cpp has them hard-coded.

Reflection: FbUiBundle is still 0x14038 bytes and every bundle field the mod uses has its
Patch #1 offset (checked by field path, e.g. sounds.hud.buildActionFailed 0x12d20,
namedStyles.menu.header1 0x11d44, hud.itemWheel.background 0xdd00). The startup log on
the hotfix client shows every feature available.

## 0.29.0: exact mode

- HudScreenType values (from the enum's strings in the exe, counted back from the end):
  1 InGame, 2 SystemMenu, 29 BlueprintMultiTool (hammer building), 30
  BlueprintMultiToolMaterialCatalog, 31 OvergrowthMultiTool, 32 DecayMultiTool, 33-35
  ScrewdriverMultiTool BuildingMode / GraphManager / EntityConfig, 36 TerraformingSideMenu,
  52 TechGallery (the mod's screen), 53 YcPlayground (Creative Mode's).
- Frame render function 13fe20 (1.0.1.1, size 0x838d): `SIG_RENDER_FRAME`, prologue
  `mov [rsp+18],r8; mov [rsp+8],rcx; push rbp..r15; lea rbp,[rsp-0x6258]; sub rsp,0x6358`.
  It reads stack arguments 5-8 ([rbp+0x62c0..0x62d8]), so it is hooked through a
  register- and stack-preserving thunk. The render camera (int64[3] position, then the
  rotation quaternion) is at arg 2 + 0x8d0 on 1.0.1.1; the mod scans for it at run time
  (the 1.0 build had it at arg 3 + 0xad0).
- The game drops a hammer click while its ghost is invalid (flying, aiming at the sky):
  PlaceShape is not called. Forcing the cursor read's valid byte (out+0x28) with a
  position in front of the camera, as free placement does, lets the click through.
- Material catalog: toggling screen 30 from screen 52 opens the game's picker for the
  item in hand (its category only); closing it returns to 29.

## 0.29.1: long cost lists

- An area action charges one cost entry per shape (0x18 bytes, item id at +0x10, count
  at +0x14). PayCost (495e40) mode 0 runs 3f8900, which takes every entry through 411250
  (backpack, then chests by the order byte at transaction+0xb1) and returns 4 ("not
  enough") when an entry is left over. With 856 entries of 1 item it failed although the
  backpack held about 1300 of that item; 324 entries always worked. The exact limit was
  not found. Charging the same total as one entry per distinct item (first 0x14 bytes
  equal) succeeds. The mod passes a merged copy to PayCost and leaves the game's list as
  it was, so undo and the commit still see one entry per shape.

## Object placement (0.30.0, client 1.0.1.1)

- System `player_building_place_prop` (2da200) runs on the place animation's ChangeVoxel
  event (type hash 0x92273809, +0x60 changeType: 1 add, 4 undo). Held item = e94390(id) in rdi.
- Place info at rbp+0x120 (filled before paying): +0 placedEntity template guid (item +0x3c0),
  +0x10 float3 position in metres, +0x1c rotation quaternion, +0x2c / +0x38 local bounds
  (494130), +0x44 an id (41aa60).
- PayCost (495e40) at 2dab64 with the undo step's list ([rbp+0x260]+0x110, consume = step
  +0x1f8) when an undo step exists, else at 2dabc1 with a stack list (rbp+0x4c0, consume 1);
  rcx = rbp+0x3d0 at both. Then **PlaceProp 48ddd0(ctx = rbp+0x170, info, item id)** at 2dac34
  (returns al = placed): spawn 48d330, an undo record per object (498830, only when ctx+0xf0
  holds a step: an undo step takes 32 records, a full one is fatal), BuildingPlaceEvent
  4973a0; then the result event 492ff0.
- Undo step for props: set up (storage 4996b0 -> [rbp+0x258], step 498960 -> [rbp+0x260])
  only when item +0x5d8 & 0x88, +0x440 (building voxel) or +0xcc == 6 (test at 2da4ed). Most
  objects have none: the game cannot undo them. Forcing the step (tested in 0.30.0-dev5/6)
  left it without records (the record add in 48ddd0 is skipped) and the undo key never
  reached player_building_undo (2dad50; it takes the latest step with 4997a0 and undoes prop
  records through 491320 -> 490950), so object lines have no undo.
- Client: client_cursor (294540) builds its cursor query with 28c350(q, sys, item, ...) (second
  call at 2954ea, SIG_CURSOR_QUERY), which gives the held item; q[3] is 4 for props too. Its
  cursor transform call (2904e0) at 295546 gives the ghost. World for the voxel query =
  *(sys - 0x220).
- The mod: clicks before the last return success from both hooks without paying or placing;
  the last click pays the cost list with every count times N and calls PlaceProp N times.
  Clicks more than 1.5 m from this game's ghost are other players' (a hosted game runs their
  placements too) and stay vanilla.

## Object undo (0.31.0, client 1.0.1.1 RVAs; 1.0.1.2 about +0xE0)

- Undo apply 491320 switches on record type (+0x4c): 0 PlaceProp 48ddd0, 1 RemoveProp
  490950, 2 48e610, 3 48f9d0, 4 48ee40, 5 48e0f0, 6 490d10, 9 terrain. Record +0x48 = item.
- RemoveProp 490950(ctx, {u32 entity, u8 method}, item): queues the entity's destroy
  (9601f0), then method 1 (PickUp) event 0x795389e9 or 2 (TearDown) 4974e0. The dismantle
  system's pick-up (2d9fdb) calls it after paying the refund (492750 list).
- PlaceProp adds {entity (spawn 48d330's result), method = item ? 1 : 2} with 498830 at
  48df63, behind `call eb39e0` (`mov al,1; ret`), ctx+0x151 and ctx+0xf0. eb39e0 also gates
  the pick-up's own record in 490950 and the dismantle pick-up's undo step.
- Server cursor (server_cursor_no_prediction 297760) sets ServerCursor +0x69 bit 3
  CanUndoBuildingAction from 4962f0(player, item, nearZone) (same item test as place_prop,
  then 21d7b0(player, 0x6000000080) == 0), the storage and 4999e0 (a step exists).
- Client player_control_action_sequence (2dcbc0; body 2dde25): 2c5de0(out, sys, item) at
  2de6d4 picks the hammer branch (out[0] != 0), which calls 2c72b0(sys) at 2df35e: needs
  ServerCursor bits 4 and 8 and input action 40 (BuildingUndo) pressed in the action set
  at sys+0x2f8 (4739a0: bit set in +8, clear in +0), then requests action sequence 0x28
  (219ae0, 0x8d132f0e). With an object in hand bit 40 is never set.
- Mod: SIG_PROP_UNDO_TEST (cave), SIG_PROP_REC_CALL, SIG_CAN_UNDO_CALL, SIG_BUILD_MODE_CALL,
  SIG_UNDO_KEY_CALL. The undo's refund call (SIG_PAY_UNDO) tells the key code the undo ran.

## Exact rotation and nudge (fork 0.31.2-props1, Hotfix #5 RVAs)

- Server `player_building_place_prop` = c6280 in enshrouded_server.exe (client 2da2e0; the
  mod's SIG_PROP_PLACE_CALL, SIG_PROP_PAY_A / _B and SIG_PROP_UNDO_TEST match once in the
  server exe too: c6ca3, c6bd5, c6c30, c656d). The place info's position and rotation come
  from `39420(out, rdx = [rbp-0x68], r8 = [rbp-0x58])` at c6a4e: a 0x38-byte cursor
  transform, int64 position converted to float metres (×2^-32) into info +0x10, rotation
  floats into +0x1c.
- 39420: `test byte [rdx+0x1b1], 1` and word [rdx+0x18] == 5 or 8 -> copy r8+0x338 (the
  aim), else r8+0x300 (the place cursor). r8 = PlayerInput: +0x300 / +0x338 are the
  client's ClientPlayerInputData +0x2f8 / +0x330 (fromClient at +8). So the server places
  an object at the transform the client sent; it does not cast its own ray for objects.
  It reads the latest input when the place animation's ChangeVoxel event runs, not the
  click's packet, so a transform patched only in the click's packet would be lost.
- Client: client_cursor (294620) calls the cursor transform 2905c0 at 295626 with out =
  rbp+0x350, then 2994d0 twice (rsi+0x10 and rsi+0x48, the cursor component's place and
  aim) with out as an argument; 2994d0 checks the placement (valid flags at [r12]) and
  stores the component's transform. Whether the stored rotation is the one in out was not
  traced to the end: the input hook logs what goes out (moved already / moved here).

## Hotfix #5 (1.0.1.2, build 1287369, 2026-11-04): no mod change

0.30.0 works unchanged (checked offline on the hotfix exes). All 86 client signatures
match once. Almost all client code moved +0xE0 (PlaceShape 491a10, HudUpdate b34ce0,
UiSound b3651a, InputEncode 3aba10, ...); the 0x11a..0x11d UI primitives moved +0xD0.
Data did not move (UI sound context 283f4a8, hover spawn defaults 1528a88, highlight guid
b0718612). The bytes the mod patches or steals are the same: hook prologues, blit patch
site/continue/skip, dismantle pickup writer (rbp+0x350) and aim state (rbp+0x1d8), zone
vis getter (now 3aa980). The cursor query hook moved to 28c430.

Reflection: all 12,658 reflected types (names, sizes, field offsets and enum values) hash
the same as on Hotfix #4, so FbUiBundle and every struct the mod reads are unchanged.

Server: the same 31 signatures match as on Hotfix #4, at the same RVAs except BlitCheck
a38bb7 -> a38ba7 and MaterialItem 9333c0 -> 9333b0.
