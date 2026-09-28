#include "Pet.h"

#include "Native.h"

#include <QCursor>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>

#include <algorithm>
#include <cmath>

static_assert(int(Pet::Startled) == int(kit::Activity::Startled), "Pet::Activity must mirror kit::Activity");

namespace {

// The window is a fixed box around the fox; the feet sit this far from its
// bottom so a dangling fox, hearts and "Zzz" all fit.
constexpr double kWindowW = 300;
constexpr double kWindowH = 260;
constexpr double kFeetFromBottom = 64;

constexpr int kWorldFastMs = 33;    // standing on a window: follow it closely
constexpr int kWorldSlowMs = 150;

double scaleFor(Pet::Size size)
{
    switch (size) {
    case Pet::Small: return 0.7;
    case Pet::Large: return 1.2;
    case Pet::Medium: break;
    }
    return 0.92;
}

const QString kSizeKey = QStringLiteral("size");
const QString kHiddenKey = QStringLiteral("hidden");

} // namespace

Pet::Pet(QObject* parent)
    : QObject(parent)
    , m_sim(QRandomGenerator::global()->generate())
{
    QSettings settings;
    m_size = static_cast<Size>(std::clamp(settings.value(kSizeKey, int(Medium)).toInt(), 0, 2));
    m_hidden = settings.value(kHiddenKey, false).toBool();

    kit::Tuning tuning;
    tuning.scale = scaleFor(m_size);
    m_sim.setTuning(tuning);

    m_worldTimer.setInterval(kWorldSlowMs);
    connect(&m_worldTimer, &QTimer::timeout, this, &Pet::refreshWorld);
}

int Pet::windowWidth() const
{
    return int(std::ceil(kWindowW * size()));
}

int Pet::windowHeight() const
{
    return int(std::ceil(kWindowH * size()));
}

std::vector<kit::Screen> Pet::screens() const
{
    std::vector<kit::Screen> result;
    for (const QScreen* screen : QGuiApplication::screens()) {
        const QRect f = screen->geometry();
        const QRect a = screen->availableGeometry();
        kit::Screen s;
        s.frame = { double(f.x()), double(f.y()), double(f.width()), double(f.height()) };
        s.available = { double(a.x()), double(a.y()), double(a.width()), double(a.height()) };
        if (m_hasDock && m_dock.x >= s.frame.x - 1 && m_dock.right() <= s.frame.right() + 1
            && m_dock.bottom() > s.frame.bottom() - 30 && m_dock.bottom() <= s.frame.bottom() + 1) {
            s.dock = m_dock;
            s.hasDock = true;
        }
        result.push_back(s);
    }
    return result;
}

void Pet::attach(QQuickWindow* window)
{
    m_window = window;
    refreshWorld();

    // Drop in from the top of the main screen.
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect a = screen->availableGeometry();
        const double x = a.x() + a.width() * (0.25 + 0.5 * QRandomGenerator::global()->generateDouble());
        m_sim.spawn({ x, a.y() + 70 * size() });
    }

    window->create();
    native::configureWindow(window);
    placeWindow();
    window->show();
    native::configureWindow(window);   // NSWindow definitely exists now
    native::setClickThrough(window, true);
    updateVisibility();

    auto reconfigure = [this] { QTimer::singleShot(300, this, &Pet::refreshWorld); };
    connect(qApp, &QGuiApplication::screenAdded, this, reconfigure);
    connect(qApp, &QGuiApplication::screenRemoved, this, reconfigure);

    // Walking on the Dock needs its frame, which only Accessibility exposes.
    // Ask once; the tray menu can ask again.
    QSettings settings;
    if (!native::accessibilityTrusted() && !settings.value(QStringLiteral("askedAccessibility")).toBool()) {
        settings.setValue(QStringLiteral("askedAccessibility"), true);
        native::requestAccessibility();
    }
    m_dockTimer.setInterval(2000);
    connect(&m_dockTimer, &QTimer::timeout, this, &Pet::refreshDock);
    refreshDock();
    m_dockTimer.start();

    m_worldTimer.start();
}

void Pet::tick(qreal frameTime)
{
    if (!m_window || !m_shown)
        return;
    const double dt = std::clamp(double(frameTime), 0.0, 0.05);
    m_clock += dt;

    const QPoint c = QCursor::pos();
    const kit::Vec2 cursor { double(c.x()), double(c.y()) };
    const bool over = m_sim.hitBox().contains(cursor);

    // Mouse is polled rather than taken from QML events: the window moves
    // under the cursor while dragging, and polling keeps that simple.
    // A press counts only if it really landed on our window: QML reports it
    // (onPressed). Polling is the fallback, except over the Dock's strip,
    // where the fox walks *behind* the Dock and clicks belong to the Dock.
    const bool down = native::isLeftMouseDown();
    if (down) {
        if (!m_wasDown)
            m_pressCandidate = over;   // the press began on the fox
        if (m_pressCandidate && !m_sim.pressed()) {
            const QScreen* screen = QGuiApplication::screenAt(c);
            const bool dockStrip = screen && c.y() >= screen->availableGeometry().bottom();
            if (m_qmlPressed || !dockStrip)   // QML's event may arrive a frame late
                m_sim.press(cursor);
        }
    } else {
        if (m_sim.pressed())
            m_sim.release();
        m_pressCandidate = false;
        m_qmlPressed = false;
    }
    m_wasDown = down;

    kit::Input input;
    input.dt = dt;
    input.cursor = cursor;
    input.idleSeconds = native::secondsSinceInput();
    input.cursorOverPet = over;
    m_sim.step(input);

    placeWindow();
    setInteractive(over || m_sim.pressed());

    // Below the Dock and menu bar while on the ground; above them while
    // carried or flying, so it can be dragged anywhere.
    const bool raised = m_sim.dragging() || !m_sim.grounded() || m_sim.standingOn() == kit::kDockId;
    if (raised != m_raised) {
        m_raised = raised;
        native::setWindowRaised(m_window, raised);
    }

    if (m_sim.activity() != m_lastActivity) {
        m_lastActivity = m_sim.activity();
        emit activityChanged();
    }
    emit frame();
}

void Pet::notePress()
{
    m_qmlPressed = true;
}

void Pet::placeWindow()
{
    if (!m_window)
        return;
    const kit::Vec2 feet = m_sim.position();
    const double baseX = windowWidth() / 2.0;
    const double baseY = windowHeight() - kFeetFromBottom * size();
    const double left = feet.x - baseX;
    const double top = feet.y - baseY;
    const QPoint pos(int(std::floor(left)), int(std::floor(top)));
    const QSize size(windowWidth(), windowHeight());
    if (pos != m_windowPos || size != m_windowSize) {
        m_windowPos = pos;
        m_windowSize = size;
        native::placeWindow(m_window, QRect(pos, size));
    }
    // Sub-point remainder is applied inside the window for smooth motion.
    m_anchorX = feet.x - pos.x();
    m_anchorY = feet.y - pos.y();
}

void Pet::setInteractive(bool interactive)
{
    if (interactive == m_interactive || !m_window)
        return;
    m_interactive = interactive;
    native::setClickThrough(m_window, !interactive);
}

void Pet::refreshDock()
{
    kit::Rect bar;
    const bool has = native::dockBar(&bar);
    // An auto-hidden Dock reports its bar off the bottom of the screen; the
    // screen check in screens() drops it then.
    m_hasDock = has;
    if (has)
        m_dock = bar;
}

void Pet::refreshWorld()
{
    m_sim.setWorld(screens(), native::windowList());
    m_worldTimer.setInterval(m_sim.grounded() && m_sim.standingOn() != 0 ? kWorldFastMs : kWorldSlowMs);

    const kit::Vec2 p = m_sim.position();
    const bool fullScreen = native::isFullScreenActive(QPoint(int(p.x), int(p.y) - 10));
    if (fullScreen != m_fullScreen) {
        m_fullScreen = fullScreen;
        updateVisibility();
    }
}

void Pet::updateVisibility()
{
    const bool shown = !m_hidden && !m_fullScreen;
    if (!m_window)
        return;
    if (shown != m_shown || !shown) {
        m_shown = shown;
        native::setWindowShown(m_window, shown);
        if (shown)
            m_window->requestUpdate();   // restart the frame loop
    }
}

void Pet::setSizeSetting(Size size)
{
    if (size == m_size)
        return;
    m_size = size;
    QSettings().setValue(kSizeKey, int(size));
    kit::Tuning tuning = m_sim.tuning();
    tuning.scale = scaleFor(size);
    m_sim.setTuning(tuning);
    emit sizeChanged();
    placeWindow();
}

void Pet::setHidden(bool hidden)
{
    if (hidden == m_hidden)
        return;
    m_hidden = hidden;
    QSettings().setValue(kHiddenKey, hidden);
    updateVisibility();
}

void Pet::summon()
{
    const QPoint c = QCursor::pos();
    const QScreen* screen = QGuiApplication::screenAt(c);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect a = screen->availableGeometry();
    const double x = std::clamp(double(c.x()), a.x() + 60.0, a.x() + a.width() - 60.0);
    m_sim.spawn({ x, a.y() + 70 * size() });
    if (m_hidden)
        setHidden(false);
}
