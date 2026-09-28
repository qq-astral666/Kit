#include "PetSim.h"

#include <algorithm>
#include <cmath>

namespace kit {

namespace {

constexpr double kPi = 3.14159265358979323846;

double approach(double value, double target, double rate, double dt)
{
    return value + (target - value) * (1.0 - std::exp(-rate * dt));
}

int signOf(double v)
{
    return v < 0 ? -1 : 1;
}

} // namespace

const char* toString(Activity a)
{
    switch (a) {
    case Activity::Idle: return "Idle";
    case Activity::Walk: return "Walk";
    case Activity::Run: return "Run";
    case Activity::Sit: return "Sit";
    case Activity::LookAround: return "LookAround";
    case Activity::Sleep: return "Sleep";
    case Activity::Wake: return "Wake";
    case Activity::Crouch: return "Crouch";
    case Activity::Jump: return "Jump";
    case Activity::Pounce: return "Pounce";
    case Activity::Fall: return "Fall";
    case Activity::Land: return "Land";
    case Activity::Dizzy: return "Dizzy";
    case Activity::Dragged: return "Dragged";
    case Activity::Happy: return "Happy";
    case Activity::Flip: return "Flip";
    case Activity::ChaseTail: return "ChaseTail";
    case Activity::Wag: return "Wag";
    case Activity::Petted: return "Petted";
    case Activity::Startled: return "Startled";
    }
    return "?";
}

PetSim::PetSim(std::uint32_t seed)
    : m_rng(seed)
{
}

void PetSim::setTuning(const Tuning& tuning)
{
    m_tuning = tuning;
    setWorld(std::move(m_screens), std::move(m_windows));
}

double PetSim::rand(double lo, double hi)
{
    return std::uniform_real_distribution<double>(lo, hi)(m_rng);
}

bool PetSim::chance(double p)
{
    return rand(0.0, 1.0) < p;
}

void PetSim::setActivity(Activity a, double duration)
{
    m_act = a;
    m_actTime = 0;
    m_actDuration = duration;
    if (a != Activity::Walk)
        m_hasWalkTarget = false;
}

void PetSim::forceActivity(Activity a, double duration)
{
    setActivity(a, duration);
}

bool PetSim::isCalm() const
{
    switch (m_act) {
    case Activity::Idle:
    case Activity::Walk:
    case Activity::Run:
    case Activity::Sit:
    case Activity::LookAround:
    case Activity::Wag:
        return true;
    default:
        return false;
    }
}

// ------------------------------------------------------------------ world --

void PetSim::setWorld(std::vector<Screen> screens, std::vector<WindowInfo> windows)
{
    // Ride along with the window we stand on.
    if (m_grounded && m_standId != 0) {
        auto byId = [id = m_standId](const WindowInfo& w) { return w.id == id; };
        const auto before = std::find_if(m_windows.begin(), m_windows.end(), byId);
        const auto after = std::find_if(windows.begin(), windows.end(), byId);
        if (before != m_windows.end() && after != windows.end()) {
            const Vec2 delta { after->frame.x - before->frame.x, after->frame.y - before->frame.y };
            if (delta.length() > 0 && delta.length() < 900)
                m_pos = m_pos + delta;
        }
    }

    m_screens = std::move(screens);
    m_windows = std::move(windows);

    PlatformOptions options;
    options.minLength = 44 * s();
    options.minTopGap = m_tuning.height * s() + 10;
    m_platforms = buildPlatforms(m_windows, m_screens, options);
}

const Segment* PetSim::segmentUnder(Vec2 p, std::uint64_t preferId) const
{
    const Segment* best = nullptr;
    for (const Segment& seg : m_platforms) {
        if (std::abs(seg.y - p.y) > 2.5 || !seg.spans(p.x, 1.0))
            continue;
        if (seg.windowId == preferId)
            return &seg;
        if (!best || seg.y < best->y)
            best = &seg;
    }
    return best;
}

const Screen* PetSim::screenFor(Vec2 p) const
{
    const Screen* nearest = nullptr;
    double bestDistance = 1e18;
    for (const Screen& screen : m_screens) {
        const Rect& f = screen.frame;
        if (f.contains({ p.x, p.y - 1 }))
            return &screen;
        const double dx = std::max({ f.x - p.x, 0.0, p.x - f.right() });
        const double dy = std::max({ f.y - p.y, 0.0, p.y - f.bottom() });
        const double d = dx * dx + dy * dy;
        if (d < bestDistance) {
            bestDistance = d;
            nearest = &screen;
        }
    }
    return nearest;
}

void PetSim::spawn(Vec2 feet)
{
    m_pos = feet;
    m_vel = {};
    m_grounded = false;
    m_standId = 0;
    m_dragging = false;
    m_pressed = false;
    setActivity(Activity::Fall, 0);
}

// ------------------------------------------------------------------- step --

void PetSim::step(const Input& in)
{
    const double dt = std::clamp(in.dt, 0.0, 0.05);
    if (dt <= 0 || m_screens.empty())
        return;

    m_actTime += dt;
    m_startleCooldown -= dt;
    m_clickCooldown -= dt;
    if (m_act != Activity::Sleep)
        m_awake += dt;

    trackCursor(in, dt);
    updateLook(dt);

    if (m_pressed && !m_dragging && (in.cursor - m_pressCursor).length() > 5) {
        m_dragging = true;
        m_grounded = false;
        m_standId = 0;
        m_dragVel = {};
        m_swing = 0;
        m_swingVel = 0;
        m_pending.valid = false;
        setActivity(Activity::Dragged, 0);
    }

    if (m_dragging) {
        updateDrag(dt);
    } else {
        if (m_grounded)
            updateGround(dt);
        else
            integrateAir(dt);
        if (m_grounded)
            think(in, dt);
    }
    updateSquash(dt);
    ensureOnScreen();
}

void PetSim::trackCursor(const Input& in, double dt)
{
    if (!m_haveCursor) {
        m_lastCursor = in.cursor;
        m_haveCursor = true;
    }
    const double speed = (in.cursor - m_lastCursor).length() / dt;
    m_cursorSpeed = approach(m_cursorSpeed, speed, 14, dt);
    m_lastCursor = in.cursor;
    m_cursor = in.cursor;
    m_cursorOver = in.cursorOverPet;
    m_cursorStill = m_cursorSpeed < 25 ? m_cursorStill + dt : 0;

    // Stroking = moving the cursor back and forth over the fox.
    if (in.cursorOverPet && !m_pressed && speed > 40 && speed < 2500)
        m_petAccum += speed * dt;
    else
        m_petAccum = std::max(0.0, m_petAccum - 320 * dt);
}

void PetSim::updateLook(double dt)
{
    const Vec2 head = m_pos + Vec2 { m_facing * 30 * s(), -48 * s() };
    const Vec2 d = m_cursor - head;
    const double len = d.length();
    Vec2 target;
    if (len > 1) {
        const double k = std::min(1.0, len / (160 * s())) / len;
        target = { d.x * k * m_facing, d.y * k };
    }
    m_look.x = approach(m_look.x, target.x, 9, dt);
    m_look.y = approach(m_look.y, target.y, 9, dt);
}

// -------------------------------------------------------------------- air --

void PetSim::integrateAir(double dt)
{
    const Vec2 prev = m_pos;
    m_vel.y = std::min(m_vel.y + m_tuning.gravity * dt, 3200.0);
    Vec2 next = m_pos + m_vel * dt;

    if (const Screen* screen = screenFor(m_pos)) {
        const double hw = m_tuning.halfWidth * s();
        const double left = screen->frame.x + hw;
        const double right = screen->frame.right() - hw;
        if (next.x < left) {
            next.x = left;
            m_vel.x = std::abs(m_vel.x) * 0.45;
        } else if (next.x > right) {
            next.x = right;
            m_vel.x = -std::abs(m_vel.x) * 0.45;
        }
        const double top = screen->available.y + m_tuning.height * s();
        if (next.y < top && m_vel.y < 0) {
            next.y = top;
            m_vel.y = -m_vel.y * 0.25;
        }
    }

    if (m_vel.y > 0) {
        // One-way platforms: only land when crossing an edge from above.
        const Segment* hit = nullptr;
        for (const Segment& seg : m_platforms) {
            if (prev.y <= seg.y + 0.01 && next.y >= seg.y && seg.spans(next.x, 2)
                && (!hit || seg.y < hit->y))
                hit = &seg;
        }
        if (hit) {
            m_pos = { next.x, hit->y };
            land(*hit, m_vel.y);
            return;
        }
    }
    m_pos = next;
}

void PetSim::land(const Segment& seg, double impact)
{
    const Activity was = m_act;
    m_grounded = true;
    m_standId = seg.windowId;
    m_vel.y = 0;
    m_squashVel -= std::min(impact, 2600.0) * 0.0022;
    if (std::abs(m_vel.x) > 20)
        m_facing = signOf(m_vel.x);

    if (impact > 2000) {
        m_vel.x = 0;
        setActivity(Activity::Dizzy, 2.4);
        return;
    }
    switch (was) {
    case Activity::Startled:
        m_vel.x = 0;
        m_facing = m_cursor.x > m_pos.x ? -1 : 1;
        setActivity(Activity::Run, 1.1);
        return;
    case Activity::Pounce: {
        m_vel.x = 0;
        const bool caught = std::abs(m_cursor.x - m_pos.x) < 55 * s()
            && m_pos.y - m_cursor.y < 140 * s() && m_pos.y - m_cursor.y > -20 * s();
        setActivity(caught ? Activity::Wag : Activity::Land, caught ? 1.6 : 0.3);
        return;
    }
    case Activity::Fall:
        // Thrown fox slides a bit.
        setActivity(Activity::Land, 0.35);
        return;
    default:
        m_vel.x = 0;
        setActivity(Activity::Land, 0.22);
        return;
    }
}

// ----------------------------------------------------------------- ground --

void PetSim::updateGround(double dt)
{
    const Segment* seg = segmentUnder(m_pos, m_standId);
    if (!seg) {
        // The window under us moved away or closed.
        m_grounded = false;
        m_standId = 0;
        m_vel.y = 0;
        m_pending.valid = false;
        setActivity(Activity::Fall, 0);
        return;
    }
    m_standId = seg->windowId;
    m_pos.y = seg->y;

    switch (m_act) {
    case Activity::Walk:
        m_vel.x = m_facing * m_tuning.walkSpeed * s();
        break;
    case Activity::Run:
        m_vel.x = m_facing * m_tuning.runSpeed * s();
        break;
    case Activity::Land: {
        const double v = std::max(0.0, std::abs(m_vel.x) - 1800 * dt);
        m_vel.x = signOf(m_vel.x) * v;
        break;
    }
    default:
        m_vel.x = 0;
        break;
    }

    double nx = m_pos.x + m_vel.x * dt;

    if (const Screen* screen = screenFor(m_pos)) {
        const double hw = m_tuning.halfWidth * s();
        const double left = screen->frame.x + hw;
        const double right = screen->frame.right() - hw;
        if (nx < left || nx > right) {
            nx = std::clamp(nx, left, right);
            m_vel.x = 0;
            if (m_act == Activity::Walk || m_act == Activity::Run)
                m_facing = -m_facing;
        }
    }

    const double edgeL = seg->x0 + 4 * s();
    const double edgeR = seg->x1 - 4 * s();
    if ((m_vel.x < 0 && nx < edgeL) || (m_vel.x > 0 && nx > edgeR)) {
        if (m_act == Activity::Walk || m_act == Activity::Run) {
            onEdge();
            if (!m_grounded)
                return;
        } else if (m_act == Activity::Land && (nx < seg->x0 || nx > seg->x1)) {
            // Slid off the edge.
            m_grounded = false;
            m_standId = 0;
            m_pos.x = nx;
            setActivity(Activity::Fall, 0);
            return;
        }
        nx = std::clamp(nx, std::min(edgeL, edgeR), std::max(edgeL, edgeR));
    }

    m_pos.x = nx;
    m_walkPhase = std::fmod(m_walkPhase + std::abs(m_vel.x) * dt * 2 * kPi / (30 * s()), 2 * kPi * 1000);
}

void PetSim::onEdge()
{
    const double r = rand(0, 1);
    if (r < 0.35) {
        m_facing = -m_facing;
    } else if (r < 0.8) {
        if (!tryJump(m_facing))
            m_facing = -m_facing;
    } else {
        // Just hop off.
        const double speed = std::max(std::abs(m_vel.x), 90 * s());
        launch({ m_facing * speed, -160 * std::sqrt(s()) }, Activity::Jump, 0);
    }
}

bool PetSim::tryJump(int direction)
{
    struct Candidate {
        Vec2 target;
        double weight;
    };
    std::vector<Candidate> candidates;
    const double margin = 22 * s();

    for (const Segment& seg : m_platforms) {
        if (seg.windowId == m_standId && std::abs(seg.y - m_pos.y) < 3 && seg.spans(m_pos.x))
            continue;
        if (seg.length() < 2 * margin + 4)
            continue;
        const double tx = std::clamp(m_pos.x + direction * 70 * s(), seg.x0 + margin, seg.x1 - margin);
        const double dx = tx - m_pos.x;
        const double dy = seg.y - m_pos.y;
        if (direction != 0 && dx * direction < 20 * s())
            continue;
        if (std::abs(dx) > 340 * s() || dy < -340 * s() || dy > 420 * s())
            continue;
        if (std::abs(dy) < 6 && std::abs(dx) < 60 * s())
            continue;
        candidates.push_back({ { tx, seg.y }, dy < 0 ? 2.5 : 1.0 });
    }
    if (candidates.empty())
        return false;

    double total = 0;
    for (const Candidate& c : candidates)
        total += c.weight;
    double pick = rand(0, total);
    const Candidate* chosen = &candidates.back();
    for (const Candidate& c : candidates) {
        pick -= c.weight;
        if (pick <= 0) {
            chosen = &c;
            break;
        }
    }

    const double apex = (chosen->target.y < m_pos.y ? 40 : 26) * s();
    const JumpPlan plan = planJump(m_pos, chosen->target, m_tuning.gravity, apex, 1100 * s());
    if (!plan.ok)
        return false;
    crouchThen({ plan.velocity, plan.duration, Activity::Jump, true }, 0.28);
    return true;
}

bool PetSim::tryPounce(const Segment& seg)
{
    const double dx = m_cursor.x - m_pos.x;
    const double above = m_pos.y - m_cursor.y;
    if (std::abs(dx) < 70 * s() || std::abs(dx) > 320 * s() || above < -10 * s() || above > 170 * s())
        return false;
    const double tx = std::clamp(m_cursor.x - signOf(dx) * 10 * s(), seg.x0 + 10 * s(), seg.x1 - 10 * s());
    if (std::abs(tx - m_pos.x) < 40 * s())
        return false;
    const JumpPlan plan = planJump(m_pos, { tx, m_pos.y }, m_tuning.gravity,
                                   std::max(above * 0.8, 50 * s()), 1200 * s());
    if (!plan.ok)
        return false;
    m_facing = signOf(dx);
    crouchThen({ plan.velocity, plan.duration, Activity::Pounce, true }, 0.75);
    return true;
}

void PetSim::crouchThen(const PendingJump& jump, double crouchTime)
{
    m_pending = jump;
    m_facing = std::abs(jump.velocity.x) > 5 ? signOf(jump.velocity.x) : m_facing;
    setActivity(Activity::Crouch, crouchTime);
}

void PetSim::launch(Vec2 velocity, Activity a, double duration)
{
    m_grounded = false;
    m_standId = 0;
    m_vel = velocity;
    if (std::abs(velocity.x) > 5 && a != Activity::Flip)
        m_facing = signOf(velocity.x);
    m_squashVel += 2.4;
    if (duration <= 0)
        duration = 2 * std::max(0.0, -velocity.y) / m_tuning.gravity;
    setActivity(a, duration);
}

// -------------------------------------------------------------- behaviour --

void PetSim::think(const Input& in, double dt)
{
    const bool calm = isCalm();
    const bool sleeping = m_act == Activity::Sleep;

    // Petting beats everything; it even wakes a sleeping fox (happily).
    if ((calm || sleeping || m_act == Activity::Petted) && m_petAccum > 420 * s()) {
        if (m_act != Activity::Petted)
            setActivity(Activity::Petted, 1.4);
        else
            m_actDuration = std::max(m_actDuration, m_actTime + 0.9);
        m_petAccum = 420 * s();
        return;
    }

    const Vec2 centre = m_pos + Vec2 { 0, -30 * s() };
    if ((calm || sleeping) && m_startleCooldown <= 0 && !m_cursorOver
        && (m_cursor - centre).length() < 120 * s() && m_cursorSpeed > 2800) {
        startle();
        return;
    }

    if (calm && in.idleSeconds > m_tuning.sleepAfter) {
        m_napping = false;
        setActivity(Activity::Sleep, 1e9);
        return;
    }

    const bool timeUp = m_actTime >= m_actDuration;
    switch (m_act) {
    case Activity::Idle:
    case Activity::Sit:
    case Activity::LookAround: {
        // Turn towards the cursor if it hangs around behind us.
        const double dx = m_cursor.x - m_pos.x;
        if (dx * m_facing < -30 * s() && std::abs(dx) < 600 * s())
            m_behindTime += dt;
        else
            m_behindTime = 0;
        if (m_behindTime > 1.3) {
            m_facing = -m_facing;
            m_behindTime = 0;
        }
        if (m_cursorStill > 0.7 && chance(dt * 0.5)) {
            if (const Segment* seg = segmentUnder(m_pos, m_standId); seg && tryPounce(*seg))
                return;
        }
        if (timeUp) {
            if (m_act == Activity::Idle)
                pickNext();
            else
                setActivity(Activity::Idle, rand(0.8, 2.2));
        }
        break;
    }
    case Activity::Walk:
        if (m_hasWalkTarget && std::abs(m_pos.x - m_walkTarget) < 40 * s()) {
            setActivity(Activity::Sit, rand(3, 8));
            break;
        }
        [[fallthrough]];
    case Activity::Run:
        if (timeUp)
            setActivity(Activity::Idle, rand(1.0, 2.6));
        break;
    case Activity::Sleep: {
        if (m_napping && in.idleSeconds > m_tuning.sleepAfter)
            m_napping = false;   // the user left: keep sleeping until they return
        const bool wake = m_napping ? timeUp : in.idleSeconds < 0.5;
        if (wake) {
            m_awake = 0;
            setActivity(Activity::Wake, 1.7);
        }
        break;
    }
    case Activity::Crouch:
        if (timeUp) {
            if (m_pending.valid) {
                m_pending.valid = false;
                launch(m_pending.velocity, m_pending.activity, m_pending.duration);
            } else {
                setActivity(Activity::Idle, 1.0);
            }
        }
        break;
    case Activity::ChaseTail:
        m_spin += dt;
        if (m_spin > 0.22) {
            m_spin = 0;
            m_facing = -m_facing;
        }
        if (timeUp)
            setActivity(Activity::Idle, rand(0.8, 1.6));
        break;
    case Activity::Wake:
    case Activity::Land:
    case Activity::Dizzy:
    case Activity::Wag:
    case Activity::Petted:
        if (timeUp)
            setActivity(Activity::Idle, rand(0.8, 2.0));
        break;
    default:
        // Airborne activities that ended up on the ground somehow.
        setActivity(Activity::Idle, 1.0);
        break;
    }
}

void PetSim::pickNext()
{
    const double dx = m_cursor.x - m_pos.x;
    const bool cursorNear = std::abs(dx) < 700 * s() && std::abs(m_cursor.y - m_pos.y) < 260 * s()
        && std::abs(dx) > 90 * s();

    enum Choice { Walk, ToCursor, Sit, Look, Jump, Run, Nap, Count };
    double weights[Count] = { 34, cursorNear ? 14.0 : 0.0, 15, 9, 24, 5, m_awake > 150 ? 6.0 : 0.0 };
    double total = 0;
    for (double w : weights)
        total += w;
    double pick = rand(0, total);
    int choice = Walk;
    for (int i = 0; i < Count; ++i) {
        pick -= weights[i];
        if (pick <= 0) {
            choice = i;
            break;
        }
    }

    switch (choice) {
    case Jump:
        if (tryJump(0))
            return;
        [[fallthrough]];
    case Walk: {
        int dir = chance(0.5) ? 1 : -1;
        if (const Segment* seg = segmentUnder(m_pos, m_standId)) {
            if (m_pos.x - seg->x0 < 60 * s())
                dir = 1;
            else if (seg->x1 - m_pos.x < 60 * s())
                dir = -1;
        }
        m_facing = dir;
        setActivity(Activity::Walk, rand(1.8, 5.5));
        return;
    }
    case ToCursor:
        m_facing = signOf(dx);
        setActivity(Activity::Walk, 6.0);
        m_walkTarget = m_cursor.x;
        m_hasWalkTarget = true;
        return;
    case Sit:
        setActivity(Activity::Sit, rand(4, 12));
        return;
    case Look:
        setActivity(Activity::LookAround, rand(2.0, 3.2));
        return;
    case Run:
        m_facing = chance(0.5) ? 1 : -1;
        setActivity(Activity::Run, rand(0.7, 1.5));
        return;
    case Nap:
        m_napping = true;
        m_awake = 0;
        setActivity(Activity::Sleep, rand(25, 70));
        return;
    }
}

void PetSim::startle()
{
    m_startleCooldown = 5;
    m_pending.valid = false;
    const int away = m_cursor.x > m_pos.x ? -1 : 1;
    launch({ away * 120 * s(), -560 * std::sqrt(s()) }, Activity::Startled, 0);
}

void PetSim::click()
{
    if (!m_grounded || m_clickCooldown > 0)
        return;
    m_clickCooldown = 0.5;

    if (m_act == Activity::Sleep) {
        m_awake = 0;
        setActivity(Activity::Wake, 1.7);
        return;
    }
    if (m_act == Activity::Crouch || m_act == Activity::Dizzy)
        return;

    const double r = rand(0, 1);
    if (r < 0.32)
        launch({ 0, -430 * std::sqrt(s()) }, Activity::Happy, 0);
    else if (r < 0.56)
        launch({ m_facing * 30 * s(), -720 * std::sqrt(s()) }, Activity::Flip, 0);
    else if (r < 0.78)
        setActivity(Activity::ChaseTail, 1.8);
    else
        setActivity(Activity::Wag, 1.8);
}

// ------------------------------------------------------------------ mouse --

void PetSim::press(Vec2 cursor)
{
    if (m_pressed)
        return;
    m_pressed = true;
    m_pressCursor = cursor;
}

void PetSim::release()
{
    if (!m_pressed)
        return;
    m_pressed = false;
    if (m_dragging) {
        m_dragging = false;
        m_vel = m_dragVel;
        const double speed = m_vel.length();
        if (speed > 2600)
            m_vel = m_vel * (2600 / speed);
        m_grounded = false;
        setActivity(Activity::Fall, 0);
    } else {
        click();
    }
}

void PetSim::updateDrag(double dt)
{
    const Vec2 prev = m_pos;
    m_pos = m_cursor - scruffOffset();
    const Vec2 v = (m_pos - prev) * (1.0 / dt);
    m_dragVel.x = approach(m_dragVel.x, v.x, 16, dt);
    m_dragVel.y = approach(m_dragVel.y, v.y, 16, dt);

    // A pendulum hanging from the cursor.
    const double target = std::clamp(-m_dragVel.x * 0.025, -55.0, 55.0);
    m_swingVel += ((target - m_swing) * 90 - m_swingVel * 7) * dt;
    m_swing += m_swingVel * dt;
}

Vec2 PetSim::scruffOffset() const
{
    return { m_facing * 20 * s(), -40 * s() };
}

void PetSim::updateSquash(double dt)
{
    const double target = m_act == Activity::Crouch ? 0.8 : 1.0;
    m_squashVel += ((target - m_squash) * 320 - m_squashVel * 16) * dt;
    m_squash = std::clamp(m_squash + m_squashVel * dt, 0.55, 1.45);
}

void PetSim::ensureOnScreen()
{
    if (m_dragging || m_screens.empty())
        return;
    for (const Screen& screen : m_screens) {
        const Rect& f = screen.frame;
        if (m_pos.x >= f.x - 50 && m_pos.x <= f.right() + 50 && m_pos.y >= f.y - 400
            && m_pos.y <= f.bottom() + 5)
            return;
    }
    const Rect& av = m_screens.front().available;
    spawn({ av.x + av.w / 2, av.y + m_tuning.height * s() + 10 });
}

Rect PetSim::hitBox() const
{
    const double k = s();
    if (m_act == Activity::Dragged) {
        const Vec2 grab = m_pos + scruffOffset();
        return { grab.x - 34 * k, grab.y - 16 * k, 68 * k, 96 * k };
    }
    const double h = m_act == Activity::Sleep ? 44 * k : 70 * k;
    const double back = 40 * k;
    const double front = 48 * k;
    const double x = m_facing > 0 ? m_pos.x - back : m_pos.x - front;
    return { x, m_pos.y - h, back + front, h + 2 * k };
}

} // namespace kit
