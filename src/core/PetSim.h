#pragma once

// The fox's brain and body: a small physics simulation plus a behaviour state
// machine. Pure C++ with no Qt/OS dependencies, so it is fully unit-tested;
// the app layer feeds it the cursor and the desktop, the QML layer draws it.

#include "World.h"

#include <cstdint>
#include <random>
#include <vector>

namespace kit {

enum class Activity {
    Idle,
    Walk,
    Run,
    Sit,
    LookAround,
    Sleep,
    Wake,        // stretch after sleeping
    Crouch,      // wind-up before any jump
    Jump,        // hop to another platform / off an edge
    Pounce,      // jump at the cursor
    Fall,        // thrown, dropped or lost its footing
    Land,
    Dizzy,       // landed too hard
    Dragged,
    Happy,       // little hop with hearts (click)
    Flip,        // backflip (click)
    ChaseTail,   // (click)
    Wag,         // sits and wags (click, caught the cursor)
    Petted,      // cursor stroking it
    Startled,    // cursor flew past too fast
};

const char* toString(Activity a);

struct Tuning {
    double scale = 1.0;         // fox size; distances and speeds scale with it
    double gravity = 2600;      // points/s²
    double walkSpeed = 64;      // at scale 1
    double runSpeed = 210;
    double halfWidth = 38;      // distance kept from screen edges
    double height = 64;
    double sleepAfter = 45;     // seconds without user input before napping
};

struct Input {
    double dt = 0;
    Vec2 cursor;
    double idleSeconds = 0;     // since the last keyboard/mouse input anywhere
    bool cursorOverPet = false;
};

class PetSim {
public:
    explicit PetSim(std::uint32_t seed = 1);

    void setTuning(const Tuning& tuning);
    const Tuning& tuning() const { return m_tuning; }

    // Rebuilds platforms. If the fox stands on a window that moved, it rides
    // along with it.
    void setWorld(std::vector<Screen> screens, std::vector<WindowInfo> windowsFrontToBack);
    const std::vector<Segment>& platforms() const { return m_platforms; }
    const std::vector<Screen>& screens() const { return m_screens; }

    void spawn(Vec2 feet);
    void step(const Input& input);

    void press(Vec2 cursor);
    void release();
    bool pressed() const { return m_pressed; }
    bool dragging() const { return m_dragging; }

    // Test hook.
    void forceActivity(Activity a, double duration);

    Vec2 position() const { return m_pos; }        // feet, bottom-centre
    Vec2 velocity() const { return m_vel; }
    int facing() const { return m_facing; }        // +1 right, -1 left
    Activity activity() const { return m_act; }
    double activityTime() const { return m_actTime; }
    double activityDuration() const { return m_actDuration; }
    bool grounded() const { return m_grounded; }
    std::uint64_t standingOn() const { return m_standId; }
    double walkPhase() const { return m_walkPhase; }   // radians
    double squash() const { return m_squash; }         // 1 = neutral
    Vec2 look() const { return m_look; }               // x: +forward, y: +down, |v| <= 1
    double swing() const { return m_swing; }           // degrees while dragged
    Rect hitBox() const;
    Vec2 scruffOffset() const;                          // grab point relative to the feet

private:
    struct PendingJump {
        Vec2 velocity;
        double duration = 0;
        Activity activity = Activity::Jump;
        bool valid = false;
    };

    double s() const { return m_tuning.scale; }
    double rand(double lo, double hi);
    bool chance(double p);
    void setActivity(Activity a, double duration);
    bool isCalm() const;

    const Segment* segmentUnder(Vec2 p, std::uint64_t preferId) const;
    const Screen* screenFor(Vec2 p) const;

    void trackCursor(const Input& in, double dt);
    void updateLook(double dt);
    void integrateAir(double dt);
    void land(const Segment& seg, double impact);
    void updateGround(double dt);
    void think(const Input& in, double dt);
    void pickNext();
    void onEdge();
    bool tryJump(int direction);
    bool tryPounce(const Segment& seg);
    void launch(Vec2 velocity, Activity a, double duration);
    void crouchThen(const PendingJump& jump, double crouchTime);
    void click();
    void startle();
    void updateDrag(double dt);
    void updateSquash(double dt);
    void ensureOnScreen();

    Tuning m_tuning;
    std::mt19937 m_rng;

    std::vector<Screen> m_screens;
    std::vector<WindowInfo> m_windows;
    std::vector<Segment> m_platforms;

    Vec2 m_pos;
    Vec2 m_vel;
    int m_facing = 1;
    bool m_grounded = false;
    std::uint64_t m_standId = 0;

    Activity m_act = Activity::Fall;
    double m_actTime = 0;
    double m_actDuration = 0;
    PendingJump m_pending;

    double m_walkPhase = 0;
    double m_squash = 1;
    double m_squashVel = 0;
    Vec2 m_look;
    double m_swing = 0;
    double m_swingVel = 0;

    // cursor
    Vec2 m_cursor;
    Vec2 m_lastCursor;
    bool m_haveCursor = false;
    bool m_cursorOver = false;
    double m_cursorSpeed = 0;
    double m_cursorStill = 0;
    double m_petAccum = 0;
    double m_behindTime = 0;

    // mouse
    bool m_pressed = false;
    bool m_dragging = false;
    Vec2 m_pressCursor;
    Vec2 m_dragVel;

    // behaviour
    double m_walkTarget = 0;
    bool m_hasWalkTarget = false;
    bool m_napping = false;
    double m_awake = 0;
    double m_spin = 0;
    double m_startleCooldown = 0;
    double m_clickCooldown = 0;
};

} // namespace kit
