/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#import <qwidget.h>
#import <WebCoreFrameView.h>
#import <KWQView.h>
#import <kwqdebug.h>
#import <KWQWindowWidget.h>

/*
    A QWidget rougly corresponds to an NSView.  In Qt a QFrame and QMainWindow inherit
    from a QWidget.  In Cocoa a NSWindow does not inherit from NSView.  We
    emulate QWidgets using NSViews.
*/


class QWidgetPrivate
{
public:
    QWidget::FocusPolicy focusPolicy;
    QStyle *style;
    QFont font;
    QCursor cursor;
    QPalette pal;
    NSView *view;
};

QWidget::QWidget(QWidget *parent, const char *name, int f) 
{
    static QStyle defaultStyle;
    
    data = new QWidgetPrivate;
    data->view = [[KWQView alloc] initWithFrame:NSMakeRect(0,0,0,0) widget:this];
    data->style = &defaultStyle;
}

QWidget::~QWidget() 
{
    [data->view release];
    delete data;
}

QSize QWidget::sizeHint() const 
{
    // May be overriden by subclasses.
    return QSize(0,0);
}

void QWidget::resize(int w, int h) 
{
    internalSetGeometry(pos().x(), pos().y(), w, h);
}

void QWidget::setActiveWindow() 
{
    [[data->view window] makeKeyAndOrderFront:nil];
}

void QWidget::setEnabled(bool) 
{
}

void QWidget::setAutoMask(bool) 
{
}

void QWidget::setMouseTracking(bool) 
{
}

long QWidget::winId() const
{
    return (long)this;
}

int QWidget::x() const
{
    return frameGeometry().topLeft().x();
}

int QWidget::y() const 
{
    return frameGeometry().topLeft().y();
}

int QWidget::width() const 
{ 
    return frameGeometry().size().width();
}

int QWidget::height() const 
{
    return frameGeometry().size().height();
}

QSize QWidget::size() const 
{
    return frameGeometry().size();
}

void QWidget::resize(const QSize &s) 
{
    resize(s.width(), s.height());
}

QPoint QWidget::pos() const 
{
    return frameGeometry().topLeft();
}

void QWidget::move(int x, int y) 
{
    //KWQDEBUG ("%p %s to x %d y %d\n", getView(), [[[getView() class] className] cString], x, y);
    internalSetGeometry(x, y, width(), height());
}

void QWidget::move(const QPoint &p) 
{
    move(p.x(), p.y());
}

QRect QWidget::frameGeometry() const
{
    NSView *view = getView();
    
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
    }
    NSRect vFrame = [view frame];
    return QRect((int)vFrame.origin.x, (int)vFrame.origin.y, (int)vFrame.size.width, (int)vFrame.size.height);
}

QWidget *QWidget::topLevelWidget() const 
{
    NSWindow *window = nil;
    NSView *view = getView();

    window = [view window];
    while (window == nil && view != nil) { 
	view = [view superview]; 
	window = [view window];
    }

    if (window != nil) {	
	return KWQWindowWidget::fromNSWindow(window);
    } else {
	return NULL;
    }
}

QPoint QWidget::mapToGlobal(const QPoint &p) const
{
    // This is only used by JavaScript to implement the getting
    // the screenX and screen Y coordinates.

    if (topLevelWidget() != nil) {
	return topLevelWidget()->mapToGlobal(p);
    } else {
	return p;
    }
}

void QWidget::setFocus()
{
    _logNeverImplemented();
}

void QWidget::clearFocus()
{
    _logNeverImplemented();
}

QWidget::FocusPolicy QWidget::focusPolicy() const
{
    return data->focusPolicy;
}

void QWidget::setFocusPolicy(FocusPolicy fp)
{
    data->focusPolicy = fp;
}

void QWidget::setFocusProxy(QWidget *w)
{
    if (w)
        data->focusPolicy = w->focusPolicy();
    // else?  FIXME: [rjw] we need to understand kde's focus policy.  I don't
    // think this is even relevant for us.
}

const QPalette& QWidget::palette() const
{
    return data->pal;
}

void QWidget::setPalette(const QPalette &palette)
{
    data->pal = palette;
}

void QWidget::unsetPalette()
{
    // Only called by RenderFormElement::layout, which I suspect will
    // have to be rewritten.  Do nothing.
}

QStyle &QWidget::style() const
{
    return *data->style;
}

void QWidget::setStyle(QStyle *style)
{
    // According to the Qt implementation 
    /*
    Sets the widget's GUI style to \a style. Ownership of the style
    object is not transferred.
    */
    data->style = style;
}

QFont QWidget::font() const
{
    return data->font;
}

void QWidget::setFont(const QFont &font)
{
    data->font = font;
}

void QWidget::constPolish() const
{
}

QSize QWidget::minimumSizeHint() const
{
    NSView *view = getView();
    
    if ([view isKindOfClass:[NSControl class]]) {
        NSControl *control = (NSControl *)view;
        [control sizeToFit];
        NSRect frame = [view frame];
        return QSize((int)frame.size.width, (int)frame.size.height);
    }

    return QSize(0,0);
}

bool QWidget::isVisible() const
{
    // FIXME - rewrite interms of top level widget?
    return [[data->view window] isVisible];
}

void QWidget::setCursor(const QCursor &cur)
{
    data->cursor = cur;
    
    id view = data->view;
    while (view) {
        if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) { 
            [view setCursor:data->cursor.handle()];
        }
        view = [view superview];
    }
}

QCursor QWidget::cursor()
{
    return data->cursor;
}

void QWidget::unsetCursor()
{
    setCursor(QCursor());
}

bool QWidget::event(QEvent *)
{
    return false;
}

bool QWidget::focusNextPrevChild(bool)
{
    _logNeverImplemented();
    return TRUE;
}

bool QWidget::hasMouseTracking() const
{
    return true;
}

void QWidget::show()
{
}

void QWidget::hide()
{
}

void QWidget::internalSetGeometry(int x, int y, int w, int h)
{
    NSView *view = getView();
    
    // A QScrollView is a widget only used to represent a frame.  If
    // this widget's view is a WebCoreFrameView the we resize it's containing
    // view,  an IFWebView.  The scrollview contained by the IFWebView
    // will be autosized.
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
    }
    
    [view setFrame:NSMakeRect(x, y, w, h)];
}

void QWidget::showEvent(QShowEvent *)
{
}

void QWidget::hideEvent(QHideEvent *)
{
}

void QWidget::wheelEvent(QWheelEvent *)
{
}

void QWidget::keyPressEvent(QKeyEvent *)
{
}

void QWidget::keyReleaseEvent(QKeyEvent *)
{
}

void QWidget::focusOutEvent(QFocusEvent *)
{
}

QPoint QWidget::mapFromGlobal(const QPoint &p) const
{
    NSPoint bp;
    bp = [[data->view window] convertScreenToBase: [data->view convertPoint: NSMakePoint(p.x(), p.y()) toView: nil]];
    return QPoint((int)bp.x, (int)bp.y);
}

NSView *QWidget::getView() const
{
    return data->view;
}

void QWidget::setView(NSView *view)
{
    [view retain];
    [data->view release];
    data->view = view;
}

void QWidget::endEditing()
{
    id window, firstResponder;
    
    // Catch the field editor case.
    window = [getView() window];
    [window endEditingFor: nil];
    
    // The previous case is probably not necessary, given that we whack
    // any NSText first responders.
    firstResponder = [window firstResponder];
    if ([firstResponder isKindOfClass: [NSText class]])
        [window makeFirstResponder: nil];
}
