#pragma once

#include "World.h"

#include <QPoint>
#include <QRect>
#include <QString>

#include <vector>

class QWindow;

// macOS glue. Native_mac.mm / Platform_stub.cpp.
namespace native {

// Borderless, non-activating floating panel on every Space: clicking the fox
// never steals focus from the app you're working in.
void configureWindow(QWindow* window);
// Sets the frame directly (Qt/Cocoa would keep the window below the menu
// bar, so the fox couldn't reach the top of the screen).
void placeWindow(QWindow* window, const QRect& qtGeometry);
// Raised: above the menu bar and Dock (carried/flying). Otherwise a floating
// window: above apps, but behind the Dock and menu bar.
void setWindowRaised(QWindow* window, bool raised);
// true = clicks go through to whatever is underneath.
void setClickThrough(QWindow* window, bool clickThrough);
// orderOut / orderFrontRegardless without touching Qt's visibility.
void setWindowShown(QWindow* window, bool shown);

// Other apps' normal windows on screen, front to back, in Qt global
// coordinates. Only frames are read, so no Screen Recording permission.
std::vector<kit::WindowInfo> windowList();

// The Dock's icon bar, via the Accessibility API (the Dock's window itself
// covers the whole screen). False without Accessibility permission.
bool dockBar(kit::Rect* bar);
bool accessibilityTrusted();
// Shows the system prompt that sends the user to the Accessibility settings.
void requestAccessibility();

// Seconds since the last keyboard/mouse input anywhere in the system.
double secondsSinceInput();
bool isLeftMouseDown();

// The display under `globalPoint` shows a full-screen Space.
bool isFullScreenActive(QPoint globalPoint);

bool launchAtLoginEnabled();
bool setLaunchAtLogin(bool enable, QString* error);

} // namespace native
