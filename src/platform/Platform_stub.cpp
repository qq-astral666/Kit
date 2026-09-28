// Non-macOS fallbacks: the fox walks along the bottom of the screen only.

#include "Native.h"

#include <QGuiApplication>
#include <QWindow>

namespace native {

void configureWindow(QWindow*) {}
void placeWindow(QWindow* window, const QRect& g)
{
    if (window)
        window->setGeometry(g);
}
void setClickThrough(QWindow*, bool) {}
void setWindowShown(QWindow*, bool) {}
std::vector<kit::WindowInfo> windowList() { return {}; }
void setWindowRaised(QWindow*, bool) {}
double secondsSinceInput() { return 0; }
bool dockBar(kit::Rect*) { return false; }
bool accessibilityTrusted() { return false; }
void requestAccessibility() {}
bool isLeftMouseDown() { return QGuiApplication::mouseButtons() & Qt::LeftButton; }
bool isFullScreenActive(QPoint) { return false; }
bool launchAtLoginEnabled() { return false; }
bool setLaunchAtLogin(bool, QString* error)
{
    if (error)
        *error = QStringLiteral("Not supported on this platform");
    return false;
}

} // namespace native
