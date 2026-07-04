// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureGlUi. PureGlUi is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureGlUi is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureGlUi. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "common/bit.h"
#include "ui/gl/localglew.h"
#include "ui/res/type/border.h"
#include "ui/window/platform/macosevent.h"
#include "ui/window/windowbase.h"

#include <iostream>

// NOLINTBEGIN(clang-diagnostic-import-preprocessor-directive-pedantic)
// Workaround for Xcode 16.4 SDK regression: Icons.h in HIServices uses CALLBACK_API
// which is undefined in 64-bit C++ mode, causing "use of undeclared identifier" errors.
// Defining __ICONS__ prevents the broken Icons.h from being processed.
#define __ICONS__
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
// NOLINTEND(clang-diagnostic-import-preprocessor-directive-pedantic)

namespace Ui::Window::Platform {

/**
 * @brief macOS/Cocoa window implementation
 *
 * Implements Ui::IWindow for macOS using Cocoa for window management and NSOpenGL for OpenGL context.
 */
class MacOsWindow : public WindowBase<MacOsWindow> {
public:
    explicit MacOsWindow(Ui::PubSub::Subscribe& subscribe, id_t subscribeId = Ui::INVALID_ID)
        : WindowBase(subscribe, subscribeId)
    {
    }
    ~MacOsWindow() override
    {
        if (m_renderer) {
            m_renderer->cleanup();
        }
        // Tear the renderer down while the GL context is still alive.
        // ~WindowBase would otherwise destroy it *after* destroy() has released the NSOpenGLView,
        // leaving the renderer's ~dtor calling glDelete* against a dead context.
        setRenderer(nullptr);
        // Qualified: in a destructor virtual dispatch stops at this class anyway;
        // spelling it out documents that and keeps derived overrides out of play.
        MacOsWindow::destroy();
    }

    // -------- Ui::IWindow implementation --------

    [[nodiscard]] NativeDisplayHandle nativeDisplay() const override { return nullptr; }

    // Core 3.2 pixel format. Tries hardware acceleration first; on failure falls back to
    // software. Returns the format paired with a flag: true = hardware-accelerated, false =
    // software fallback. Callers store the flag on m_isHardwareGl so downstream renderers
    // can gate features that misbehave on software GL.
    static std::pair<NSOpenGLPixelFormat*, bool> pickPixelFormat()
    {
        NSOpenGLPixelFormatAttribute acceleratedAttrs[] = { NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFADepthSize, 24,
            NSOpenGLPFADoubleBuffer, NSOpenGLPFAAccelerated,
            0 };
        NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:acceleratedAttrs];
        if (pixelFormat) {
            return { pixelFormat, true };
        }
        NSOpenGLPixelFormatAttribute softwareAttrs[] = { NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFADepthSize, 24,
            NSOpenGLPFADoubleBuffer,
            0 };
        pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:softwareAttrs];
        if (pixelFormat) {
            std::cout << "[MacOsWindow] Using software OpenGL renderer" << std::endl;
        }
        return { pixelFormat, false };
    }

    // Convert our top-left-origin bounds (physical px) to the subview's bottom-left-origin
    // frame (points) inside `parentContentView`.
    static NSRect flippedChildFrame(NSView* parentContentView, const Ui::Res::Type::bound_t& bound)
    {
        const CGFloat bsf = backingScaleFactor();
        const CGFloat parentH = parentContentView.bounds.size.height;
        const CGFloat wPts = bound.w / bsf;
        const CGFloat hPts = bound.h / bsf;
        const CGFloat xPts = bound.x / bsf;
        const CGFloat yPts = parentH - (bound.y / bsf) - hPts;
        return NSMakeRect(xPts, yPts, wPts, hPts);
    }

    // Embedded mode: create NSOpenGLView as a subview of `parent`'s contentView instead of a
    // top-level NSWindow. Used for content surfaces that must live inside the main UI window,
    // not float as separate windows.
    bool createAsChildView(fpx_t width, fpx_t height, NSWindow* parent)
    {
        m_bound.w = width;
        m_bound.h = height;

        @autoreleasepool {
            NSView* parentContent = [parent contentView];
            NSRect frame = flippedChildFrame(parentContent, m_bound);

            auto [pixelFormat, accelerated] = pickPixelFormat();
            if (!pixelFormat) {
                std::cerr << "[MacOsWindow] Child view pixel format init failed" << std::endl;
                return false;
            }
            m_isHardwareGl = accelerated;
            m_glView = [[NSOpenGLView alloc] initWithFrame:frame pixelFormat:pixelFormat];
            if (!m_glView) {
                std::cerr << "[MacOsWindow] Child NSOpenGLView alloc/init returned nil" << std::endl;
                return false;
            }
            [parentContent addSubview:m_glView];
            NSOpenGLContext* initialCtx = [m_glView openGLContext];
            [initialCtx makeCurrentContext];
            const NSSize framePts = [m_glView frame].size;
            const NSSize backingPx = [m_glView convertSizeToBacking:framePts];
            std::cout << "[MacOsWindow] Child view created: view=" << (__bridge void*)m_glView
                      << " initialCtx=" << (__bridge void*)initialCtx
                      << " framePts=" << framePts.width << "x" << framePts.height
                      << " backingPx=" << backingPx.width << "x" << backingPx.height
                      << " wantsBestRes=" << ([m_glView wantsBestResolutionOpenGLSurface] ? "YES" : "NO")
                      << " m_bound=" << m_bound.w << "x" << m_bound.h
                      << std::endl;
            return true;
        }
    }

    bool create(fpx_t width, fpx_t height, NativeDisplayHandle /*display*/, NativeWindowHandle parentWindow,
        const std::string& title) override
    {
        // Parent given -> embed as a subview of the parent window (NativeWindowHandle is NSView*).
        if (parentWindow) {
            NSWindow* parentNs = [parentWindow window];
            if (!parentNs) {
                std::cerr << "[MacOsWindow] Parent NSView has no window" << std::endl;
                return false;
            }
            return createAsChildView(width, height, parentNs);
        }

        m_bound.w = width;
        m_bound.h = height;

        @autoreleasepool {
            // Bootstrap NSApplication before creating any NSWindow.
            // Without this AppKit is not initialized and NSWindow creation yields a non-functional window.
            NSApplication* app = [NSApplication sharedApplication];
            [app setActivationPolicy:NSApplicationActivationPolicyRegular];

            // Without an .app bundle the Dock / menu bar show the executable name
            // (`pureglui`). Override the process name so "PureGlUi" appears instead.
            [[NSProcessInfo processInfo] setProcessName:[NSString stringWithUTF8String:title.c_str()]];

            // NSWindow frames are in POINTS, not pixels. `width`/`height` are physical pixels
            // (matching X11 semantics); divide by backingScaleFactor to get points.
            const CGFloat bsf = backingScaleFactor();
            NSRect frame = NSMakeRect(0, 0, width / bsf, height / bsf);
            NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
            m_nsWindow = [[NSWindow alloc] initWithContentRect:frame
                                                     styleMask:style
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
            if (!m_nsWindow) {
                std::cerr << "[MacOsWindow] NSWindow alloc/init returned nil" << std::endl;
                return false;
            }
            // Default releasedWhenClosed=YES deallocates the NSWindow (and view + GL context)
            // the moment the user clicks the red close button, before our shutdown path runs.
            // We want to drive lifetime via destroy(), so disable it.
            [m_nsWindow setReleasedWhenClosed:NO];
            [m_nsWindow setTitle:[NSString stringWithUTF8String:title.c_str()]];
            [m_nsWindow center];

            auto [pixelFormat, accelerated] = pickPixelFormat();
            if (!pixelFormat) {
                std::cerr << "[MacOsWindow] NSOpenGLPixelFormat init failed (Core 3.2 unavailable)" << std::endl;
                return false;
            }
            m_isHardwareGl = accelerated;
            m_glView = [[NSOpenGLView alloc] initWithFrame:frame pixelFormat:pixelFormat];
            if (!m_glView) {
                std::cerr << "[MacOsWindow] NSOpenGLView alloc/init returned nil" << std::endl;
                return false;
            }
            [m_nsWindow setContentView:m_glView];
            [[m_glView openGLContext] makeCurrentContext];
            {
                const NSSize framePts = [m_glView frame].size;
                const NSSize backingPx = [m_glView convertSizeToBacking:framePts];
                std::cout << "[MacOsWindow] Top-level view created: view=" << (__bridge void*)m_glView
                          << " framePts=" << framePts.width << "x" << framePts.height
                          << " backingPx=" << backingPx.width << "x" << backingPx.height
                          << " wantsBestRes=" << ([m_glView wantsBestResolutionOpenGLSurface] ? "YES" : "NO")
                          << " m_bound=" << m_bound.w << "x" << m_bound.h
                          << std::endl;
            }

            applyRoundedCorners();

            // Observe close / terminate so the main loop sees a CloseRequested event.
            NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
            m_closeObserver = [nc addObserverForName:NSWindowWillCloseNotification
                                              object:m_nsWindow
                                               queue:nil
                                          usingBlock:^(NSNotification*) {
                                              g_macCloseRequested.store(true);
                                          }];
            m_terminateObserver = [nc addObserverForName:NSApplicationWillTerminateNotification
                                                  object:app
                                                   queue:nil
                                              usingBlock:^(NSNotification*) {
                                                  g_macCloseRequested.store(true);
                                              }];
            m_resizeObserver = [nc addObserverForName:NSWindowDidResizeNotification
                                               object:m_nsWindow
                                                queue:nil
                                           usingBlock:^(NSNotification* note) {
                                               NSWindow* win = note.object;
                                               const NSSize pts = [[win contentView] bounds].size;
                                               const CGFloat bsf = macBackingScaleFactor();
                                               g_macResizeWidth.store(static_cast<int>(pts.width * bsf));
                                               g_macResizeHeight.store(static_cast<int>(pts.height * bsf));
                                               g_macResizePending.store(true);
                                           }];

            [m_nsWindow makeKeyAndOrderFront:nil];
            [app activateIgnoringOtherApps:YES];
            [app finishLaunching];

            return true;
        }
    }

    [[nodiscard]] bool isValid() const override { return m_glView != nullptr; }

    // Returns the NSOpenGLView (the drawable). Consumers needing the NSWindow
    // resolve it via [view window] - works for both top-level (contentView of our m_nsWindow)
    // and embedded subviews (resolves up to m_parentNsWindow).
    [[nodiscard]] NativeWindowHandle nativeHandle() const override { return m_glView; }

    void destroy() override
    {
        @autoreleasepool {
            NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
            if (m_closeObserver) {
                [nc removeObserver:m_closeObserver];
                m_closeObserver = nil;
            }
            if (m_terminateObserver) {
                [nc removeObserver:m_terminateObserver];
                m_terminateObserver = nil;
            }
            if (m_resizeObserver) {
                [nc removeObserver:m_resizeObserver];
                m_resizeObserver = nil;
            }
            if (m_nsWindow) {
                // Popup NSWindows are retained by their parent via addChildWindow:; nilling our
                // reference alone leaves the popup visible. Detach + close so the parent drops
                // its strong reference.
                NSWindow* parent = [m_nsWindow parentWindow];
                if (parent) {
                    [parent removeChildWindow:m_nsWindow];
                    [m_nsWindow close];
                }
            } else if (m_glView) {
                // Embedded subview: remove from superview so parent releases it.
                [m_glView removeFromSuperview];
            }
            // NSOpenGLView owns its openGLContext; it is released when m_glView is deallocated.
            m_glView = nil;
            m_nsWindow = nil;
        }
    }

    // Retina multiplier. 1.0 on non-Retina displays, 2.0 on Retina Macs.
    // On macOS this IS the app scale factor - DPI is a separate concept.
    static CGFloat backingScaleFactor()
    {
        NSScreen* screen = [NSScreen mainScreen];
        return screen ? [screen backingScaleFactor] : 1.0;
    }

    // Create a borderless popup NSWindow as a child of `parent`, shared GL context.
    // screenX/screenY are top-left in physical pixels; width/height are physical pixels.
    // Leaves m_bound.x/y untouched (callers set those via setPosition() as parent-relative).
    bool createAsPopup(int screenX, int screenY, fpx_t width, fpx_t height, NSWindow* parent)
    {
        if (!parent) {
            std::cerr << "[MacOsWindow] createAsPopup requires parent NSWindow" << std::endl;
            return false;
        }

        m_bound.w = width;
        m_bound.h = height;

        @autoreleasepool {
            const CGFloat bsf = backingScaleFactor();
            // Convert screen-top-left physical pixels -> screen-bottom-left points (NSScreen coord system).
            const CGFloat screenHeightPts = [[NSScreen mainScreen] frame].size.height;
            const CGFloat xPts = screenX / bsf;
            const CGFloat yPts = screenHeightPts - (screenY / bsf) - (height / bsf);
            NSRect frame = NSMakeRect(xPts, yPts, width / bsf, height / bsf);

            m_nsWindow = [[NSWindow alloc] initWithContentRect:frame
                                                     styleMask:NSWindowStyleMaskBorderless
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
            if (!m_nsWindow) {
                std::cerr << "[MacOsWindow] Popup NSWindow alloc/init returned nil" << std::endl;
                return false;
            }
            [m_nsWindow setReleasedWhenClosed:NO];
            [m_nsWindow setLevel:NSPopUpMenuWindowLevel];
            [m_nsWindow setOpaque:NO];
            [m_nsWindow setBackgroundColor:[NSColor clearColor]];
            [m_nsWindow setHasShadow:NO];
            [m_nsWindow setIgnoresMouseEvents:NO];
            [m_nsWindow setAcceptsMouseMovedEvents:YES];

            // Share GL pixel format with the parent for context sharing (works with either HW or SW renderer).
            NSOpenGLPixelFormatAttribute attrs[] = { NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
                NSOpenGLPFAColorSize, 24,
                NSOpenGLPFAAlphaSize, 8,
                NSOpenGLPFADepthSize, 24,
                NSOpenGLPFADoubleBuffer,
                0 };
            NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
            if (!pixelFormat) {
                std::cerr << "[MacOsWindow] Popup pixel format init failed" << std::endl;
                return false;
            }
            m_glView = [[NSOpenGLView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height)
                                               pixelFormat:pixelFormat];
            if (!m_glView) {
                std::cerr << "[MacOsWindow] Popup NSOpenGLView alloc/init returned nil" << std::endl;
                return false;
            }
            [m_nsWindow setContentView:m_glView];
            [[m_glView openGLContext] makeCurrentContext];

            // Child-window relationship: popup closes/moves with parent and gets proper ordering.
            // addChildWindow does NOT order-front by itself if we haven't mapped the window.
            [parent addChildWindow:m_nsWindow ordered:NSWindowAbove];

            applyRoundedCorners();
            // Do NOT orderFront here. Caller renders the first frame first, then calls show().
            // That avoids a flash of empty borderless window on dialog/popup open.

            return true;
        }
    }

    static int queryDpi(NativeDisplayHandle /*nativeDisplay*/)
    {
        // g_scale = g_dpi / 96, so return 96 * backingScaleFactor to produce the correct scale.
        const CGFloat bsf = backingScaleFactor();
        const int dpiVal = static_cast<int>(96.0 * bsf);
        std::cout << "[MacOsWindow] DPI: " << dpiVal << " (backingScaleFactor=" << bsf << ")" << std::endl;
        return dpiVal;
    }

    void resize(fpx_t width, fpx_t height) override
    {
        m_bound.w = width;
        m_bound.h = height;
        this->requestRender();
        const CGFloat bsf = backingScaleFactor();
        if (m_nsWindow) {
            NSRect frame = [m_nsWindow frame];
            frame.size = NSMakeSize(width / bsf, height / bsf);
            [m_nsWindow setFrame:frame display:YES];
            applyRoundedCorners();
        } else if (m_glView) {
            [m_glView setFrame:flippedChildFrame(m_glView.superview, m_bound)];
            [[m_glView openGLContext] update];
        }
    }

    void move(fpx_t x, fpx_t y) override
    {
        m_bound.x = x;
        m_bound.y = y;
        const CGFloat bsf = backingScaleFactor();
        if (m_nsWindow) {
            NSRect frame = [m_nsWindow frame];
            frame.origin = NSMakePoint(x / bsf, y / bsf);
            [m_nsWindow setFrame:frame display:YES];
        } else if (m_glView) {
            [m_glView setFrame:flippedChildFrame(m_glView.superview, m_bound)];
        }
    }

    void show() override
    {
        if (m_nsWindow) {
            [m_nsWindow orderFront:nil];
            // NSOpenGLView lazily attaches its NSOpenGLContext to the view inside
            // -drawRect:. We render via direct flushBuffer and skip that path, so
            // without an explicit bind the context's view stays nil and every swap
            // silently drops (glError 0x506, GL_INVALID_FRAMEBUFFER_OPERATION).
            // Popups self-heal once mouse traffic tickles AppKit; a cold-open dialog
            // sits invisible until a user event. Bind explicitly now that the
            // window is visible so subsequent swaps reach the drawable.
            // NSOpenGLContext.setView: is deprecated (macOS 10.14) alongside
            // NSOpenGLContext itself - we accept that since the whole GL path here
            // is legacy; Metal migration is a separate concern.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            NSOpenGLContext* ctx = [m_glView openGLContext];
            if ([ctx view] != m_glView) {
                [ctx setView:m_glView];
            }
#pragma clang diagnostic pop
            [m_nsWindow display];
        } else if (m_glView) {
            [m_glView setHidden:NO];
        }
    }

    void hide() override
    {
        if (m_nsWindow) {
            [m_nsWindow orderOut:nil];
        } else if (m_glView) {
            [m_glView setHidden:YES];
        }
    }

    void moveResize(const Ui::Res::Type::bound_t& bound) override
    {
        m_bound = bound;
        this->requestRender();
        const CGFloat bsf = backingScaleFactor();
        if (m_nsWindow) {
            NSRect frame = NSMakeRect(bound.x / bsf, bound.y / bsf, bound.w / bsf, bound.h / bsf);
            [m_nsWindow setFrame:frame display:YES];
            applyRoundedCorners();
        } else if (m_glView) {
            [m_glView setFrame:flippedChildFrame(m_glView.superview, m_bound)];
            [[m_glView openGLContext] update];
        }
    }

    void setTitle(const std::string& title) override
    {
        if (m_nsWindow) {
            [m_nsWindow setTitle:[NSString stringWithUTF8String:title.c_str()]];
        }
        // Embedded views have no title.
        (void)title;
    }

    // NSApp drives the app-wide Dock / menu bar icon; window-level icons do not exist on macOS.
    // The symbolic variant (used on X11 for small-tray rendering) is ignored.
    void setWindowIcon(const std::string& mainIconPath, const std::string& /*symbolicIconPath*/) override
    {
        if (mainIconPath.empty()) {
            return;
        }
        @autoreleasepool {
            NSString* path = [NSString stringWithUTF8String:mainIconPath.c_str()];
            NSImage* image = [[NSImage alloc] initWithContentsOfFile:path];
            if (!image) {
                std::cerr << "[MacOsWindow] Failed to load icon: " << mainIconPath << std::endl;
                return;
            }
            [[NSApplication sharedApplication] setApplicationIconImage:image];
        }
    }

    void screenPosition(int& screenX, int& screenY) const override
    {
        // Return top-left of the content in physical pixels (matches X11/Win32).
        const CGFloat bsf = backingScaleFactor();
        const CGFloat screenHeightPts = [[NSScreen mainScreen] frame].size.height;
        if (m_nsWindow) {
            // NSWindow.frame includes the title bar; our UI coordinates start at the content rect.
            const NSRect contentRect = [m_nsWindow contentRectForFrameRect:[m_nsWindow frame]];
            screenX = static_cast<int>(contentRect.origin.x * bsf);
            screenY = static_cast<int>((screenHeightPts - contentRect.origin.y - contentRect.size.height) * bsf);
            return;
        }
        if (m_glView) {
            // Embedded subview: convert its local origin to screen coords via its window.
            NSWindow* parent = [m_glView window];
            if (parent) {
                const NSRect viewFrame = [m_glView frame];
                const NSRect windowRect = [m_glView.superview convertRect:viewFrame toView:nil];
                const NSRect screenRect = [parent convertRectToScreen:windowRect];
                screenX = static_cast<int>(screenRect.origin.x * bsf);
                screenY = static_cast<int>((screenHeightPts - screenRect.origin.y - screenRect.size.height) * bsf);
                return;
            }
        }
        screenX = 0;
        screenY = 0;
    }

    // Always go through the view's current openGLContext: a content surface's renderer calls
    // -[NSOpenGLContext setView:] on its own context, detaching the one captured at view
    // creation time. Using the view-resolved context keeps makeCurrent / flushBuffer
    // targeting the context that is actually attached to the drawable.
    void makeCurrent() override
    {
        NSOpenGLContext* ctx = [m_glView openGLContext];
        [ctx makeCurrentContext];
        // Log only on context transitions so frame-rate logging does not flood the console.
        if (m_lastCtxLog != (__bridge void*)ctx) {
            std::cout << "[MacOsWindow] makeCurrent: view=" << (__bridge void*)m_glView
                      << " ctx=" << (__bridge void*)ctx << std::endl;
            m_lastCtxLog = (__bridge void*)ctx;
        }
    }

    void swapBuffers() override
    {
        [[m_glView openGLContext] flushBuffer];
    }

    void clear() override
    {
        makeCurrent();
        glClear(Common::Bit::Or(GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT));
    }

    // -------- macOS-specific methods (called by WindowBase via CRTP) --------

    // Apply per-corner mask via CAShapeLayer. Used for popups/submenus/dialogs with per-corner radii
    // (some corners flat). Values are in physical pixels (converted to points internally).
    void applyCornerMask(const Ui::Res::Type::border_t& radii)
    {
        if (!m_nsWindow) {
            return;
        }
        NSView* content = [m_nsWindow contentView];
        [content setWantsLayer:YES];

        const CGFloat bsf = backingScaleFactor();
        const CGFloat w = content.bounds.size.width;
        const CGFloat h = content.bounds.size.height;
        const CGFloat tl = radii.topLeft / bsf;
        const CGFloat tr = radii.topRight / bsf;
        const CGFloat br = radii.bottomRight / bsf;
        const CGFloat bl = radii.bottomLeft / bsf;

        // CALayer coordinate system is bottom-left origin, so "top" in CSS = max Y here.
        NSBezierPath* path = [NSBezierPath bezierPath];
        [path moveToPoint:NSMakePoint(bl, 0)];
        [path lineToPoint:NSMakePoint(w - br, 0)];
        if (br > 0) {
            [path appendBezierPathWithArcWithCenter:NSMakePoint(w - br, br) radius:br startAngle:270 endAngle:360];
        }
        [path lineToPoint:NSMakePoint(w, h - tr)];
        if (tr > 0) {
            [path appendBezierPathWithArcWithCenter:NSMakePoint(w - tr, h - tr) radius:tr startAngle:0 endAngle:90];
        }
        [path lineToPoint:NSMakePoint(tl, h)];
        if (tl > 0) {
            [path appendBezierPathWithArcWithCenter:NSMakePoint(tl, h - tl) radius:tl startAngle:90 endAngle:180];
        }
        [path lineToPoint:NSMakePoint(0, bl)];
        if (bl > 0) {
            [path appendBezierPathWithArcWithCenter:NSMakePoint(bl, bl) radius:bl startAngle:180 endAngle:270];
        }
        [path closePath];

        CAShapeLayer* mask = [CAShapeLayer layer];
        mask.frame = content.bounds;
        // NSBezierPath.CGPath exists since macOS 14; convert manually for older targets.
        CGMutablePathRef cg = CGPathCreateMutable();
        NSInteger n = [path elementCount];
        for (NSInteger i = 0; i < n; ++i) {
            NSPoint pts[3];
            switch ([path elementAtIndex:i associatedPoints:pts]) {
            case NSBezierPathElementMoveTo:
                CGPathMoveToPoint(cg, nullptr, pts[0].x, pts[0].y);
                break;
            case NSBezierPathElementLineTo:
                CGPathAddLineToPoint(cg, nullptr, pts[0].x, pts[0].y);
                break;
            case NSBezierPathElementCubicCurveTo:
                CGPathAddCurveToPoint(cg, nullptr, pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
                break;
            case NSBezierPathElementQuadraticCurveTo:
                CGPathAddQuadCurveToPoint(cg, nullptr, pts[0].x, pts[0].y, pts[1].x, pts[1].y);
                break;
            case NSBezierPathElementClosePath:
                CGPathCloseSubpath(cg);
                break;
            default:
                break;
            }
        }
        mask.path = cg;
        CGPathRelease(cg);
        content.layer.mask = mask;
    }

    void applyRoundedCorners()
    {
        if (!m_nsWindow) {
            return;
        }
        NSView* content = [m_nsWindow contentView];

        // Borderless windows (popups, submenus, dialogs) use a CAShapeLayer mask for
        // per-corner radii (e.g. dropdown TL flat). A uniform CALayer.cornerRadius clip
        // would intersect the mask and round flattened corners anyway. Skip.
        // NSWindowStyleMaskBorderless is defined as 0, so equality is the correct test.
        const NSWindowStyleMask style = [m_nsWindow styleMask];
        if (style == NSWindowStyleMaskBorderless) {
            if (content.layer) {
                content.layer.cornerRadius = 0;
                content.layer.masksToBounds = NO;
            }
            return;
        }

        if (m_rounded && m_cornerRadius > 0) {
            [m_nsWindow setTitleVisibility:NSWindowTitleHidden];
            [m_nsWindow setTitlebarAppearsTransparent:YES];
            [m_nsWindow setStyleMask:([m_nsWindow styleMask] | NSWindowStyleMaskFullSizeContentView)];
            [content setWantsLayer:YES];
            content.layer.cornerRadius = m_cornerRadius;
            content.layer.masksToBounds = YES;
        } else {
            if (content.layer) {
                content.layer.cornerRadius = 0;
                content.layer.masksToBounds = NO;
            }
        }
    }

private:
    NSWindow* m_nsWindow = nullptr;
    NSOpenGLView* m_glView = nullptr;
    id<NSObject> m_closeObserver = nullptr;
    id<NSObject> m_terminateObserver = nullptr;
    id<NSObject> m_resizeObserver = nullptr;
    // Diagnostic: logs makeCurrent only when the resolved context changes, to detect
    // content-surface setView swaps and context detach issues without flooding logs per frame.
    mutable void* m_lastCtxLog = nullptr;
};

} // namespace Ui::Window::Platform
