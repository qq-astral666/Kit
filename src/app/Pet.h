#pragma once

#include "PetSim.h"

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <climits>

// Bridges the core simulation to the desktop and to QML:
//  * feeds it the cursor, mouse button, idle time and other apps' windows;
//  * moves the fox's small transparent window so it follows the feet;
//  * makes the window click-through everywhere except over the fox;
//  * exposes the pose numbers the Fox.qml rig is driven by.
class Pet : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created in main.cpp")

public:
    // Mirrors kit::Activity (same order).
    enum Activity {
        Idle,
        Walk,
        Run,
        Sit,
        LookAround,
        Sleep,
        Wake,
        Crouch,
        Jump,
        Pounce,
        Fall,
        Land,
        Dizzy,
        Dragged,
        Happy,
        Flip,
        ChaseTail,
        Wag,
        Petted,
        Startled,
    };
    Q_ENUM(Activity)

    enum Size { Small, Medium, Large };
    Q_ENUM(Size)

private:
    Q_PROPERTY(Activity activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(qreal clock READ clock NOTIFY frame)
    Q_PROPERTY(int facing READ facing NOTIFY frame)
    Q_PROPERTY(bool grounded READ grounded NOTIFY frame)
    Q_PROPERTY(qreal walkPhase READ walkPhase NOTIFY frame)
    Q_PROPERTY(qreal squash READ squash NOTIFY frame)
    Q_PROPERTY(qreal lookX READ lookX NOTIFY frame)
    Q_PROPERTY(qreal lookY READ lookY NOTIFY frame)
    Q_PROPERTY(qreal swing READ swing NOTIFY frame)
    Q_PROPERTY(qreal velX READ velX NOTIFY frame)
    Q_PROPERTY(qreal velY READ velY NOTIFY frame)
    Q_PROPERTY(qreal actTime READ actTime NOTIFY frame)
    Q_PROPERTY(qreal actDuration READ actDuration NOTIFY frame)
    Q_PROPERTY(qreal anchorX READ anchorX NOTIFY frame)
    Q_PROPERTY(qreal anchorY READ anchorY NOTIFY frame)
    Q_PROPERTY(qreal size READ size NOTIFY sizeChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY sizeChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY sizeChanged)

public:
    explicit Pet(QObject* parent = nullptr);

    void attach(QQuickWindow* window);

    Activity activity() const { return static_cast<Activity>(m_sim.activity()); }
    qreal clock() const { return m_clock; }
    int facing() const { return m_sim.facing(); }
    bool grounded() const { return m_sim.grounded(); }
    qreal walkPhase() const { return m_sim.walkPhase(); }
    qreal squash() const { return m_sim.squash(); }
    qreal lookX() const { return m_sim.look().x; }
    qreal lookY() const { return m_sim.look().y; }
    qreal swing() const { return m_sim.swing(); }
    qreal velX() const { return m_sim.velocity().x; }
    qreal velY() const { return m_sim.velocity().y; }
    qreal actTime() const { return m_sim.activityTime(); }
    qreal actDuration() const { return m_sim.activityDuration(); }
    qreal anchorX() const { return m_anchorX; }
    qreal anchorY() const { return m_anchorY; }
    qreal size() const { return m_sim.tuning().scale; }
    int windowWidth() const;
    int windowHeight() const;

    Size sizeSetting() const { return m_size; }
    void setSizeSetting(Size size);
    bool hidden() const { return m_hidden; }
    void setHidden(bool hidden);
    void summon();

    // Called by QML once per rendered frame.
    Q_INVOKABLE void tick(qreal frameTime);
    // QML saw a mouse press on the window.
    Q_INVOKABLE void notePress();

signals:
    void frame();
    void activityChanged();
    void sizeChanged();

private:
    void refreshWorld();
    void refreshDock();
    void placeWindow();
    void setInteractive(bool interactive);
    void updateVisibility();
    std::vector<kit::Screen> screens() const;

    kit::PetSim m_sim;
    QPointer<QQuickWindow> m_window;
    QTimer m_worldTimer;
    QTimer m_dockTimer;
    kit::Rect m_dock;
    bool m_hasDock = false;

    Size m_size = Medium;
    bool m_hidden = false;
    bool m_fullScreen = false;
    bool m_shown = true;
    bool m_interactive = false;
    bool m_wasDown = false;
    bool m_qmlPressed = false;
    bool m_pressCandidate = false;
    bool m_raised = true;
    kit::Activity m_lastActivity = kit::Activity::Fall;

    qreal m_clock = 0;
    QPoint m_windowPos { INT_MIN, INT_MIN };
    QSize m_windowSize;
    qreal m_anchorX = 0;
    qreal m_anchorY = 0;
};
