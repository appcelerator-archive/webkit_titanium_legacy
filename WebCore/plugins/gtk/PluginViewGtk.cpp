/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2009, 2010 Kakai, Inc. <brian@kakai.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginView.h"

#include "Bridge.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HostWindow.h"
#include "Image.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PluginDebug.h"
#include "PluginMainThreadScheduler.h"
#include "PluginPackage.h"
#include "RenderLayer.h"
#include "Settings.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>

#include <gdkconfig.h>
#include <gtk/gtk.h>

#if defined(XP_UNIX)
#include "gtk2xtbin.h"
#define Bool int // this got undefined somewhere
#define Status int // ditto
#include <X11/extensions/Xrender.h>
#include <cairo/cairo-xlib.h>
#include <gdk/gdkx.h>
#elif defined(GDK_WINDOWING_WIN32)
#include "PluginMessageThrottlerWin.h"
#include <gdk/gdkwin32.h>
#endif

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

bool PluginView::dispatchNPEvent(NPEvent& event)
{
    // sanity check
    if (!m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);

    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
    return accepted;
}

#if defined(XP_UNIX)
static Window getRootWindow(Frame* parentFrame)
{
    GtkWidget* parentWidget = parentFrame->view()->hostWindow()->platformPageClient();
    GdkScreen* gscreen = gtk_widget_get_screen(parentWidget);
    return GDK_WINDOW_XWINDOW(gdk_screen_get_root_window(gscreen));
}
#endif

void PluginView::updatePluginWidget()
{
    if (!parent())
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    if (platformPluginWidget() && (m_windowRect != oldWindowRect || m_clipRect != oldClipRect))
        setNPWindowIfNeeded();

#if defined(XP_UNIX)
    if (!m_isWindowed && m_windowRect.size() != oldWindowRect.size()) {
        if (m_drawable)
            XFreePixmap(GDK_DISPLAY(), m_drawable);

        m_drawable = XCreatePixmap(GDK_DISPLAY(), getRootWindow(m_parentFrame.get()),
                                   m_windowRect.width(), m_windowRect.height(),
                                   ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->depth);
        XSync(GDK_DISPLAY(), False); // make sure that the server knows about the Drawable
    }

    // do not call setNPWindowIfNeeded() immediately, will be called on paint()
    m_hasPendingGeometryChange = true;
#endif

    // In order to move/resize the plugin window at the same time as the
    // rest of frame during e.g. scrolling, we set the window geometry
    // in the paint() function, but as paint() isn't called when the
    // plugin window is outside the frame which can be caused by a
    // scroll, we need to move/resize immediately.
    if (!m_windowRect.intersects(frameView->frameRect()))
        setNPWindowIfNeeded();
}

void PluginView::setFocus()
{
    if (platformPluginWidget())
        gtk_widget_grab_focus(platformPluginWidget());

    Widget::setFocus();
}

void PluginView::show()
{
    setSelfVisible(true);

    if (isParentVisible() && platformPluginWidget())
        gtk_widget_show(platformPluginWidget());

    Widget::show();
}

void PluginView::hide()
{
    setSelfVisible(false);

    if (isParentVisible() && platformPluginWidget())
        gtk_widget_hide(platformPluginWidget());

    Widget::hide();
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        paintMissingPluginIcon(context, rect);
        return;
    }

    if (context->paintingDisabled())
        return;

    setNPWindowIfNeeded();

    if (m_isWindowed)
        return;

#if defined(XP_UNIX)
    if (!m_drawable)
        return;

    const bool syncX = m_pluginDisplay && m_pluginDisplay != GDK_DISPLAY();

    IntRect exposedRect(rect);
    exposedRect.intersect(frameRect());
    exposedRect.move(-frameRect().x(), -frameRect().y());

    Window dummyW;
    int dummyI;
    unsigned int dummyUI, actualDepth = 0;
    XGetGeometry(GDK_DISPLAY(), m_drawable, &dummyW, &dummyI, &dummyI,
                 &dummyUI, &dummyUI, &dummyUI, &actualDepth);

    const unsigned int drawableDepth = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->depth;
    ASSERT(drawableDepth == actualDepth);

    cairo_surface_t* drawableSurface = cairo_xlib_surface_create(GDK_DISPLAY(),
                                                                 m_drawable,
                                                                 m_visual,
                                                                 m_windowRect.width(),
                                                                 m_windowRect.height());

    if (m_isTransparent && drawableDepth != 32) {
        // Attempt to fake it when we don't have an alpha channel on our
        // pixmap.  If that's not possible, at least clear the window to
        // avoid drawing artifacts.
        GtkWidget* widget = m_parentFrame->view()->hostWindow()->platformPageClient();
        GdkDrawable* gdkBackingStore = 0;
        gint xoff = 0, yoff = 0;

        gdk_window_get_internal_paint_info(widget->window, &gdkBackingStore, &xoff, &yoff);

        GC gc = XDefaultGC(GDK_DISPLAY(), gdk_screen_get_number(gdk_screen_get_default()));
        if (gdkBackingStore) {
            XCopyArea(GDK_DISPLAY(), GDK_DRAWABLE_XID(gdkBackingStore), m_drawable, gc,
                      m_windowRect.x() + exposedRect.x() - xoff,
                      m_windowRect.y() + exposedRect.y() - yoff,
                      exposedRect.width(), exposedRect.height(),
                      exposedRect.x(), exposedRect.y());
        } else {
            // no valid backing store; clear to the background color
            XFillRectangle(GDK_DISPLAY(), m_drawable, gc,
                           exposedRect.x(), exposedRect.y(),
                           exposedRect.width(), exposedRect.height());
        }
    } else if (m_isTransparent) {
        // If we have a 32 bit drawable and the plugin wants transparency,
        // we'll clear the exposed area to transparent first.  Otherwise,
        // we'd end up with junk in there from the last paint, or, worse,
        // uninitialized data.
        cairo_t* crFill = cairo_create(drawableSurface);

        cairo_set_operator(crFill, CAIRO_OPERATOR_SOURCE);
        cairo_pattern_t* fill = cairo_pattern_create_rgba(0., 0., 0., 0.);
        cairo_set_source(crFill, fill);

        cairo_rectangle(crFill, exposedRect.x(), exposedRect.y(),
                        exposedRect.width(), exposedRect.height());
        cairo_clip(crFill);
        cairo_paint(crFill);

        cairo_destroy(crFill);
        cairo_pattern_destroy(fill);
    }

    XEvent xevent;
    memset(&xevent, 0, sizeof(XEvent));
    XGraphicsExposeEvent& exposeEvent = xevent.xgraphicsexpose;
    exposeEvent.type = GraphicsExpose;
    exposeEvent.display = GDK_DISPLAY();
    exposeEvent.drawable = m_drawable;
    exposeEvent.x = exposedRect.x();
    exposeEvent.y = exposedRect.y();
    exposeEvent.width = exposedRect.x() + exposedRect.width(); // flash bug? it thinks width is the right in transparent mode
    exposeEvent.height = exposedRect.y() + exposedRect.height(); // flash bug? it thinks height is the bottom in transparent mode

    dispatchNPEvent(xevent);

    if (syncX)
        XSync(m_pluginDisplay, False); // sync changes by plugin

    cairo_t* cr = context->platformContext();
    cairo_save(cr);

    cairo_set_source_surface(cr, drawableSurface, frameRect().x(), frameRect().y());

    cairo_rectangle(cr,
                    frameRect().x() + exposedRect.x(), frameRect().y() + exposedRect.y(),
                    exposedRect.width(), exposedRect.height());
    cairo_clip(cr);

    if (m_isTransparent)
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    else
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    cairo_restore(cr);
    cairo_surface_destroy(drawableSurface);
#endif // defined(XP_UNIX)
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);

    if (m_isWindowed)
        return;

    if (event->type() != eventNames().keydownEvent && event->type() != eventNames().keyupEvent)
        return;

    NPEvent xEvent;
#if defined(XP_UNIX)
    GdkEventKey* gdkEvent = event->keyEvent()->gdkEventKey();

    xEvent.type = (event->type() == eventNames().keydownEvent) ? 2 : 3; // KeyPress/Release get unset somewhere
    xEvent.xkey.root = getRootWindow(m_parentFrame.get());
    xEvent.xkey.subwindow = 0; // we have no child window
    xEvent.xkey.time = event->timeStamp();
    xEvent.xkey.state = gdkEvent->state; // GdkModifierType mirrors xlib state masks
    xEvent.xkey.keycode = gdkEvent->hardware_keycode;
    xEvent.xkey.same_screen = true;

    // NOTE: As the XEvents sent to the plug-in are synthesized and there is not a native window
    // corresponding to the plug-in rectangle, some of the members of the XEvent structures are not
    // set to their normal Xserver values. e.g. Key events don't have a position.
    // source: https://developer.mozilla.org/en/NPEvent
    xEvent.xkey.x = 0;
    xEvent.xkey.y = 0;
    xEvent.xkey.x_root = 0;
    xEvent.xkey.y_root = 0;
#endif

    if (!dispatchNPEvent(xEvent))
        event->setDefaultHandled();
}

#if defined(XP_UNIX)
static unsigned int inputEventState(MouseEvent* event)
{
    unsigned int state = 0;
    if (event->ctrlKey())
        state |= ControlMask;
    if (event->shiftKey())
        state |= ShiftMask;
    if (event->altKey())
        state |= Mod1Mask;
    if (event->metaKey())
        state |= Mod4Mask;
    return state;
}

void PluginView::initXEvent(XEvent* xEvent)
{
    memset(xEvent, 0, sizeof(XEvent));

    xEvent->xany.serial = 0; // we are unaware of the last request processed by X Server
    xEvent->xany.send_event = false;
    xEvent->xany.display = GDK_DISPLAY();
    // NOTE: event->xany.window doesn't always correspond to the .window property of other XEvent's
    // but does in the case of KeyPress, KeyRelease, ButtonPress, ButtonRelease, and MotionNotify
    // events; thus, this is right:
    GtkWidget* widget = m_parentFrame->view()->hostWindow()->platformPageClient();
    xEvent->xany.window = widget ? GDK_WINDOW_XWINDOW(widget->window) : 0;
}

static void setXButtonEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos, Frame* parentFrame)
{
    XButtonEvent& xbutton = xEvent->xbutton;
    xbutton.type = event->type() == eventNames().mousedownEvent ? ButtonPress : ButtonRelease;
    xbutton.root = getRootWindow(parentFrame);
    xbutton.subwindow = 0;
    xbutton.time = event->timeStamp();
    xbutton.x = postZoomPos.x();
    xbutton.y = postZoomPos.y();
    xbutton.x_root = event->screenX();
    xbutton.y_root = event->screenY();
    xbutton.state = inputEventState(event);
    switch (event->button()) {
    case MiddleButton:
        xbutton.button = Button2;
        break;
    case RightButton:
        xbutton.button = Button3;
        break;
    case LeftButton:
    default:
        xbutton.button = Button1;
        break;
    }
    xbutton.same_screen = true;
}

static void setXMotionEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos, Frame* parentFrame)
{
    XMotionEvent& xmotion = xEvent->xmotion;
    xmotion.type = MotionNotify;
    xmotion.root = getRootWindow(parentFrame);
    xmotion.subwindow = 0;
    xmotion.time = event->timeStamp();
    xmotion.x = postZoomPos.x();
    xmotion.y = postZoomPos.y();
    xmotion.x_root = event->screenX();
    xmotion.y_root = event->screenY();
    xmotion.state = inputEventState(event);
    xmotion.is_hint = NotifyNormal;
    xmotion.same_screen = true;
}

static void setXCrossingEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos, Frame* parentFrame)
{
    XCrossingEvent& xcrossing = xEvent->xcrossing;
    xcrossing.type = event->type() == eventNames().mouseoverEvent ? EnterNotify : LeaveNotify;
    xcrossing.root = getRootWindow(parentFrame);
    xcrossing.subwindow = 0;
    xcrossing.time = event->timeStamp();
    xcrossing.x = postZoomPos.y();
    xcrossing.y = postZoomPos.x();
    xcrossing.x_root = event->screenX();
    xcrossing.y_root = event->screenY();
    xcrossing.state = inputEventState(event);
    xcrossing.mode = NotifyNormal;
    xcrossing.detail = NotifyDetailNone;
    xcrossing.same_screen = true;
    xcrossing.focus = false;
}
#endif

void PluginView::handleMouseEvent(MouseEvent* event)
{
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);

    if (m_isWindowed)
        return;

    NPEvent xEvent;
#if defined(XP_UNIX)
    initXEvent(&xEvent);

    IntPoint postZoomPos = roundedIntPoint(m_element->renderer()->absoluteToLocal(event->absoluteLocation()));

    if (event->type() == eventNames().mousedownEvent || event->type() == eventNames().mouseupEvent)
        setXButtonEventSpecificFields(&xEvent, event, postZoomPos, m_parentFrame.get());
    else if (event->type() == eventNames().mousemoveEvent)
        setXMotionEventSpecificFields(&xEvent, event, postZoomPos, m_parentFrame.get());
    else if (event->type() == eventNames().mouseoutEvent || event->type() == eventNames().mouseoverEvent)
        setXCrossingEventSpecificFields(&xEvent, event, postZoomPos, m_parentFrame.get());
    else
        return;
#endif

    if (!dispatchNPEvent(xEvent))
        event->setDefaultHandled();
}

#if defined(XP_UNIX)
void PluginView::handleFocusInEvent()
{
    XEvent npEvent;
    initXEvent(&npEvent);

    XFocusChangeEvent& event = npEvent.xfocus;
    event.type = 9; // FocusIn gets unset somewhere
    event.mode = NotifyNormal;
    event.detail = NotifyDetailNone;

    dispatchNPEvent(npEvent);
}

void PluginView::handleFocusOutEvent()
{
    XEvent npEvent;
    initXEvent(&npEvent);

    XFocusChangeEvent& event = npEvent.xfocus;
    event.type = 10; // FocusOut gets unset somewhere
    event.mode = NotifyNormal;
    event.detail = NotifyDetailNone;

    dispatchNPEvent(npEvent);
}
#endif

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
}

void PluginView::setNPWindowRect(const IntRect&)
{
    if (!m_isWindowed)
        setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;

    // If the plugin didn't load sucessfully, no point in calling setwindow
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    // On Unix, only call plugin's setwindow if it's full-page or windowed
    if (m_mode != NP_FULL && m_mode != NP_EMBED)
        return;

    // Check if the platformPluginWidget still exists
    if (m_isWindowed && !platformPluginWidget())
        return;

#if defined(XP_UNIX)
    if (!m_hasPendingGeometryChange)
        return;
    m_hasPendingGeometryChange = false;
#endif

    if (m_isWindowed) {
        m_npWindow.x = m_windowRect.x();
        m_npWindow.y = m_windowRect.y();
        m_npWindow.width = m_windowRect.width();
        m_npWindow.height = m_windowRect.height();

        m_npWindow.clipRect.left = m_clipRect.x();
        m_npWindow.clipRect.top = m_clipRect.y();
        m_npWindow.clipRect.right = m_clipRect.width();
        m_npWindow.clipRect.bottom = m_clipRect.height();

        GtkAllocation allocation = { m_windowRect.x(), m_windowRect.y(), m_windowRect.width(), m_windowRect.height() };
        gtk_widget_size_allocate(platformPluginWidget(), &allocation);
#if defined(XP_UNIX)
        if (!m_needsXEmbed) {
            gtk_xtbin_set_position(GTK_XTBIN(platformPluginWidget()), m_windowRect.x(), m_windowRect.y());
            gtk_xtbin_resize(platformPluginWidget(), m_windowRect.width(), m_windowRect.height());
        }
#endif
    } else {
        m_npWindow.x = 0;
        m_npWindow.y = 0;

        m_npWindow.clipRect.left = 0;
        m_npWindow.clipRect.top = 0;
        m_npWindow.clipRect.right = 0;
        m_npWindow.clipRect.bottom = 0;
    }

    // FLASH WORKAROUND: Only set initially. Multiple calls to
    // setNPWindow() cause the plugin to crash in windowed mode.
    if (!m_isWindowed || m_npWindow.width == (unsigned int)-1 || m_npWindow.height == (unsigned int)-1) {
        m_npWindow.width = m_windowRect.width();
        m_npWindow.height = m_windowRect.height();
    }

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    if (isSelfVisible() && platformPluginWidget()) {
        if (visible)
            gtk_widget_show(platformPluginWidget());
        else
            gtk_widget_hide(platformPluginWidget());
    }
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    // Get file info
    if (!g_file_test ((filename.utf8()).data(), (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)))
        return NPERR_FILE_NOT_FOUND;

    //FIXME - read the file data into buffer
    FILE* fileHandle = fopen((filename.utf8()).data(), "r");
    
    if (fileHandle == 0)
        return NPERR_FILE_NOT_FOUND;

    //buffer.resize();

    int bytesRead = fread(buffer.data(), 1, 0, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    LOG(Plugins, "PluginView::getValueStatic(%s)", prettyNameForNPNVariable(variable).data());

    switch (variable) {
    case NPNVToolkit:
#if defined(XP_UNIX)
        *static_cast<uint32*>(value) = 2;
#else
        *static_cast<uint32*>(value) = 0;
#endif
        return NPERR_NO_ERROR;

    case NPNVSupportsXEmbedBool:
#if defined(XP_UNIX)
        *static_cast<NPBool*>(value) = true;
#else
        *static_cast<NPBool*>(value) = false;
#endif
        return NPERR_NO_ERROR;

    case NPNVjavascriptEnabledBool:
        *static_cast<NPBool*>(value) = true;
        return NPERR_NO_ERROR;

    case NPNVSupportsWindowless:
#if defined(XP_UNIX)
        *static_cast<NPBool*>(value) = true;
#else
        *static_cast<NPBool*>(value) = false;
#endif
        return NPERR_NO_ERROR;

    default:
        return NPERR_GENERIC_ERROR;
    }
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    LOG(Plugins, "PluginView::getValue(%s)", prettyNameForNPNVariable(variable).data());

    switch (variable) {
    case NPNVxDisplay:
#if defined(XP_UNIX)
        if (m_needsXEmbed)
            *(void **)value = (void *)GDK_DISPLAY();
        else
            *(void **)value = (void *)GTK_XTBIN(platformPluginWidget())->xtclient.xtdisplay;
        return NPERR_NO_ERROR;
#else
        return NPERR_GENERIC_ERROR;
#endif

#if defined(XP_UNIX)
    case NPNVxtAppContext:
        if (!m_needsXEmbed) {
            *(void **)value = XtDisplayToApplicationContext (GTK_XTBIN(platformPluginWidget())->xtclient.xtdisplay);

            return NPERR_NO_ERROR;
        } else
            return NPERR_GENERIC_ERROR;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
        case NPNVWindowNPObject: {
            if (m_isJavaScriptPaused)
                return NPERR_GENERIC_ERROR;

            NPObject* windowScriptObject = m_parentFrame->script()->windowScriptNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (windowScriptObject)
                _NPN_RetainObject(windowScriptObject);

            void** v = (void**)value;
            *v = windowScriptObject;
            
            return NPERR_NO_ERROR;
        }

        case NPNVPluginElementNPObject: {
            if (m_isJavaScriptPaused)
                return NPERR_GENERIC_ERROR;

            NPObject* pluginScriptObject = 0;

            if (m_element->hasTagName(appletTag) || m_element->hasTagName(embedTag) || m_element->hasTagName(objectTag))
                pluginScriptObject = static_cast<HTMLPlugInElement*>(m_element)->getNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (pluginScriptObject)
                _NPN_RetainObject(pluginScriptObject);

            void** v = (void**)value;
            *v = pluginScriptObject;

            return NPERR_NO_ERROR;
        }
#endif

        case NPNVnetscapeWindow: {
#if defined(XP_UNIX)
            void* w = reinterpret_cast<void*>(value);
            *((XID *)w) = GDK_WINDOW_XWINDOW(m_parentFrame->view()->hostWindow()->platformPageClient()->window);
#endif
#ifdef GDK_WINDOWING_WIN32
            HGDIOBJ* w = reinterpret_cast<HGDIOBJ*>(value);
            *w = GDK_WINDOW_HWND(m_parentFrame->view()->hostWindow()->platformPageClient()->window);
#endif
            return NPERR_NO_ERROR;
        }

        default:
            return getValueStatic(variable, value);
    }
}

void PluginView::invalidateRect(const IntRect& rect)
{
    if (m_isWindowed) {
        gtk_widget_queue_draw_area(GTK_WIDGET(platformPluginWidget()), rect.x(), rect.y(), rect.width(), rect.height());
        return;
    }

    invalidateWindowlessPluginRect(rect);
}

void PluginView::invalidateRect(NPRect* rect)
{
    if (!rect) {
        invalidate();
        return;
    }

    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
    invalidateWindowlessPluginRect(r);
}

void PluginView::invalidateRegion(NPRegion)
{
    // TODO: optimize
    invalidate();
}

void PluginView::forceRedraw()
{
    if (m_isWindowed)
        gtk_widget_queue_draw(platformPluginWidget());
    else
        gtk_widget_queue_draw(m_parentFrame->view()->hostWindow()->platformPageClient());
}

static Display* getPluginDisplay()
{
    // The plugin toolkit might have a different X connection open.  Since we're
    // a gdk/gtk app, we'll (probably?) have the same X connection as any gdk-based
    // plugins, so we can return that.  We might want to add other implementations here
    // later.

#if defined(XP_UNIX)
    return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
#else
    return 0;
#endif
}

static gboolean
plug_removed_cb(GtkSocket* socket, gpointer)
{
    return TRUE;
}

#if defined(XP_UNIX)
static void getVisualAndColormap(int depth, Visual** visual, Colormap* colormap)
{
    *visual = 0;
    *colormap = 0;

    int rmaj, rmin;
    if (depth == 32 && (!XRenderQueryVersion(GDK_DISPLAY(), &rmaj, &rmin) || (rmaj == 0 && rmin < 5)))
        return;

    XVisualInfo templ;
    templ.screen  = gdk_screen_get_number(gdk_screen_get_default());
    templ.depth   = depth;
    templ.c_class = TrueColor;
    int nVisuals;
    XVisualInfo* visualInfo = XGetVisualInfo(GDK_DISPLAY(), VisualScreenMask | VisualDepthMask | VisualClassMask, &templ, &nVisuals);

    if (!nVisuals)
        return;

    if (depth == 32) {
        for (int idx = 0; idx < nVisuals; ++idx) {
            XRenderPictFormat* format = XRenderFindVisualFormat(GDK_DISPLAY(), visualInfo[idx].visual);
            if (format->type == PictTypeDirect && format->direct.alphaMask) {
                 *visual = visualInfo[idx].visual;
                 break;
            }
         }
    } else
        *visual = visualInfo[0].visual;

    XFree(visualInfo);

    if (*visual)
        *colormap = XCreateColormap(GDK_DISPLAY(), GDK_ROOT_WINDOW(), *visual, AllocNone);
}
#endif

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    if (m_isWindowed) {
#if defined(XP_UNIX)
        if (m_needsXEmbed) {
            setPlatformWidget(gtk_socket_new());
            gtk_container_add(GTK_CONTAINER(m_parentFrame->view()->hostWindow()->platformPageClient()), platformPluginWidget());
            g_signal_connect(platformPluginWidget(), "plug_removed", G_CALLBACK(plug_removed_cb), NULL);
        } else
            setPlatformWidget(gtk_xtbin_new(m_parentFrame->view()->hostWindow()->platformPageClient()->window, 0));
#else
        setPlatformWidget(gtk_socket_new());
        gtk_container_add(GTK_CONTAINER(m_parentFrame->view()->hostWindow()->platformPageClient()), platformPluginWidget());
#endif
    } else {
        setPlatformWidget(0);
        m_pluginDisplay = getPluginDisplay();
    }

    show();

#if defined(XP_UNIX)
        NPSetWindowCallbackStruct* ws = new NPSetWindowCallbackStruct();
        ws->type = 0;
#endif

    if (m_isWindowed) {
        m_npWindow.type = NPWindowTypeWindow;
#if defined(XP_UNIX)
        if (m_needsXEmbed) {
            gtk_widget_realize(platformPluginWidget());
            m_npWindow.window = (void*)gtk_socket_get_id(GTK_SOCKET(platformPluginWidget()));
            ws->display = GDK_WINDOW_XDISPLAY(platformPluginWidget()->window);
            ws->visual = GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(GDK_DRAWABLE(platformPluginWidget()->window)));
            ws->depth = gdk_drawable_get_visual(GDK_DRAWABLE(platformPluginWidget()->window))->depth;
            ws->colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(GDK_DRAWABLE(platformPluginWidget()->window)));
        } else {
            m_npWindow.window = (void*)GTK_XTBIN(platformPluginWidget())->xtwindow;
            ws->display = GTK_XTBIN(platformPluginWidget())->xtdisplay;
            ws->visual = GTK_XTBIN(platformPluginWidget())->xtclient.xtvisual;
            ws->depth = GTK_XTBIN(platformPluginWidget())->xtclient.xtdepth;
            ws->colormap = GTK_XTBIN(platformPluginWidget())->xtclient.xtcolormap;
        }
        XFlush (ws->display);
#elif defined(GDK_WINDOWING_WIN32)
        m_npWindow.window = (void*)GDK_WINDOW_HWND(platformPluginWidget()->window);
#endif
    } else {
        m_npWindow.type = NPWindowTypeDrawable;
        m_npWindow.window = 0; // Not used?

#if defined(XP_UNIX)
        GdkScreen* gscreen = gdk_screen_get_default();
        GdkVisual* gvisual = gdk_screen_get_system_visual(gscreen);

        if (gvisual->depth == 32 || !m_plugin->quirks().contains(PluginQuirkRequiresDefaultScreenDepth)) {
            getVisualAndColormap(32, &m_visual, &m_colormap);
            ws->depth = 32;
        }

        if (!m_visual) {
            getVisualAndColormap(gvisual->depth, &m_visual, &m_colormap);
            ws->depth = gvisual->depth;
        }

        ws->display = GDK_DISPLAY();
        ws->visual = m_visual;
        ws->colormap = m_colormap;

        m_npWindow.x = 0;
        m_npWindow.y = 0;
        m_npWindow.width = -1;
        m_npWindow.height = -1;
#else
        notImplemented();
        m_status = PluginStatusCanNotLoadPlugin;
        return false;
#endif
    }

#if defined(XP_UNIX)
    m_npWindow.ws_info = ws;
#endif

    // TODO remove in favor of null events, like mac port?
    if (!(m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall))) {
        updatePluginWidget(); // was: setNPWindowIfNeeded(), but this doesn't produce 0x0 rects at first go
        setNPWindowIfNeeded();
    }

    return true;
}

void PluginView::platformDestroy()
{
#if defined(XP_UNIX)
    if (m_drawable) {
        XFreePixmap(GDK_DISPLAY(), m_drawable);
        m_drawable = 0;
    }
#endif
}

void PluginView::halt()
{
}

void PluginView::restart()
{
}

} // namespace WebCore

