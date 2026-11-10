// ---- Exact rotation and nudge of the object in hand
// Included from main.cpp (single translation unit), after objects.h.
//
// How the game places an object (1.0.1.2 client and server, docs/reverse-engineering-notes.md): the
// place_prop system takes the object's position and rotation from the player's place cursor, the
// one the player's game sends with its input every frame (ClientPlayerInputData +0x2f8; on the
// server PlayerInput +0x300, or the aim at +0x338 for some modes). client_cursor works it out each
// frame: the cursor transform (cursor_xf_hook) gives the spot and the surface normal (its +0x18 is
// the normal, not a rotation), then the placement check 2994d0 turns that and the game's own turn
// into the object's transform (int64 position, rotation quaternion at +0x18) and stores it, once
// for the place cursor and once for the aim. So a dedicated server places the object wherever the
// client's ghost is.
//
// The mod: the turns and the nudge are put on top of the placement check's transform (its two
// calls in client_cursor), so the game draws and sends the moved ghost. The input encoder hook
// looks at the cursor that goes out and moves it there too when it is still the ghost from before
// the move; the single player place hook does the same for the click. Which of them moved it is
// logged, once a click.

using core::PropXf;

static PropXf prop_xf() {
    PropXf x = { g_cfg.propTurn, g_cfg.propTilt, g_cfg.propRoll, { g_cfg.propNudge[0], g_cfg.propNudge[1], g_cfg.propNudge[2] },
                 g_cfg.propWorld };
    return x;
}
static bool prop_xf_now() { return obj_tool_on() && prop_now() && core::prop_xf_on(prop_xf()); }

// The ghost as the game worked it out (before the turns and the nudge) and as moved, the last few
// frames (the cursor the input sends may be a frame behind the ghost); game thread
struct PropGhost { int64_t pos[3]; float rot[4]; int64_t movedPos[3]; float movedRot[4]; DWORD tick; uint8_t moved; };
static PropGhost g_propGhost[8];
static volatile LONG g_propGhostN = 0;               // frames seen (the latest is [(n - 1) & 7])
static PropGhost prop_last() { LONG n = g_propGhostN; PropGhost g = {}; if (n > 0) g = g_propGhost[(n - 1) & 7]; return g; }

// from prop_check_hook: the transform the placement check stored (slot 0 = the place cursor, 1 =
// the aim). When it holds an object (an invalid ghost stores nothing: the transform is then still
// the one moved last frame, and is left), the turns and the nudge go on top.
static uint8_t g_propOut[2][0x28];                  // each transform as the mod left it last frame
static void prop_ghost(uint8_t* xf, int slot) {
    if (!prop_now()) return;
    uint8_t* last = g_propOut[slot];
    if (!memcmp(xf, last, sizeof g_propOut[0])) return;
    PropGhost g = {};
    memcpy(g.pos, xf, 24); memcpy(g.rot, xf + 0x18, 16);
    g.tick = GetTickCount();
    g.moved = prop_xf_now();
    if (g.moved) {
        core::prop_apply(prop_xf(), g.pos, g.rot, (int64_t*)xf, (float*)(xf + 0x18));
        memcpy(g.movedPos, xf, 24); memcpy(g.movedRot, xf + 0x18, 16);
    }
    memcpy(last, xf, sizeof g_propOut[0]);
    if (slot) return;
    // objects.h's ghost (lines, own_click) is the cursor transform's spot: moved by the same nudge
    if (g.moved) for (int a = 0; a < 3; a++) g_ghost.pos[a] += g.movedPos[a] - g.pos[a];
    LONG n = g_propGhostN;
    g_propGhost[n & 7] = g;
    g_propGhostN = n + 1;
}

// the placement check's two calls in client_cursor (place cursor, aim), right after the cursor
// transform call: flags, the transform it stores, and six more arguments passed on as they are
typedef uint64_t (*PropCheckFn)(void* flags, uint8_t* xf, void* a3, void* a4, void* a5, void* a6, void* a7, void* a8);
static PropCheckFn g_propCheckOrig = nullptr;
static uint8_t* g_propCheckRet[2] = {};             // return addresses of the two calls
static uint64_t prop_check_hook(void* flags, uint8_t* xf, void* a3, void* a4, void* a5, void* a6, void* a7, void* a8) {
    uint8_t* ret = (uint8_t*)__builtin_return_address(0);
    const uint64_t r = g_propCheckOrig(flags, xf, a3, a4, a5, a6, a7, a8);
    if (xf && (ret == g_propCheckRet[0] || ret == g_propCheckRet[1])) prop_ghost(xf, ret == g_propCheckRet[1]);
    return r;
}
static void install_props() {
    auto rel = [](uint8_t* at) -> uint8_t* { return at && at[0] == 0xE8 ? at + 5 + *(int32_t*)(at + 1) : nullptr; };
    uint8_t* sig = g_cxClientRet ? find_unique("PropCheckCall", SIG_PROP_CHECK_CALL) : nullptr;
    uint8_t* sites[2] = { sig ? sig + 0x17 : nullptr, sig ? sig + 0x57 : nullptr };
    uint8_t* fn = rel(sites[0]);
    // both calls go to the check, a little after the cursor transform call
    const bool ok = fn && rel(sites[1]) == fn && sig > g_cxClientRet && sig - g_cxClientRet < 0x200 &&
                    !memcmp(fn, "\x48\x89\x5C\x24\x18\x4C\x89\x4C\x24\x20", 10);
    if (ok) {
        g_propCheckOrig = (PropCheckFn)fn;
        g_propCheckRet[0] = sites[0] + 5; g_propCheckRet[1] = sites[1] + 5;
        if (hook_calls(fn, (void*)&prop_check_hook, sites, 2) != 2) g_propCheckOrig = nullptr;
    }
    LOG(.str("exact rotation and nudge of objects: ").str(g_propCheckOrig ? "available" : "not available (placement check not found)"));
}

// A transform (position in metres) against the last frames' ghosts: 1 = a moved ghost, 2 = a ghost
// from before the move, 0 = neither. `tol` in metres.
static int prop_match(const double* pos, const float* rot, double tol, PropGhost* hit) {
    const DWORD now = GetTickCount();
    auto same = [&](const int64_t* gp, const float* gr) {
        double d2 = 0;
        for (int a = 0; a < 3; a++) { double d = pos[a] - (double)gp[a] / core::kFix; d2 += d * d; }
        float dot = rot[0] * gr[0] + rot[1] * gr[1] + rot[2] * gr[2] + rot[3] * gr[3];
        return d2 < tol * tol && (dot > 0.9999f || dot < -0.9999f);
    };
    int best = 0;
    LONG n = g_propGhostN;
    for (LONG i = n - 1; i >= n - 8 && i >= 0; i--) {
        const PropGhost g = g_propGhost[i & 7];
        if (!g.moved || now - g.tick > 1500) continue;
        if (same(g.movedPos, g.movedRot)) { if (hit) *hit = g; return 1; }
        if (!best && same(g.pos, g.rot)) { best = 2; if (hit) *hit = g; }
    }
    return best;
}
// the click's spot is next to this player's ghost from before the move (objects.h, own_click)
static bool prop_at_base(const float* pos) {
    const PropGhost g = prop_last();
    if (!pos || !g.moved || GetTickCount() - g.tick > 1500) return false;
    double d2 = 0;
    for (int a = 0; a < 3; a++) { double d = (double)g.pos[a] / core::kFix - pos[a]; d2 += d * d; }
    return d2 < 1.5 * 1.5;
}

// ---- client: the cursors that go out with the input (input_encode_hook, PropSent there)
static const char* const kPropMatch[3] = { "neither ghost", "moved already", "moved here" };
static void prop_encode_begin(uint8_t* src, uint8_t flags, PropSent* ps) {
    ps->on = false;
    static uint64_t prevDigital = 0;
    const uint64_t d = src ? *(uint64_t*)(src + 0x3a8) : 0;
    const bool click = (d & ~prevDigital & 1) != 0;   // MainhandAction pressed in this packet
    prevDigital = d;
    const PropGhost g = prop_last();
    if (!src || !(flags & 1) || !g.moved || GetTickCount() - g.tick > 500 || !prop_xf_now()) return;
    int m[2];
    for (int k = 0; k < 2; k++) {
        CursorXf* c = (CursorXf*)(src + (k ? 0x330 : 0x2f8));
        const double p[3] = { (double)c->pos[0] / core::kFix, (double)c->pos[1] / core::kFix, (double)c->pos[2] / core::kFix };
        m[k] = prop_match(p, c->rot, 0.002, nullptr);
        if (m[k] != 2) continue;
        if (!ps->on) { ps->saved[0] = *(CursorXf*)(src + 0x2f8); ps->saved[1] = *(CursorXf*)(src + 0x330); ps->on = true; }
        core::prop_apply(prop_xf(), c->pos, c->rot, c->pos, c->rot);
    }
    // logged on each click, and when what goes out changes (at most every 2 s)
    static int last[2] = { -1, -1 };
    static DWORD logAt = 0;
    const DWORD now = GetTickCount();
    if (click || ((m[0] != last[0] || m[1] != last[1]) && now - logAt > 2000)) {
        logAt = now; last[0] = m[0]; last[1] = m[1];
        const CursorXf* c = ps->on ? ps->saved : (const CursorXf*)(src + 0x2f8);
        LOG(.str("props: ").str(click ? "click" : "input").str(": ghost (").fix(g.pos[0]).c(',').fix(g.pos[1]).c(',').fix(g.pos[2])
            .str(") moved to (").fix(g.movedPos[0]).c(',').fix(g.movedPos[1]).c(',').fix(g.movedPos[2])
            .str("), rot (").f(g.rot[0]).c(',').f(g.rot[1]).c(',').f(g.rot[2]).c(',').f(g.rot[3])
            .str(") moved to (").f(g.movedRot[0]).c(',').f(g.movedRot[1]).c(',').f(g.movedRot[2]).c(',').f(g.movedRot[3])
            .str("); place cursor sent (").fix(c[0].pos[0]).c(',').fix(c[0].pos[1]).c(',').fix(c[0].pos[2])
            .str(") rot (").f(c[0].rot[0]).c(',').f(c[0].rot[1]).c(',').f(c[0].rot[2]).c(',').f(c[0].rot[3]).str(") ").str(kPropMatch[m[0]])
            .str(", aim ").str(kPropMatch[m[1]]).str(flags & 2 ? "" : " (not sent)"));
    }
}
static void prop_encode_end(uint8_t* src, const PropSent* ps) {
    if (!ps->on) return;
    *(CursorXf*)(src + 0x2f8) = ps->saved[0]; *(CursorXf*)(src + 0x330) = ps->saved[1];
}

// ---- single player server: the click (prop_place_hook). The place info holds the position in
// metres (+0x10) and the rotation (+0x1c); moved into `copy` when it is the ghost from before.
static bool prop_place_fix(const uint8_t* info, uint8_t* copy) {
    if (!info || g_dedicated || !prop_xf_now()) return false;
    const float* fp = (const float*)(info + 0x10);
    const double p[3] = { fp[0], fp[1], fp[2] };
    PropGhost g;
    const int m = prop_match(p, (const float*)(info + 0x1c), 0.01, &g);   // float metres: a coarser grain
    LOG(.str("props: placed at (").f(p[0]).c(',').f(p[1]).c(',').f(p[2]).str("): ").str(kPropMatch[m]));
    if (m != 2) return false;
    // the ghost from before, moved: the click's own rotation, the ghost's exact position
    int64_t pos[3];
    memcpy(copy, info, 0x50);
    core::prop_apply(prop_xf(), g.pos, (const float*)(info + 0x1c), pos, (float*)(copy + 0x1c));
    float* cp = (float*)(copy + 0x10);
    for (int a = 0; a < 3; a++) cp[a] = (float)((double)pos[a] / core::kFix);
    return true;
}

// ---- keys (an object in hand)
static volatile DWORD g_propSaveAt = 0;              // settings changed by a key: written to the ini a bit later
static void prop_flash() {
    Buf b; b.fmt(T(S_PROP_FLOAT_ROT), num(g_cfg.propTurn).s, num(g_cfg.propTilt).s, num(g_cfg.propRoll).s).str("  |  ")
        .fmt(T(S_PROP_FLOAT_NUDGE), num(g_cfg.propNudge[0]).s, num(g_cfg.propNudge[1]).s, num(g_cfg.propNudge[2]).s);
    flash(b.z());
}
static void prop_reset() {
    g_cfg.propTurn = g_cfg.propTilt = g_cfg.propRoll = 0;
    g_cfg.propNudge[0] = g_cfg.propNudge[1] = g_cfg.propNudge[2] = 0;
    g_propSaveAt = GetTickCount() | 1;
    LOG(.str("props: rotation and nudge reset"));
}
static bool prop_repeats(int a) { return a >= ACT_PROP_TURN_P && a < ACT_PROP_RESET; }
static void prop_act(int a) {
    if (!obj_tool_on() || !prop_now()) return;
    if (a == ACT_PROP_RESET) { prop_reset(); flash(T(S_PROP_RESET_MSG)); beep(MB_ICONQUESTION); return; }
    const bool fine = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const int deg = fine ? 1 : g_cfg.propTurnStep, cm = fine ? 1 : g_cfg.propNudgeStep;
    const int k = a - ACT_PROP_TURN_P, sign = k % 2 ? -1 : 1;
    if (k < 6) {
        int* v = k < 2 ? &g_cfg.propTurn : k < 4 ? &g_cfg.propTilt : &g_cfg.propRoll;
        *v = core::prop_wrap_deg(*v + sign * deg);
    } else {
        int& v = g_cfg.propNudge[(k - 6) / 2];
        v += sign * cm;
        if (v > 1000) v = 1000; else if (v < -1000) v = -1000;
    }
    g_propSaveAt = GetTickCount() | 1;
    prop_flash();
}
// input thread: write the ini once the keys rest
static void prop_save_tick() {
    DWORD at = g_propSaveAt;
    if (at && GetTickCount() - at > 2000) {
        g_propSaveAt = 0;
        write_full_ini(g_cfg);
        LOG(.str("props: turn ").i(g_cfg.propTurn).str(" tilt ").i(g_cfg.propTilt).str(" roll ").i(g_cfg.propRoll)
            .str(" nudge ").i(g_cfg.propNudge[0]).c('/').i(g_cfg.propNudge[1]).c('/').i(g_cfg.propNudge[2])
            .str(g_cfg.propWorld ? " cm (world axes)" : " cm (object axes)"));
    }
}

// floating text: the turns and the nudge while they are set
static bool prop_hud_info(Buf& b) {
    if (!prop_xf_now()) return false;
    if (b.n) b.str("  |  ");
    b.fmt(T(S_PROP_FLOAT_ROT), num(g_cfg.propTurn).s, num(g_cfg.propTilt).s, num(g_cfg.propRoll).s).str("  |  ")
     .fmt(T(S_PROP_FLOAT_NUDGE), num(g_cfg.propNudge[0]).s, num(g_cfg.propNudge[1]).s, num(g_cfg.propNudge[2]).s);
    return true;
}

// rows of the object settings window (objects.h)
static void draw_prop_rows() {
    ui_text(T(S_PROP_TITLE), STYLE_BODY);
    ui_text(T(S_PROP_HELP), STYLE_BODY);
    int iv; LONG v;
    if (ui_int(T(S_PROP_TURN), g_cfg.propTurn, -179, 180, 1, &iv)) g_cfg.propTurn = iv;
    if (ui_int(T(S_PROP_TILT), g_cfg.propTilt, -179, 180, 1, &iv)) g_cfg.propTilt = iv;
    if (ui_int(T(S_PROP_ROLL), g_cfg.propRoll, -179, 180, 1, &iv)) g_cfg.propRoll = iv;
    if (ui_int(T(S_PROP_NUDGE_X), g_cfg.propNudge[0], -1000, 1000, 1, &iv)) g_cfg.propNudge[0] = iv;
    if (ui_int(T(S_PROP_NUDGE_Y), g_cfg.propNudge[1], -1000, 1000, 1, &iv)) g_cfg.propNudge[1] = iv;
    if (ui_int(T(S_PROP_NUDGE_Z), g_cfg.propNudge[2], -1000, 1000, 1, &iv)) g_cfg.propNudge[2] = iv;
    const char* axes[2] = { T(S_PROP_AXES_OBJECT), T(S_PROP_AXES_WORLD) };
    if (ui_option(T(S_PROP_AXES), axes, 2, g_cfg.propWorld ? 1 : 0, &v)) g_cfg.propWorld = v != 0;
    if (ui_int(T(S_PROP_TURN_STEP), g_cfg.propTurnStep, 1, 90, 1, &iv)) g_cfg.propTurnStep = iv;
    if (ui_int(T(S_PROP_NUDGE_STEP), g_cfg.propNudgeStep, 1, 100, 1, &iv)) g_cfg.propNudgeStep = iv;
}
