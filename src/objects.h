// ---- Object placement (issue #18): objects along a line, a curve or a circle
// Included from main.cpp (single translation unit), after the UI helpers.
//
// How the game places an object (a prop held in hand, client 1.0.1.1): the server system
// player_building_place_prop (2da200) runs on the place animation's ChangeVoxel event, pays
// the object (PayCost, call sites found by SIG_PROP_PAY_A / _B; the cost list is the held item)
// and calls PlaceProp (48ddd0, call site SIG_PROP_PLACE_CALL) with an info block: +0x00
// placedEntity template guid (item +0x3c0), +0x10 float3 position (metres), +0x1c rotation
// quaternion, +0x2c / +0x38 local bounds, +0x44 an extra id. PlaceProp spawns the entity, records
// its undo step and sends BuildingPlaceEvent.
//
// The mod (single player: the server runs in the game's process): in Line / Curve / Circle
// mode the clicks before the last only mark a point (PayCost and PlaceProp are skipped); the
// last click pays all objects at once and calls PlaceProp once per object with its own position
// and rotation. The client side (the game's client_cursor system, game thread) knows the held
// item (its cursor query builder 28c350 gets it, call SIG_CURSOR_QUERY), reads the ghost
// (cursor transform), works out the spots with the ground under them (the voxel query of the
// hover code) and draws the path (vkdraw.h). The server uses those spots when they were worked
// out for the same points, else it works them out itself (slope height instead of ground).

using core::ObjOpts; using core::ObjSpot;
static const int kObjMax = core::kObjMax;

// ---- client: what is in hand, the ghost
typedef uint64_t (*CursorQueryFn)(uint8_t* q, uint8_t* sys, uint8_t* item, void* d, uint64_t e, uint64_t f);
static CursorQueryFn g_cursorQueryOrig = nullptr;
static uint8_t* volatile g_propItem = nullptr;    // the held object (ItemInfo), or null
static volatile DWORD g_propTick = 0;
static void* volatile g_objWorld = nullptr;        // client world (voxel query) seen by client_cursor
struct ObjGhost { int64_t pos[3]; float rot[4]; uint8_t valid; DWORD tick; };
static ObjGhost g_ghost;
static bool prop_item(const uint8_t* it) {
    if (!it || !readable(it + 0x3c0, 0x120)) return false;
    if (item_kind(it) != ITEM_OTHER) return false;                             // blocks, pieces, terrain
    if (!(*(const uint64_t*)(it + 0x3c0) | *(const uint64_t*)(it + 0x3c8))) return false;   // EquipmentSetup.placedEntity
    if (it[0x427]) return false;                                               // requiresAltarSlot (flame altar)
    return true;
}
static bool prop_now() { return g_propItem && (GetTickCount() - g_propTick < 300 || g_objWin); }   // the window stops the cursor
// Joined someone else's world (the host's game runs the placement): only when known for sure. A game
// you host (also when alone) runs the world in this process, so the lines work there.
static bool obj_remote() { return g_dedicated || g_remoteKnown == 1; }
// The click is this player's own (a hosted game also places the objects of the players who joined):
// the click's spot is next to this game's own ghost of the held object (moved by props.h, or as
// the game had it before the move).
static bool prop_at_base(const float* pos);
static bool own_click(const float* pos) {
    if (!pos || !prop_now() || !g_ghost.valid || GetTickCount() - g_ghost.tick > 1500) return false;
    double d2 = 0;
    for (int a = 0; a < 3; a++) { double d = (double)g_ghost.pos[a] / core::kFix - pos[a]; d2 += d * d; }
    return d2 < 1.5 * 1.5 || prop_at_base(pos);
}
// Plants (ItemInfo.growthInfo.hasGrowthInfo, +0x7d8: seeds and seedlings) are left to XHL
// Planting Assistant when it is loaded with planting on (XHL\config\xhl-area-harvest.json,
// "planting.enabled", read when its DLL is first seen): it plants rows and squares itself.
static bool plant_item(const uint8_t* it) { return it && readable(it + 0x7d8, 1) && it[0x7d8] != 0; }
static bool other_mod_plants() {
    static volatile LONG known = -1;                 // -1 not looked yet, 0 no, 1 yes
    static DWORD lookTick = 0;
    if (known >= 0) return known == 1;
    const DWORD now = GetTickCount();
    if (now - lookTick < 2000) return false;
    lookTick = now;
    if (!GetModuleHandleW(L"XHL-Area-Harvest.dll")) return false;   // not (yet) loaded: look again later
    bool on = true;                                  // its default
    wchar_t path[MAX_PATH];
    lstrcpynW(path, g_dir, MAX_PATH - 40); lstrcatW(path, L"XHL\\config\\xhl-area-harvest.json");
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char buf[2048 + 32] = {}; DWORD n = 0;
        if (ReadFile(h, buf, 2048, &n, nullptr)) {
            buf[n] = 0;
            const char* k = buf;
            for (; *k; k++) if (!memcmp(k, "\"planting.enabled\"", 18)) break;
            if (*k) { k += 18; while (*k == ' ' || *k == ':') k++; on = *k != 'f'; }
        }
        CloseHandle(h);
    }
    known = on ? 1 : 0;
    LOG(.str("objects: XHL Planting Assistant loaded, planting ").str(on ? "on: plants are left to it" : "off"));
    return on;
}
static bool obj_tool_on() {
    if (!g_cfg.toolObjects || g_modOff) return false;
    uint8_t* it = g_propItem;
    return !(it && plant_item(it) && other_mod_plants());
}
static int obj_mode() { return obj_tool_on() ? g_cfg.objMode : core::OBJ_SINGLE; }
static ObjOpts obj_opts() {
    return ObjOpts{ g_cfg.objMode, g_cfg.objCount, g_cfg.objSpacing, g_cfg.objRot, g_cfg.objElev, g_cfg.objRef, g_cfg.objTurn,
                    (uint32_t)g_cfg.objSeed };
}

// ---- clicked points (written by the server side, read by the client: one process)
static void obj_undo_flag();
static double g_objPt[3][3];
static float g_objPtRot[4];
static volatile LONG g_objPtN = 0;
static void obj_clear(const char* why) {
    EnterCriticalSection(&g_stateLock);
    LONG had = g_objPtN;
    g_objPtN = 0;
    LeaveCriticalSection(&g_stateLock);
    if (had) LOG(.str("objects: points cleared (").str(why).c(')'));
    obj_undo_flag();
}
static const Str kObjModeName[core::OBJ_MODE_COUNT] = { S_OBJ_SINGLE, S_OBJ_LINE, S_OBJ_CURVE, S_OBJ_CIRCLE, S_OBJ_ARCH, S_OBJ_VCIRCLE };
static void obj_set_mode(int m) {
    g_cfg.objMode = ((m % core::OBJ_MODE_COUNT) + core::OBJ_MODE_COUNT) % core::OBJ_MODE_COUNT;
    obj_clear("mode");
    write_full_ini(g_cfg);
    LOG(.str("objects: mode -> ").i(g_cfg.objMode));
}

// ---- ground under a spot: the hover code's voxel query, one column of 48 voxels (24 m),
// cached; a few new columns a frame (game thread only)
struct ObjCol { int32_t vx, vz, v0; uint32_t tick; uint64_t bits; };
static ObjCol g_objCol[4096];
static int g_objProbeBudget = 0;
static LONG g_objProbes = 0;
static bool obj_ground(void*, double x, double z, double yRef, double* y) {
    int32_t vx = (int32_t)core::d_floor(x * 2), vz = (int32_t)core::d_floor(z * 2);
    // the column (48 voxels) starts 12..20 m below yRef on a 8 m grid: the same column for heights a
    // few metres apart, so a moving line keeps reading the cache (no jumps while the camera turns)
    int32_t v0 = (int32_t)core::d_floor(yRef * 2) - 24;
    v0 = (int32_t)core::floor_div(v0, 16) * 16;
    if (vx < 0 || vz < 0 || v0 < 0) return false;
    uint32_t h = ((uint32_t)vx * 73856093u ^ (uint32_t)vz * 19349663u ^ (uint32_t)v0 * 83492791u) & 4095;
    ObjCol& c = g_objCol[h];
    const DWORD now = GetTickCount();
    if (!(c.tick && c.vx == vx && c.vz == vz && c.v0 == v0 && now - c.tick < 10000)) {
        void* world = g_objWorld;
        if (g_objProbeBudget <= 0 || !g_voxQuery || !world || readable_len(world, 16) < 16) return c.tick && c.vx == vx && c.vz == vz && c.v0 == v0 ?
            core::obj_column_top(c.bits, 48, v0, yRef, y) : false;
        void* w0 = ((void**)world)[0]; void* w1 = ((void**)world)[1];
        if (!w0 || !w1) return false;
        g_objProbeBudget--;
        if (!g_voxOnes[0]) memset(g_voxOnes, 1, sizeof(g_voxOnes));
        static int32_t hits[48][3];
        VoxQuery q = {};
        q.data = g_voxOnes; q.count = 48; q.bits = 0;
        q.size[0] = q.rsize[0] = 1; q.size[1] = q.rsize[1] = 48; q.size[2] = q.rsize[2] = 1;
        q.pos[0] = vx; q.pos[1] = v0; q.pos[2] = vz;
        for (int a = 0; a < 3; a++) { q.axis[a] = (uint32_t)a; q.pos3[a] = 1; }
        VoxList list = { hits, 0, 48, nullptr, 0 };
        g_voxQuery(&list, w0, w1, &q);
        uint64_t bits = 0, nh = list.count < 48 ? list.count : 48;
        for (uint64_t k = 0; k < nh; k++) {
            int d = hits[k][1] - v0;
            if (hits[k][0] == vx && hits[k][2] == vz && d >= 0 && d < 48) bits |= 1ull << d;
        }
        c.vx = vx; c.vz = vz; c.v0 = v0; c.bits = bits; c.tick = now ? now : 1;
        g_objProbes++;
    }
    return core::obj_column_top(c.bits, 48, v0, yRef, y);
}

// ---- the plan: the spots for the points clicked so far and the ghost as the next point
struct ObjPlan {
    int pts;                     // points used (clicked + the ghost)
    int clicked;                 // of them clicked
    double pt[3][3];
    ObjOpts opts;
    int n; ObjSpot spot[kObjMax];
    bool ground;                 // every spot found its ground
    DWORD tick;
};
static ObjPlan g_plan;           // under g_stateLock
static bool same_opts(const ObjOpts& a, const ObjOpts& b) {
    return a.mode == b.mode && a.count == b.count && a.spacing == b.spacing && a.rot == b.rot && a.elev == b.elev &&
           a.ref == b.ref && a.turn == b.turn && a.seed == b.seed;
}
struct GroundCtx { bool all; };
static bool obj_ground_note(void* ctx, double x, double z, double yRef, double* y) {
    bool ok = obj_ground(nullptr, x, z, yRef, y);
    if (!ok) ((GroundCtx*)ctx)->all = false;
    return ok;
}
// Works out the spots and the drawn path. probe = new ground columns may be read (game thread).
static void obj_plan_body(bool probe);
static void obj_plan_update(bool probe) {
    static volatile LONG busy = 0;                  // game thread and the settings window (UI thread)
    if (InterlockedExchange(&busy, 1)) return;
    obj_plan_body(probe);
    busy = 0;
}
static void obj_plan_body(bool probe) {
    const int mode = obj_mode();
    if (mode == core::OBJ_SINGLE || !prop_now() || obj_remote()) { set_draw_path(nullptr, 0, nullptr, 0); return; }
    g_objProbeBudget = probe ? 16 : 0;
    ObjOpts o = obj_opts();
    double pt[3][3]; int n, clicked;
    EnterCriticalSection(&g_stateLock);
    clicked = n = g_objPtN;
    memcpy(pt, g_objPt, sizeof pt);
    LeaveCriticalSection(&g_stateLock);
    const int need = core::obj_points(mode);
    const DWORD now = GetTickCount();
    if (n < need && g_ghost.valid && (now - g_ghost.tick < 500 || g_objWin)) {
        for (int a = 0; a < 3; a++) pt[n][a] = (double)g_ghost.pos[a] / core::kFix;
        n++;
    }
    static ObjPlan p;
    // nothing moved and the ground is all read: the last plan stands (a long line costs frames)
    if (p.tick && n == p.pts && clicked == p.clicked && !memcmp(pt, p.pt, sizeof pt) && same_opts(o, p.opts) && p.ground &&
        now - p.tick < 1000) {
        EnterCriticalSection(&g_stateLock); g_plan.tick = now; LeaveCriticalSection(&g_stateLock);
        p.tick = now;
        return;
    }
    p.pts = n; p.clicked = clicked; memcpy(p.pt, pt, sizeof pt); p.opts = o;
    // Ground: only when the clicks sit on the ground (an object on a wall keeps the line's height)
    ObjOpts lo = o;
    bool onWall = false;
    if (lo.ref == core::OBJ_REF_GROUND && !core::obj_upright(mode))
        for (int k = 0; k < n; k++) {
            double g;
            if (!obj_ground(nullptr, pt[k][0], pt[k][2], pt[k][1], &g) || g - pt[k][1] > 0.6 || pt[k][1] - g > 0.6) { onWall = true; break; }
        }
    if (onWall) lo.ref = core::OBJ_REF_SLOPE;
    GroundCtx gc = { true };
    p.n = n >= 2 ? core::obj_layout(lo, pt, n, obj_ground_note, &gc, p.spot, kObjMax) : 0;
    p.ground = gc.all;
    p.tick = now;
    EnterCriticalSection(&g_stateLock);
    g_plan = p;
    LeaveCriticalSection(&g_stateLock);
    // drawn: the path (a little above the ground) and a marker per object
    static double path[kPathMax][3];
    static int64_t pf[kPathMax][3], mf[kObjMax][3];
    int np = n >= 2 ? core::obj_path(lo, pt, n, obj_ground, nullptr, path, kPathMax) : 0;
    for (int i = 0; i < np; i++) { pf[i][0] = (int64_t)(path[i][0] * core::kFix); pf[i][1] = (int64_t)((path[i][1] + 0.06) * core::kFix); pf[i][2] = (int64_t)(path[i][2] * core::kFix); }
    for (int i = 0; i < p.n; i++) { mf[i][0] = (int64_t)(p.spot[i].p[0] * core::kFix); mf[i][1] = (int64_t)((p.spot[i].p[1] + 0.12) * core::kFix); mf[i][2] = (int64_t)(p.spot[i].p[2] * core::kFix); }
    set_draw_path(pf, np, mf, p.n);
    static DWORD logAt = 0;
    if (g_cfg.log && now - logAt > 5000 && n >= 2) {
        logAt = now;
        LOG(.str("objects: plan mode ").i(mode).str(" points ").i(clicked).c('+').i(n - clicked).str(", ").i(p.n).str(" spots, path ").i(np)
            .str(", ground ").str(o.ref == core::OBJ_REF_GROUND ? (onWall ? "off (clicks not on the ground)" : p.ground ? "all found" : "partly") : "not used")
            .str(", columns read ").i(g_objProbes).str(", world ").str(g_objWorld ? "yes" : "no"));
    }
}

static uint64_t cursor_query_hook(uint8_t* q, uint8_t* sys, uint8_t* item, void* d, uint64_t e, uint64_t f) {
    PROF(PF_CQUERY);
    uint64_t r = PROF_CALL(PF_CQUERY_G, g_cursorQueryOrig(q, sys, item, d, e, f));
    if (sys && readable(sys - 0x220, 8)) g_objWorld = *(void**)(sys - 0x220);     // client_cursor's deps (rbp+0xa0) + 0x130
    uint8_t* it = prop_item(item) ? item : nullptr;
    if (it != g_propItem && g_propItem && it && g_objPtN) {
        obj_clear("other object in hand");
        flash(T(S_OBJ_CLEARED));
    }
    if (it) {
        static uint8_t* logged = nullptr;
        if (logged != it) {
            logged = it;
            LOG(.str("objects: object in hand, item ").x(*(uint32_t*)it).str(" template ").x(*(uint64_t*)(it + 0x3c0))
                .str(" flags ").x(*(uint32_t*)(it + 0x5d8)).str(" query mode ").u(q ? q[3] : 255)
                .str(plant_item(it) ? " plant" : ""));
        }
        g_propItem = it; g_propTick = GetTickCount();
    } else if (item != nullptr || GetTickCount() - g_propTick > 300) g_propItem = nullptr;
    obj_plan_update(true);
    obj_undo_flag();
    return r;
}
// from cursor_xf_hook: the ghost of the held object (client_cursor's own call)
static void obj_ghost(const uint8_t* out, const uint8_t* q) {
    if (!prop_now()) return;
    memcpy(g_ghost.pos, out, 24); memcpy(g_ghost.rot, out + 0x18, 16);
    g_ghost.valid = out[0x28]; g_ghost.tick = GetTickCount();
    static LONG logged = 0;
    if (!logged && q) { logged = 1; LOG(.str("objects: ghost query mode ").u(q[3]).str(" valid ").u(out[0x28])); }
}

// ---- server: the clicks (single player)
typedef uint8_t (*PlacePropFn)(void* ctx, uint8_t* info, uint32_t item);
static PlacePropFn g_placePropOrig = nullptr;
static CostList* volatile g_objStepList = nullptr;
static volatile LONG g_objStage = 0;            // 0 = vanilla, -1 = this click marks a point, N = place N
static volatile DWORD g_objStageTid = 0;
static uint8_t prop_pay_hook(uint8_t* mode, CostList* list, uint8_t consume) {
    g_objStage = 0;
    const int m = obj_mode();
    if (m == core::OBJ_SINGLE || g_dedicated) return g_payOrig(mode, list, consume);   // consume 0: the game charges nothing (counts x n are harmless)
    // the place info (rbp+0x120) is filled before the pay call; mode is rbp+0x3d0 at both calls
    const float* clickPos = readable(mode - 0x2b0 + 0x10, 12) ? (const float*)(mode - 0x2b0 + 0x10) : nullptr;
    if (!own_click(clickPos)) {
        static LONG logged = 0;
        if (InterlockedIncrement(&logged) <= 5) LOG(.str("objects: a click not next to this player's ghost (another player?): placed as in the game"));
        return g_payOrig(mode, list, consume);
    }
    const int need = core::obj_points(m);
    if (g_objPtN + 1 < need) {                    // a point of the line: nothing paid, nothing placed
        g_objStage = -1; g_objStageTid = GetCurrentThreadId();
        return 1;
    }
    const int n = core::obj_clamp_count(m, g_cfg.objCount);
    // the same list with every count times n (the held object, usually one entry)
    static CostEntry e[16];
    CostList many = {};
    if (list && list->data && list->count <= 16) {
        memcpy(e, list->data, (size_t)list->count * sizeof(CostEntry));
        for (uint64_t k = 0; k < list->count; k++) *(uint32_t*)(e[k].b + 0x14) *= (uint32_t)n;
        many = { e, list->count, list->count, nullptr };
    }
    uint8_t r = g_payOrig(mode, many.data ? &many : list, consume);
    LOG(.str("objects: paying ").i(n).str(" objects (").u(list ? list->count : 0).str(" cost entries) -> ").i(r));
    if (!r) {
        Buf b; b.fmt(T(S_OBJ_NOT_ENOUGH), num(n).s); flash(b.z()); beep(MB_ICONHAND);
        return 0;
    }
    g_objStage = many.data ? n : 1; g_objStageTid = GetCurrentThreadId();
    // the undo step's own cost list (first call; the second call's list is on the stack): undo gives
    // back what it holds, set to the objects undo covers after placing
    g_objStepList = (uint8_t*)list != mode + 0xf0 && many.data ? list : nullptr;
    return 1;
}
static bool prop_place_fix(const uint8_t* info, uint8_t* copy);
static uint8_t prop_place_hook(void* ctx, uint8_t* info, uint32_t item) {
    alignas(16) static uint8_t moved[0x50];          // the click with the exact rotation and nudge (props.h)
    if (prop_place_fix(info, moved)) info = moved;
    LONG stage = g_objStageTid == GetCurrentThreadId() ? InterlockedExchange(&g_objStage, 0) : 0;
    const int m = obj_mode();
    if (!stage && info && m == core::OBJ_SINGLE && obj_tool_on() && !g_dedicated && (g_cfg.objRot || g_cfg.objElev) &&
        own_click((const float*)(info + 0x10))) {
        // single: the random turn and height on each object too
        static uint32_t k = 0; k++;
        alignas(16) uint8_t one[0x50];
        memcpy(one, info, sizeof one);
        float* pos = (float*)(one + 0x10);
        pos[1] += (float)(core::obj_rand((uint32_t)g_cfg.objSeed, (int)k, 2) * g_cfg.objElev / 100.0);
        float base[4]; memcpy(base, info + 0x1c, 16);
        core::obj_turn(base, core::obj_rand((uint32_t)g_cfg.objSeed, (int)k, 1) * g_cfg.objRot * core::kPi / 180.0, (float*)(one + 0x1c));
        return g_placePropOrig(ctx, one, item);
    }
    if (!stage || !info || m == core::OBJ_SINGLE) return g_placePropOrig(ctx, info, item);
    const float* fp = (const float*)(info + 0x10);
    const double click[3] = { fp[0], fp[1], fp[2] };
    const int need = core::obj_points(m);
    if (stage < 0) {
        EnterCriticalSection(&g_stateLock);
        int k = g_objPtN < 3 ? g_objPtN : 2;
        for (int a = 0; a < 3; a++) g_objPt[k][a] = click[a];
        memcpy(g_objPtRot, info + 0x1c, 16);
        g_objPtN = k + 1;
        LeaveCriticalSection(&g_stateLock);
        obj_undo_flag();
        // a point places nothing: an undo step the game set up for this click must give nothing back
        uint8_t* st = ctx && readable((uint8_t*)ctx + 0xf0, 8) ? *(uint8_t**)((uint8_t*)ctx + 0xf0) : nullptr;
        if (st && readable(st + 0x110, 0x20)) ((CostList*)(st + 0x110))->count = 0;
            Buf b; b.fmt(T(S_OBJ_POINT), T(kObjModeName[m]), num(k + 1).s, num(need).s); flash(b.z());
        beep(MB_OK);
        LOG(.str("objects: point ").i(k + 1).str(" of ").i(need).str(" at (").f(click[0]).c(',').f(click[1]).c(',').f(click[2]).c(')'));
        return 1;
    }
    // the last click: the client's spots when they were worked out for these points, else here
    static ObjSpot spot[kObjMax];
    double pt[3][3]; int clicked;
    EnterCriticalSection(&g_stateLock);
    clicked = g_objPtN;
    memcpy(pt, g_objPt, sizeof pt);
    const ObjPlan& p = g_plan;
    ObjOpts o = obj_opts();
    int n = 0; bool fromClient = false;
    double dx = p.pt[need - 1][0] - click[0], dy = p.pt[need - 1][1] - click[1], dz = p.pt[need - 1][2] - click[2];
    if (p.pts == need && p.clicked == clicked && clicked == need - 1 && !memcmp(p.pt, pt, sizeof(double) * 3 * clicked) &&
        same_opts(p.opts, o) && GetTickCount() - p.tick < 2000 && dx * dx + dy * dy + dz * dz < 0.75 * 0.75 &&
        (p.ground || o.ref != core::OBJ_REF_GROUND)) {
        n = p.n; memcpy(spot, p.spot, sizeof(ObjSpot) * n); fromClient = true;
    }
    g_objPtN = 0;
    LeaveCriticalSection(&g_stateLock);
    obj_undo_flag();
    if (clicked != need - 1) { LOG(.str("objects: ").i(clicked).str(" points clicked, ").i(need - 1).str(" needed; placed one")); return g_placePropOrig(ctx, info, item); }
    if (!fromClient) {
        for (int a = 0; a < 3; a++) pt[need - 1][a] = click[a];
        if (o.ref == core::OBJ_REF_GROUND) o.ref = core::OBJ_REF_SLOPE;
        n = core::obj_layout(o, pt, need, nullptr, nullptr, spot, kObjMax);
    }
    const int paid = (int)stage;
    if (n > paid) n = paid;
    float base[4]; memcpy(base, info + 0x1c, 16);
    alignas(16) uint8_t one[0x50];
    int placed = 0;
    // one undo step for the line: each object adds a record to the click's step, which holds 32 at
    // most (the game stops when it is full), so past that the objects are placed without undo
    UndoSplit us; undo_begin(us, ctx, nullptr, nullptr, n);
    for (int i = 0; i < n; i++) {
        memcpy(one, info, sizeof one);
        float* pos = (float*)(one + 0x10);
        pos[0] = (float)spot[i].p[0]; pos[1] = (float)spot[i].p[1]; pos[2] = (float)spot[i].p[2];
        core::obj_spot_rot(base, spot[i], (float*)(one + 0x1c));
        undo_before(us, i);
        if (g_placePropOrig(ctx, one, item)) placed++;
        undo_after(us);
    }
    undo_end(us);
    CostList* sl = g_objStepList; g_objStepList = nullptr;
    const int undoN = us.step0 ? (us.recorded < placed ? us.recorded : placed) : 0;
    if (sl && sl->data && sl->count <= 16 && undoN > 1)
        for (uint64_t k = 0; k < sl->count; k++) *(uint32_t*)(((CostEntry*)sl->data)[k].b + 0x14) *= (uint32_t)undoN;
    LOG(.str("objects: placed ").i(placed).str(" of ").i(n).str(fromClient ? " (spots from the preview)" : " (spots worked out on the click)")
        .str(", undo covers ").i(undoN).str(sl ? " (refund set)" : ""));
    Buf b; b.fmt(T(S_OBJ_PLACED), num(placed).s); flash(b.z());
    beep(MB_ICONASTERISK);
    return placed > 0;
}

// ---- settings window (screen 52 while g_objWin)
static void draw_prop_rows();
static void prop_reset();
// g_objWinShown: main.cpp (before the play-state UI)
// One number row: the game's option row over values around the current one, then -10 / +10.
static bool ui_int(const char* label, int cur, int lo, int hi, int step, int* out) {
    static char txt[21][16];
    const char* names[21]; int vals[21];
    int n = 0, idx = 0;
    for (int k = -10; k <= 10; k++) {
        int v = cur + k * step;
        if (v < lo || v > hi) continue;
        if (k == 0) idx = n;
        vals[n] = v;
        Buf b; b.i(v); copy_text(txt[n], b.z(), sizeof txt[n]); names[n] = txt[n];
        n++;
    }
    alignas(16) uint8_t row[0x40] = {}, opt[0x40] = {};
    ui.beginBox(row, 0);
    UiVec2 gap{ 12, 0 };
    ui.setSpacingX(ui_node(row), &gap, 0);
    uint16_t midLeft = 0x201;
    ui.setLayout(ui_node(row), &midLeft);
    ui.beginBox(opt, 0);
    ui.setWidth(ui_node(opt), 540);
    LONG v; bool changed = false; int nv = cur;
    if (ui_option(label, names, n, idx, &v)) { nv = vals[v]; changed = true; }
    ui.endBox(opt);
    auto clampv = [&](int x) { return x < lo ? lo : x > hi ? hi : x; };
    if (ui_button("-10")) { nv = clampv(cur - 10); changed = true; }
    if (ui_button("+10")) { nv = clampv(cur + 10); changed = true; }
    ui.endBox(row);
    if (changed) *out = nv;
    return changed;
}
static void draw_obj_window() {
    g_objWinShown = 1;
    alignas(16) uint8_t win[0x40] = {}, root[0x40] = {}, panel[0x40] = {}, btns[0x40] = {}, list[0x40] = {};
    UiStringView name{ "buildingToolsObjects", 20 };
    g_beginWindow(win, &name, 1, 9, -1, 1);
    ui.beginRoot(root, ui.windowContext(win));
    ui.setStretch(ui_node(root), UiVec2{ 1, 1 });
    UiVec2 rgap{ 0, 0 };
    ui.setSpacing(ui_node(root), &rgap, 0);
    ui.beginBox(panel, 0);
    void* n = ui_node(panel);
    {
        const float* r = (const float*)((uint8_t*)n + 0x114);
        if (r[2] > 0 && r[3] > 0) ui.fillRect(r, 0x3f800000c0000000ull);
        const float* rr = (const float*)((uint8_t*)ui_node(root) + 0x114);
        float x = rr[2] > 0 && r[2] > 0 ? (r[0] + r[2] - rr[0]) / rr[2] : 0.f;
        g_exactPanelX = x > 0.f && x < 1.f ? x : 0.f;
    }
    const float pad[4] = { 120, 50, 60, 40 };
    ui.setPadding(n, pad);
    UiVec2 gap{ 0, 18 };
    ui.setSpacing(n, &gap, 0);
    uint16_t topLeft = 0x101;
    ui.setSelfAlign(n, &topLeft);
    ui.setLayout(n, &topLeft);
    ui.setWidth(n, 1180);
    ui.setStretch(n, UiVec2{ 0, 1 });
    ui_text(T(S_OBJ_TITLE), STYLE_TITLE);
    {   // one short line for the current mode (the panel does not wrap text)
        static const Str how[core::OBJ_MODE_COUNT] = { S_OBJ_HOW_SINGLE, S_OBJ_HOW_LINE, S_OBJ_HOW_CURVE, S_OBJ_HOW_CIRCLE, S_OBJ_HOW_ARCH, S_OBJ_HOW_VCIRCLE };
        ui_text(T(how[g_cfg.objMode % core::OBJ_MODE_COUNT]), STYLE_BODY);
        if (g_cfg.objMode != core::OBJ_SINGLE) ui_text(T(S_OBJ_HELP), STYLE_BODY);
    }
    alignas(16) static uint8_t st[0x500];
    static float offset[4];
    bool scroll = ui_scroll_begin(st, sizeof st, offset, 1000);
    ui.beginBox(list, 0);
    UiVec2 lgap{ 0, 18 };
    ui.setSpacing(ui_node(list), &lgap, 0);
    ui.setLayout(ui_node(list), &topLeft);
    ui.setStretch(ui_node(list), UiVec2{ 1, 0 });
    {
        const char* modes[core::OBJ_MODE_COUNT];
        for (int k = 0; k < core::OBJ_MODE_COUNT; k++) modes[k] = T(kObjModeName[k]);
        LONG v;
        if (ui_option(T(S_OBJ_MODE_LABEL), modes, core::OBJ_MODE_COUNT, g_cfg.objMode, &v) && v != g_cfg.objMode) obj_set_mode(v);
        int iv;
        const int m = g_cfg.objMode == core::OBJ_SINGLE ? core::OBJ_LINE : g_cfg.objMode;
        const bool many = g_cfg.objMode != core::OBJ_SINGLE;     // single: only the random turn and height
        if (many && ui_int(T(S_OBJ_COUNT), core::obj_clamp_count(m, g_cfg.objCount), core::obj_min_count(m), kObjMax, 1, &iv)) g_cfg.objCount = iv;
        if (many && ui_int(T(S_OBJ_SPACING), g_cfg.objSpacing, 0, 100, 5, &iv)) g_cfg.objSpacing = iv;
        if (ui_int(T(S_OBJ_ROT), g_cfg.objRot, 0, 180, 5, &iv)) g_cfg.objRot = iv;
        if (ui_int(T(S_OBJ_ELEV), g_cfg.objElev, 0, 300, 5, &iv)) g_cfg.objElev = iv;
        const char* refs[core::OBJ_REF_COUNT] = { T(S_OBJ_REF_GROUND), T(S_OBJ_REF_LEVEL), T(S_OBJ_REF_SLOPE) };
        const bool flat = many && !core::obj_upright(g_cfg.objMode);   // arch / standing circle: their own height
        if (flat && ui_option(T(S_OBJ_REF), refs, core::OBJ_REF_COUNT, g_cfg.objRef, &v)) g_cfg.objRef = v;
        const char* const offOn[] = { T(S_OPT_OFF), T(S_OPT_ON) };
        if (many && ui_option(T(S_OBJ_TURN), offOn, 2, g_cfg.objTurn ? 1 : 0, &v)) g_cfg.objTurn = v;
        draw_prop_rows();
    }
    ui.endBox(list);
    if (scroll) ui.endScroll(st);
    ui.beginBox(btns, 0);
    UiVec2 bgap{ 24, 0 };
    ui.setSpacingX(ui_node(btns), &bgap, 0);
    if (ui_button(T(S_PROP_RESET))) prop_reset();
    if (ui_button(T(S_OBJ_REROLL))) { g_cfg.objSeed = (int)((GetTickCount() * 2654435761u) >> 1) | 1; LOG(.str("objects: new random values")); }
    if (ui_button(T(S_OBJ_CLOSE))) g_menuReq = 2;
    ui.endBox(btns);
    ui.endBox(panel);
    ui.endBox(root);
    ui.endWindow(win);
    if (GetTickCount() - g_propTick > 200) obj_plan_update(false);   // the cursor is paused: the settings still move the preview
}
// the window closed: settings kept
static void obj_window_closed() {
    if (!g_objWinShown) return;
    g_objWin = 0; g_objWinShown = 0;
    write_full_ini(g_cfg);
    LOG(.str("objects: settings window closed"));
}

// ---- tool actions
static void obj_act(int a) {
    if (!obj_tool_on()) return;
    if (a == ACT_OBJ_MODE) {
        if (obj_remote()) { flash(T(S_OBJ_REMOTE)); beep(MB_ICONHAND); return; }
        obj_set_mode(g_cfg.objMode + 1);
        Buf b; b.fmt(T(S_OBJ_MODE), T(kObjModeName[g_cfg.objMode])); flash(b.z());
    } else if (a == ACT_OBJ_MENU) {
        if (g_menuOpen) return;
        lang_update();
        g_objWin = 1; g_objWinShown = 0; g_menuReq = 1;
        LOG(.str("objects: settings window"));
        return;
    } else if (a == ACT_OBJ_CLEAR) {
        obj_clear("key");
        flash(T(S_OBJ_CLEARED));
    }
    beep(MB_ICONQUESTION);
}
static bool obj_hud_info(Buf& b) {
    const int m = obj_mode();
    if (m == core::OBJ_SINGLE || !prop_now()) return false;
    if (obj_remote()) { b.str(T(S_OBJ_REMOTE)); return true; }
    const int need = core::obj_points(m);
    LONG k = g_objPtN;
    b.fmt(T(S_FLOAT_OBJ), T(kObjModeName[m]), num(k + 1 > need ? need : k + 1).s, num(need).s, num(core::obj_clamp_count(m, g_cfg.objCount)).s);
    return true;
}

// ---- install (client and single player server)
// ---- undo (issue #51): the game's own object undo, switched back on
// The game can undo an object: an undo record of type 1 {entity id, method} makes its undo
// (player_building_undo -> 491320) remove the object with 490950, the same function picking an
// object up uses (destroys the entity, PickUp event), after giving back the step's cost list. It is
// switched off in three places, all opened here while the Object lines tool is on:
//  1. place_prop sets up an undo step only for items with flag 0x88 or a building voxel: a cave at
//     SIG_PROP_UNDO_TEST also sets it up while g_objUndoFlag is 1 (single object, or the click that
//     ends a line; never for a click that only marks a point, which places nothing)
//  2. PlaceProp adds the record only when a check (eb39e0 on 1.0.1.1) says no, and it always says
//     yes: that call (SIG_PROP_REC_CALL) is redirected to obj_rec_check
//  3. the server cursor tells the client "can undo" (ServerCursor +0x69 bit 3, without it the undo
//     key does nothing) through 4962f0, which has the same item test: redirected to can_undo_hook
static volatile uint8_t* g_objUndoFlag = nullptr;   // set once the step's jump is written
static int g_objUndoParts = 0;                     // patches in place (3 = undo works)
// The step's jump goes into place_prop's body, which XHL Planting Assistant compares with the
// game file when it starts (it accepts redirected calls, not a jump in place of an instruction).
// So it is written only when an object is first used with Object lines on, after the other mod
// has checked the body; until then the cave waits here.
static uint8_t* g_objUndoSite = nullptr;
static uint8_t* g_objUndoMem = nullptr;
static void obj_undo_arm() {
    uint8_t* site = (uint8_t*)InterlockedExchangePointer((void* volatile*)&g_objUndoSite, nullptr);
    if (!site) return;
    static const uint8_t orig[9] = { 0xF6, 0x87, 0xD8, 0x05, 0x00, 0x00, 0x88, 0x75, 0x09 };
    if (memcmp(site, orig, 9)) { LOG(.str("object undo: place_prop changed by another mod, undo off")); return; }
    DWORD old;
    if (!VirtualProtect(site, 9, PAGE_EXECUTE_READWRITE, &old)) return;
    uint8_t patch[9] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90 };
    int32_t r = (int32_t)(g_objUndoMem - (site + 5)); memcpy(patch + 1, &r, 4);
    memcpy(site + 5, patch + 5, 4);                  // the tail first: the jump skips it
    memcpy(site, patch, 5);
    VirtualProtect(site, 9, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 9);
    g_objUndoFlag = g_objUndoMem + 30;
    LOG(.str("object undo: step patch written"));
}
static void obj_undo_flag() {
    if (!g_objUndoFlag && !g_objUndoSite) return;
    const int m = obj_mode();
    const uint8_t on = obj_tool_on() && !obj_remote() && (m == core::OBJ_SINGLE || g_objPtN + 1 >= core::obj_points(m)) ? 1 : 0;
    if (on && !g_objUndoFlag && prop_now()) obj_undo_arm();
    if (g_objUndoFlag) *g_objUndoFlag = on;
}
static uint8_t obj_rec_check() { return g_objUndoFlag && obj_tool_on() ? 0 : 1; }   // 0: add the record
typedef uint8_t (*CanUndoFn)(void* player, uint8_t* item, uint8_t nearZone);
typedef uint8_t (*StateTestFn)(void* player, uint64_t mask);
static CanUndoFn g_canUndoOrig = nullptr;
static StateTestFn g_stateTest = nullptr;
static uint64_t g_stateMask = 0;
static uint8_t can_undo_hook(void* player, uint8_t* item, uint8_t nearZone) {
    uint8_t r = g_canUndoOrig(player, item, nearZone);
    if (r || !nearZone || !item || !g_objUndoFlag || !obj_tool_on() || !prop_item(item)) return r;
    // the rest of the game's test for building items: the player's state allows building
    return g_stateTest(player, g_stateMask) ? 0 : 1;
}
static bool install_prop_undo() {
    auto rel = [](uint8_t* at) -> uint8_t* { return at && at[0] == 0xE8 ? at + 5 + *(int32_t*)(at + 1) : nullptr; };
    uint8_t* site = find_unique("PropUndoTest", SIG_PROP_UNDO_TEST);
    uint8_t* rec = find_unique("PropRecordCall", SIG_PROP_REC_CALL);
    uint8_t* cu = find_unique("CanUndoCall", SIG_CAN_UNDO_CALL);
    uint8_t* cuSite = cu ? cu + 0x1a : nullptr;
    uint8_t* recFn = rel(rec);
    uint8_t* cuFn = rel(cuSite);
    // recFn: "mov al, 1; ret"; cuFn: its state test with the mask at +0x3e, call at +0x4b
    bool ok = site && recFn && !memcmp(recFn, "\xB0\x01\xC3", 3) && cuFn &&
              !memcmp(cuFn + 0x3e, "\x48\xBA", 2) && !memcmp(cuFn + 0x48, "\x48\x8B\xCF\xE8", 4) &&
              !memcmp(cuFn + 0x13, "\x74\x4B\x48\x85\xD2\x74\x46", 7);
    if (!ok) {
        LOG(.str("object undo: not available (step ").i(site != nullptr).str(" record ").i(recFn != nullptr).str(" can undo ").i(cuFn != nullptr).c(')'));
        return false;
    }
    g_stateMask = *(uint64_t*)(cuFn + 0x40);
    g_stateTest = (StateTestFn)rel(cuFn + 0x4b);
    g_canUndoOrig = (CanUndoFn)cuFn;
    // 1. the step: cave
    static const uint8_t code[30] = {
        0x80, 0x3D, 23, 0, 0, 0, 0,                 // cmp byte [rip+flag], 0
        0x75, 0x0F,                                 // jne force
        0xF6, 0x87, 0xD8, 0x05, 0x00, 0x00, 0x88,   // test byte [rdi+0x5d8], 0x88 (the game's own test)
        0x75, 0x06,                                 // jne force
        0xFF, 0x25, 8, 0, 0, 0,                     // jmp [rip+cont]   (the next test, item +0x440)
        0xFF, 0x25, 10, 0, 0, 0,                    // force: jmp [rip+undo] (the undo step's set up)
    };
    uint8_t* mem = alloc_near(site, 48);
    if (!mem) return false;
    memset(mem, 0, 48);
    memcpy(mem, code, sizeof code);
    uint64_t cont = (uint64_t)(site + 9), undo = (uint64_t)(site + 0x12);
    memcpy(mem + 32, &cont, 8); memcpy(mem + 40, &undo, 8);
    FlushInstructionCache(GetCurrentProcess(), mem, 48);
    // 2. and 3. first (the step alone would give back objects without taking them)
    if (hook_calls(recFn, (void*)&obj_rec_check, &rec, 1) != 1) return false;
    g_objUndoParts = 1;
    if (hook_calls(cuFn, (void*)&can_undo_hook, &cuSite, 1) == 1) g_objUndoParts++;
    g_objUndoMem = mem;                           // the jump to it: obj_undo_arm
    g_objUndoSite = site;
    g_objUndoParts++;
    return g_objUndoParts == 3;
}

// The undo key on the client: player_control_action_sequence reads it (UndoKey, 2c72b0 on 1.0.1.1:
// BuildingUndo pressed and the server's "can undo" bit -> undo request) only in the hammer's branch.
// With an object in hand that branch is not taken, so after the branch test (BuildModeFn) the mod
// reads the undo key the same way.
typedef void (*BuildModeFn)(uint8_t* out, uint8_t* sys, void* item);
typedef uint8_t (*UndoKeyFn)(uint8_t* sys);
static BuildModeFn g_buildModeOrig = nullptr;
static UndoKeyFn g_undoKey = nullptr;
static void build_mode_hook(uint8_t* out, uint8_t* sys, void* item) {
    PROF(PF_BMODE);
    PROF_CALL(PF_BMODE_G, g_buildModeOrig(out, sys, item));
    if (!out || out[0] || !g_undoKey || !g_objUndoFlag || !obj_tool_on() || !prop_now() || !sys) return;
    uint8_t* sc = readable(sys + 0x90, 8) ? *(uint8_t**)(sys + 0x90) : nullptr;
    uint8_t flags = sc && readable(sc + 0x69, 1) ? sc[0x69] : 0xff;
    static int lastFlags = -1;
    if (flags != lastFlags) { lastFlags = flags; LOG(.str("object undo: object in hand, server flags ").x(flags).str((flags & 0xc) == 0xc ? " (undo possible)" : "")); }
    // The game's undo key is an input action (BuildingUndo, bit 40 of the action set at sys+0x2f8: +8
    // down, +0 already taken) that only the hammer's controls produce. On the mod's key (default Y,
    // [Keys] ObjectUndo) that bit is set for this one call, so the game's own undo code runs.
    // Right after placing, the place animation still runs and the game drops the undo request, so
    // a press is kept for 1.5 s and sent again every 0.3 s until the game's undo has run.
    static bool was = false;
    static DWORD pressTick = 0, sentTick = 0; static bool pending = false;
    const bool down = g_cfg.keyObjUndo > 0 && game_focused() && !g_objWin && key_down(g_cfg.keyObjUndo);
    const DWORD now = GetTickCount();
    if (down && !was) { pressTick = now; sentTick = 0; pending = true; }
    was = down;
    if (!pending) return;
    if (now - pressTick > 1500 || (g_undoRanTick && (LONG)(g_undoRanTick - pressTick) >= 0)) {
        LOG(.str("object undo: ").str(g_undoRanTick && (LONG)(g_undoRanTick - pressTick) >= 0 ? "undone after " : "no undo after ")
            .u(now - pressTick).str(" ms"));
        pending = false; return;
    }
    if (sentTick && now - sentTick < 300) return;
    sentTick = now ? now : 1;
    uint64_t* in = readable(sys + 0x2f8, 8) ? *(uint64_t**)(sys + 0x2f8) : nullptr;
    if (!in || !readable(in, 16)) return;
    const uint64_t bit = 1ull << 40, in0 = in[0], in1 = in[1];
    in[1] |= bit; in[0] &= ~bit;
    uint8_t r = g_undoKey(sys);
    in[0] = in0; in[1] = in1;
    LOG(.str("object undo: undo key with an object in hand, server flags ").x(flags).str(", sent ").u(now - pressTick).str(" ms after the press"));
    (void)r;
}
static void install_undo_key() {
    auto rel = [](uint8_t* at) -> uint8_t* { return at && at[0] == 0xE8 ? at + 5 + *(int32_t*)(at + 1) : nullptr; };
    uint8_t* bm = find_unique("BuildModeCall", SIG_BUILD_MODE_CALL);
    uint8_t* uk = find_unique("UndoKeyCall", SIG_UNDO_KEY_CALL);
    uint8_t* bmSite = bm ? bm + 0xb : nullptr;
    uint8_t* bmFn = rel(bmSite); uint8_t* ukFn = rel(uk ? uk + 4 : nullptr);
    if (!bmFn || !ukFn) { LOG(.str("object undo: no undo key with an object in hand")); return; }
    g_undoKey = (UndoKeyFn)ukFn;
    g_buildModeOrig = (BuildModeFn)bmFn;
    if (hook_calls(bmFn, (void*)&build_mode_hook, &bmSite, 1) != 1) { g_buildModeOrig = nullptr; g_undoKey = nullptr; }
    LOG(.str("object undo: undo key with an object in hand ").str(g_undoKey ? "on" : "not hooked"));
}

static void install_objects() {
    if (g_dedicated) { LOG(.str("object placement: not on a dedicated server yet")); return; }
    auto rel = [](uint8_t* at) -> uint8_t* { return at && at[0] == 0xE8 ? at + 5 + *(int32_t*)(at + 1) : nullptr; };
    uint8_t* pc = find_unique("PropPlaceCall", SIG_PROP_PLACE_CALL);
    uint8_t* pa = find_unique("PropPayA", SIG_PROP_PAY_A);
    uint8_t* pb = find_unique("PropPayB", SIG_PROP_PAY_B);
    uint8_t* cq = find_unique("CursorQueryCall", SIG_CURSOR_QUERY);
    uint8_t* placeSite = pc ? pc + 0x11 : nullptr;
    uint8_t* paySites[2] = { pa ? pa + 0xf : nullptr, pb ? pb + 0x11 : nullptr };
    uint8_t* cqSite = cq ? cq + 0x1a : nullptr;
    uint8_t* pay = (uint8_t*)g_payOrig;
    uint8_t* placeFn = rel(placeSite);
    uint8_t* queryFn = rel(cqSite);
    // client_cursor's cursor transform call after the query (the ghost)
    uint8_t* cxSite = cqSite ? cqSite + 0x5c : nullptr;
    bool ok = placeFn && pay && rel(paySites[0]) == pay && rel(paySites[1]) == pay && queryFn && rel(cqSite - 0x1f) == queryFn &&
              placeFn - placeSite < 0x1000000 && !memcmp(placeFn, "\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x55\x41\x55\x41\x57", 15);
    if (!ok) {
        LOG(.str("object placement: not available (place ").i(placeFn != nullptr).str(" pay ").i(rel(paySites[0]) == pay && pay)
            .i(rel(paySites[1]) == pay && pay).str(" query ").i(queryFn != nullptr).c(')'));
        return;
    }
    if (rel(cxSite) && rel(cxSite) == (uint8_t*)g_cxFn) g_cxClientRet = cxSite + 5;
    g_placePropOrig = (PlacePropFn)placeFn;
    g_cursorQueryOrig = (CursorQueryFn)queryFn;
    int a = hook_calls(placeFn, (void*)&prop_place_hook, &placeSite, 1);
    int b = hook_calls(pay, (void*)&prop_pay_hook, paySites, 2);
    int c = hook_calls(queryFn, (void*)&cursor_query_hook, &cqSite, 1);
    bool undo = a == 1 && b == 2 && install_prop_undo();
    if (undo) install_undo_key();
    LOG(.str("object placement: ").str(a == 1 && b == 2 && c == 1 ? "available" : "partly hooked").str(undo ? ", undo on" : ", no undo").str(" (place ").i(a).str(", pay ").i(b)
        .str(", held item ").i(c).str(", ghost ").str(g_cxClientRet ? "yes" : "no").str("); mode ").i(g_cfg.objMode));
}
