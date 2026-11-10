// Linux unit tests: run the mod's signature scanner against the real game
// executable (mapped at its virtual layout) and exercise the fill planner.
#include "../src/core.h"
#include "../src/strings.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <regex>
#include <string>
#include <map>
#include <algorithm>

static int fails = 0;
#define CHECK(c, msg) do { if (c) printf("  ok   %s\n", msg); else { printf("  FAIL %s\n", msg); fails++; } } while (0)

// pull SIG_* string literals out of main.cpp so the test uses the shipped patterns
static std::map<std::string, std::string> load_sigs(const char* path) {
    std::ifstream f(path); std::string s((std::istreambuf_iterator<char>(f)), {});
    std::regex rx("static const char\\* (SIG_[A-Z_]+)\\s*=\\s*\"([^\"]+)\"");
    std::map<std::string, std::string> out;
    for (auto it = std::sregex_iterator(s.begin(), s.end(), rx); it != std::sregex_iterator(); ++it)
        out[(*it)[1]] = (*it)[2];
    return out;
}

#ifndef SRC_DIR
#define SRC_DIR "."
#endif
// Checks against the real game executable (not part of the repository): every
// signature must match exactly once at the expected address, hooks must steal
// position-independent bytes. Skipped when the exe is not available.
static void binary_checks(const char* exe) {
    std::ifstream f(exe, std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), {});
    if (file.size() < 0x1000) { printf("game executable not found (%s): skipping signature checks\n", exe); return; }
    uint32_t pe = *(uint32_t*)&file[0x3c];
    uint16_t ns = *(uint16_t*)&file[pe + 6];
    uint32_t soi = *(uint32_t*)&file[pe + 24 + 56];
    uint16_t optsz = *(uint16_t*)&file[pe + 20];
    std::vector<uint8_t> img(soi);
    uint32_t textVa = 0, textSz = 0;
    for (int i = 0; i < ns; i++) {
        uint8_t* sh = &file[pe + 24 + optsz + 40 * i];
        uint32_t vs = *(uint32_t*)(sh + 8), va = *(uint32_t*)(sh + 12), rs = *(uint32_t*)(sh + 16), ro = *(uint32_t*)(sh + 20);
        memcpy(&img[va], &file[ro], rs < vs ? rs : vs);
        if (!memcmp(sh, ".text", 5)) { textVa = va; textSz = vs; }
    }
    printf("image mapped: size 0x%x, .text 0x%x+0x%x\n", soi, textVa, textSz);

    auto sigs = load_sigs(SRC_DIR "/../src/main.cpp");
    printf("signatures from main.cpp: %zu\n", sigs.size());
    std::map<std::string, uint32_t> expect = {
        {"SIG_PLACE", 0x491a10}, {"SIG_REMOVE", 0x4924f0}, {"SIG_BOUNDS", 0x495910},
        {"SIG_CURSOR_CALL", 0x2d85fb}, {"SIG_PLACE_CALL", 0x2d8943},
        {"SIG_SERVER_CHECK", 0x2d88f2}, {"SIG_PAY", 0x495f20},
        {"SIG_PLACE_COMMIT", 0x48e6f0}, {"SIG_REMOVE_COMMIT", 0x48ef20}, {"SIG_MATERIAL_ITEM", 0xe8c270}, {"SIG_REFUND_BUILD", 0x4921d0}, {"SIG_BLIT_CHECK", 0xa8e2a7}, {"SIG_POP_UNDO", 0x499540}, {"SIG_HUD_UPDATE", 0xb34ce0}, {"SIG_UI_SOUND", 0xb3651a}, {"SIG_CURSOR_XF", 0x2905c0}, {"SIG_RENDER_FRAME", 0x13fef0}, {"SIG_SYS_DEPS", 0x9c7b80}, {"SIG_HOVER_FX", 0x292c50}, {"SIG_DISM_DEPS", 0x2d8e99}, {"SIG_INPUT_ENCODE", 0x3aba10},
        {"SIG_PAY_PLACE", 0x2d8bab}, {"SIG_PAY_REMOVE", 0x2d98bc}, {"SIG_PAY_REMOVE_PREFAB", 0x2d9e85}, {"SIG_PAY_UNDO", 0x2db350}, {"SIG_MISSING_CALL", 0x29c092}, {"SIG_STOCKS", 0x40e5d0}, {"SIG_PICK_CALL", 0x2b47e7}, {"SIG_ZONE_VIS_CALL", 0x860230},
        {"SIG_UI_DRAW_RECT", 0x11a7d30}, {"SIG_UI_DRAW_ROTATED", 0x11a8330}, {"SIG_UI_BOX_BLEND", 0x11c9b50},
        {"SIG_UI_SCROLL_PARAMS", 0xa3eeb0}, {"SIG_UI_SCROLL_BEGIN", 0xa3e020}, {"SIG_UI_SCROLL_END", 0xa40160},
        {"SIG_UI_SCROLL_SPEED", 0xd4e3e0}, {"SIG_PIECE_CHECK_CALL", 0x2d95d2}, {"SIG_UI_SETTING_OPTION_CALL", 0xc54c13},
        {"SIG_TOGGLE_SCREEN", 0xd74850}, {"SIG_UI_SMALL_TOGGLE", 0xc37f29},
    };
    bool allThere = true;
    for (auto& e : expect) allThere &= sigs.count(e.first) == 1;
    CHECK(allThere, "every checked signature is in main.cpp");
    for (auto& [name, rva] : expect) {
        const uint8_t* hits[4];
        int n = core::scan(&img[textVa], textSz, sigs[name].c_str(), hits, 4);
        char m[160];
        snprintf(m, sizeof m, "%s unique (%d) at expected rva 0x%x (got 0x%lx)", name.c_str(), n, rva,
                 n ? (unsigned long)(hits[0] - &img[0]) : 0ul);
        CHECK(n == 1 && (uint32_t)(hits[0] - &img[0]) == rva, m);
    }
    // the hook steals 10 bytes: mov [rsp+18],rsi ; mov [rsp+20],rdi
    const uint8_t stolen[10] = {0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20};
    CHECK(!memcmp(&img[0x491a10], stolen, 10), "hook prologue is two position-independent movs");
    const uint8_t payStolen[7] = {0x40,0x55,0x56,0x48,0x83,0xEC,0x78};
    CHECK(!memcmp(&img[0x495f20], payStolen, 7), "pay hook steals push rbp; push rsi; sub rsp,78h");
    {
        const uint32_t sig = 0xa8e2a7;
        const uint8_t site[4] = {0x41,0x80,0xFB,0x80};
        CHECK(!memcmp(&img[sig + 0x29], site, 4), "blit patch site = cmp r11b,80h");
        CHECK(sig + 0x29 + 35 == 0xa8e2f3 && img[0xa8e2f3] == 0x41 && img[0xa8e2f4] == 0x80 && img[0xa8e2f5] == 0xBA, "blit continue = cmp byte [r10+0A0h],0");
        CHECK(sig + 0x104 == 0xa8e3ab, "blit skip target");
        CHECK(sig + 0xdb == 0xa8e382 && img[0xa8e382] == 0x41 && img[0xa8e383] == 0x80 && img[0xa8e384] == 0x7A, "blit building write path = cmp byte [r10+30h],0");
    }
    { const uint32_t u = 0xb3651a;
      uint32_t ctx = u + 7 + *(int32_t*)&img[u + 3]; uint32_t call = u + 0x2d + *(int32_t*)&img[u + 0x29];
      CHECK(ctx == 0x283f4a8 && call == 0xa4f310 && img[u + 0x28] == 0xE8, "ui sound: context global and play function"); }
    CHECK(!memcmp(&img[0xb34ce0], "\x48\x8B\xC4\x55\x53", 5), "hud hook steals mov rax,rsp; push rbp; push rbx");
    CHECK(!memcmp(&img[0x28c430], "\x48\x89\x5C\x24\x18", 5), "cursor query hook steals mov [rsp+18h],rbx");
    CHECK(0x2d8e99 + 0x15 + *(int32_t*)&img[0x2d8e99 + 0x11] == 0x9c7b80, "dismantle deps call targets the deps function");
    CHECK(!memcmp(&img[0x2d93a4], "\x48\x8B\x85\x50\x03\x00\x00", 7), "dismantle reads pickup writer at deps+0x180 (rbp+0x350)");
    CHECK(*(int32_t*)&img[0x2d95d2 + 0x1c] == 0x1d8, "dismantle aim state at rbp+0x1d8");
    CHECK(!memcmp(&img[0x4921d0], "\x48\x8B\xC4\x48\x89\x48\x08", 7), "refund hook steals mov rax,rsp; mov [rax+8],rcx");
    CHECK(!memcmp(&img[0x48ef20], "\x48\x89\x5C\x24\x18", 5), "remove commit hook steals mov [rsp+18h],rbx");
    CHECK(!memcmp(&img[0x3aa980], "\x48\x8D\x81\xD0\xFF\x00\x00\xC3", 8) && 0x860230 + 0xc + *(int32_t*)&img[0x860230 + 8] == 0x3aa980, "zone vis getter: lea rax,[rcx+0xffd0]; ret");
    { const uint32_t h = 0x292c50;
      auto rel = [&](uint32_t at) { return at + 5 + *(int32_t*)&img[at + 1]; };
      CHECK(rel(h + 0x4fd) == 0x9c4dc0 && rel(h + 0x51d) == 0x2a1f70 && rel(h + 0x607) == 0x2a1f20, "hover fn: time / stop / start calls");
      CHECK(h + 0x5e7 + 7 + *(int32_t*)&img[h + 0x5e7 + 3] == 0x1528a88 && *(uint32_t*)&img[h + 0x436] == 0xb0718612, "hover fn: spawn defaults and highlight guid");
      CHECK(!memcmp(&img[h], "\x48\x89\x5C\x24\x10", 5), "hover fn hook steals mov [rsp+10h],rbx"); }
    const uint8_t popStolen[5] = {0x48,0x89,0x5C,0x24,0x10};
    CHECK(!memcmp(&img[0x499540], popStolen, 5), "pop hook steals mov [rsp+10h],rbx");
    const uint8_t commitStolen[5] = {0x48,0x8B,0xC4,0x55,0x53};
    CHECK(!memcmp(&img[0x48e6f0], commitStolen, 5), "commit hook steals mov rax,rsp; push rbp; push rbx");
    CHECK(0x2d88f2 + 81 == 0x2d8943, "server branch sits directly before the placement call");
    // call-site distance check used by the mod
    CHECK(0x2d8943 > 0x2d85fb && 0x2d8943 - 0x2d85fb < 0x400, "call-site layout check passes");

}

// Translation files (lang/<code>.txt): one per language of the game's menu, only keys
// of src/strings.h, each once, every text with the same {n} placeholders as English,
// and lang/en.txt equal to the built-in English.
static const char* const kKeys[S_COUNT] = {
#define K(id, key, en) key,
    BT_STRINGS(K)
#undef K
};
static const char* const kEn[S_COUNT] = {
#define E(id, key, en) en,
    BT_STRINGS(E)
#undef E
};
static std::string placeholders(const std::string& t) {
    std::string out;
    for (size_t i = 0; i + 2 < t.size(); i++)
        if (t[i] == '{' && t[i + 1] >= '0' && t[i + 1] <= '3' && t[i + 2] == '}') { out += t[i + 1]; }
    std::sort(out.begin(), out.end());
    return out;
}
static void translation_checks() {
    std::map<std::string, int> index;
    for (int k = 0; k < S_COUNT; k++) {
        CHECK(index.count(kKeys[k]) == 0 || !printf("    duplicate key %s\n", kKeys[k]), kKeys[k]);
        index[kKeys[k]] = k;
    }
    for (int l = 0; l < kLanguageCount; l++) {
        std::string code = kLanguages[l].code, path = std::string(SRC_DIR) + "/../lang/" + code + ".txt";
        std::ifstream f(path);
        std::string head = "lang/" + code + ".txt";
        if (!f) { CHECK(false, (head + " exists").c_str()); continue; }
        std::vector<int> seen(S_COUNT, 0);
        std::string line; int bad = 0, n = 0;
        while (std::getline(f, line)) {
            n++;
            if (n == 1 && line.size() >= 3 && (uint8_t)line[0] == 0xef) line = line.substr(3);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            size_t a = line.find_first_not_of(" \t");
            if (a == std::string::npos || line[a] == '#' || line[a] == ';') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) { printf("    %s:%d no '='\n", head.c_str(), n); bad++; continue; }
            std::string key = line.substr(a, eq - a), val = line.substr(eq + 1);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
            val.erase(0, val.find_first_not_of(" \t") == std::string::npos ? val.size() : val.find_first_not_of(" \t"));
            auto it = index.find(key);
            if (it == index.end()) { printf("    %s:%d unknown key %s\n", head.c_str(), n, key.c_str()); bad++; continue; }
            int k = it->second;
            if (seen[k]++) { printf("    %s:%d duplicate %s\n", head.c_str(), n, key.c_str()); bad++; }
            std::string v; for (size_t i = 0; i < val.size(); i++) { if (val[i] == '\\' && i + 1 < val.size() && val[i + 1] == 'n') { v += '\n'; i++; } else v += val[i]; }
            if (placeholders(v) != placeholders(kEn[k])) { printf("    %s:%d %s placeholders differ from English\n", head.c_str(), n, key.c_str()); bad++; }
            if (code == "en" && v != kEn[k]) { printf("    %s:%d %s differs from src/strings.h\n", head.c_str(), n, key.c_str()); bad++; }
            if (code != "en" && v.empty() && k != S_MENU_TRANSLATION_NOTE) { printf("    %s:%d %s is empty\n", head.c_str(), n, key.c_str()); bad++; }
        }
        int missing = 0;
        for (int k = 0; k < S_COUNT; k++) if (!seen[k]) { printf("    %s: missing %s\n", head.c_str(), kKeys[k]); missing++; }
        CHECK(bad == 0 && missing == 0, (head + ": every key once, placeholders as English").c_str());
    }
}

int main(int argc, char** argv) {
    const char* exe = argc > 1 ? argv[1] : (getenv("ENSHROUDED_EXE") ? getenv("ENSHROUDED_EXE") : "enshrouded.exe");
    binary_checks(exe);
    translation_checks();
    // ---------------------------------------------------------------- planner
    const int64_t M = (int64_t)1 << 32;
    float step2[3] = {2.f, 2.f, 2.f};
    core::Plan pl;
    // line of 5 blocks along x (corners 0 and 8 m; 3rd click repeats corner 2)
    core::I3 line[3] = {{0, 0, 0}, {8 * M, 0, 0}, {8 * M, 0, 0}};
    CHECK(core::plan_fill(line, 3, step2, 2000, &pl) && pl.count[0] == 5 && pl.count[1] == 1 && pl.count[2] == 1, "line 0..8m step 2 -> 5x1x1");
    core::I3 last = core::plan_pos(pl, 4, 0, 0), first = core::plan_pos(pl, 0, 0, 0), mid = core::plan_pos(pl, 2, 0, 0);
    CHECK(first.x == 0 && last.x == 8 * M && mid.x == 4 * M, "line endpoints and midpoint exact");
    // plane on the floor, corners in any order
    core::I3 plane[3] = {{10 * M, 5 * M, -4 * M}, {4 * M, 5 * M, 6 * M}, {4 * M, 5 * M, 6 * M}};
    CHECK(core::plan_fill(plane, 3, step2, 2000, &pl) && pl.count[0] == 4 && pl.count[1] == 1 && pl.count[2] == 6, "floor plane 6x10m -> 4x1x6");
    CHECK(pl.total == 24, "plane total 24");
    // cube: base diagonal then height
    core::I3 cube[3] = {{0, 0, 0}, {4 * M, 0, 4 * M}, {4 * M, 6 * M, 4 * M}};
    CHECK(core::plan_fill(cube, 3, step2, 2000, &pl) && pl.count[0] == 3 && pl.count[1] == 4 && pl.count[2] == 3, "cube 4x6x4 -> 3x4x3");
    core::I3 top = core::plan_pos(pl, 2, 3, 2);
    CHECK(top.x == 4 * M && top.y == 6 * M && top.z == 4 * M, "cube far corner exact");
    // negative world coordinates
    core::I3 neg[3] = {{-100 * M, -3 * M, -50 * M}, {-96 * M, -3 * M, -50 * M}, {-96 * M, -3 * M, -50 * M}};
    CHECK(core::plan_fill(neg, 3, step2, 2000, &pl) && pl.count[0] == 3, "negative coordinates");
    // limit
    core::I3 huge[3] = {{0, 0, 0}, {200 * M, 0, 200 * M}, {200 * M, 40 * M, 200 * M}};
    CHECK(!core::plan_fill(huge, 3, step2, 2000, &pl) && pl.total > 2000, "over MaxShapes is refused");
    // half-metre shapes
    float stepHalf[3] = {0.5f, 0.5f, 0.5f};
    core::I3 small[3] = {{0, 0, 0}, {M, 0, 0}, {M, 0, 0}};
    CHECK(core::plan_fill(small, 3, stepHalf, 2000, &pl) && pl.count[0] == 3, "0.5 m shapes: 1 m line -> 3");
    // single point (same spot 3 times) -> one shape
    core::I3 pt[3] = {{7 * M, 1 * M, 2 * M}, {7 * M, 1 * M, 2 * M}, {7 * M, 1 * M, 2 * M}};
    CHECK(core::plan_fill(pt, 3, step2, 2000, &pl) && pl.total == 1, "same spot 3x -> 1 shape");
    // slightly off-grid corner (float noise) still gives whole steps
    core::I3 noisy[3] = {{0, 0, 0}, {8 * M + 3000, 0, 0}, {8 * M + 3000, 0, 0}};
    CHECK(core::plan_fill(noisy, 3, step2, 2000, &pl) && pl.count[0] == 5, "noise does not add a step");

    // shapes side by side: spacing is exactly the shape size
    {
        core::I3 odd[2] = {{0, 0, 0}, {(int64_t)(3.5 * M), 0, 0}};
        CHECK(core::plan_fill(odd, 2, step2, 2000, &pl) && pl.count[0] == 3, "3.5 m with 2 m shapes -> 3");
        core::I3 p1 = core::plan_pos(pl, 1, 0, 0), p2 = core::plan_pos(pl, 2, 0, 0);
        CHECK(p1.x == 2 * M && p2.x == 4 * M, "spacing exactly 2 m (no overlap)");
        core::I3 tiny[2] = {{0, 0, 0}, {M / 2, 0, 0}};
        CHECK(core::plan_fill(tiny, 2, step2, 2000, &pl) && pl.total == 1, "0.5 m offset with 2 m shapes -> 1 shape, no overlap");
        core::I3 back[2] = {{10 * M, 0, 0}, {4 * M, 0, 0}};
        CHECK(core::plan_fill(back, 2, step2, 2000, &pl) && pl.count[0] == 4 && core::plan_pos(pl, 3, 0, 0).x == 4 * M &&
              core::plan_pos(pl, 1, 0, 0).x == 8 * M, "runs from corner 1 toward corner 2 (negative direction)");
        core::I3 box2[2] = {{0, 0, 0}, {4 * M, 2 * M, 6 * M}};
        CHECK(core::plan_fill(box2, 2, step2, 2000, &pl) && pl.count[0] == 3 && pl.count[1] == 2 && pl.count[2] == 4, "2-corner box 3x2x4");
    }

    // fill modes
    {
        core::I3 room[2] = {{0, 0, 0}, {8 * M, 6 * M, 10 * M}};       // 5 x 4 x 6 shapes
        CHECK(core::plan_fill(room, 2, step2, 2000, &pl) && pl.total == 120, "room solid 5x4x6 = 120");
        CHECK(core::fill_count(pl, core::FILL_HOLLOW) == 120 - 3 * 2 * 4, "hollow removes the 3x2x4 inside");
        CHECK(core::fill_count(pl, core::FILL_WALLS) == 120 - 3 * 4 * 4, "walls only: no floor/ceiling inside");
        long n = 0; for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) for (int k = 0; k < 6; k++) n += core::fill_keep(pl, i, j, k, core::FILL_HOLLOW);
        CHECK(n == core::fill_count(pl, core::FILL_HOLLOW), "hollow keep matches count");
        n = 0; for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) for (int k = 0; k < 6; k++) n += core::fill_keep(pl, i, j, k, core::FILL_WALLS);
        CHECK(n == core::fill_count(pl, core::FILL_WALLS), "walls keep matches count");
        core::I3 floor2[2] = {{0, 0, 0}, {8 * M, 0, 8 * M}};
        CHECK(core::plan_fill(floor2, 2, step2, 2000, &pl) && core::fill_count(pl, core::FILL_HOLLOW) == pl.total, "hollow floor stays full");
        core::I3 big[2] = {{0, 0, 0}, {2000 * M, 2000 * M, 2000 * M}};
        CHECK(!core::plan_fill(big, 2, step2, 50000000, &pl), "huge area refused without overflow");
    }

    // roofs
    {
        const float step1[3] = { 1.f, 1.f, 1.f };
        auto keepCount = [&](const core::Plan& p, int mode) {
            long n = 0;
            for (int i = 0; i < p.count[0]; i++) for (int j = 0; j < p.count[1]; j++) for (int k = 0; k < p.count[2]; k++)
                n += core::fill_keep(p, i, j, k, mode);
            return n;
        };
        const int R = core::FILL_ROOF;
        auto fo = [](int shape, int rot = 0, int thick = 1, int o = 0) { return core::FillOpts{ shape, rot, thick, o }; };
        // flat 7 x 4 area: gable slopes across the short side (z)
        core::I3 flat[2] = {{0, 0, 0}, {6 * M, 0, 3 * M}};
        CHECK(core::plan_area(flat, 2, step1, 2000, R, fo(core::ROOF_GABLE), &pl) && !pl.roofAxisX && pl.count[1] == 2,
              "flat 7x4 gable: ridge along the long side, grows to 2 high");
        CHECK(core::fill_count(pl, R) == 7 * 4 && keepCount(pl, R) == 7 * 4, "45 degree gable: one shape per column");
        CHECK(core::fill_keep(pl, 3, 0, 0, R) && core::fill_keep(pl, 3, 1, 1, R) && core::fill_keep(pl, 3, 1, 2, R) &&
              core::fill_keep(pl, 3, 0, 3, R) && !core::fill_keep(pl, 3, 1, 0, R), "gable steps: up one per shape in");
        CHECK(core::plan_area(flat, 2, step1, 2000, R, fo(core::ROOF_GABLE, 1), &pl) && pl.roofAxisX && pl.count[1] == 4,
              "turned gable: ridge along the short side, 4 high");
        // hip over a flat 5 x 5: a pyramid 3 high
        core::I3 sq[2] = {{0, 0, 0}, {4 * M, 0, 4 * M}};
        CHECK(core::plan_area(sq, 2, step1, 2000, R, fo(core::ROOF_HIP), &pl) && pl.count[1] == 3, "hip 5x5 -> 3 high");
        CHECK(core::fill_count(pl, R) == 25 && core::fill_keep(pl, 2, 2, 2, R) && core::fill_keep(pl, 1, 1, 3, R) &&
              core::fill_keep(pl, 0, 0, 4, R), "hip pyramid");
        // shed over a flat 5 wide: rises one per shape, 5 high, 4 ways round
        CHECK(core::plan_area(sq, 2, step1, 2000, R, fo(core::ROOF_SHED), &pl) && pl.count[1] == 5 &&
              core::fill_keep(pl, 0, 0, 0, R) && core::fill_keep(pl, 0, 4, 4, R), "shed rises across the area");
        CHECK(core::plan_area(sq, 2, step1, 2000, R, fo(core::ROOF_SHED, 2), &pl) && core::fill_keep(pl, 0, 4, 0, R) &&
              core::fill_keep(pl, 0, 0, 4, R), "shed turned twice rises the other way");
        // gambrel / mansard: steep below, flatter above
        core::I3 wide[2] = {{0, 0, 0}, {8 * M, 0, 20 * M}};       // 9 across -> D 4, 5 high
        CHECK(core::plan_area(wide, 2, step1, 2000, R, fo(core::ROOF_GAMBREL), &pl) && pl.count[1] == 5, "gambrel 5 high");
        int lo, hi; core::roof_column(pl, 1, 0, &lo, &hi);
        int lo2, hi2; core::roof_column(pl, 4, 0, &lo2, &hi2);
        CHECK(hi == 2 && hi2 == 4 && lo2 == 4, "gambrel: steep lower slope (2 up in the first shape), flat-ish top");
        CHECK(core::plan_area(wide, 2, step1, 2000, R, fo(core::ROOF_MANSARD), &pl) && keepCount(pl, R) == core::fill_count(pl, R),
              "mansard keep matches count");
        // 3D area keeps its height (steep: columns stack)
        core::I3 tall[2] = {{0, 0, 0}, {10 * M, 8 * M, 8 * M}};
        CHECK(core::plan_area(tall, 2, step1, 2000, R, fo(core::ROOF_GABLE), &pl) && pl.count[1] == 9, "3D gable keeps its height");
        CHECK(core::fill_keep(pl, 0, 8, 4, R) && core::fill_keep(pl, 0, 0, 0, R) && core::fill_keep(pl, 0, 2, 1, R) &&
              core::fill_keep(pl, 0, 1, 1, R) && !core::fill_keep(pl, 0, 0, 1, R), "steep gable: each column stacks up to its own top");
        // overhang: the plan widens and drops; the slope carries on down
        CHECK(core::plan_area(flat, 2, step1, 2000, R, fo(core::ROOF_GABLE, 0, 1, 2), &pl) && pl.count[0] == 11 && pl.count[2] == 8 &&
              pl.count[1] == 4 && pl.roofBase == 2 && pl.origin[0] == -2 * M && pl.origin[1] == -2 * M, "overhang 2 widens and drops the plan");
        CHECK(core::fill_keep(pl, 0, 0, 0, R) && core::fill_keep(pl, 5, 2, 2, R) && core::fill_keep(pl, 5, 3, 3, R) &&
              keepCount(pl, R) == 11 * 8, "overhang: eave 2 below the area edge, edge at the area's level");
        // thickness
        CHECK(core::plan_area(flat, 2, step1, 2000, R, fo(core::ROOF_GABLE, 0, 2), &pl) && pl.roofBase == 1 && pl.count[1] == 3 &&
              keepCount(pl, R) == core::fill_count(pl, R) && !core::fill_keep(pl, 0, 0, 1, R) && core::fill_keep(pl, 0, 1, 1, R) && core::fill_keep(pl, 0, 2, 1, R) &&
              core::fill_keep(pl, 0, 0, 0, R) && core::fill_keep(pl, 0, 1, 0, R),
              "roof thickness 2: each step two deep");
        core::I3 room[2] = {{0, 0, 0}, {9 * M, 9 * M, 9 * M}};
        CHECK(core::plan_area(room, 2, step1, 2000, core::FILL_HOLLOW, fo(0, 0, 2), &pl) &&
              core::fill_count(pl, core::FILL_HOLLOW) == 1000 - 6 * 6 * 6 && keepCount(pl, core::FILL_HOLLOW) == 1000 - 216, "hollow 2 thick");
        CHECK(core::fill_count(pl, core::FILL_WALLS) == 1000 - 6 * 10 * 6 && keepCount(pl, core::FILL_WALLS) == 1000 - 360, "walls 2 thick");
        // every column connected, keep == count, for every shape / turn / overhang / thickness
        bool okAll = true;
        core::I3 r[2] = {{0, 0, 0}, {9 * M, 3 * M, 6 * M}};
        for (int sh = 0; sh < core::ROOF_COUNT; sh++) for (int rt = 0; rt < 4; rt++) for (int t = 1; t <= 3; t++) for (int o = 0; o <= 3; o++)
            for (int flatToo = 0; flatToo < 2; flatToo++) {
                core::I3 c2[2] = { r[0], r[1] }; if (flatToo) c2[1].y = 0;
                if (!core::plan_area(c2, 2, step1, 20000, R, fo(sh, rt, t, o), &pl)) { okAll = false; continue; }
                for (int i = 0; i < pl.count[0]; i++) for (int k = 0; k < pl.count[2]; k++) {
                    int col = 0; for (int j = 0; j < pl.count[1]; j++) col += core::fill_keep(pl, i, j, k, R);
                    // round roofs: exactly the columns inside the outline (the area's ellipse widened by the overhang)
                    bool want = !core::roof_is_round(sh) || core::round_inside(core::ROUND_FULL, 0, i, k, pl.count[0], pl.count[2]);
                    if ((col >= 1) != want) okAll = false;
                }
                if (keepCount(pl, R) != core::fill_count(pl, R)) okAll = false;
            }
        CHECK(okAll, "every roof column holds a shape, keep matches count (all shapes and options)");
        // corner 2 below corner 1: the roof still rises upward
        core::I3 down[2] = {{0, 4 * M, 0}, {0, 0, 8 * M}};
        CHECK(core::plan_area(down, 2, step1, 2000, R, fo(core::ROOF_GABLE), &pl) && pl.stepFix[1] < 0, "downward area");
        CHECK(core::fill_keep(pl, 0, pl.count[1] - 1, 0, R) && core::fill_keep(pl, 0, 0, 4, R), "downward area: eaves at the bottom, ridge at the top");
        // pose packing
        bool poses = true;
        for (int sh = 0; sh < core::ROOF_COUNT; sh++) for (int rt = 0; rt < core::roof_turns(sh); rt++) {
            int s2, r2; int v = core::roof_pose_pack(sh, rt); core::roof_pose_unpack(v, &s2, &r2);
            if (v > 15 || s2 != sh || r2 != rt) poses = false;
        }
        CHECK(poses, "roof shape + turn fit 4 bits");
        // cone over a flat 9 x 9: a circle, 45 degrees, 5 high, peak in the middle
        core::I3 c9[2] = {{0, 0, 0}, {8 * M, 0, 8 * M}};
        CHECK(core::plan_area(c9, 2, step1, 2000, R, fo(core::ROOF_CONE), &pl) && pl.count[1] == 5 && pl.roofBase == 0 &&
              core::roof_turns(core::ROOF_CONE) == 1, "cone 9x9 -> 5 high");
        CHECK(core::fill_keep(pl, 4, 4, 4, R) && !core::fill_keep(pl, 4, 3, 4, R) && core::fill_keep(pl, 0, 0, 4, R) &&
              !core::fill_keep(pl, 0, 0, 0, R) && core::fill_keep(pl, 2, 2, 4, R), "cone: peak in the middle, round outline, empty corners");
        {   // same outline as the full round fill of the area (sits on round walls)
            core::Plan rp; core::FillOpts rf = {}; rf.thick = 1;
            core::plan_area(c9, 2, step1, 2000, core::FILL_ROUND, rf, &rp);
            bool same = true;
            for (int i = 0; i < 9; i++) for (int k = 0; k < 9; k++) {
                int col = 0; for (int j = 0; j < pl.count[1]; j++) col += core::fill_keep(pl, i, j, k, R);
                if ((col > 0) != core::fill_keep(rp, i, 0, k, core::FILL_ROUND)) same = false;
            }
            CHECK(same, "cone outline = round fill outline");
        }
        // the cone is a closed surface: every column's span reaches down to its lowest neighbour's top
        {
            bool closed = true;
            for (int i = 0; i < 9; i++) for (int k = 0; k < 9; k++) {
                int lo, hi; core::roof_column(pl, i, k, &lo, &hi);
                if (hi < lo) continue;
                const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                for (int q = 0; q < 4; q++) {
                    int x = i + nb[q][0], z = k + nb[q][1], lo2, hi2;
                    if (x < 0 || z < 0 || x >= 9 || z >= 9) continue;
                    core::roof_column(pl, x, z, &lo2, &hi2);
                    if (hi2 >= lo2 && (hi2 < lo - 1 || hi < lo2 - 1)) closed = false;
                }
            }
            CHECK(closed, "cone steps touch (no gaps)");
            core::I3 c10[2] = {{0, 0, 0}, {9 * M, 0, 8 * M}};
            core::Plan q; bool ring = true;
            for (int sh = core::ROOF_CONE; sh <= core::ROOF_DOME; sh++) {
                core::plan_area(c10, 2, step1, 2000, R, fo(sh), &q);
                for (int i = 0; i < q.count[0]; i++) for (int k = 0; k < q.count[2]; k++) {
                    int lo, hi; core::roof_column(q, i, k, &lo, &hi);
                    if (hi < lo) continue;
                    bool edge = false;
                    const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                    for (int e = 0; e < 4; e++)
                        if (!core::round_inside(core::ROUND_FULL, 0, i + nb[e][0], k + nb[e][1], q.count[0], q.count[2])) edge = true;
                    if (edge && lo != 0) ring = false;
                }
            }
            CHECK(ring, "cone / dome 10x9: every outline column reaches down to the eave (closed bottom ring)");
        }
        // dome over 9 x 9: steep at the eave (an edge column reaches up), round on top
        CHECK(core::plan_area(c9, 2, step1, 2000, R, fo(core::ROOF_DOME), &pl) && pl.count[1] == 5 && pl.roofBase == 0, "dome 9x9 -> 5 high");
        {
            int lo, hi; core::roof_column(pl, 4, 0, &lo, &hi);
            int loC, hiC; core::roof_column(pl, 4, 4, &loC, &hiC);
            CHECK(lo == 0 && hi >= 2 && hiC == 4, "dome: edge column from the eave up the steep side, top in the middle");
        }
        // overhang: cone slope carries on down, dome gets a flat brim
        CHECK(core::plan_area(c9, 2, step1, 2000, R, fo(core::ROOF_CONE, 0, 1, 2), &pl) && pl.count[0] == 13 && pl.roofBase >= 2,
              "cone overhang drops");
        CHECK(core::plan_area(c9, 2, step1, 2000, R, fo(core::ROOF_DOME, 0, 1, 2), &pl) && pl.count[0] == 13 && pl.roofBase == 0 &&
              core::fill_keep(pl, 6, 0, 0, R) && core::fill_keep(pl, 6, 0, 1, R) && !core::fill_keep(pl, 6, 1, 1, R), "dome overhang = level brim");
        // ellipse: rectangle area
        core::I3 el[2] = {{0, 0, 0}, {14 * M, 0, 6 * M}};
        CHECK(core::plan_area(el, 2, step1, 5000, R, fo(core::ROOF_DOME, 0, 2), &pl) && pl.count[1] == 5 &&
              keepCount(pl, R) == core::fill_count(pl, R), "dome over an ellipse");
        CHECK(core::plan_area(flat, 2, step1, 2000, core::FILL_SOLID, fo(core::ROOF_GABLE, 1, 1, 3), &pl) && pl.count[1] == 1 && pl.total == 28, "solid unchanged");
    }

    // round fills
    {
        const float step1[3] = { 1.f, 1.f, 1.f };
        const int O = core::FILL_ROUND;
        core::Plan pl;
        auto ro = [](int shape, int rot, int hollow, int thick = 1, int lying = 0) {
            core::FillOpts f = {}; f.thick = thick; f.roundShape = shape; f.roundRot = rot; f.roundHollow = hollow; f.roundLying = lying; return f;
        };
        auto keepCount = [&](const core::Plan& p) {
            long n = 0;
            for (int i = 0; i < p.count[0]; i++) for (int j = 0; j < p.count[1]; j++) for (int k = 0; k < p.count[2]; k++)
                n += core::fill_keep(p, i, j, k, O);
            return n;
        };
        // full circle 9 x 9, 2 high: corners out, centre and edge middles in, symmetric
        core::I3 sq[2] = {{0, 0, 0}, {8 * M, 1 * M, 8 * M}};
        CHECK(core::plan_area(sq, 2, step1, 5000, O, ro(core::ROUND_FULL, 0, 0), &pl) && pl.roundAxis == 1, "round: ground plan");
        CHECK(!core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 4, 0, 4, O) && core::fill_keep(pl, 0, 1, 4, O) &&
              core::fill_keep(pl, 4, 1, 8, O), "full circle: corners out, centre and sides in");
        bool sym = true;
        for (int i = 0; i < 9; i++) for (int k = 0; k < 9; k++)
            if (core::fill_keep(pl, i, 0, k, O) != core::fill_keep(pl, 8 - i, 0, k, O) ||
                core::fill_keep(pl, i, 0, k, O) != core::fill_keep(pl, k, 0, i, O)) sym = false;
        CHECK(sym, "full circle is symmetric");
        CHECK(core::fill_count(pl, O) == keepCount(pl), "full circle: count matches keep");
        // hollow ring: no centre, no gap a step can pass through (4-neighbour flood from the centre stays inside)
        for (int t = 1; t <= 3; t++) {
            core::I3 big[2] = {{0, 0, 0}, {14 * M, 0, 10 * M}};
            CHECK(core::plan_area(big, 2, step1, 5000, O, ro(core::ROUND_FULL, 0, 1, t), &pl), "ellipse 15 x 11 planned");
            const int nx = pl.count[0], nz = pl.count[2];
            CHECK(!core::fill_keep(pl, 7, 0, 5, O) && core::fill_keep(pl, 0, 0, 5, O) && core::fill_keep(pl, 7, 0, 0, O), "ring: centre empty, sides walled");
            CHECK(core::fill_keep(pl, t - 1, 0, 5, O) && !core::fill_keep(pl, t, 0, 5, O), "ring is `thick` shapes thick at the side");
            std::vector<int> seen(nx * nz, 0), st{ 7 + nx * 5 }; seen[7 + nx * 5] = 1; bool leak = false;
            while (!st.empty()) {
                int c = st.back(); st.pop_back(); int x = c % nx, z = c / nx;
                const int d[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                for (auto& e : d) {
                    int X = x + e[0], Z = z + e[1];
                    if (X < 0 || Z < 0 || X >= nx || Z >= nz) { leak = true; continue; }
                    if (seen[X + nx * Z] || core::fill_keep(pl, X, 0, Z, O)) continue;
                    seen[X + nx * Z] = 1; st.push_back(X + nx * Z);
                }
            }
            CHECK(!leak, "ring wall has no gaps");
            CHECK(core::fill_count(pl, O) == keepCount(pl), "ring: count matches keep");
        }
        // quarter: the curve runs from corner 1 to corner 2, also with corner 1 on the far side
        core::I3 q[2] = {{0, 0, 0}, {5 * M, 0, 5 * M}};
        CHECK(core::plan_area(q, 2, step1, 5000, O, ro(core::ROUND_QUARTER, 0, 1), &pl), "quarter planned");
        CHECK(core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 5, 0, 5, O) && !core::fill_keep(pl, 5, 0, 0, O) &&
              !core::fill_keep(pl, 1, 0, 4, O), "quarter wall from corner 1 to corner 2, open toward its centre");
        core::I3 qb[2] = {{5 * M, 0, 5 * M}, {0, 0, 0}};
        CHECK(core::plan_area(qb, 2, step1, 5000, O, ro(core::ROUND_QUARTER, 0, 1), &pl) &&
              core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 5, 0, 5, O) && !core::fill_keep(pl, 5, 0, 0, O), "quarter from the far corner");
        CHECK(core::plan_area(q, 2, step1, 5000, O, ro(core::ROUND_QUARTER, 1, 1), &pl) &&
              core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 5, 0, 5, O) && !core::fill_keep(pl, 0, 0, 5, O), "quarter turned bends the other way");
        CHECK(core::plan_area(q, 2, step1, 5000, O, ro(core::ROUND_QUARTER, 2, 0), &pl) &&
              core::fill_keep(pl, 0, 0, 0, O) && !core::fill_keep(pl, 5, 0, 5, O), "quarter turned twice: centre on corner 1");
        // half: flat side on the long side, open there when hollow
        core::I3 h[2] = {{0, 0, 0}, {10 * M, 0, 5 * M}};
        CHECK(core::plan_area(h, 2, step1, 5000, O, ro(core::ROUND_HALF, 0, 1), &pl) && pl.roundSide == 0, "half: flat on the long side");
        CHECK(core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 10, 0, 0, O) && core::fill_keep(pl, 5, 0, 5, O) &&
              !core::fill_keep(pl, 5, 0, 0, O) && !core::fill_keep(pl, 5, 0, 3, O), "half wall: ends and top in, flat side open");
        CHECK(core::plan_area(h, 2, step1, 5000, O, ro(core::ROUND_HALF, 1, 0), &pl) && pl.roundSide == 1 &&
              core::fill_keep(pl, 5, 0, 0, O) && core::fill_keep(pl, 0, 0, 5, O) && !core::fill_keep(pl, 0, 0, 0, O), "half turned: flat on the far side");
        // standing wall (one shape thin): the round stands in it, an arch with the flat side down
        core::I3 w[2] = {{0, 0, 0}, {8 * M, 4 * M, 0}};
        CHECK(core::plan_area(w, 2, step1, 5000, O, ro(core::ROUND_HALF, 0, 1), &pl) && pl.roundAxis == 2, "round in a standing wall");
        CHECK(core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 4, 4, 0, O) && !core::fill_keep(pl, 4, 0, 0, O) &&
              !core::fill_keep(pl, 0, 4, 0, O), "arch: feet and top in, opening below");
        core::I3 wd[2] = {{8 * M, 4 * M, 0}, {0, 0, 0}};
        CHECK(core::plan_area(wd, 2, step1, 5000, O, ro(core::ROUND_HALF, 0, 1), &pl) && core::fill_keep(pl, 4, 0, 0, O) &&
              !core::fill_keep(pl, 4, 4, 0, O), "arch from the top corner: still standing on the ground");
        // lying: a tube along the longer side, the round standing across it (half: a tunnel, flat side down)
        core::I3 tube[2] = {{0, 0, 0}, {9 * M, 4 * M, 8 * M}};
        CHECK(core::plan_area(tube, 2, step1, 5000, O, ro(core::ROUND_HALF, 0, 1, 1, 1), &pl) && pl.roundAxis == 0, "lying: along x");
        bool same = true;
        for (int i = 0; i < 10; i++) for (int j = 0; j < 5; j++) for (int k = 0; k < 9; k++)
            if (core::fill_keep(pl, i, j, k, O) != core::fill_keep(pl, 0, j, k, O)) same = false;
        CHECK(same && core::fill_keep(pl, 3, 0, 0, O) && core::fill_keep(pl, 3, 4, 4, O) && !core::fill_keep(pl, 3, 0, 4, O),
              "tunnel: the same cross-section all along, feet and top in, opening below");
        core::I3 flatTube[2] = {{0, 0, 0}, {4 * M, 0, 9 * M}};
        CHECK(core::plan_area(flatTube, 2, step1, 5000, O, ro(core::ROUND_FULL, 0, 0, 1, 1), &pl) && pl.roundAxis == 2, "lying: along z when z is longer");
        // every pose fits the state (as the roof fill with poses 10..15) and comes back
        bool ok = true;
        for (int sh = 0; sh < core::ROUND_COUNT; sh++)
            for (int rt = 0; rt < core::round_turns(sh); rt++)
                for (int hl = 0; hl < 2; hl++)
                    for (int ly = 0; ly < 2; ly++)
                        for (int t = 1; t <= 3; t++) {
                            core::FillOpts f = ro(sh, rt, hl, t, ly), back = {};
                            uint32_t bits = core::net_fill_bits(O, f);
                            if (core::net_fill_unpack(bits, &back) != O || back.roundShape != sh || back.roundRot != rt ||
                                back.roundHollow != hl || back.roundLying != ly || (hl && back.thick != t)) ok = false;
                            if (bits & ~((3u << core::NET_FILL_SHIFT) | (15u << core::NET_ROOF_SHIFT) | (3u << core::NET_OVERHANG_SHIFT) |
                                         (3u << core::NET_THICK_SHIFT))) ok = false;
                        }
        for (int sh = 0; sh < core::ROOF_COUNT; sh++)
            for (int rt = 0; rt < core::roof_turns(sh); rt++) {
                core::FillOpts f = {}, back = {}; f.roofShape = sh; f.roofRot = rt; f.overhang = 3; f.thick = 2;
                if (core::net_fill_unpack(core::net_fill_bits(core::FILL_ROOF, f), &back) != core::FILL_ROOF || back.roofShape != sh ||
                    back.roofRot != rt || back.overhang != 3 || back.thick != 2) ok = false;
            }
        for (int sh = core::ROOF_CONE; sh <= core::ROOF_DOME; sh++)
            for (int t = 1; t <= 3; t++)
                for (int o = 0; o <= 3; o++) {
                    core::FillOpts f = {}, back = {}; f.roofShape = sh; f.overhang = o; f.thick = t;
                    uint32_t bits = core::net_fill_bits(core::FILL_ROOF, f);
                    if (core::net_fill_unpack(bits, &back) != core::FILL_ROOF || back.roofShape != sh || back.roofRot != 0 ||
                        back.overhang != o || back.thick != t) ok = false;
                    if (bits & ~((3u << core::NET_FILL_SHIFT) | (15u << core::NET_ROOF_SHIFT) | (3u << core::NET_OVERHANG_SHIFT) |
                                 (3u << core::NET_THICK_SHIFT))) ok = false;
                }
        CHECK(ok, "round and roof settings round trip through the state");
    }

    // diagonal fills
    {
        const float step1[3] = { 1.f, 1.f, 1.f };
        const int O = core::FILL_DIAG;
        core::Plan pl;
        auto dg = [](int rot, int lying, int thick = 1) {
            core::FillOpts f = {}; f.thick = thick; f.diagRot = rot; f.diagLying = lying; return f;
        };
        // face-connected: all kept cells of the plane form one piece through shared faces
        auto connected = [](const core::Plan& p) {
            const int nu = p.count[p.diagU], nv = p.count[p.diagV];
            static int stack[4096]; static bool seen[4096];
            if (nu * nv > 4096) return false;
            int kept = 0, start = -1;
            for (int c = 0; c < nu * nv; c++) {
                seen[c] = false;
                if (core::diag_keep(p.diagFlip, p.thick, c % nu, c / nu, nu, nv)) { kept++; if (start < 0) start = c; }
            }
            if (start < 0) return false;
            int top = 0, found = 0; stack[top++] = start; seen[start] = true;
            while (top) {
                int c = stack[--top], u = c % nu, v = c / nu; found++;
                const int du[4] = { 1, -1, 0, 0 }, dv[4] = { 0, 0, 1, -1 };
                for (int k = 0; k < 4; k++) {
                    int a = u + du[k], b = v + dv[k];
                    if (!core::diag_keep(p.diagFlip, p.thick, a, b, nu, nv) || seen[a + nu * b]) continue;
                    seen[a + nu * b] = true; stack[top++] = a + nu * b;
                }
            }
            return found == kept;
        };
        // standing 8 x 8, 3 high: the wall runs from corner 1 to corner 2 in the ground plan
        core::I3 sq[2] = {{0, 0, 0}, {7 * M, 2 * M, 7 * M}};
        CHECK(core::plan_area(sq, 2, step1, 5000, O, dg(0, 0), &pl) && pl.diagU == 0 && pl.diagV == 2, "diag standing: ground plan");
        CHECK(core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 7, 2, 7, O) && !core::fill_keep(pl, 7, 0, 0, O) &&
              !core::fill_keep(pl, 0, 1, 7, O), "corner 1 to corner 2, not the other corners");
        CHECK(core::fill_count(pl, O) == 15 * 3, "45 degrees: a staircase of 15 cells per layer");
        CHECK(connected(pl), "no gaps along the staircase");
        CHECK(core::plan_area(sq, 2, step1, 5000, O, dg(1, 0), &pl) && core::fill_keep(pl, 7, 0, 0, O) &&
              core::fill_keep(pl, 0, 0, 7, O) && !core::fill_keep(pl, 0, 0, 0, O), "turned: the other diagonal");
        // corner 2 on the negative side: still from corner 1 to corner 2
        core::I3 neg[2] = {{0, 0, 0}, {-9 * M, 0, 4 * M}};
        CHECK(core::plan_area(neg, 2, step1, 5000, O, dg(0, 0), &pl) && core::fill_keep(pl, 0, 0, 0, O) &&
              core::fill_keep(pl, 9, 0, 4, O) && !core::fill_keep(pl, 9, 0, 0, O) && connected(pl), "mirrored area");
        long c1 = core::fill_count(pl, O);
        CHECK(c1 == 14, "10 x 5: 10 + 5 - 1 cells");
        CHECK(core::plan_area(neg, 2, step1, 5000, O, dg(0, 0, 3), &pl) && core::fill_count(pl, O) > c1 && connected(pl), "thicker");
        // thickness counts the slots of the band: brute force count matches fill_count
        {
            long n = 0;
            for (int i = 0; i < pl.count[0]; i++) for (int j = 0; j < pl.count[1]; j++) for (int k = 0; k < pl.count[2]; k++)
                n += core::fill_keep(pl, i, j, k, O);
            CHECK(n == core::fill_count(pl, O), "count matches the cells");
        }
        // standing wall one thin: a diagonal brace in it
        core::I3 w[2] = {{0, 0, 0}, {0, 4 * M, 8 * M}};
        CHECK(core::plan_area(w, 2, step1, 5000, O, dg(0, 0), &pl) && pl.diagU == 2 && pl.diagV == 1 &&
              core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 0, 4, 8, O) && !core::fill_keep(pl, 0, 4, 0, O), "brace in a wall");
        // lying: a ramp along the longer side; over a flat area 45 degrees, growing upward
        core::I3 flat[2] = {{0, 0, 0}, {5 * M, 0, 2 * M}};
        CHECK(core::plan_area(flat, 2, step1, 5000, O, dg(0, 1), &pl) && pl.diagU == 0 && pl.diagV == 1 && pl.count[1] == 6 &&
              core::fill_keep(pl, 0, 0, 0, O) && core::fill_keep(pl, 5, 5, 2, O) && !core::fill_keep(pl, 5, 0, 1, O), "ramp up along x");
        CHECK(core::plan_area(flat, 2, step1, 5000, O, dg(1, 1), &pl) && core::fill_keep(pl, 0, 5, 0, O) && core::fill_keep(pl, 5, 0, 2, O),
              "ramp turned: down");
        CHECK(core::plan_area(flat, 2, step1, 5000, O, dg(2, 1), &pl) && pl.diagU == 2 && pl.count[1] == 3, "ramp along the shorter side");
        core::I3 r3[2] = {{0, 0, 0}, {9 * M, 3 * M, 1 * M}};
        CHECK(core::plan_area(r3, 2, step1, 5000, O, dg(0, 1), &pl) && pl.count[1] == 4 && core::fill_count(pl, O) == 13 * 2 &&
              connected(pl), "3D area: ramp corner to corner, height kept");
        // any size, thickness and turn: no gaps, both ends kept
        bool all = true;
        for (int x = 0; x < 14; x++)
            for (int z = 0; z < 14; z++)
                for (int t = 1; t <= 3; t++)
                    for (int rt = 0; rt < 2; rt++) {
                        core::I3 c2[2] = {{0, 0, 0}, {x * M, 0, z * M}};
                        if (!core::plan_area(c2, 2, step1, 5000, O, dg(rt, 0, t), &pl) || !connected(pl)) all = false;
                        if (!core::fill_keep(pl, rt ? x : 0, 0, 0, O) || !core::fill_keep(pl, rt ? 0 : x, 0, z, O)) all = false;
                    }
        CHECK(all, "every size: gap-free, from corner to corner");
        // settings round trip
        bool ok = true;
        for (int ly = 0; ly < 2; ly++)
            for (int rt = 0; rt < core::diag_turns(ly); rt++)
                for (int t = 1; t <= 3; t++) {
                    core::FillOpts f = dg(rt, ly, t), back = {};
                    uint32_t bits = core::net_fill_bits(O, f);
                    if (core::net_fill_unpack(bits, &back) != O || back.diagRot != rt || back.diagLying != ly || back.thick != t) ok = false;
                }
        for (int sh = 0; sh < core::ROUND_COUNT; sh++)
            for (int rt = 0; rt < core::round_turns(sh); rt++) {
                core::FillOpts f = {}, back = {}; f.roundShape = sh; f.roundRot = rt; f.roundLying = 1;
                if (core::net_fill_unpack(core::net_fill_bits(core::FILL_ROUND, f), &back) != core::FILL_ROUND) ok = false;
            }
        CHECK(ok, "diagonal settings round trip through the state, rounds still rounds");
    }

    // ----------------------------------------------------- axis lock
    {
        const int64_t T = M / 10;
        core::I3 a{0, 0, 0}, b{6 * M, 0, 4 * M}, c{9 * M, 3 * M, -7 * M};   // floor plane, c drifts
        core::I3 l = core::lock_third(a, b, c, T);
        CHECK(l.x == 6 * M && l.z == 4 * M && l.y == 3 * M, "plane: corner 3 only adds height");
        core::I3 b2{6 * M, 0, 0};                                         // line along x
        l = core::lock_third(a, b2, c, T);
        CHECK(l.x == 6 * M && l.y == 3 * M && l.z == -7 * M, "line: corner 3 free on the other two axes");
        core::I3 b3{6 * M, 2 * M, 4 * M};                                 // already a box
        l = core::lock_third(a, b3, c, T);
        CHECK(l.x == b3.x && l.y == b3.y && l.z == b3.z, "box: corner 3 ignored");
        core::I3 b4{6 * M, 1000, 4 * M};                                  // sub-tolerance wobble on y
        l = core::lock_third(a, b4, c, T);
        CHECK(l.y == 3 * M, "tiny difference does not count as a used axis");
        core::I3 wall[3] = {a, {0, 4 * M, 8 * M}, core::lock_third(a, {0, 4 * M, 8 * M}, {4 * M, 9 * M, 1 * M}, T)};
        float s2[3] = {2.f, 2.f, 2.f};
        CHECK(core::plan_fill(wall, 3, s2, 2000, &pl) && pl.count[1] == 3 && pl.count[2] == 5 && pl.count[0] == 3,
              "wall 4x8 then 4 m thickness -> 3x3x5");
    }

    // ----------------------------------------------------- centre rounding
    CHECK(core::centre_fixed(1.f, 3.f) == 2 * M, "centre of 1..3 m is 2 m");
    CHECK(core::centre_fixed(-3.f, -1.f) == -2 * M, "centre of -3..-1 m is -2 m");

    // ----------------------------------------------------- pattern parser
    uint8_t b[8], m[8];
    CHECK(core::parse_sig("48 ?? 8b ?", b, m, 8) == 4 && m[1] == 0 && m[3] == 0 && b[2] == 0x8b, "wildcards parse");
    CHECK(core::parse_sig("4G", b, m, 8) == -1, "bad hex rejected");

    // ----------------------------------------------------- client -> server state
    {
        const uint32_t s = core::NET_TAG | core::NET_ON | core::NET_AREA | (3u << core::NET_STYLE_SHIFT) |
                           (1u << core::NET_FILL_SHIFT) | (2u << core::NET_COUNT_SHIFT) | (5u << core::NET_SEQ_SHIFT) |
                           ((uint32_t)core::NET_VERSION << core::NET_VERSION_SHIFT);
        int64_t cam[3] = { 1234 * M + 987654321, -77 * M - 5, 3 * M };
        const int64_t orig[3] = { cam[0], cam[1], cam[2] };
        core::net_pack(s, cam);
        bool small = true;
        for (int a = 0; a < 3; a++) if (core::iabs64(cam[a] - orig[a]) >= 0x10000) small = false;
        CHECK(small, "state moves the camera by less than 2^16 units (0.015 mm)");
        uint32_t back = 0;
        CHECK(core::net_unpack(cam, &back) && back == s, "state round trip (negative coordinates too)");
        CHECK(core::net_field(back, core::NET_COUNT_SHIFT, 2) == 2 && core::net_field(back, core::NET_SEQ_SHIFT, 3) == 5 &&
              core::net_field(back, core::NET_STYLE_SHIFT, 2) == 3, "state fields");
        int64_t plain[3] = { orig[0], orig[1], orig[2] };
        CHECK(!core::net_unpack(plain, &back), "camera without mod data is rejected");
        int64_t bad[3] = { cam[0], cam[1], cam[2] ^ 1 };
        CHECK(!core::net_unpack(bad, &back), "damaged state is rejected");
        // random camera positions almost never pass
        uint64_t x = 88172645463325252ull; int pass = 0;
        for (int i = 0; i < 1000000; i++) {
            int64_t r[3];
            for (int a = 0; a < 3; a++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; r[a] = (int64_t)x; }
            if (core::net_unpack(r, &back)) pass++;
        }
        CHECK(pass <= 2, "random camera positions are rejected");
        // server corner index (2 corners)
        CHECK(core::net_corner_index(0, -1, 1, 1, 2) == 0, "first corner");
        CHECK(core::net_corner_index(1, 1, 2, 0, 2) == 1, "final corner");
        CHECK(core::net_corner_index(1, 2, 2, 0, 2) == -1, "repeated click ignored");
        CHECK(core::net_corner_index(0, 1, 2, 0, 2) == 0, "server missed corner 1: starts over");
        CHECK(core::net_corner_index(1, 2, 3, 1, 2) == 0, "client started over (cancel): server starts over");
        CHECK(core::net_corner_index(2, 2, 3, 0, 3) == 2, "3 corners: final");
    }

    {   // exact mode: one click + sizes (shapes past the clicked one)
        const float st[3] = { 0.5f, 0.5f, 0.5f };
        const int64_t H = (int64_t)(0.5 * core::kFix);
        core::I3 p{ 10 * H, 2 * H, -4 * H }, c[2];
        int fa, fs, ra, rs;
        const float fwd[3] = { 0.1f, -0.3f, 0.9f }, right[3] = { -0.99f, 0.f, 0.1f };
        core::exact_facing(fwd, right, &fa, &fs, &ra, &rs);
        CHECK(fa == 2 && fs == 1 && ra == 0 && rs == -1, "exact: facing snapped to +z, right -x");
        core::exact_corners(p, core::EXACT_FRONT_LEFT, 3, 2, 1, fa, fs, ra, rs, st, c);
        CHECK(c[0].x == p.x && c[0].y == p.y && c[0].z == p.z, "exact: front left corner is the click");
        CHECK(c[1].x == p.x - 3 * H && c[1].z == p.z + 2 * H && c[1].y == p.y + H, "exact: front left grows right, away, up");
        core::exact_corners(p, core::EXACT_BACK_RIGHT, 3, 2, -1, fa, fs, ra, rs, st, c);
        CHECK(c[1].x == p.x + 3 * H && c[1].z == p.z - 2 * H && c[1].y == p.y - H, "exact: back right grows left, toward, negative height down");
        core::exact_corners(p, core::EXACT_FRONT_LEFT, -1, 0, 0, fa, fs, ra, rs, st, c);
        CHECK(c[1].x == p.x + H && c[1].z == p.z && c[1].y == p.y, "exact: -1 is one shape the other way");
        core::exact_corners(p, core::EXACT_CENTRE, 4, 3, 0, fa, fs, ra, rs, st, c);
        core::Plan pl = {};
        CHECK(core::plan_area(c, 2, st, 100000, core::FILL_SOLID, core::FillOpts{}, &pl) &&
              pl.count[0] == 5 && pl.count[2] == 4 && pl.count[1] == 1, "exact: centre, 4 / 3 / 0 past = 5 x 4 x 1");
        CHECK(c[0].x == p.x + 2 * H && c[1].x == p.x - 2 * H, "exact: odd width centred on the click");
        core::exact_corners(p, core::EXACT_FRONT_RIGHT, 0, 0, 0, fa, fs, ra, rs, st, c);
        CHECK(c[0].x == c[1].x && c[0].y == c[1].y && c[0].z == c[1].z && c[0].x == p.x, "exact: 0, 0, 0 is the clicked shape");
    }


    // ---- object placement
    {
        using namespace core;
        bool trig = true;
        for (int i = -40; i <= 40; i++) {
            double x = i * 0.37;
            if (fabs(d_sin(x) - sin(x)) > 1e-8 || fabs(d_cos(x) - cos(x)) > 1e-8) trig = false;
            double y = sin(i * 0.9) * 3; double xx = cos(i * 1.3) * 2;
            if (fabs(d_atan2(y, xx) - atan2(y, xx)) > 1e-8) trig = false;
        }
        CHECK(trig, "objects: sin / cos / atan2 without a C runtime");
        const double line[2][3] = { { 10, 5, 20 }, { 20, 7, 20 } };
        ObjOpts o = { OBJ_LINE, 5, 0, 0, 0, OBJ_REF_SLOPE, 0, 1 };
        ObjSpot sp[kObjMax];
        int n = obj_layout(o, line, 2, nullptr, nullptr, sp, kObjMax);
        CHECK(n == 5 && fabs(sp[0].p[0] - 10) < 1e-9 && fabs(sp[4].p[0] - 20) < 1e-9 && fabs(sp[2].p[0] - 15) < 1e-9 &&
              fabs(sp[2].p[1] - 6) < 1e-9 && sp[1].yaw == 0, "objects: line of 5, both ends, slope height");
        o.ref = OBJ_REF_LEVEL;
        n = obj_layout(o, line, 2, nullptr, nullptr, sp, kObjMax);
        CHECK(fabs(sp[4].p[1] - 5) < 1e-9, "objects: level = the first click's height");
        const double circ[2][3] = { { 0, 1, 0 }, { 4, 3, 0 } };
        ObjOpts c = { OBJ_CIRCLE, 4, 0, 0, 0, OBJ_REF_SLOPE, 1, 1 };
        n = obj_layout(c, circ, 2, nullptr, nullptr, sp, kObjMax);
        CHECK(n == 4 && fabs(sp[0].p[0] - 4) < 1e-9 && fabs(sp[1].p[2] - 4) < 1e-9 && fabs(sp[2].p[0] + 4) < 1e-9 &&
              fabs(sp[3].p[1] - 1) < 1e-9, "objects: circle of 4 from the clicked point, level at the centre");
        CHECK(fabs(sp[0].yaw) < 1e-9 && fabs(sp[1].yaw + kPi / 2) < 1e-6 && fabs(fabs(sp[2].yaw) - kPi) < 1e-6,
              "objects: circle, turned with the line");
        const double cur[3][3] = { { 0, 0, 0 }, { 10, 0, 0 }, { 5, 0, 5 } };
        ObjOpts cv = { OBJ_CURVE, 3, 0, 0, 0, OBJ_REF_SLOPE, 0, 1 };
        n = obj_layout(cv, cur, 3, nullptr, nullptr, sp, kObjMax);
        CHECK(n == 3 && fabs(sp[1].p[0] - 5) < 1e-6 && fabs(sp[1].p[2] - 5) < 1e-6 && fabs(sp[2].p[0] - 10) < 1e-9,
              "objects: curve passes through the third click half way");
        {   // arch: half circle up between the clicks; vertical circle: centre + point, standing
            const double ar[3][3] = { { 0, 0, 0 }, { 10, 0, 0 } };
            ObjOpts ao = { OBJ_ARCH, 3, 0, 0, 0, OBJ_REF_GROUND, 1, 1 };
            int na = obj_layout(ao, ar, 2, nullptr, nullptr, sp, kObjMax);
            CHECK(na == 3 && fabs(sp[0].p[0]) < 1e-9 && fabs(sp[1].p[0] - 5) < 1e-6 && fabs(sp[1].p[1] - 5) < 1e-6 &&
                  fabs(sp[2].p[0] - 10) < 1e-6 && fabs(sp[2].p[1]) < 1e-6 && fabs(sp[1].yaw) < 1e-9, "objects: arch over the top, not turned");
            const double vc[3][3] = { { 0, 5, 0 }, { 0, 5, 3 } };
            ObjOpts vo = { OBJ_VCIRCLE, 4, 0, 0, 0, OBJ_REF_LEVEL, 1, 1 };
            int nv = obj_layout(vo, vc, 2, nullptr, nullptr, sp, kObjMax);
            CHECK(nv == 4 && fabs(sp[0].p[2] - 3) < 1e-9 && fabs(sp[1].p[1] - 8) < 1e-6 && fabs(sp[1].p[2]) < 1e-6 &&
                  fabs(sp[2].p[2] + 3) < 1e-6 && fabs(sp[3].p[1] - 2) < 1e-6 && fabs(sp[0].p[0]) < 1e-9, "objects: vertical circle stands");
            double path[300][3];
            int pn = obj_path(vo, vc, 2, nullptr, nullptr, path, 300);
            CHECK(pn >= 49 && fabs(path[pn - 1][2] - 3) < 1e-6 && fabs(path[pn - 1][1] - 5) < 1e-6, "objects: vertical circle path closed");
        }
        ObjOpts r = { OBJ_LINE, 20, 50, 30, 40, OBJ_REF_SLOPE, 0, 77 };
        n = obj_layout(r, line, 2, nullptr, nullptr, sp, kObjMax);
        bool in = true, moved = false;
        for (int i = 0; i < n; i++) {
            double ideal = 10 + 10.0 * i / 19;
            if (fabs(sp[i].p[0] - ideal) > 0.5 * 10.0 / 19 + 1e-9 || sp[i].p[0] < 10 || sp[i].p[0] > 20) in = false;
            if (fabs(sp[i].yaw) > 30 * kPi / 180 + 1e-9) in = false;
            double h = 5 + 2 * (sp[i].p[0] - 10) / 10;   // slope height at its x
            if (fabs(sp[i].p[1] - h) > 0.4 + 1e-9) in = false;
            if (fabs(sp[i].p[0] - ideal) > 1e-3) moved = true;
        }
        ObjSpot sp2[kObjMax];
        obj_layout(r, line, 2, nullptr, nullptr, sp2, kObjMax);
        CHECK(in && moved && !memcmp(sp, sp2, sizeof(ObjSpot) * n), "objects: random offsets within the limits, same seed same spots");
        double path[64][3];
        int pn = obj_path(c, circ, 2, nullptr, nullptr, path, 64);
        CHECK(pn >= 48 && fabs(path[0][0] - path[pn - 1][0]) < 1e-6 && fabs(path[0][2] - path[pn - 1][2]) < 1e-6, "objects: circle path closed");
        auto flat = [](void*, double, double, double, double* y) { *y = 2.5; return true; };
        o.ref = OBJ_REF_GROUND;
        n = obj_layout(o, line, 2, flat, nullptr, sp, kObjMax);
        pn = obj_path(o, line, 2, flat, nullptr, path, 64);
        CHECK(fabs(sp[3].p[1] - 2.5) < 1e-9 && pn >= 21 && pn <= 23 && fabs(path[7][1] - 2.5) < 1e-9, "objects: ground reference, path every 0.5 m");
        double y = 0;
        CHECK(obj_column_top(0x0Full | (0x3ull << 10), 48, 100, 50.0, &y) && fabs(y - 52.0) < 1e-9 &&
              obj_column_top(0x0Full | (0x3ull << 10), 48, 100, 55.0, &y) && fabs(y - 56.0) < 1e-9, "objects: ground = nearest surface top");
        float q[4] = { 0, 0, 0, 1 }, t[4];
        obj_turn(q, kPi / 2, t);
        CHECK(fabs(t[1] - sin(kPi / 4)) < 1e-6 && fabs(t[3] - cos(kPi / 4)) < 1e-6, "objects: turn about up");
    }
    {   // exact rotation and nudge of the object in hand
        using namespace core;
        const int64_t p0[3] = { 100 * M, 50 * M, -20 * M };
        const float id[4] = { 0, 0, 0, 1 };
        int64_t p[3]; float r[4];
        PropXf x = {};
        prop_apply(x, p0, id, p, r);
        CHECK(!memcmp(p, p0, sizeof p) && r[0] == 0 && r[1] == 0 && r[2] == 0 && r[3] == 1 && !prop_xf_on(x), "props: nothing set = the held transform");
        x.turn = 90;
        prop_apply(x, p0, id, p, r);
        float w[4]; obj_turn(id, kPi / 2, w);
        CHECK(fabs(r[1] - w[1]) < 1e-6 && fabs(r[3] - w[3]) < 1e-6 && fabs(r[0]) < 1e-6 && fabs(r[2]) < 1e-6, "props: turn 90 about up, as the line turn");
        auto axis = [](const float* q, double x, double y, double z, double* out) {
            const double qd[4] = { q[0], q[1], q[2], q[3] }, v[3] = { x, y, z };
            quat_apply(qd, v, out);
        };
        double a[3], b[3];
        x = {}; x.turn = 90; x.tilt = 90;
        prop_apply(x, p0, id, p, r);
        axis(r, 0, 1, 0, a);
        CHECK(fabs(a[0] - 1) < 1e-6 && fabs(a[1]) < 1e-6 && fabs(a[2]) < 1e-6, "props: tilt about the turned object's x (up ends on +x)");
        const float wall[4] = { (float)sin(kPi / 4), 0, 0, (float)cos(kPi / 4) };   // on a wall: up = +z
        x = {}; x.turn = 90;
        prop_apply(x, p0, wall, p, r);
        axis(r, 0, 1, 0, a); axis(r, 1, 0, 0, b);
        CHECK(fabs(a[2] - 1) < 1e-6 && fabs(b[1] - 1) < 1e-6, "props: turn about the object's own up (object on a wall)");
        x = {}; x.nudge[0] = 100;
        float t90[4]; obj_turn(id, kPi / 2, t90);
        prop_apply(x, p0, t90, p, r);
        bool local = llabs(p[0] - p0[0]) < 16 && llabs(p[1] - p0[1]) < 16 && llabs(p[2] - (p0[2] - M)) < 16;
        x.world = 1;
        prop_apply(x, p0, t90, p, r);
        bool world = p[0] == p0[0] + M && p[1] == p0[1] && p[2] == p0[2];
        CHECK(local && world, "props: nudge along the object's axes or the world's");
        x = {}; x.nudge[1] = 5; x.nudge[2] = -5; x.world = 1;
        prop_apply(x, p0, id, p, r);
        const int64_t cm5 = (int64_t)(0.05 * kFix + 0.5);
        CHECK(p[1] == p0[1] + cm5 && p[2] == p0[2] - cm5, "props: nudge in cm, rounded to the fixed point");
        const float big[4] = { 0, 0, 0, 2 };
        x = {}; x.roll = 30;
        prop_apply(x, p0, big, p, r);
        CHECK(fabs(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3] - 1) < 1e-6 && fabs(r[2] - sin(kPi / 12)) < 1e-6, "props: roll about z, unit result");
        CHECK(prop_wrap_deg(195) == -165 && prop_wrap_deg(-180) == 180 && prop_wrap_deg(180) == 180 && prop_wrap_deg(-195) == 165 &&
              prop_wrap_deg(0) == 0, "props: degrees kept in -179..180");
    }

    // ---------------------------------------------------------------- blueprints
    {
        using namespace core;
        BpHeader h = {};
        h.step[0] = h.step[1] = h.step[2] = 0.5f; h.n[0] = 3; h.n[1] = 2; h.n[2] = 4; h.shape = 0x1234; h.nm = 2;
        h.m[0] = BpMat{ 0xaabbccdd, 0x81, 5, 10 }; h.m[1] = BpMat{ 0x11223344, 0x90, 2, 4 };
        uint8_t cells[24]; for (int q = 0; q < 24; q++) cells[q] = BP_EMPTY;
        cells[bp_index(h.n, 0, 0, 0)] = 0; cells[bp_index(h.n, 1, 0, 0)] = 0; cells[bp_index(h.n, 2, 1, 3)] = 1;
        uint8_t buf[512]; int sz = bp_encode(h, cells, buf, sizeof buf);
        BpHeader g = {}; uint8_t back[24];
        bool ok = sz > 0 && bp_decode(buf, sz, &g, back, 24);
        CHECK(ok && g.n[0] == 3 && g.n[1] == 2 && g.n[2] == 4 && g.nm == 2 && g.m[1].item == 0x11223344 && g.m[0].mat == 0x81 &&
              memcmp(back, cells, 24) == 0 && g.shape == 0x1234 && g.step[1] == 0.5f, "blueprint: file round trip");
        CHECK(!bp_decode(buf, sz, &g, back, 23), "blueprint: too many cells refused");
        CHECK(!bp_decode(buf, sz - 6, &g, back, 24), "blueprint: cut-off file refused");
        CHECK(bp_encode(h, cells, buf, 20) == 0, "blueprint: small buffer -> 0");
        sz = bp_encode(h, cells, buf, sizeof buf);
        buf[4] = 2; CHECK(!bp_decode(buf, sz, &g, back, 24), "blueprint: newer version refused");
        bool turns = true;
        for (int r = 0; r < 4; r++)
            for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) {
                int ri, rk, i2, k2; bp_turn(r, i, k, 3, 4, &ri, &rk); bp_unturn(r, ri, rk, 3, 4, &i2, &k2);
                int rnx = r & 1 ? 4 : 3, rnz = r & 1 ? 3 : 4;
                if (i2 != i || k2 != k || ri < 0 || rk < 0 || ri >= rnx || rk >= rnz) turns = false;
            }
        int ri, rk; bp_turn(1, 0, 0, 3, 4, &ri, &rk);
        CHECK(turns && ri == 3 && rk == 0, "blueprint: quarter turns stay in the plan and undo");
        // objects in the file
        static BpHeader ho; ho = h; ho.objects = 2;
        ho.obj[0] = BpObject{ 0x2a7992b7u, { 1.25f, 0.5f, 2.f }, { 0.f, 0.7071f, 0.f, 0.7071f } };
        ho.obj[1] = BpObject{ 0xcc076a8u, { 0.f, 1.f, 0.5f }, { 0.f, 0.f, 0.f, 1.f } };
        static uint8_t ob[1024]; int osz = bp_encode(ho, cells, ob, sizeof ob);
        static BpHeader go; uint8_t gc[24];
        CHECK(osz > 0 && bp_decode(ob, osz, &go, gc, 24) && go.objects == 2 && go.obj[0].item == 0x2a7992b7u &&
              go.obj[1].pos[2] == 0.5f && go.obj[0].rot[1] == 0.7071f, "blueprint: objects round trip");
        // points and rotations turn like the cells: a point near cell (0, 0) of a 3 x 4 cell box (1.5 x 2 m) at turn 1
        float pt[3] = { 0.25f, 0.f, 0.25f }, tp[3];
        bp_turn_point(1, pt, 1.5f, 2.f, tp);
        int cri, crk; bp_turn(1, 0, 0, 3, 4, &cri, &crk);
        CHECK(fabs(tp[0] - (cri * 0.5 + 0.25)) < 1e-6 && fabs(tp[2] - (crk * 0.5 + 0.25)) < 1e-6, "blueprint: object points turn with the cells");
        // four turns of an object's rotation give it back (up to sign), one turn is -90 degrees about y
        float q0[4] = { 0.f, 0.f, 0.f, 1.f }, q1[4], q2[4];
        bp_turn_rot(1, q0, q1);
        CHECK(fabs(q1[1] + 0.70710678f) < 1e-5 && fabs(q1[3] - 0.70710678f) < 1e-5, "blueprint: object yaw per turn");
        bp_turn_rot(2, q0, q2);
        CHECK(fabs(fabs(q2[1]) - 1.f) < 1e-5, "blueprint: half turn");
    }

    printf(fails ? "\n%d FAILED\n" : "\nall tests passed\n", fails);
    return fails ? 1 : 0;
}
