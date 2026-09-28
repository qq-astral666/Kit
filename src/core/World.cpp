#include "World.h"

#include <algorithm>
#include <cmath>

namespace kit {

double Vec2::length() const
{
    return std::hypot(x, y);
}

namespace {

struct Interval {
    double a;
    double b;
};

// Removes [cut.a, cut.b) from every interval in `pieces`.
void subtract(std::vector<Interval>& pieces, Interval cut)
{
    std::vector<Interval> out;
    out.reserve(pieces.size() + 1);
    for (const Interval& p : pieces) {
        if (cut.b <= p.a || cut.a >= p.b) {
            out.push_back(p);
            continue;
        }
        if (cut.a > p.a)
            out.push_back({ p.a, cut.a });
        if (cut.b < p.b)
            out.push_back({ cut.b, p.b });
    }
    pieces.swap(out);
}

const Screen* screenAt(const std::vector<Screen>& screens, double x, double y)
{
    for (const Screen& s : screens)
        if (x >= s.frame.x && x < s.frame.right() && y >= s.frame.y && y < s.frame.bottom())
            return &s;
    return nullptr;
}

} // namespace

std::vector<Segment> buildPlatforms(const std::vector<WindowInfo>& windows,
                                    const std::vector<Screen>& screens,
                                    const PlatformOptions& options)
{
    std::vector<Segment> result;

    for (const Screen& s : screens) {
        // The floor runs along the whole bottom edge (behind the Dock, too).
        result.push_back({ s.frame.x, s.frame.right(), s.frame.bottom(), 0 });
        const bool dockAtBottom = s.hasDock && s.dock.w < s.frame.w * 0.97 && s.dock.h < s.frame.h * 0.4
            && s.dock.bottom() > s.frame.bottom() - 30 && s.dock.bottom() <= s.frame.bottom() + 1;
        if (dockAtBottom && s.dock.w >= options.minLength)
            result.push_back({ s.dock.x, s.dock.right(), s.dock.y, kDockId });
    }

    for (std::size_t i = 0; i < windows.size(); ++i) {
        const Rect& r = windows[i].frame;
        const double y = r.y;

        // The edge must be on a screen, below the menu bar with some room
        // for the fox's head, and above the floor.
        const Screen* screen = screenAt(screens, std::clamp(r.x + r.w / 2, r.x, r.right() - 1), y);
        if (!screen)
            continue;
        const Rect& av = screen->available;
        if (y < av.y + options.minTopGap || y >= av.bottom() - 1)
            continue;

        std::vector<Interval> pieces { { std::max(r.x, av.x), std::min(r.right(), av.right()) } };
        if (pieces.front().b - pieces.front().a < options.minLength)
            continue;

        // Windows in front that cover the edge line hide that part of it.
        for (std::size_t j = 0; j < i && !pieces.empty(); ++j) {
            const Rect& f = windows[j].frame;
            if (f.y <= y && f.bottom() > y)
                subtract(pieces, { f.x, f.right() });
        }
        for (const Interval& p : pieces)
            if (p.b - p.a >= options.minLength)
                result.push_back({ p.a, p.b, y, windows[i].id });
    }
    return result;
}

JumpPlan planJump(Vec2 from, Vec2 to, double gravity, double apexAbove, double maxSpeedX)
{
    JumpPlan plan;
    if (gravity <= 0)
        return plan;
    const double apexY = std::min(from.y, to.y) - std::max(0.0, apexAbove);
    const double rise = from.y - apexY;   // >= 0
    const double drop = to.y - apexY;     // >= 0
    const double tUp = std::sqrt(2 * rise / gravity);
    const double tDown = std::sqrt(2 * drop / gravity);
    plan.duration = tUp + tDown;
    if (plan.duration <= 1e-6)
        return plan;
    plan.velocity = { (to.x - from.x) / plan.duration, -gravity * tUp };
    plan.ok = std::abs(plan.velocity.x) <= maxSpeedX;
    return plan;
}

} // namespace kit
