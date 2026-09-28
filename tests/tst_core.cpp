// Unit tests for the fox's core (platforms, jumps, behaviour).
// Dependency-free on purpose: the core is plain C++20.

#include "PetSim.h"
#include "World.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace kit;

namespace {

int g_failed = 0;
int g_checks = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failed;                                                               \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                             \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::abs((a) - (b)) <= (eps))

struct Test {
    const char* name;
    std::function<void()> fn;
};
std::vector<Test>& registry()
{
    static std::vector<Test> tests;
    return tests;
}
struct Register {
    Register(const char* name, std::function<void()> fn) { registry().push_back({ name, std::move(fn) }); }
};
#define TEST(name)                                     \
    static void name();                                \
    static Register reg_##name(#name, name);           \
    static void name()

// A 1440x900 laptop screen: 33 pt menu bar, no Dock.
Screen laptop()
{
    Screen s;
    s.frame = { 0, 0, 1440, 900 };
    s.available = { 0, 33, 1440, 867 };
    return s;
}

const Segment* find(const std::vector<Segment>& segs, std::uint64_t id)
{
    for (const Segment& s : segs)
        if (s.windowId == id)
            return &s;
    return nullptr;
}

// Runs the simulation with a fixed cursor for `seconds`.
void run(PetSim& sim, double seconds, Input in = {}, const std::function<void(PetSim&)>& each = {})
{
    in.dt = 1.0 / 60;
    for (int i = 0; i < int(seconds * 60); ++i) {
        sim.step(in);
        if (each)
            each(sim);
    }
}

Input farCursor()
{
    Input in;
    in.cursor = { 5000, 5000 };
    return in;
}

} // namespace

// ------------------------------------------------------------- platforms --

TEST(floorIsAlwaysAPlatform)
{
    const auto segs = buildPlatforms({}, { laptop() });
    CHECK(segs.size() == 1);
    CHECK(segs[0].windowId == 0);
    CHECK_NEAR(segs[0].y, 900, 1e-9);
    CHECK_NEAR(segs[0].length(), 1440, 1e-9);
}

TEST(floorIsTheRealScreenBottomEvenWithADock)
{
    Screen s = laptop();
    s.available = { 0, 33, 1440, 797 };   // Dock reserves 70 pt, extent unknown
    const auto segs = buildPlatforms({}, { s });
    CHECK(segs.size() == 1);
    CHECK_NEAR(segs[0].y, 900, 1e-9);
    CHECK_NEAR(segs[0].length(), 1440, 1e-9);
}

TEST(knownDockIsAPlatformAboveTheFloor)
{
    Screen s = laptop();
    s.available = { 0, 33, 1440, 797 };
    s.dock = { 300, 836, 840, 60 };        // bar floats a few points above the edge
    s.hasDock = true;
    const auto segs = buildPlatforms({}, { s });
    CHECK(segs.size() == 2);
    const Segment* floor = find(segs, 0);
    CHECK(floor && std::abs(floor->y - 900) < 1e-9 && std::abs(floor->length() - 1440) < 1e-9);
    const Segment* dock = find(segs, kDockId);
    CHECK(dock != nullptr);
    if (dock) {
        CHECK_NEAR(dock->y, 836, 1e-9);
        CHECK_NEAR(dock->length(), 840, 1e-9);
    }
}

TEST(walksOntoTheDockFromTheFloor)
{
    // A fox on the floor beside the Dock eventually gets onto it.
    Screen s = laptop();
    s.dock = { 300, 836, 840, 60 };
    s.hasDock = true;
    PetSim sim(11);
    sim.setWorld({ s }, {});
    sim.spawn({ 150, 850 });
    bool onDock = false;
    run(sim, 180, farCursor(), [&](PetSim& p) { onDock = onDock || (p.grounded() && p.standingOn() == kDockId); });
    CHECK(onDock);
}

TEST(windowTopBecomesPlatform)
{
    const auto segs = buildPlatforms({ { 7, { 100, 300, 500, 400 } } }, { laptop() });
    const Segment* s = find(segs, 7);
    CHECK(s != nullptr);
    if (s) {
        CHECK_NEAR(s->x0, 100, 1e-9);
        CHECK_NEAR(s->x1, 600, 1e-9);
        CHECK_NEAR(s->y, 300, 1e-9);
    }
}

TEST(windowInFrontHidesPartOfEdge)
{
    // Window 2 (behind) has its top edge at y=300; window 1 in front covers
    // x 250..400 at that height. Expect two visible pieces.
    const std::vector<WindowInfo> windows {
        { 1, { 250, 200, 150, 300 } },
        { 2, { 100, 300, 500, 400 } },
    };
    const auto segs = buildPlatforms(windows, { laptop() });
    std::vector<Segment> pieces;
    for (const Segment& s : segs)
        if (s.windowId == 2)
            pieces.push_back(s);
    CHECK(pieces.size() == 2);
    if (pieces.size() == 2) {
        CHECK_NEAR(pieces[0].x0, 100, 1e-9);
        CHECK_NEAR(pieces[0].x1, 250, 1e-9);
        CHECK_NEAR(pieces[1].x0, 400, 1e-9);
        CHECK_NEAR(pieces[1].x1, 600, 1e-9);
    }
    // The front window's own edge is fully visible.
    CHECK(find(segs, 1) != nullptr);
}

TEST(edgesUnderMenuBarAndTinyPiecesAreIgnored)
{
    PlatformOptions options;
    options.minTopGap = 60;
    options.minLength = 40;
    const std::vector<WindowInfo> windows {
        { 1, { 0, 33, 1440, 867 } },     // maximised: edge right under the menu bar
        { 2, { 500, 400, 30, 100 } },    // too narrow
    };
    const auto segs = buildPlatforms(windows, { laptop() }, options);
    CHECK(find(segs, 1) == nullptr);
    CHECK(find(segs, 2) == nullptr);
}

TEST(offscreenWindowsAreIgnored)
{
    const auto segs = buildPlatforms({ { 3, { 3000, 400, 300, 100 } } }, { laptop() });
    CHECK(find(segs, 3) == nullptr);
}

// ------------------------------------------------------------------ jump --

TEST(jumpPlanHitsTarget)
{
    const double g = 2600;
    const Vec2 from { 100, 800 };
    for (const Vec2 to : { Vec2 { 300, 600 }, Vec2 { 50, 900 }, Vec2 { 400, 800 } }) {
        const JumpPlan plan = planJump(from, to, g, 40);
        CHECK(plan.ok);
        const double t = plan.duration;
        const double x = from.x + plan.velocity.x * t;
        const double y = from.y + plan.velocity.y * t + 0.5 * g * t * t;
        CHECK_NEAR(x, to.x, 1e-6);
        CHECK_NEAR(y, to.y, 1e-6);
        CHECK(plan.velocity.y < 0);
    }
}

TEST(jumpPlanRejectsTooFast)
{
    CHECK(!planJump({ 0, 0 }, { 5000, 0 }, 2600, 10, 900).ok);
}

// ------------------------------------------------------------ simulation --

TEST(fallsAndLandsOnFloor)
{
    PetSim sim(1);
    sim.setWorld({ laptop() }, {});
    sim.spawn({ 700, 200 });
    CHECK(!sim.grounded());
    run(sim, 2.0, farCursor());
    CHECK(sim.grounded());
    CHECK_NEAR(sim.position().y, 900, 0.01);
    CHECK(sim.standingOn() == 0);
}

TEST(landsOnWindowTop)
{
    PetSim sim(2);
    sim.setWorld({ laptop() }, { { 42, { 500, 500, 400, 300 } } });
    sim.spawn({ 700, 150 });
    bool landedOnWindow = false;
    run(sim, 1.0, farCursor(), [&](PetSim& p) {
        if (p.grounded() && !landedOnWindow) {
            landedOnWindow = p.standingOn() == 42;
            CHECK_NEAR(p.position().y, 500, 0.01);
        }
    });
    CHECK(landedOnWindow);
}

TEST(ridesAMovingWindow)
{
    PetSim sim(3);
    sim.setWorld({ laptop() }, { { 5, { 400, 500, 600, 300 } } });
    sim.spawn({ 700, 400 });
    run(sim, 0.6, farCursor());
    CHECK(sim.standingOn() == 5);
    sim.forceActivity(Activity::Sit, 100);   // keep still
    const Vec2 before = sim.position();
    sim.setWorld({ laptop() }, { { 5, { 460, 470, 600, 300 } } });
    run(sim, 0.05, farCursor());
    CHECK_NEAR(sim.position().x, before.x + 60, 0.01);
    CHECK_NEAR(sim.position().y, 470, 0.01);
    CHECK(sim.standingOn() == 5);
}

TEST(fallsWhenItsWindowCloses)
{
    PetSim sim(4);
    sim.setWorld({ laptop() }, { { 9, { 400, 500, 600, 300 } } });
    sim.spawn({ 700, 400 });
    run(sim, 0.6, farCursor());
    CHECK(sim.standingOn() == 9);
    sim.setWorld({ laptop() }, {});
    run(sim, 0.05, farCursor());
    CHECK(!sim.grounded() || sim.position().y > 500);
    run(sim, 2.0, farCursor());
    CHECK(sim.grounded());
    CHECK_NEAR(sim.position().y, 900, 0.01);
}

TEST(dragAndThrow)
{
    PetSim sim(5);
    sim.setWorld({ laptop() }, {});
    sim.spawn({ 700, 890 });
    run(sim, 1.0, farCursor());

    Input in;
    in.dt = 1.0 / 60;
    in.cursor = sim.position() + sim.scruffOffset();
    in.cursorOverPet = true;
    sim.press(in.cursor);
    sim.step(in);
    CHECK(!sim.dragging());   // no movement yet → not a drag

    for (int i = 0; i < 20; ++i) {
        in.cursor.x += 15;    // 900 pt/s to the right
        in.cursor.y -= 10;
        sim.step(in);
    }
    CHECK(sim.dragging());
    CHECK(sim.activity() == Activity::Dragged);
    CHECK_NEAR(sim.position().x + sim.scruffOffset().x, in.cursor.x, 0.01);

    sim.release();
    CHECK(!sim.dragging());
    CHECK(sim.activity() == Activity::Fall);
    CHECK(sim.velocity().x > 500);
    CHECK(sim.velocity().y < -200);
    run(sim, 3.0, farCursor());
    CHECK(sim.grounded());
}

TEST(clickTriggersAReaction)
{
    int reactions = 0;
    for (std::uint32_t seed = 1; seed <= 20; ++seed) {
        PetSim sim(seed);
        sim.setWorld({ laptop() }, {});
        sim.spawn({ 700, 890 });
        run(sim, 1.0, farCursor());
        sim.forceActivity(Activity::Idle, 100);
        sim.press(sim.position());
        sim.release();
        const Activity a = sim.activity();
        if (a == Activity::Happy || a == Activity::Flip || a == Activity::ChaseTail || a == Activity::Wag)
            ++reactions;
    }
    CHECK(reactions == 20);
}

TEST(sleepsWhenUserIsAwayAndWakesOnReturn)
{
    PetSim sim(6);
    sim.setWorld({ laptop() }, {});
    sim.spawn({ 700, 890 });
    Input in = farCursor();
    run(sim, 1.0, in);
    sim.forceActivity(Activity::Idle, 100);
    in.idleSeconds = 60;
    run(sim, 0.1, in);
    CHECK(sim.activity() == Activity::Sleep);
    run(sim, 5.0, in);
    CHECK(sim.activity() == Activity::Sleep);
    in.idleSeconds = 0;
    run(sim, 0.05, in);
    CHECK(sim.activity() == Activity::Wake);
}

TEST(pettingMakesItHappy)
{
    PetSim sim(7);
    sim.setWorld({ laptop() }, {});
    sim.spawn({ 700, 890 });
    run(sim, 1.0, farCursor());
    sim.forceActivity(Activity::Sit, 100);

    Input in;
    in.dt = 1.0 / 60;
    in.cursorOverPet = true;
    const Vec2 base = sim.position() + Vec2 { 0, -30 };
    bool petted = false;
    for (int i = 0; i < 120 && !petted; ++i) {
        in.cursor = base + Vec2 { 25 * std::sin(i * 0.5), 0 };   // stroke back and forth
        sim.step(in);
        petted = sim.activity() == Activity::Petted;
    }
    CHECK(petted);
}

TEST(fastCursorStartles)
{
    PetSim sim(8);
    sim.setWorld({ laptop() }, {});
    sim.spawn({ 700, 890 });
    run(sim, 1.0, farCursor());
    sim.forceActivity(Activity::Idle, 100);
    Input in;
    in.dt = 1.0 / 60;
    in.cursor = { 500, 860 };
    sim.step(in);
    bool startled = false;
    for (int i = 0; i < 12 && !startled; ++i) {
        in.cursor.x += 70;    // 4200 pt/s straight past its nose
        sim.step(in);
        startled = sim.activity() == Activity::Startled;
    }
    CHECK(startled);
}

TEST(longRandomRunStaysSane)
{
    // Ten simulated minutes of random windows and cursor: the fox must stay
    // on screen, never produce NaN and spend most of its time on the ground.
    PetSim sim(99);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> u(0, 1);
    auto randomWindows = [&] {
        std::vector<WindowInfo> w;
        const int n = int(u(rng) * 6);
        for (int i = 0; i < n; ++i)
            w.push_back({ std::uint64_t(i + 1), { u(rng) * 1200, 100 + u(rng) * 600, 150 + u(rng) * 500, 100 + u(rng) * 300 } });
        return w;
    };
    sim.setWorld({ laptop() }, randomWindows());
    sim.spawn({ 700, 300 });

    Input in;
    in.dt = 1.0 / 60;
    in.cursor = { 700, 500 };
    int groundedFrames = 0;
    const int frames = 10 * 60 * 60;
    for (int i = 0; i < frames; ++i) {
        if (i % 600 == 0)
            sim.setWorld({ laptop() }, randomWindows());
        if (i % 90 == 0)
            in.cursor = { u(rng) * 1440, u(rng) * 900 };
        in.idleSeconds = (i / 3600) % 3 == 2 ? 100 : 0;
        sim.step(in);
        const Vec2 p = sim.position();
        CHECK(std::isfinite(p.x) && std::isfinite(p.y));
        CHECK(p.x >= 0 && p.x <= 1440);
        CHECK(p.y <= 900.01);
        if (sim.grounded())
            ++groundedFrames;
        if (g_failed > 20)
            break;
    }
    CHECK(groundedFrames > frames * 0.7);
}

int main()
{
    for (const Test& t : registry()) {
        const int before = g_failed;
        t.fn();
        std::printf("%s %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
