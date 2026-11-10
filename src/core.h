// Platform-independent core of the Enshrouded Building Tools mod.
// Compiled into the Windows DLL and into the Linux unit tests.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace core {

// ---------------------------------------------------------------- signatures
// "48 89 ?? 24" style patterns. Returns number of parsed bytes.
inline int parse_sig(const char* s, uint8_t* bytes, uint8_t* mask, int cap) {
    int n = 0;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    while (*s && n < cap) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (s[0] == '?') {
            bytes[n] = 0; mask[n] = 0; n++;
            s += (s[1] == '?') ? 2 : 1;
            continue;
        }
        int h = hex(s[0]), l = hex(s[1]);
        if (h < 0 || l < 0) return -1;
        bytes[n] = (uint8_t)(h * 16 + l); mask[n] = 1; n++;
        s += 2;
    }
    return n;
}

// Find all matches of a pattern in [base, base+len). Writes up to `cap` hits.
inline int scan(const uint8_t* base, size_t len, const char* sig, const uint8_t** hits, int cap) {
    uint8_t b[128], m[128];
    int n = parse_sig(sig, b, m, 128);
    if (n <= 0 || (size_t)n > len) return 0;
    int found = 0;
    // first fixed byte as anchor
    int a = 0;
    while (a < n && !m[a]) a++;
    for (size_t i = 0; i + n <= len; i++) {
        if (base[i + a] != b[a]) continue;
        int k = 0;
        for (; k < n; k++)
            if (m[k] && base[i + k] != b[k]) break;
        if (k == n) {
            if (found < cap) hits[found] = base + i;
            found++;
        }
    }
    return found;
}

// --------------------------------------------------------------- game layout
// World positions are signed 64-bit fixed point, 1 m == 2^32.
static const double kFix = 4294967296.0;

struct I3 { int64_t x, y, z; };

// Cursor transform as stored in the ServerCursor component (0x38 bytes).
struct CursorXf {
    int64_t pos[3];      // +0x00
    float   rot[4];      // +0x18 quaternion
    float   scale;       // +0x28
    uint8_t rest[0x0c];  // +0x2c
};
static_assert(sizeof(CursorXf) == 0x38, "cursor transform size");

// Transform the shape add/remove functions take (built by the game from the
// shape's bounding-box centre).
struct IntXf {
    int64_t pos[3];      // +0x00
    float   rot[4];      // +0x18
    float   scale[3];    // +0x28  always 1,1,1
    uint32_t pad;        // +0x34
};
static_assert(sizeof(IntXf) == 0x38, "int transform size");

struct Aabb { float mn[3]; float pad0; float mx[3]; float pad1; };
static_assert(sizeof(Aabb) == 0x20, "aabb size");

// Same float arithmetic the game uses to turn a bounding box into an
// integer centre: cvttss2si(((max + min) * 0.5) * 2^32 + 0.5).
inline int64_t centre_fixed(float mn, float mx) {
    volatile float s = mx + mn;
    volatile float h = s * 0.5f;
    volatile float f = h * 4294967296.0f;
    volatile float r = f + 0.5f;
    return (int64_t)r;
}

// --------------------------------------------------------------- fill planner
// Plans a structure fill: the axis-aligned box spanned by `n` corner positions
// (n = 2 or 3), stepping by the shape size on each axis. Corner positions are
// already snapped by the game, so spans are multiples of the step; any rounding
// is absorbed by interpolating between the exact corner coordinates.
// Shapes are laid side by side: exactly one shape size apart, starting at corner 1
// and running toward the farthest other corner on each axis. The number of shapes
// is the corner-to-corner distance in shape sizes, rounded (so the last shape sits
// as close to the far corner as the grid allows).
struct Plan {
    int64_t origin[3];  // corner 1 (centre of the first shape)
    int64_t stepFix[3]; // signed spacing per axis (fixed point)
    int     count[3];   // shapes per axis (>= 1)
    long    total;
    // fill options (plan_area)
    int     thick;       // wall / room / roof thickness in shapes (1..3; 0 counts as 1)
    int     roofShape;   // ROOF_*
    int     roofAxisX;   // the slopes rise along x (ridge along z), else along z
    int     roofFlip;    // shed: rises toward the low end of the axis
    int     overhang;    // shapes the roof reaches past the area on every side
    int     roofH;       // height of the roof itself (shapes)
    int     roofBase;    // shapes of the plan below the area's bottom (overhang drop + thickness)
    int     roundShape;  // ROUND_*
    int     roundSide;   // half: flat side, quarter: centre corner, in box space (see round_inside)
    int     roundHollow; // a wall `thick` shapes thick along the curve, else solid
    int     roundAxis;   // the axis the round shape runs along (1 = y: a round in the ground plan)
    int     diagU, diagV; // diagonal fill: the plane the line runs in (axes, u < v not required)
    int     diagFlip;    // the line runs from (0, nv-1) to (nu-1, 0) in box space, else (0, 0) to (nu-1, nv-1)
};

inline int64_t iabs64(int64_t v) { return v < 0 ? -v : v; }

inline bool plan_fill(const I3* c, int n, const float step_m[3], long max_total, Plan* out) {
    if (n < 1) return false;
    const int64_t o[3] = { c[0].x, c[0].y, c[0].z };
    int64_t farOff[3] = { 0, 0, 0 };          // signed offset of the farthest corner
    for (int i = 1; i < n; i++) {
        const int64_t v[3] = { c[i].x - o[0], c[i].y - o[1], c[i].z - o[2] };
        for (int a = 0; a < 3; a++) if (iabs64(v[a]) > iabs64(farOff[a])) farOff[a] = v[a];
    }
    int64_t total = 1;
    for (int a = 0; a < 3; a++) {
        double step = (double)step_m[a];
        if (step < 0.05) step = 0.05;
        int64_t sf = (int64_t)(step * kFix + 0.5);
        double k = (double)iabs64(farOff[a]) / (double)sf;
        long steps = (long)(k + 0.5);
        if (steps > 100000) return false;
        out->origin[a] = o[a];
        out->stepFix[a] = farOff[a] < 0 ? -sf : sf;
        out->count[a] = (int)steps + 1;
        total *= out->count[a];
        if (total > (int64_t)max_total) { out->total = total > 0x7fffffff ? 0x7fffffff : (long)total; return false; }
    }
    out->total = (long)total;
    out->thick = 1; out->roofShape = out->roofAxisX = out->roofFlip = out->overhang = out->roofH = out->roofBase = 0;
    out->roundShape = out->roundSide = out->roundHollow = 0; out->roundAxis = 1;
    out->diagU = 0; out->diagV = 2; out->diagFlip = 0;
    return true;
}

// Axis lock for structure corners. Corners 1 and 2 are free. Axes on which they
// differ are "used"; the third corner may only extend along the unused axes, so
//   1->2 along one axis  (line)  : corner 3 opens it into a plane or box,
//   1->2 in a plane      (plane) : corner 3 only adds height/thickness,
//   1->2 differ on all 3 (box)   : corner 3 is ignored.
// `tol` is how close (fixed point) two coordinates must be to count as equal.
inline I3 lock_third(const I3& a, const I3& b, const I3& c, int64_t tol) {
    const int64_t av[3] = { a.x, a.y, a.z }, bv[3] = { b.x, b.y, b.z }, cv[3] = { c.x, c.y, c.z };
    int64_t r[3];
    for (int k = 0; k < 3; k++) {
        bool used = iabs64(bv[k] - av[k]) > tol;
        r[k] = used ? bv[k] : cv[k];
    }
    return I3{ r[0], r[1], r[2] };
}

// Position of shape (i,j,k) of a plan.
inline I3 plan_pos(const Plan& p, int i, int j, int k) {
    return I3{ p.origin[0] + p.stepFix[0] * i, p.origin[1] + p.stepFix[1] * j, p.origin[2] + p.stepFix[2] * k };
}

// Fill shape of an area: solid, hollow room (walls + floor + ceiling), walls only, or a
// roof. Rooms, walls and roofs are `thick` shapes thick. A side too thin to have an inside
// stays complete.
enum { FILL_SOLID = 0, FILL_HOLLOW = 1, FILL_WALLS = 2, FILL_ROOF = 3, FILL_ROUND = 4, FILL_DIAG = 5, FILL_COUNT = 6 };
enum { ROOF_GABLE = 0, ROOF_HIP = 1, ROOF_GAMBREL = 2, ROOF_MANSARD = 3, ROOF_SHED = 4, ROOF_CONE = 5, ROOF_DOME = 6, ROOF_COUNT = 7 };
// cone and dome: round roofs over the ellipse of the area (a circle in a square area), the
// same outline as a full round fill of that area, so they sit on round walls
inline bool roof_is_round(int shape) { return shape == ROOF_CONE || shape == ROOF_DOME; }
// how many ways a roof shape can be turned (gable / gambrel: ridge along either side,
// shed: rising toward each of the four sides; hip, mansard, cone and dome look the same every way)
inline int roof_turns(int shape) {
    return shape == ROOF_SHED ? 4 : (shape == ROOF_HIP || shape == ROOF_MANSARD || roof_is_round(shape)) ? 1 : 2;
}

enum { ROUND_FULL = 0, ROUND_HALF = 1, ROUND_QUARTER = 2, ROUND_COUNT = 3 };
inline int round_turns(int shape) { return shape == ROUND_FULL ? 1 : 4; }

struct FillOpts { int roofShape, roofRot, thick, overhang, roundShape = 0, roundRot = 0, roundHollow = 0, roundLying = 0,
                  diagRot = 0, diagLying = 0; };

// Diagonal fills: a wall `t` shapes thick along the line from one corner of a plane of the
// box to the opposite one. Standing: the ground plan (a wall from corner 1 to corner 2 seen
// from above); lying: the upright plane along the area (a ramp from corner 1 up or down to
// the far side). The cells form a digital line whose blocks share faces (no gaps where it
// steps): (u, v) in box space is kept when f = u * dv - v * du lies in [-W/2, W - W/2),
// W = t * (du + dv), du = nu - 1, dv = nv - 1. Same rule in cells.vert.
inline int diag_turns(int lying) { return lying ? 4 : 2; }
inline bool diag_keep(int flip, int t, int u, int v, int nu, int nv) {
    if (u < 0 || v < 0 || u >= nu || v >= nv) return false;
    const int64_t du = nu - 1, dv = nv - 1;
    if (du + dv == 0) return true;
    if (flip) v = nv - 1 - v;
    const int64_t W = (int64_t)(t < 1 ? 1 : t) * (du + dv), h = W / 2;
    const int64_t f = (int64_t)u * dv - (int64_t)v * du;
    return f >= -h && f < W - h;
}

// Round fills: an ellipse (a circle in a square area) that fills the area's box, or half of
// one (flat side on one side of the box) or a quarter (centre on one corner of the box).
// Cell (u, v) of a nu x nv plane, counted from the box's low corner, belongs to the shape
// when its centre is inside the ellipse. Cells may lie outside the box (neighbour tests):
// past a half's flat side or a quarter's straight sides the ellipse goes on, so those sides
// are not walls. side: half 0 = flat side at v = 0, 1 at v = nv, 2 at u = 0, 3 at u = nu;
// quarter: centre corner, bit 0 = at u = nu, bit 1 = at v = nv. Same rule in cells.vert.
inline bool round_inside(int shape, int side, int u, int v, int nu, int nv) {
    float cu = (float)nu, cv = (float)nv, au = (float)nu, av = (float)nv;   // doubled units
    if (shape == ROUND_HALF) {
        if (side < 2) { av = 2.f * nv; cv = side == 1 ? 2.f * nv : 0.f; }
        else { au = 2.f * nu; cu = side == 3 ? 2.f * nu : 0.f; }
    } else if (shape == ROUND_QUARTER) {
        au = 2.f * nu; av = 2.f * nv;
        cu = (side & 1) ? 2.f * nu : 0.f; cv = (side & 2) ? 2.f * nv : 0.f;
    }
    float x = ((float)(2 * u + 1) - cu) / au, y = ((float)(2 * v + 1) - cv) / av;
    return x * x + y * y <= 1.0f;
}
// solid: every cell inside; hollow: the inside cells near a cell outside: one diagonal or
// straight step, then t - 1 straight steps (t shapes thick across a straight side, and
// where the curve runs diagonally the blocks still share faces, so the wall has no gaps)
inline bool round_keep(int shape, int side, bool hollow, int t, int u, int v, int nu, int nv) {
    if (!round_inside(shape, side, u, v, nu, nv)) return false;
    if (!hollow) return true;
    for (int du = -t; du <= t; du++)
        for (int dv = -t; dv <= t; dv++) {
            int au = du < 0 ? -du : du, av = dv < 0 ? -dv : dv;
            if ((au > 1 ? au - 1 : 0) + (av > 1 ? av - 1 : 0) > t - 1) continue;
            if (!round_inside(shape, side, u + du, v + dv, nu, nv)) return true;
        }
    return false;
}
// the plane of a round fill's plan: the ground plan (along y), unless the area is a
// standing wall (one cell thin across x or z and more than one high): then the shape
// stands in that wall
inline void round_axes(int axis, int* ua, int* va) {
    if (axis == 0) { *ua = 2; *va = 1; }
    else if (axis == 2) { *ua = 0; *va = 1; }
    else { *ua = 0; *va = 2; }
}
inline void round_plane(const Plan& p, int* ua, int* va) { round_axes(p.roundAxis, ua, va); }

inline int64_t floor_div(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }   // b > 0

// Roofs. Gable = two slopes meeting at a ridge, hip = four slopes, gambrel = a barn roof
// (two slopes, each steep below and flatter above), mansard = the same on four sides,
// shed = one slope. Over a flat area the roof is as high as it is deep (one shape up per
// shape in on average; 45 degrees for gable, hip and shed) and the plan grows upward to
// fit. A 3D area keeps its height. The surface height at distance d (in shapes) from the
// eave, with the ridge at D and the roof h shapes high; d < 0 is the overhang, which
// carries the slope on down past the area's edge.
inline int roof_profile(int shape, int d, int D, int h) {
    if (D <= 0) return d >= 0 ? h : h + d;
    if (d > D) d = D;
    int64_t num = d, den = 1;
    if (shape == ROOF_GAMBREL) { num = 3 * (int64_t)d < (int64_t)D + d ? 3 * (int64_t)d : (int64_t)D + d; den = 2; }
    if (shape == ROOF_MANSARD) { num = 8 * (int64_t)d < 3 * (int64_t)D + 2 * d ? 8 * (int64_t)d : 3 * (int64_t)D + 2 * d; den = 5; }
    return (int)floor_div(2 * (int64_t)h * num + den * D, 2 * den * D);
}
// distance from the eave and ridge distance of roof column (x, z) of a plan (n* = plan
// counts including the overhang `o`)
inline void roof_dist(int shape, bool axisX, bool flip, int o, int x, int z, int nx, int nz, int* d, int* D) {
    int ox = nx - 2 * o, oz = nz - 2 * o;
    int dx = (x < nx - 1 - x ? x : nx - 1 - x) - o, dz = (z < nz - 1 - z ? z : nz - 1 - z) - o;
    if (shape == ROOF_HIP || shape == ROOF_MANSARD || roof_is_round(shape)) {
        *d = dx < dz ? dx : dz; *D = ((ox < oz ? ox : oz) - 1) / 2;
    } else if (shape == ROOF_SHED) {
        int c = axisX ? x : z, n = axisX ? nx : nz;
        *d = (flip ? n - 1 - c : c) - o; *D = (axisX ? ox : oz) - 1;
    } else {
        *d = axisX ? dx : dz; *D = ((axisX ? ox : oz) - 1) / 2;
    }
}
// Round roofs. A cone falls in a straight line from its peak to the area's ellipse (45
// degrees over a circle in a flat area, like the hip roof); a dome is the upper half of an
// ellipsoid, steep at the eave. r = the column centre's distance from the middle, 1 at the
// area's ellipse (the round fill's rule, round_inside). Over the overhang a cone's slope
// carries on down and a dome gets a flat brim at the eave. Height in shapes above the eave
// for any column (outside the overhang too: neighbour tests); -1 past a dome's brim.
inline float core_sqrt(float x) {
    if (x <= 0.f) return 0.f;
#if defined(__x86_64__) || defined(_M_X64)
    float r; __asm__("sqrtss %1, %0" : "=x"(r) : "x"(x)); return r;
#else
    return __builtin_sqrtf(x);
#endif
}
inline int round_half_up(float v) { int k = (int)v; if ((float)k > v) k--; float f = v - (float)k; return f >= 0.5f ? k + 1 : k; }
inline int roof_round_height(int shape, int o, int h, int x, int z, int nx, int nz) {
    const int ax = nx - 2 * o, az = nz - 2 * o;
    const float ex = ((float)(2 * (x - o) + 1) - (float)ax) / (float)ax, ez = ((float)(2 * (z - o) + 1) - (float)az) / (float)az;
    const float r2 = ex * ex + ez * ez;
    if (shape == ROOF_DOME) {
        if (r2 > 1.0f) return round_inside(ROUND_FULL, 0, x, z, nx, nz) ? 0 : -1;
        return round_half_up((float)h * core_sqrt(1.0f - r2));
    }
    return round_half_up((float)h * (1.0f - core_sqrt(r2)));
}
// shapes of round roof column (x, z) above the eave: from just above its lowest neighbour's
// top (so the steps touch; a neighbour past the outline counts as below the eave, so the
// outline is a closed ring) up to its own top; false outside the roof's outline
inline bool roof_round_span(int shape, int o, int h, int x, int z, int nx, int nz, int* bot, int* top) {
    if (!round_inside(ROUND_FULL, 0, x, z, nx, nz)) return false;
    const int t = roof_round_height(shape, o, h, x, z, nx, nz);
    int m = t;
    static const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (int k = 0; k < 4; k++) {
        const int x2 = x + nb[k][0], z2 = z + nb[k][1];
        int v = roof_round_height(shape, o, h, x2, z2, nx, nz);
        // past the outline the roof ends below the eave: the outline columns reach down to it (no holes in the bottom ring)
        if (v > -1 && !round_inside(ROUND_FULL, 0, x2, z2, nx, nz)) v = -1;
        if (v < m) m = v;
    }
    *top = t; *bot = m + 1 < t ? m + 1 : t;
    return true;
}

// Shapes of roof column (x, z), counted up from the plan's bottom: from just above the
// previous column's top (so every step touches the next one) up to its own top, made
// `thick` shapes deeper.
inline void roof_column(const Plan& p, int x, int z, int* lo, int* hi) {
    int t = p.thick < 1 ? 1 : p.thick;
    if (roof_is_round(p.roofShape)) {
        int bot, top;
        if (!roof_round_span(p.roofShape, p.overhang, p.roofH - 1, x, z, p.count[0], p.count[2], &bot, &top)) { *lo = 1; *hi = 0; return; }
        *lo = bot - (t - 1) + p.roofBase; *hi = top + p.roofBase;
        if (*lo < 0) *lo = 0;
        if (*hi > p.count[1] - 1) *hi = p.count[1] - 1;
        return;
    }
    int d, D;
    roof_dist(p.roofShape, p.roofAxisX != 0, p.roofFlip != 0, p.overhang, x, z, p.count[0], p.count[2], &d, &D);
    int h = p.roofH - 1;
    int top = roof_profile(p.roofShape, d, D, h);
    int bot = d > -p.overhang ? roof_profile(p.roofShape, d - 1, D, h) + 1 : (p.overhang ? top : 0);
    if (bot > top) bot = top;
    *lo = bot - (t - 1) + p.roofBase; *hi = top + p.roofBase;
    if (*lo < 0) *lo = 0;
    if (*hi > p.count[1] - 1) *hi = p.count[1] - 1;
}
inline bool fill_keep(const Plan& p, int i, int j, int k, int mode) {
    if (mode == FILL_DIAG) {
        int c[3] = { i, j, k };
        for (int a = 0; a < 3; a++) if (p.stepFix[a] < 0) c[a] = p.count[a] - 1 - c[a];   // box space
        return diag_keep(p.diagFlip, p.thick, c[p.diagU], c[p.diagV], p.count[p.diagU], p.count[p.diagV]);
    }
    if (mode == FILL_ROUND) {
        int c[3] = { i, j, k }, ua, va;
        for (int a = 0; a < 3; a++) if (p.stepFix[a] < 0) c[a] = p.count[a] - 1 - c[a];   // box space
        round_plane(p, &ua, &va);
        return round_keep(p.roundShape, p.roundSide, p.roundHollow != 0, p.thick < 1 ? 1 : p.thick,
                          c[ua], c[va], p.count[ua], p.count[va]);
    }
    if (mode == FILL_ROOF) {
        int y = p.stepFix[1] >= 0 ? j : p.count[1] - 1 - j, lo, hi;
        roof_column(p, i, k, &lo, &hi);
        return y >= lo && y <= hi;
    }
    int t = p.thick < 1 ? 1 : p.thick;
    bool ex = i < t || i >= p.count[0] - t, ez = k < t || k >= p.count[2] - t;
    bool ey = j < t || j >= p.count[1] - t;
    if (mode == FILL_HOLLOW) return ex || ey || ez;
    if (mode == FILL_WALLS) return ex || ez;
    return true;
}
inline long fill_count(const Plan& p, int mode) {
    long cx = p.count[0], cy = p.count[1], cz = p.count[2];
    if (mode == FILL_DIAG) {
        const int ua = p.diagU, va = p.diagV, wa = 3 - ua - va;
        long nu = p.count[ua], nv = p.count[va], n = 0;
        if (nu * nv > 1000000) return cx * cy * cz;
        for (int u = 0; u < nu; u++)
            for (int v = 0; v < nv; v++) n += diag_keep(p.diagFlip, p.thick, u, v, (int)nu, (int)nv);
        return n * p.count[wa];
    }
    if (mode == FILL_ROUND) {
        int ua, va; round_plane(p, &ua, &va);
        const int wa = 3 - ua - va;
        long nu = p.count[ua], nv = p.count[va], n = 0;
        if (nu * nv > 1000000) return cx * cy * cz;
        for (int u = 0; u < nu; u++)
            for (int v = 0; v < nv; v++)
                n += round_keep(p.roundShape, p.roundSide, p.roundHollow != 0, p.thick < 1 ? 1 : p.thick, u, v, (int)nu, (int)nv);
        return n * p.count[wa];
    }
    if (mode == FILL_ROOF) {
        // every column holds at least one shape (round roofs: about 4 in 5): a huge area is over any limit anyway
        if (cx * cz > 1000000) return cx * cz;
        long n = 0;
        for (int x = 0; x < cx; x++)
            for (int z = 0; z < cz; z++) { int lo, hi; roof_column(p, x, z, &lo, &hi); if (hi >= lo) n += hi - lo + 1; }
        return n;
    }
    long t = p.thick < 1 ? 1 : p.thick;
    long ix = cx > 2 * t ? cx - 2 * t : 0, iy = cy > 2 * t ? cy - 2 * t : 0, iz = cz > 2 * t ? cz - 2 * t : 0;
    if (mode == FILL_HOLLOW) return cx * cy * cz - ix * iy * iz;
    if (mode == FILL_WALLS) return cx * cy * cz - ix * cy * iz;
    return cx * cy * cz;
}

// plan_fill plus the fill options. A roof's slopes rise across the shorter side of the
// area (turned: the longer side; a shed's turns go round all four sides). A flat area grows
// upward to the roof's height; the overhang widens the plan on every side and the roof's
// drop over it (and its thickness) extend it downward.
inline bool plan_area(const I3* c, int n, const float step_m[3], long max_total, int mode, const FillOpts& fo, Plan* out) {
    if (!plan_fill(c, n, step_m, max_total, out)) return false;
    out->thick = fo.thick < 1 ? 1 : fo.thick > 3 ? 3 : fo.thick;
    out->roofShape = out->roofAxisX = out->roofFlip = out->overhang = out->roofH = out->roofBase = 0;
    if (mode == FILL_DIAG) {
        // standing: the ground plan, or a standing wall the area is one cell thin across (a
        // diagonal brace in it); lying: the upright plane along the longer side of the ground
        // plan (turns 2 and 3: the shorter side). Over a flat area a lying one rises one shape
        // per shape (45 degrees) and the plan grows upward to fit.
        const int rot = fo.diagRot & 3;
        int ua = 0, va = 2;
        if (fo.diagLying) {
            bool alongX = out->count[0] >= out->count[2];
            if (rot & 2) alongX = !alongX;
            ua = alongX ? 0 : 2; va = 1;
            if (out->count[1] == 1 && out->count[ua] > 1) {
                out->count[1] = out->count[ua];
                int64_t total = (int64_t)out->count[0] * out->count[1] * out->count[2];
                out->total = total > 0x7fffffff ? 0x7fffffff : (long)total;
                if (total > (int64_t)max_total) return false;
            }
        } else if (out->count[1] > 1 && out->count[0] == 1 && out->count[2] > 1) { ua = 2; va = 1; }
        else if (out->count[1] > 1 && out->count[2] == 1 && out->count[0] > 1) { ua = 0; va = 1; }
        out->diagU = ua; out->diagV = va;
        // turn 0 (2 lying) runs from corner 1 (plan index 0) to the far corner, turn 1 (3) the other diagonal
        const bool mu = out->stepFix[ua] < 0, mv = out->stepFix[va] < 0;
        out->diagFlip = ((rot & 1) ? 1 : 0) ^ (mu ? 1 : 0) ^ (mv ? 1 : 0);
        return true;
    }
    if (mode == FILL_ROUND) {
        // standing: the ground plan, or a standing wall the area is one cell thin across;
        // lying: a tube along the longer side of the ground plan (the round stands across it)
        int axis = 1;
        if (fo.roundLying) axis = out->count[0] >= out->count[2] ? 0 : 2;
        else if (out->count[1] > 1 && out->count[0] == 1 && out->count[2] > 1) axis = 0;
        else if (out->count[1] > 1 && out->count[2] == 1 && out->count[0] > 1) axis = 2;
        out->roundAxis = axis;
        const int shape = fo.roundShape >= 0 && fo.roundShape < ROUND_COUNT ? fo.roundShape : ROUND_FULL;
        const int rot = fo.roundRot & 3;
        out->roundShape = shape; out->roundHollow = fo.roundHollow != 0;
        int ua, va; round_plane(*out, &ua, &va);
        // turns count from corner 1 (plan index 0; the height counts from the bottom)
        const bool mu = out->stepFix[ua] < 0, mv = va != 1 && out->stepFix[va] < 0;
        int side = 0;
        if (shape == ROUND_HALF) {
            // the flat side on the longer side of the box first (a circle in a 2:1 area),
            // on corner 1's side, then the opposite side, then the two shorter sides
            static const int longU[4] = { 0, 1, 2, 3 }, longV[4] = { 2, 3, 0, 1 };
            side = out->count[ua] >= out->count[va] ? longU[rot] : longV[rot];
            if (side < 2 && mv) side ^= 1;
            if (side >= 2 && mu) side ^= 1;
        } else if (shape == ROUND_QUARTER) {
            // the curve runs from corner 1 to corner 2 (bending one way, then the other),
            // then the centre on corner 1, then on corner 2
            static const int centre[4] = { 2, 1, 0, 3 };
            side = centre[rot] ^ (mu ? 1 : 0) ^ (mv ? 2 : 0);
        }
        out->roundSide = side;
        return true;
    }
    if (mode != FILL_ROOF) return true;
    const int shape = fo.roofShape >= 0 && fo.roofShape < ROOF_COUNT ? fo.roofShape : ROOF_GABLE;
    const int o = fo.overhang < 0 ? 0 : fo.overhang > 3 ? 3 : fo.overhang;
    const int rot = fo.roofRot & 3;
    bool axisX = out->count[0] < out->count[2];
    if (rot & 1) axisX = !axisX;
    out->roofShape = shape; out->roofAxisX = axisX; out->roofFlip = shape == ROOF_SHED && (rot & 2); out->overhang = o;
    int d, D;
    roof_dist(shape, axisX, false, 0, 0, 0, out->count[0], out->count[2], &d, &D);
    const int H = out->count[1] == 1 ? D + 1 : out->count[1];
    out->roofH = H;
    int drop = -roof_profile(shape, -o, D, H - 1);
    if (roof_is_round(shape)) {
        // the lowest step of the outline (a cone's overhang drops; a dome's brim stays level)
        const int nx = out->count[0] + 2 * o, nz = out->count[2] + 2 * o;
        if ((int64_t)nx * nz > 1000000) return false;
        drop = 0;
        for (int x = 0; x < nx; x++)
            for (int z = 0; z < nz; z++) {
                int bot, top;
                if (roof_round_span(shape, o, H - 1, x, z, nx, nz, &bot, &top) && -bot > drop) drop = -bot;
            }
    }
    out->roofBase = (drop > 0 ? drop : 0) + out->thick - 1;
    for (int a = 0; a < 3; a += 2) { out->origin[a] -= out->stepFix[a] * o; out->count[a] += 2 * o; }
    if (out->stepFix[1] >= 0) out->origin[1] -= out->stepFix[1] * out->roofBase;
    out->count[1] = H + out->roofBase;
    int64_t total = (int64_t)out->count[0] * out->count[1] * out->count[2];
    out->total = total > 0x7fffffff ? 0x7fffffff : (long)total;
    return total <= (int64_t)max_total;
}

// Roof shape + turn in 4 bits (the client sends its settings in few bits)
inline int roof_pose_pack(int shape, int rot) {
    static const int first[ROOF_COUNT] = { 0, 2, 3, 5, 6, 10, 11 };
    if (shape < 0 || shape >= ROOF_COUNT) shape = 0;
    return first[shape] + (rot % roof_turns(shape));
}
inline void roof_pose_unpack(int v, int* shape, int* rot) {
    static const int first[ROOF_COUNT] = { 0, 2, 3, 5, 6, 10, 11 };
    *shape = 0; *rot = 0;
    for (int k = ROOF_COUNT - 1; k >= 0; k--)
        if (v >= first[k]) { *shape = k; *rot = v - first[k]; if (*rot >= roof_turns(k)) *rot = 0; return; }
}
// Round shape, turn and standing / lying in 18 values (pose: full 0, half 1-4, quarter 5-8)
inline int round_pack(int shape, int rot, int lying) {
    if (shape < 0 || shape >= ROUND_COUNT) shape = 0;
    int pose = shape == ROUND_FULL ? 0 : shape == ROUND_HALF ? 1 + (rot & 3) : 5 + (rot & 3);
    return pose * 2 + (lying ? 1 : 0);
}
inline void round_unpack(int v, int* shape, int* rot, int* lying) {
    if (v < 0 || v >= 18) v = 0;
    *lying = v & 1; int pose = v >> 1;
    *shape = pose == 0 ? ROUND_FULL : pose <= 4 ? ROUND_HALF : ROUND_QUARTER;
    *rot = pose == 0 ? 0 : (pose - 1) & 3;
}

// ------------------------------------------------------ client -> server state
// On a dedicated server the mod's settings live on each player's client. The client
// sends them in the lowest 16 bits of the camera position it sends to the server
// every frame (ClientPlayerInputData.camera.position, int64 fixed point, so the
// camera moves by less than 0.02 mm): x = state bits 0-15, y = bits 16-31,
// z = a check of the state. Bits 0-7 are a fixed tag, so a camera position without
// mod data (a client without the mod) is rejected (1 in 2^24 passes by chance).
enum : uint32_t {
    NET_TAG = 0xB7, NET_TAG_MASK = 0xff,
    NET_ON = 1u << 8,            // the mod is on (not the vanilla switch)
    NET_AREA = 1u << 9,          // Area mode
    NET_REPLACE = 1u << 10,      // Replace on
    NET_STYLE_SHIFT = 11,        // 2 bits: replace style (1 overlap, 2 full, 3 connected)
    NET_FILL_SHIFT = 13,         // 2 bits: area fill (FILL_*)
    NET_CORNERS3 = 1u << 15,     // 3 area corners (else 2)
    NET_COUNT_SHIFT = 16,        // 2 bits: corners the client holds after its latest area click
    NET_OP_REMOVE = 1u << 18,    // those corners are for a removal
    NET_SEQ_SHIFT = 19,          // 3 bits: area clicks so far (wraps), tells a new click from a repeat
    NET_VERSION_SHIFT = 22,      // 2 bits: format version
    NET_VERSION = 1,
    NET_ROOF_SHIFT = 24,         // 4 bits: roof shape and turn (roof_pose_pack)
    NET_THICK_SHIFT = 28,        // 2 bits: thickness - 1
    NET_OVERHANG_SHIFT = 30,     // 2 bits: roof overhang
};
inline uint32_t net_field(uint32_t s, int shift, uint32_t bits) { return (s >> shift) & ((1u << bits) - 1); }
// The fill and its options (thickness too). The 2-bit fill field has no room for the round
// fill: it is sent as the roof fill with roof pose 10..15 (no roof uses those) and the round
// settings (round_pack) in that pose and the overhang bits; a solid round sends thickness 4
// (no fill has that), a hollow one its thickness.
inline int clamp_thick(int t) { return t < 1 ? 1 : t > 3 ? 3 : t; }
// The diagonal fill goes the same way: round values 18..23 (no round uses those) hold its
// turn and standing / lying (diag_pack), the thickness field its thickness.
inline int diag_pack(int rot, int lying) { return lying ? 2 + (rot & 3) : (rot & 1); }
inline void diag_unpack(int v, int* rot, int* lying) {
    if (v < 0 || v >= 6) v = 0;
    *lying = v >= 2; *rot = v >= 2 ? v - 2 : v;
}
inline uint32_t net_fill_bits(int fill, const FillOpts& fo) {
    if (fill == FILL_DIAG) {
        uint32_t v = 18u + (uint32_t)diag_pack(fo.diagRot, fo.diagLying);
        return ((uint32_t)FILL_ROOF << NET_FILL_SHIFT) | ((10 + v / 4) << NET_ROOF_SHIFT) | ((v % 4) << NET_OVERHANG_SHIFT) |
               (((uint32_t)clamp_thick(fo.thick) - 1) << NET_THICK_SHIFT);
    }
    if (fill == FILL_ROUND) {
        uint32_t v = (uint32_t)round_pack(fo.roundShape, fo.roundRot, fo.roundLying);
        uint32_t t = fo.roundHollow ? (uint32_t)clamp_thick(fo.thick) - 1 : 3;
        return ((uint32_t)FILL_ROOF << NET_FILL_SHIFT) | ((10 + v / 4) << NET_ROOF_SHIFT) | ((v % 4) << NET_OVERHANG_SHIFT) |
               (t << NET_THICK_SHIFT);
    }
    if (fill == FILL_ROOF && roof_is_round(fo.roofShape)) {
        // cone / dome: roof pose (shape - cone) * 3 + thickness - 1 (0..5) with thickness field 3
        // (no other roof sends that), the overhang as usual
        uint32_t v = (uint32_t)(fo.roofShape - ROOF_CONE) * 3 + (uint32_t)clamp_thick(fo.thick) - 1;
        return ((uint32_t)FILL_ROOF << NET_FILL_SHIFT) | (v << NET_ROOF_SHIFT) | (((uint32_t)fo.overhang & 3) << NET_OVERHANG_SHIFT) |
               (3u << NET_THICK_SHIFT);
    }
    return (((uint32_t)fill & 3) << NET_FILL_SHIFT) | ((uint32_t)roof_pose_pack(fo.roofShape, fo.roofRot) << NET_ROOF_SHIFT) |
           (((uint32_t)fo.overhang & 3) << NET_OVERHANG_SHIFT) | (((uint32_t)clamp_thick(fo.thick) - 1) << NET_THICK_SHIFT);
}
// the fill (FILL_*) and its options from a state
inline int net_fill_unpack(uint32_t s, FillOpts* fo) {
    int fill = (int)net_field(s, NET_FILL_SHIFT, 2);
    int pose = (int)net_field(s, NET_ROOF_SHIFT, 4), oh = (int)net_field(s, NET_OVERHANG_SHIFT, 2);
    int t = (int)net_field(s, NET_THICK_SHIFT, 2);
    if (fill == FILL_ROOF && pose >= 10 && (pose - 10) * 4 + oh >= 18) {
        diag_unpack((pose - 10) * 4 + oh - 18, &fo->diagRot, &fo->diagLying);
        fo->thick = t == 3 ? 3 : t + 1;
        return FILL_DIAG;
    }
    if (fill == FILL_ROOF && pose >= 10) {
        round_unpack((pose - 10) * 4 + oh, &fo->roundShape, &fo->roundRot, &fo->roundLying);
        fo->roundHollow = t != 3;
        fo->thick = t == 3 ? 1 : t + 1;
        return FILL_ROUND;
    }
    if (fill == FILL_ROOF && t == 3 && pose < 6) {
        fo->roofShape = ROOF_CONE + pose / 3; fo->roofRot = 0; fo->overhang = oh; fo->thick = pose % 3 + 1;
        return FILL_ROOF;
    }
    roof_pose_unpack(pose, &fo->roofShape, &fo->roofRot);
    fo->overhang = oh;
    fo->thick = t == 3 ? 3 : t + 1;
    return fill;
}
inline uint16_t net_check(uint32_t s) {
    uint32_t h = s * 0x9E3779B1u;
    h ^= h >> 15; h *= 0x85EBCA77u; h ^= h >> 13;
    return (uint16_t)((h ^ (h >> 16)) ^ 0x5A17u);
}
inline void net_pack(uint32_t s, int64_t pos[3]) {
    const uint64_t v[3] = { s & 0xffffu, s >> 16, net_check(s) };
    for (int a = 0; a < 3; a++) pos[a] = (int64_t)(((uint64_t)pos[a] & ~(uint64_t)0xffff) | v[a]);
}
inline bool net_unpack(const int64_t pos[3], uint32_t* s) {
    const uint32_t v = (uint32_t)((uint64_t)pos[0] & 0xffff) | (uint32_t)(((uint64_t)pos[1] & 0xffff) << 16);
    if ((v & NET_TAG_MASK) != NET_TAG || net_field(v, NET_VERSION_SHIFT, 2) != NET_VERSION) return false;
    if (((uint64_t)pos[2] & 0xffff) != net_check(v)) return false;
    *s = v;
    return true;
}

// Server side of an area click. The client counts its own corner clicks and sends,
// with every frame, how many corners it holds after its latest area click (`count`,
// 0 right after the final click) and a click counter (`seq`). The server keeps its
// own corners (positions from the click it processes) and uses these to stay in step:
//   - the same seq as the last click it handled: the game repeated the click, ignore it;
//   - otherwise the corner this click sets is the one before `count` (the last one when
//     count is 0); when the server holds a different number of corners (it missed a
//     click, or the client counted one the server refused), it starts over with this
//     click as corner 1.
// Returns the corner index (0-based) the click sets, or -1 to ignore the click.
inline int net_corner_index(int serverCount, int lastSeq, int seq, int count, int corners) {
    if (lastSeq == seq) return -1;
    int expect = count == 0 ? corners - 1 : count - 1;
    if (expect < 0 || expect >= corners) return 0;
    return serverCount == expect ? expect : 0;
}

// ---- Exact mode: one click, then sizes entered in a window
// The clicked shape (p, a shape centre) is the reference point of a box reaching `w` shapes
// past it across, `d` along the view and `h` up (0 = only the clicked shape's row: 0, 0, 0 is
// one shape; the box is |w| + 1 by |d| + 1 by |h| + 1). Width runs along the player's right, depth along the view
// (both snapped to world x / z), height up (y). The reference is the bottom centre or one of
// the bottom corners; front = the side toward the player. A negative size grows the other
// way (left instead of right, toward the player, downward). Out: the two
// opposite corners (shape centres) as an Area click pair would give them: corner 1 is the
// box corner at the reference's side, so turns of roofs, rounds and diagonals keep meaning
// "from corner 1".
enum { EXACT_CENTRE = 0, EXACT_FRONT_LEFT, EXACT_FRONT_RIGHT, EXACT_BACK_LEFT, EXACT_BACK_RIGHT, EXACT_REF_COUNT };
static const int kExactMax = 999;
// shapes along one axis: from a to b (inclusive) in steps from the reference cell
inline void exact_span(int n, int mode, int* a, int* b) {
    // mode: 0 = centred, 1 = grows to + from the reference, -1 = grows to -
    // n = shapes past the clicked one: 0 = just it, -1 = one more on the other side
    int s = n < 0 ? -1 : 1, m = (n < 0 ? -n : n) + 1;
    if (mode == 0) { *a = -((m - 1) / 2); *b = *a + m - 1; return; }
    *a = 0; *b = mode * s * (m - 1);
}
// fAxis / rAxis: 0 = x, 2 = z; fSign / rSign: +1 or -1 (world direction of forward / right).
inline void exact_corners(const I3& p, int ref, int w, int d, int h, int fAxis, int fSign, int rAxis, int rSign,
                          const float step_m[3], I3 out[2]) {
    int rm = ref == EXACT_CENTRE ? 0 : (ref == EXACT_FRONT_LEFT || ref == EXACT_BACK_LEFT) ? 1 : -1;
    int fm = ref == EXACT_CENTRE ? 0 : (ref == EXACT_FRONT_LEFT || ref == EXACT_FRONT_RIGHT) ? 1 : -1;
    int ra, rb, fa, fb, ha, hb;
    exact_span(w, rm, &ra, &rb);
    exact_span(d, fm, &fa, &fb);
    exact_span(h, 1, &ha, &hb);
    int64_t sf[3];
    for (int k = 0; k < 3; k++) sf[k] = (int64_t)(step_m[k] * kFix + 0.5);
    const int64_t pv[3] = { p.x, p.y, p.z };
    int64_t c[2][3];
    for (int e = 0; e < 2; e++) {
        for (int k = 0; k < 3; k++) c[e][k] = pv[k];
        c[e][rAxis] += (int64_t)rSign * (e ? rb : ra) * sf[rAxis];
        c[e][fAxis] += (int64_t)fSign * (e ? fb : fa) * sf[fAxis];
        c[e][1] += (int64_t)(e ? hb : ha) * sf[1];
    }
    out[0] = I3{ c[0][0], c[0][1], c[0][2] };
    out[1] = I3{ c[1][0], c[1][1], c[1][2] };
}
// The view direction snapped to world x / z: forward and right axes and signs from the
// camera's forward and right vectors (y up). Right is the one of the two directions across
// forward that the camera's right points to.
inline void exact_facing(const float fwd[3], const float right[3], int* fAxis, int* fSign, int* rAxis, int* rSign) {
    float ax = fwd[0] < 0 ? -fwd[0] : fwd[0], az = fwd[2] < 0 ? -fwd[2] : fwd[2];
    if (ax >= az) { *fAxis = 0; *fSign = fwd[0] < 0 ? -1 : 1; *rAxis = 2; *rSign = right[2] < 0 ? -1 : 1; }
    else          { *fAxis = 2; *fSign = fwd[2] < 0 ? -1 : 1; *rAxis = 0; *rSign = right[0] < 0 ? -1 : 1; }
}


// ---- Object placement (issue #18): objects along a line, a curve or a circle
// The clicked points are where the held object's ghost stood (its base), in metres. Line:
// start, end. Curve: start, end, then a point the curve passes through half way. Circle:
// centre, then a point on the circle. The objects are spread evenly along it (both ends of a
// line or curve, all the way round a circle), each moved by the random settings: along the
// line (spacing, % of the gap between two), turned about the up axis (rotation, degrees) and
// moved up or down (elevation, cm). The random offsets come from a seed, so the preview and
// the build agree. Reference plane: the height the objects stand at before the elevation:
// ground (the surface under each one, found by the caller), level (the first click's
// height) or slope (straight between the clicked points; a circle stays level).
// Arch: a half circle standing between the two clicks; vertical circle: centre, then a point on it
// (the circle stands in the upright plane through both)
enum { OBJ_SINGLE = 0, OBJ_LINE, OBJ_CURVE, OBJ_CIRCLE, OBJ_ARCH, OBJ_VCIRCLE, OBJ_MODE_COUNT };
inline bool obj_closed(int mode) { return mode == OBJ_CIRCLE || mode == OBJ_VCIRCLE; }
inline bool obj_upright(int mode) { return mode == OBJ_ARCH || mode == OBJ_VCIRCLE; }   // no ground, no turning
enum { OBJ_REF_GROUND = 0, OBJ_REF_LEVEL, OBJ_REF_SLOPE, OBJ_REF_COUNT };
static const int kObjMax = 200;
inline int obj_points(int mode) { return mode == OBJ_CURVE ? 3 : mode == OBJ_SINGLE ? 1 : 2; }
inline int obj_min_count(int mode) { return obj_closed(mode) ? 3 : 2; }
inline int obj_clamp_count(int mode, int n) { int lo = obj_min_count(mode); return n < lo ? lo : n > kObjMax ? kObjMax : n; }

static const double kPi = 3.14159265358979323846;
inline double d_sqrt(double x) {
    if (x <= 0) return 0;
#if defined(__x86_64__) || defined(_M_X64)
    double r; __asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x)); return r;
#else
    return __builtin_sqrt(x);
#endif
}
inline double d_floor(double x) { double t = (double)(int64_t)x; return t > x ? t - 1 : t; }
// sin / cos / atan2 without a C runtime (series after range reduction; error < 1e-9)
inline double d_sin(double x) {
    x -= 2 * kPi * d_floor(x / (2 * kPi) + 0.5);             // -pi..pi
    if (x > kPi / 2) x = kPi - x; else if (x < -kPi / 2) x = -kPi - x;
    double x2 = x * x, t = x, s = x;
    for (int k = 1; k <= 7; k++) { t *= -x2 / ((2 * k) * (2 * k + 1)); s += t; }
    return s;
}
inline double d_cos(double x) { return d_sin(x + kPi / 2); }
inline double d_atan(double t) {
    bool inv = t > 1 || t < -1;
    if (inv) t = 1 / t;
    for (int k = 0; k < 2; k++) t = t / (1 + d_sqrt(1 + t * t));   // two half angles: |t| <= tan(pi / 16)
    double t2 = t * t, p = t, s = t;
    for (int k = 1; k <= 8; k++) { p *= -t2; s += p / (2 * k + 1); }
    s *= 4;
    if (inv) s = (s > 0 ? kPi / 2 : -kPi / 2) - s;
    return s;
}
inline double d_atan2(double y, double x) {
    if (x > 0) return d_atan(y / x);
    if (x < 0) return d_atan(y / x) + (y >= 0 ? kPi : -kPi);
    return y > 0 ? kPi / 2 : y < 0 ? -kPi / 2 : 0;
}
// -1..1, the same for the same seed, object and use
inline double obj_rand(uint32_t seed, int i, int k) {
    uint64_t z = ((uint64_t)seed << 32) ^ ((uint64_t)(uint32_t)(i * 4 + k) * 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (double)(z >> 11) / (double)(1ull << 52) - 1.0;
}
struct ObjOpts { int mode, count, spacing, rot, elev, ref, turn; uint32_t seed; };
struct ObjSpot { double p[3]; double yaw; double axis[3]; double tilt; };   // turned by yaw about up, then by tilt about axis   // base position (metres), turn about up (radians) on top of the held rotation
// ground under (x, z) near height yRef; false = none found (the slope height stays)
typedef bool (*ObjGroundFn)(void* ctx, double x, double z, double yRef, double* y);

// The path at u (0..1): position and direction. Curve: the quadratic through the third point
// at its middle. With fewer points than the mode takes (setting it up), the first ones.
inline void obj_eval(int mode, const double (*pt)[3], int n, double u, double* out, double* tan) {
    const double* a = pt[0]; const double* b = n > 1 ? pt[1] : pt[0];
    if (mode == OBJ_CIRCLE) {
        double dx = b[0] - a[0], dz = b[2] - a[2], r = d_sqrt(dx * dx + dz * dz);
        double th = d_atan2(dz, dx) + 2 * kPi * u, c = d_cos(th), s = d_sin(th);
        out[0] = a[0] + r * c; out[1] = a[1]; out[2] = a[2] + r * s;
        tan[0] = -s; tan[1] = 0; tan[2] = c;
        return;
    }
    if (mode == OBJ_VCIRCLE || mode == OBJ_ARCH) {
        double d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
        double len = d_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        double hx = d[0], hz = d[2], hl = d_sqrt(hx * hx + hz * hz);
        if (hl < 1e-6) { hx = 1; hz = 0; hl = 1; }
        hx /= hl; hz /= hl;
        if (mode == OBJ_VCIRCLE) {
            double th = d_atan2(d[1], d[0] * hx + d[2] * hz) + 2 * kPi * u, c = d_cos(th), s = d_sin(th);
            out[0] = a[0] + len * c * hx; out[1] = a[1] + len * s; out[2] = a[2] + len * c * hz;
            tan[0] = -s * hx; tan[1] = c; tan[2] = -s * hz;
            return;
        }
        // arch: from a over the top to b; w = up made square to a -> b
        double e[3] = { hx, 0, hz };
        if (len > 1e-6) for (int k = 0; k < 3; k++) e[k] = d[k] / len;
        double w[3] = { -e[1] * e[0], 1 - e[1] * e[1], -e[1] * e[2] };
        double wl = d_sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
        if (wl < 1e-6) { w[0] = hx; w[1] = 0; w[2] = hz; wl = 1; }
        double r = len * 0.5, th = kPi * u, c = d_cos(th), s = d_sin(th);
        for (int k = 0; k < 3; k++) {
            out[k] = 0.5 * (a[k] + b[k]) - r * c * e[k] + r * s * w[k] / wl;
            tan[k] = s * e[k] + c * w[k] / wl;
        }
        return;
    }
    if (mode == OBJ_CURVE && n >= 3) {
        double q[3];
        for (int k = 0; k < 3; k++) q[k] = 2 * pt[2][k] - 0.5 * (a[k] + b[k]);
        double v = 1 - u;
        for (int k = 0; k < 3; k++) {
            out[k] = v * v * a[k] + 2 * u * v * q[k] + u * u * b[k];
            tan[k] = 2 * v * (q[k] - a[k]) + 2 * u * (b[k] - q[k]);
        }
        return;
    }
    for (int k = 0; k < 3; k++) { out[k] = a[k] + u * (b[k] - a[k]); tan[k] = b[k] - a[k]; }
}
// Curve parameter at a share of the length (even spacing along a curve), from a table.
inline double obj_arc_u(int mode, const double (*pt)[3], int n, double share) {
    if (!(mode == OBJ_CURVE && n >= 3)) return share;
    const int K = 128;
    double len[K + 1]; len[0] = 0;
    double prev[3], t[3], p[3];
    obj_eval(mode, pt, n, 0, prev, t);
    for (int i = 1; i <= K; i++) {
        obj_eval(mode, pt, n, (double)i / K, p, t);
        double dx = p[0] - prev[0], dy = p[1] - prev[1], dz = p[2] - prev[2];
        len[i] = len[i - 1] + d_sqrt(dx * dx + dy * dy + dz * dz);
        for (int k = 0; k < 3; k++) prev[k] = p[k];
    }
    if (len[K] <= 0) return share;
    double want = share * len[K];
    int i = 1;
    while (i < K && len[i] < want) i++;
    double seg = len[i] - len[i - 1];
    double f = seg > 0 ? (want - len[i - 1]) / seg : 0;
    return ((double)(i - 1) + f) / K;
}
inline double obj_height(const ObjOpts& o, const double (*pt)[3], double pathY) {
    if (obj_upright(o.mode)) return pathY;
    return o.ref == OBJ_REF_SLOPE && o.mode != OBJ_CIRCLE ? pathY : pt[0][1];
}
// The objects' spots. n = clicked points (obj_points(mode) for the build). Returns the count.
inline int obj_layout(const ObjOpts& o, const double (*pt)[3], int n, ObjGroundFn ground, void* gctx, ObjSpot* out, int cap) {
    if (n < 1 || o.mode == OBJ_SINGLE) return 0;
    int N = obj_clamp_count(o.mode, o.count);
    if (N > cap) N = cap;
    const bool circle = obj_closed(o.mode), upright = obj_upright(o.mode);
    double p0[3], t0[3];
    obj_eval(o.mode, pt, n, 0, p0, t0);
    const double yaw0 = d_atan2(t0[0], t0[2]);
    for (int i = 0; i < N; i++) {
        double share = circle ? (double)i / N : N > 1 ? (double)i / (N - 1) : 0.5;
        double gap = circle ? 1.0 / N : N > 1 ? 1.0 / (N - 1) : 0;
        share += obj_rand(o.seed, i, 0) * (o.spacing / 100.0) * gap;
        if (!circle) share = share < 0 ? 0 : share > 1 ? 1 : share;
        double u = obj_arc_u(o.mode, pt, n, share);
        double p[3], t[3];
        obj_eval(o.mode, pt, n, u, p, t);
        double y = obj_height(o, pt, p[1]), g;
        if (!upright && o.ref == OBJ_REF_GROUND && ground && ground(gctx, p[0], p[2], p[1], &g)) y = g;
        ObjSpot& s = out[i];
        s.p[0] = p[0]; s.p[1] = y + obj_rand(o.seed, i, 2) * o.elev / 100.0; s.p[2] = p[2];
        double yaw = o.turn && !upright && (t[0] != 0 || t[2] != 0) ? d_atan2(t[0], t[2]) - yaw0 : 0;
        s.yaw = yaw + obj_rand(o.seed, i, 1) * o.rot * kPi / 180.0;
        s.axis[0] = 0; s.axis[1] = 1; s.axis[2] = 0; s.tilt = 0;
        if (upright && o.turn) {
            // standing shapes: turned about the plane's axis as far as the spot is round it, so the side
            // the first object turns to the centre keeps facing the centre
            double tl[3], e[3];
            obj_eval(o.mode, pt, n, 0.25, tl, e);                          // a point a quarter round
            double c0[3], r0[3], r1[3];
            if (o.mode == OBJ_VCIRCLE) for (int k = 0; k < 3; k++) c0[k] = pt[0][k];
            else for (int k = 0; k < 3; k++) c0[k] = 0.5 * (pt[0][k] + (n > 1 ? pt[1][k] : pt[0][k]));
            for (int k = 0; k < 3; k++) { r0[k] = p0[k] - c0[k]; r1[k] = tl[k] - c0[k]; }
            double ax[3] = { r0[1] * r1[2] - r0[2] * r1[1], r0[2] * r1[0] - r0[0] * r1[2], r0[0] * r1[1] - r0[1] * r1[0] };
            double al = d_sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
            if (al > 1e-9) {
                for (int k = 0; k < 3; k++) s.axis[k] = ax[k] / al;
                s.tilt = (o.mode == OBJ_VCIRCLE ? 2 * kPi : kPi) * u;
            }
        }
    }
    return N;
}
// Points along the path for drawing it (on the ground with the ground reference): about one
// every 0.5 m, a circle closed. Returns the count.
inline int obj_path(const ObjOpts& o, const double (*pt)[3], int n, ObjGroundFn ground, void* gctx, double (*out)[3], int cap) {
    if (n < 2 || cap < 2) return 0;
    double len = 0, prev[3], p[3], t[3];
    obj_eval(o.mode, pt, n, 0, prev, t);
    for (int i = 1; i <= 32; i++) {
        obj_eval(o.mode, pt, n, i / 32.0, p, t);
        double dx = p[0] - prev[0], dy = p[1] - prev[1], dz = p[2] - prev[2];
        len += d_sqrt(dx * dx + dy * dy + dz * dz);
        for (int k = 0; k < 3; k++) prev[k] = p[k];
    }
    int m = (int)(len / 0.5) + 1;
    const bool onGround = o.ref == OBJ_REF_GROUND && !obj_upright(o.mode);
    if (!onGround) m = o.mode == OBJ_LINE || (o.mode == OBJ_CURVE && n < 3) ? 1 : (m < 48 ? 48 : m);
    if (m > cap - 1) m = cap - 1;
    if (m < 1) m = 1;
    for (int i = 0; i <= m; i++) {
        obj_eval(o.mode, pt, n, (double)i / m, p, t);
        double y = obj_height(o, pt, p[1]), g;
        if (onGround && ground && ground(gctx, p[0], p[2], p[1], &g)) y = g;
        out[i][0] = p[0]; out[i][1] = y; out[i][2] = p[2];
    }
    return m + 1;
}
// The held rotation q (x, y, z, w) turned by yaw about the world's up axis (y).
inline void obj_turn(const float* q, double yaw, float* out) {
    double c = d_cos(yaw * 0.5), s = d_sin(yaw * 0.5);
    double x = q[0], y = q[1], z = q[2], w = q[3];
    out[0] = (float)(c * x + s * z);
    out[1] = (float)(c * y + s * w);
    out[2] = (float)(c * z - s * x);
    out[3] = (float)(c * w - s * y);
}
// q turned by angle about a unit axis (world space): (axis sin, cos) * q.
inline void obj_rotate(const float* q, const double* a, double ang, float* out) {
    double c = d_cos(ang * 0.5), s = d_sin(ang * 0.5);
    double x = q[0], y = q[1], z = q[2], w = q[3];
    out[0] = (float)(c * x + s * a[0] * w + s * (a[1] * z - a[2] * y));
    out[1] = (float)(c * y + s * a[1] * w + s * (a[2] * x - a[0] * z));
    out[2] = (float)(c * z + s * a[2] * w + s * (a[0] * y - a[1] * x));
    out[3] = (float)(c * w - s * (a[0] * x + a[1] * y + a[2] * z));
}
// The held rotation of spot s: random / line yaw about up, then the standing shapes' turn.
inline void obj_spot_rot(const float* q, const ObjSpot& s, float* out) {
    float t[4];
    obj_turn(q, s.yaw, t);
    if (s.tilt != 0) obj_rotate(t, s.axis, s.tilt, out); else for (int k = 0; k < 4; k++) out[k] = t[k];
}
// Height of the ground in a column of voxels (0.5 m): bit k of `taken` = voxel v0 + k holds
// something. The surface = top of a taken voxel with a free one above; the one nearest yRef.
inline bool obj_column_top(uint64_t taken, int bits, int v0, double yRef, double* y) {
    bool found = false; double best = 0, bd = 0;
    for (int k = 0; k < bits; k++) {
        if (!((taken >> k) & 1)) continue;
        if (k + 1 >= bits || ((taken >> (k + 1)) & 1)) continue;   // the top voxel: unknown above
        double top = (v0 + k + 1) * 0.5, d = top > yRef ? top - yRef : yRef - top;
        if (!found || d < bd) { found = true; best = top; bd = d; }
    }
    if (found) *y = best;
    return found;
}

// ---- exact rotation and nudge of the object in hand (props.h)
// Turn (about the object's up axis y), tilt (about its x) and roll (about its z) in degrees, in
// that order on top of the held rotation; the nudge in cm along the object's axes (after the
// turns) or the world's.
struct PropXf { int turn, tilt, roll; int nudge[3]; int world; };
inline bool prop_xf_on(const PropXf& x) { return x.turn || x.tilt || x.roll || x.nudge[0] || x.nudge[1] || x.nudge[2]; }
// a * b (x, y, z, w): b turns in a's frame
inline void quat_mul(const double* a, const double* b, double* out) {
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}
// v turned by the unit quaternion q
inline void quat_apply(const double* q, const double* v, double* out) {
    // t = 2 (q.xyz x v), out = v + w t + q.xyz x t
    double t[3] = { 2 * (q[1] * v[2] - q[2] * v[1]), 2 * (q[2] * v[0] - q[0] * v[2]), 2 * (q[0] * v[1] - q[1] * v[0]) };
    out[0] = v[0] + q[3] * t[0] + (q[1] * t[2] - q[2] * t[1]);
    out[1] = v[1] + q[3] * t[1] + (q[2] * t[0] - q[0] * t[2]);
    out[2] = v[2] + q[3] * t[2] + (q[0] * t[1] - q[1] * t[0]);
}
// The held transform (position fixed point, rotation) with the turns and the nudge.
inline void prop_apply(const PropXf& x, const int64_t* pos, const float* rot, int64_t* outPos, float* outRot) {
    double q[4] = { rot[0], rot[1], rot[2], rot[3] };
    double n = d_sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n < 1e-6) { q[0] = q[1] = q[2] = 0; q[3] = 1; } else for (int k = 0; k < 4; k++) q[k] /= n;
    const double ang[3] = { x.turn * kPi / 180, x.tilt * kPi / 180, x.roll * kPi / 180 };
    const int axis[3] = { 1, 0, 2 };                          // turn y, tilt x, roll z
    for (int k = 0; k < 3; k++) {
        if (!ang[k]) continue;
        double r[4] = { 0, 0, 0, d_cos(ang[k] * 0.5) }, t[4];
        r[axis[k]] = d_sin(ang[k] * 0.5);
        quat_mul(q, r, t);
        for (int i = 0; i < 4; i++) q[i] = t[i];
    }
    n = d_sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int k = 0; k < 4; k++) outRot[k] = (float)(q[k] / n);
    double v[3] = { x.nudge[0] / 100.0, x.nudge[1] / 100.0, x.nudge[2] / 100.0 }, w[3];
    if (x.world) for (int k = 0; k < 3; k++) w[k] = v[k];
    else quat_apply(q, v, w);
    for (int k = 0; k < 3; k++) outPos[k] = pos[k] + (int64_t)(w[k] * kFix + (w[k] < 0 ? -0.5 : 0.5));
}
// degrees kept in -179..180 (keys step past the half turn)
inline int prop_wrap_deg(int d) { d %= 360; if (d > 180) d -= 360; if (d <= -180) d += 360; return d; }

} // namespace core
