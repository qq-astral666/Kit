#pragma once

// Geometry of the desktop as the fox sees it: screens, other apps' windows
// and the walkable "platforms" derived from them. Pure C++, no Qt, no OS.
//
// Coordinates are global desktop points with y pointing down (the same space
// Qt and CGWindowList use), so a platform's y is the line the fox stands on.

#include <cstdint>
#include <vector>

namespace kit {

struct Vec2 {
    double x = 0;
    double y = 0;

    Vec2 operator+(Vec2 o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(Vec2 o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(double k) const { return { x * k, y * k }; }
    double length() const;
};

struct Rect {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;

    double right() const { return x + w; }
    double bottom() const { return y + h; }
    bool contains(Vec2 p) const { return p.x >= x && p.x < right() && p.y >= y && p.y < bottom(); }
};

struct Screen {
    Rect frame;           // whole display
    Rect available;       // minus menu bar and Dock
    Rect dock;            // the Dock bar on this screen, if hasDock
    bool hasDock = false;
};

// Platform id of the top of the Dock bar.
constexpr std::uint64_t kDockId = ~std::uint64_t(0);

// Another app's window. Lists are ordered front to back.
struct WindowInfo {
    std::uint64_t id = 0;
    Rect frame;
};

// A horizontal line the fox can stand on. windowId 0 = a screen's floor.
struct Segment {
    double x0 = 0;
    double x1 = 0;
    double y = 0;
    std::uint64_t windowId = 0;

    double length() const { return x1 - x0; }
    bool spans(double x, double slack = 0) const { return x >= x0 - slack && x <= x1 + slack; }
};

struct PlatformOptions {
    double minLength = 40;     // shorter visible pieces are ignored
    double minTopGap = 60;     // edges this close to the menu bar are ignored
};

// Floors of every screen (the real bottom edge, running behind the Dock),
// the top of the Dock bar when known, plus the *visible* parts of every window's top edge:
// a piece of edge covered by a window in front of it is cut out.
std::vector<Segment> buildPlatforms(const std::vector<WindowInfo>& windowsFrontToBack,
                                    const std::vector<Screen>& screens,
                                    const PlatformOptions& options = {});

// Ballistic jump from `from` to `to` that peaks `apexAbove` points above the
// higher of the two. y grows downwards, gravity is positive.
struct JumpPlan {
    Vec2 velocity;
    double duration = 0;
    bool ok = false;
};
JumpPlan planJump(Vec2 from, Vec2 to, double gravity, double apexAbove, double maxSpeedX = 900);

} // namespace kit
