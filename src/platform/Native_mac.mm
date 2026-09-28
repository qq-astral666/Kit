// Compiled as Objective-C++ with ARC.
// Note: `template` and `class` are C++ keywords, so the matching Objective-C
// properties are used via message syntax ([X class]).

#include "Native.h"

#include <QWindow>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ServiceManagement/ServiceManagement.h>

#import <objc/runtime.h>

#include <dlfcn.h>
#include <unistd.h>

namespace {

NSWindow* nsWindowOf(QWindow* window)
{
    if (!window)
        return nil;
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
    return view.window;
}

// Qt global coordinates (y down from the top of the primary screen) to Cocoa.
NSPoint toCocoa(QPoint p)
{
    NSScreen* primary = NSScreen.screens.firstObject;
    const CGFloat h = primary ? primary.frame.size.height : 0;
    return NSMakePoint(p.x(), h - p.y());
}

// Private SkyLight calls (the same ones yabai and Hammerspoon use), resolved
// at runtime so a macOS without them just disables full-screen detection.
using CGSMainConnectionIDFn = int (*)();
using CGSCopyManagedDisplaySpacesFn = CFArrayRef (*)(int);
using CGDisplayCreateUUIDFn = CFUUIDRef (*)(uint32_t);
constexpr int kCGSSpaceTypeFullScreen = 4;

template <typename Fn>
Fn resolve(const char* name)
{
    return reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, name));
}

NSString* displayUuid(NSScreen* screen)
{
    static const auto createUuid = resolve<CGDisplayCreateUUIDFn>("CGDisplayCreateUUIDFromDisplayID");
    NSNumber* number = screen.deviceDescription[@"NSScreenNumber"];
    if (!createUuid || !number)
        return nil;
    CFUUIDRef uuid = createUuid(number.unsignedIntValue);
    if (!uuid)
        return nil;
    NSString* string = (__bridge_transfer NSString*)CFUUIDCreateString(nullptr, uuid);
    CFRelease(uuid);
    return string;
}

// AppKit keeps windows below the menu bar by "constraining" their frame.
// The fox must be draggable to the very top, so the window gets a tiny
// runtime subclass whose constrainFrameRect:toScreen: returns the frame as is.
void disableFrameConstraint(NSWindow* w)
{
    const SEL sel = @selector(constrainFrameRect:toScreen:);
    Class base = object_getClass(w);
    if ([NSStringFromClass(base) hasPrefix:@"KitFree_"])
        return;
    NSString* name = [@"KitFree_" stringByAppendingString:NSStringFromClass(base)];
    Class sub = NSClassFromString(name);
    if (!sub) {
        sub = objc_allocateClassPair(base, name.UTF8String, 0);
        if (!sub)
            return;
        Method original = class_getInstanceMethod(base, sel);
        IMP passThrough = imp_implementationWithBlock(^NSRect(id, NSRect frame, NSScreen*) {
            return frame;
        });
        class_addMethod(sub, sel, passThrough, method_getTypeEncoding(original));
        objc_registerClassPair(sub);
    }
    object_setClass(w, sub);
}

} // namespace

namespace native {

void configureWindow(QWindow* window)
{
    NSWindow* w = nsWindowOf(window);
    if (!w)
        return;
    if ([w isKindOfClass:[NSPanel class]]) {
        NSPanel* panel = (NSPanel*)w;
        panel.styleMask = panel.styleMask | NSWindowStyleMaskNonactivatingPanel;
        panel.becomesKeyOnlyIfNeeded = YES;
        panel.floatingPanel = YES;
    }
    w.level = NSMainMenuWindowLevel + 1;   // see setWindowRaised
    disableFrameConstraint(w);
    w.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                         | NSWindowCollectionBehaviorStationary
                         | NSWindowCollectionBehaviorIgnoresCycle;
    w.hidesOnDeactivate = NO;          // Qt sets YES for tool windows
    w.hasShadow = NO;
    w.opaque = NO;
    w.backgroundColor = NSColor.clearColor;
    w.animationBehavior = NSWindowAnimationBehaviorNone;
}

void placeWindow(QWindow* window, const QRect& g)
{
    NSWindow* w = nsWindowOf(window);
    if (!w)
        return;
    NSScreen* primary = NSScreen.screens.firstObject;
    const CGFloat h = primary ? primary.frame.size.height : 0;
    [w setFrame:NSMakeRect(g.x(), h - g.y() - g.height(), g.width(), g.height()) display:NO];
}

void setWindowRaised(QWindow* window, bool raised)
{
    if (NSWindow* w = nsWindowOf(window))
        w.level = raised ? NSMainMenuWindowLevel + 1 : NSFloatingWindowLevel;
}

void setClickThrough(QWindow* window, bool clickThrough)
{
    if (NSWindow* w = nsWindowOf(window))
        w.ignoresMouseEvents = clickThrough;
}

void setWindowShown(QWindow* window, bool shown)
{
    NSWindow* w = nsWindowOf(window);
    if (!w)
        return;
    if (shown)
        [w orderFrontRegardless];
    else
        [w orderOut:nil];
}

std::vector<kit::WindowInfo> windowList()
{
    std::vector<kit::WindowInfo> result;
    @autoreleasepool {
        CFArrayRef list = CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        if (!list)
            return result;
        NSArray* windows = (__bridge_transfer NSArray*)list;
        const pid_t ownPid = getpid();
        result.reserve(windows.count);

        for (NSDictionary* info in windows) {
            const int layer = [info[(__bridge NSString*)kCGWindowLayer] intValue];
            if (layer != 0)
                continue;   // menus, status items, overlays
            if ([info[(__bridge NSString*)kCGWindowOwnerPID] intValue] == ownPid)
                continue;
            NSNumber* alpha = info[(__bridge NSString*)kCGWindowAlpha];
            if (alpha && alpha.doubleValue < 0.1)
                continue;
            NSDictionary* bounds = info[(__bridge NSString*)kCGWindowBounds];
            CGRect r;
            if (!bounds || !CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)bounds, &r))
                continue;
            if (r.size.width < 60 || r.size.height < 40)
                continue;
            const auto id = [info[(__bridge NSString*)kCGWindowNumber] unsignedLongLongValue];
            result.push_back({ id, { r.origin.x, r.origin.y, r.size.width, r.size.height } });
        }
    }
    return result;
}

bool accessibilityTrusted()
{
    return AXIsProcessTrusted();
}

void requestAccessibility()
{
    NSDictionary* options = @{ (__bridge NSString*)kAXTrustedCheckOptionPrompt: @YES };
    AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

bool dockBar(kit::Rect* bar)
{
    if (!AXIsProcessTrusted())
        return false;
    NSArray<NSRunningApplication*>* apps =
        [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.dock"];
    if (apps.count == 0)
        return false;

    bool found = false;
    AXUIElementRef dock = AXUIElementCreateApplication(apps.firstObject.processIdentifier);
    CFArrayRef children = nullptr;
    if (AXUIElementCopyAttributeValue(dock, kAXChildrenAttribute, (CFTypeRef*)&children) == kAXErrorSuccess
        && children) {
        for (CFIndex i = 0; i < CFArrayGetCount(children) && !found; ++i) {
            AXUIElementRef element = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            CFTypeRef role = nullptr;
            AXUIElementCopyAttributeValue(element, kAXRoleAttribute, &role);
            const bool isList = role && CFEqual(role, kAXListRole);
            if (role)
                CFRelease(role);
            if (!isList)
                continue;

            CFTypeRef position = nullptr;
            CFTypeRef size = nullptr;
            CGPoint p;
            CGSize s;
            if (AXUIElementCopyAttributeValue(element, kAXPositionAttribute, &position) == kAXErrorSuccess
                && AXUIElementCopyAttributeValue(element, kAXSizeAttribute, &size) == kAXErrorSuccess
                && AXValueGetValue((AXValueRef)position, (AXValueType)kAXValueCGPointType, &p)
                && AXValueGetValue((AXValueRef)size, (AXValueType)kAXValueCGSizeType, &s)) {
                *bar = { p.x, p.y, s.width, s.height };
                found = s.width > 0 && s.height > 0;
            }
            if (position)
                CFRelease(position);
            if (size)
                CFRelease(size);
        }
        CFRelease(children);
    }
    CFRelease(dock);
    return found;
}

double secondsSinceInput()
{
    return CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateCombinedSessionState,
                                                  kCGAnyInputEventType);
}

bool isLeftMouseDown()
{
    return (NSEvent.pressedMouseButtons & 1) != 0;
}

bool isFullScreenActive(QPoint globalPoint)
{
    static const auto mainConnection = resolve<CGSMainConnectionIDFn>("CGSMainConnectionID");
    static const auto copySpaces = resolve<CGSCopyManagedDisplaySpacesFn>("CGSCopyManagedDisplaySpaces");
    if (!mainConnection || !copySpaces)
        return false;

    @autoreleasepool {
        NSArray* displays = (__bridge_transfer NSArray*)copySpaces(mainConnection());
        if (displays.count == 0)
            return false;

        NSScreen* screen = NSScreen.screens.firstObject;
        const NSPoint p = toCocoa(globalPoint);
        for (NSScreen* candidate in NSScreen.screens) {
            if (NSPointInRect(p, candidate.frame)) {
                screen = candidate;
                break;
            }
        }
        NSString* uuid = screen ? displayUuid(screen) : nil;

        NSDictionary* match = nil;
        for (NSDictionary* display in displays) {
            NSString* ident = display[@"Display Identifier"];
            // "Main" = "Displays have separate Spaces" is off: one set for all.
            if ([ident isEqualToString:@"Main"]
                || (uuid && [ident caseInsensitiveCompare:uuid] == NSOrderedSame)) {
                match = display;
                break;
            }
        }
        if (!match)
            match = displays.firstObject;
        NSNumber* type = match[@"Current Space"][@"type"];
        return type.intValue == kCGSSpaceTypeFullScreen;
    }
}

bool launchAtLoginEnabled()
{
    if (@available(macOS 13.0, *)) {
        const SMAppServiceStatus status = SMAppService.mainAppService.status;
        return status == SMAppServiceStatusEnabled || status == SMAppServiceStatusRequiresApproval;
    }
    return false;
}

bool setLaunchAtLogin(bool enable, QString* error)
{
    if (@available(macOS 13.0, *)) {
        SMAppService* service = SMAppService.mainAppService;
        NSError* err = nil;
        const BOOL ok = enable ? [service registerAndReturnError:&err] : [service unregisterAndReturnError:&err];
        if (!ok) {
            if (error)
                *error = QString::fromNSString(err.localizedDescription ?: @"unknown error");
            return false;
        }
        if (enable && service.status == SMAppServiceStatusRequiresApproval)
            [SMAppService openSystemSettingsLoginItems];
        return true;
    }
    if (error)
        *error = QStringLiteral("Requires macOS 13 or later");
    return false;
}

} // namespace native
