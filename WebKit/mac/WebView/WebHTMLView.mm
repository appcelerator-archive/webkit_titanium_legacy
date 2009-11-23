/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *           (C) 2006, 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebHTMLView.h"

#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMDocumentFragmentInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebArchive.h"
#import "WebClipView.h"
#import "WebDOMOperationsInternal.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebDelegateImplementationCaching.h"
#import "WebDocumentInternal.h"
#import "WebDynamicScrollBarsView.h"
#import "WebEditingDelegate.h"
#import "WebElementDictionary.h"
#import "WebFrameInternal.h"
#import "WebFramePrivate.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentationPrivate.h"
#import "WebHTMLViewInternal.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitVersionChecks.h"
#import "WebLocalizableStrings.h"
#import "WebNSAttributedStringExtras.h"
#import "WebNSEventExtras.h"
#import "WebNSFileManagerExtras.h"
#import "WebNSImageExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSPasteboardExtras.h"
#import "WebNSPrintOperationExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginView.h"
#import "WebNodeHighlight.h"
#import "WebPluginController.h"
#import "WebPreferences.h"
#import "WebPreferencesPrivate.h"
#import "WebResourcePrivate.h"
#import "WebStringTruncator.h"
#import "WebTextCompletionController.h"
#import "WebTypesInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <AppKit/NSAccessibility.h>
#import <ApplicationServices/ApplicationServices.h>
#import <WebCore/CSSMutableStyleDeclaration.h>
#import <WebCore/CachedImage.h>
#import <WebCore/CachedResourceClient.h>
#import <WebCore/ColorMac.h>
#import <WebCore/ContextMenu.h>
#import <WebCore/ContextMenuController.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/DragController.h>
#import <WebCore/Editor.h>
#import <WebCore/EditorDeleteAction.h>
#import <WebCore/Element.h>
#import <WebCore/EventHandler.h>
#import <WebCore/ExceptionHandlers.h>
#import <WebCore/FloatRect.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/Image.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/Range.h>
#import <WebCore/SelectionController.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SimpleFontData.h>
#import <WebCore/Text.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebFontCache.h>
#import <WebCore/markup.h>
#import <WebKit/DOM.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMPrivate.h>
#import <WebKitSystemInterface.h>
#import <dlfcn.h>
#import <limits>
#import <runtime/InitializeThreading.h>

#if USE(ACCELERATED_COMPOSITING)
#import <QuartzCore/QuartzCore.h>
#endif

using namespace WebCore;
using namespace HTMLNames;
using namespace WTF;
using namespace std;

@interface NSWindow (BorderViewAccess)
- (NSView*)_web_borderView;
@end

@implementation NSWindow (BorderViewAccess)
- (NSView*)_web_borderView
{
    return _borderView;
}
@end

@interface WebResponderChainSink : NSResponder {
    NSResponder* _lastResponderInChain;
    BOOL _receivedUnhandledCommand;
}
- (id)initWithResponderChain:(NSResponder *)chain;
- (void)detach;
- (BOOL)receivedUnhandledCommand;
@end

static IMP oldSetCursorIMP = NULL;

#ifdef BUILDING_ON_TIGER

static IMP oldResetCursorRectsIMP = NULL;
static BOOL canSetCursor = YES;

static void resetCursorRects(NSWindow* self, SEL cmd)
{
    NSPoint point = [self mouseLocationOutsideOfEventStream];
    NSView* view = [[self _web_borderView] hitTest:point];
    if ([view isKindOfClass:[WebHTMLView class]]) {
        WebHTMLView *htmlView = (WebHTMLView*)view;
        NSPoint localPoint = [htmlView convertPoint:point fromView:nil];
        NSDictionary *dict = [htmlView elementAtPoint:localPoint allowShadowContent:NO];
        DOMElement *element = [dict objectForKey:WebElementDOMNodeKey];
        if (![element isKindOfClass:[DOMHTMLAppletElement class]] && ![element isKindOfClass:[DOMHTMLObjectElement class]] &&
            ![element isKindOfClass:[DOMHTMLEmbedElement class]])
            canSetCursor = NO;
    }
    oldResetCursorRectsIMP(self, cmd);
    canSetCursor = YES;
}

static void setCursor(NSCursor* self, SEL cmd)
{
    if (canSetCursor)
        oldSetCursorIMP(self, cmd);
}

#else

static void setCursor(NSWindow* self, SEL cmd, NSPoint point)
{
    NSView* view = [[self _web_borderView] hitTest:point];
    if ([view isKindOfClass:[WebHTMLView class]]) {
        WebHTMLView *htmlView = (WebHTMLView*)view;
        NSPoint localPoint = [htmlView convertPoint:point fromView:nil];
        NSDictionary *dict = [htmlView elementAtPoint:localPoint allowShadowContent:NO];
        DOMElement *element = [dict objectForKey:WebElementDOMNodeKey];
        if (![element isKindOfClass:[DOMHTMLAppletElement class]] && ![element isKindOfClass:[DOMHTMLObjectElement class]] &&
            ![element isKindOfClass:[DOMHTMLEmbedElement class]])
            return;
    }
    oldSetCursorIMP(self, cmd, point);
}

#endif

extern "C" {

// Need to declare these attribute names because AppKit exports them but does not make them available in API or SPI headers.

extern NSString *NSMarkedClauseSegmentAttributeName;
extern NSString *NSTextInputReplacementRangeAttributeName;

}

@interface NSView (WebNSViewDetails)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
- (void)_recursive:(BOOL)recurse displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)context topView:(BOOL)topView;
- (NSRect)_dirtyRect;
- (void)_setDrawsOwnDescendants:(BOOL)drawsOwnDescendants;
- (void)_propagateDirtyRectsToOpaqueAncestors;
- (void)_windowChangedKeyState;
#if USE(ACCELERATED_COMPOSITING) && defined(BUILDING_ON_LEOPARD)
- (void)_updateLayerGeometryFromView;
#endif
@end

@interface NSApplication (WebNSApplicationDetails)
- (void)speakString:(NSString *)string;
@end

@interface NSWindow (WebNSWindowDetails)
- (id)_newFirstResponderAfterResigning;
@end

@interface NSAttributedString (WebNSAttributedStringDetails)
- (id)_initWithDOMRange:(DOMRange *)range;
- (DOMDocumentFragment *)_documentFromRange:(NSRange)range document:(DOMDocument *)document documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end

@interface NSSpellChecker (WebNSSpellCheckerDetails)
- (void)learnWord:(NSString *)word;
@end

// By imaging to a width a little wider than the available pixels,
// thin pages will be scaled down a little, matching the way they
// print in IE and Camino. This lets them use fewer sheets than they
// would otherwise, which is presumably why other browsers do this.
// Wide pages will be scaled down more than this.
#define PrintingMinimumShrinkFactor     1.25f

// This number determines how small we are willing to reduce the page content
// in order to accommodate the widest line. If the page would have to be
// reduced smaller to make the widest line fit, we just clip instead (this
// behavior matches MacIE and Mozilla, at least)
#define PrintingMaximumShrinkFactor     2.0f

// This number determines how short the last printed page of a multi-page print session
// can be before we try to shrink the scale in order to reduce the number of pages, and
// thus eliminate the orphan.
#define LastPrintedPageOrphanRatio      0.1f

// This number determines the amount the scale factor is adjusted to try to eliminate orphans.
// It has no direct mathematical relationship to LastPrintedPageOrphanRatio, due to variable
// numbers of pages, logic to avoid breaking elements, and CSS-supplied hard page breaks.
#define PrintingOrphanShrinkAdjustment  1.1f

#define AUTOSCROLL_INTERVAL             0.1f

#define DRAG_LABEL_BORDER_X             4.0f
//Keep border_y in synch with DragController::LinkDragBorderInset
#define DRAG_LABEL_BORDER_Y             2.0f
#define DRAG_LABEL_RADIUS               5.0f
#define DRAG_LABEL_BORDER_Y_OFFSET              2.0f

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP        120.0f
#define MAX_DRAG_LABEL_WIDTH                    320.0f

#define DRAG_LINK_LABEL_FONT_SIZE   11.0f
#define DRAG_LINK_URL_FONT_SIZE   10.0f

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

// FIXME: This constant is copied from AppKit's _NXSmartPaste constant.
#define WebSmartPastePboardType @"NeXT smart paste pasteboard type"

#define STANDARD_WEIGHT 5
#define MIN_BOLD_WEIGHT 7
#define STANDARD_BOLD_WEIGHT 9

// Fake URL scheme.
#define WebDataProtocolScheme @"webkit-fake-url"

// <rdar://problem/4985524> References to WebCoreScrollView as a subview of a WebHTMLView may be present
// in some NIB files, so NSUnarchiver must be still able to look up this now-unused class.
@interface WebCoreScrollView : NSScrollView
@end

@implementation WebCoreScrollView
@end

// if YES, do the standard NSView hit test (which can't give the right result when HTML overlaps a view)
static BOOL forceNSViewHitTest;

// if YES, do the "top WebHTMLView" hit test (which we'd like to do all the time but can't because of Java requirements [see bug 4349721])
static BOOL forceWebHTMLViewHitTest;

static WebHTMLView *lastHitView;

// We need this to be able to safely reference the CachedImage for the promised drag data
static CachedResourceClient* promisedDataClient()
{
    static CachedResourceClient* staticCachedResourceClient = new CachedResourceClient;
    return staticCachedResourceClient;
}

@interface WebHTMLView (WebHTMLViewFileInternal)
- (BOOL)_imageExistsAtPaths:(NSArray *)paths;
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard inContext:(DOMRange *)context allowPlainText:(BOOL)allowPlainText;
- (NSString *)_plainTextFromPasteboard:(NSPasteboard *)pasteboard;
- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText;
- (void)_pasteAsPlainTextWithPasteboard:(NSPasteboard *)pasteboard;
- (void)_removeMouseMovedObserverUnconditionally;
- (void)_removeSuperviewObservers;
- (void)_removeWindowObservers;
- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action;
- (float)_calculatePrintHeight;
- (DOMRange *)_selectedRange;
- (BOOL)_shouldDeleteRange:(DOMRange *)range;
- (NSView *)_hitViewForEvent:(NSEvent *)event;
- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString;
- (DOMRange *)_documentRange;
- (void)_setMouseDownEvent:(NSEvent *)event;
- (WebHTMLView *)_topHTMLView;
- (BOOL)_isTopHTMLView;
- (void)_web_setPrintingModeRecursive;
- (void)_web_setPrintingModeRecursiveAndAdjustViewSize;
- (void)_web_clearPrintingModeRecursive;
@end

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)

@interface WebHTMLView (WebHTMLViewTextCheckingInternal)
- (void)orderFrontSubstitutionsPanel:(id)sender;
- (BOOL)smartInsertDeleteEnabled;
- (void)setSmartInsertDeleteEnabled:(BOOL)flag;
- (void)toggleSmartInsertDelete:(id)sender;
- (BOOL)isAutomaticQuoteSubstitutionEnabled;
- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag;
- (void)toggleAutomaticQuoteSubstitution:(id)sender;
- (BOOL)isAutomaticLinkDetectionEnabled;
- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag;
- (void)toggleAutomaticLinkDetection:(id)sender;
- (BOOL)isAutomaticDashSubstitutionEnabled;
- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag;
- (void)toggleAutomaticDashSubstitution:(id)sender;
- (BOOL)isAutomaticTextReplacementEnabled;
- (void)setAutomaticTextReplacementEnabled:(BOOL)flag;
- (void)toggleAutomaticTextReplacement:(id)sender;
- (BOOL)isAutomaticSpellingCorrectionEnabled;
- (void)setAutomaticSpellingCorrectionEnabled:(BOOL)flag;
- (void)toggleAutomaticSpellingCorrection:(id)sender;
@end

#endif

@interface WebHTMLView (WebForwardDeclaration) // FIXME: Put this in a normal category and stop doing the forward declaration trick.
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize;
@end

@class NSTextInputContext;
@interface NSResponder (AppKitDetails)
- (NSTextInputContext *)inputContext;
@end

@interface NSObject (NSTextInputContextDetails)
- (BOOL)wantsToHandleMouseEvents;
- (BOOL)handleMouseEvent:(NSEvent *)event;
@end

@interface WebHTMLView (WebNSTextInputSupport) <NSTextInput>
- (void)_updateSelectionForInputManager;
@end

@interface WebHTMLView (WebEditingStyleSupport)
- (DOMCSSStyleDeclaration *)_emptyStyle;
- (NSString *)_colorAsString:(NSColor *)color;
@end

@interface NSView (WebHTMLViewFileInternal)
- (void)_web_addDescendantWebHTMLViewsToArray:(NSMutableArray *) array;
@end

@interface NSMutableDictionary (WebHTMLViewFileInternal)
- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key;
@end

struct WebHTMLViewInterpretKeyEventsParameters {
    KeyboardEvent* event;
    BOOL eventWasHandled;
    BOOL shouldSaveCommand;
    // The Input Method may consume an event and not tell us, in
    // which case we should not bubble the event up the DOM
    BOOL consumedByIM;
};

@interface WebHTMLViewPrivate : NSObject {
@public
    BOOL closed;
    BOOL needsToApplyStyles;
    BOOL ignoringMouseDraggedEvents;
    BOOL printing;
    BOOL avoidingPrintOrphan;
    BOOL observingMouseMovedNotifications;
    BOOL observingSuperviewNotifications;
    BOOL observingWindowNotifications;
    
    id savedSubviews;
    BOOL subviewsSetAside;
    
#if USE(ACCELERATED_COMPOSITING)
    NSView *layerHostingView;
#endif

    NSEvent *mouseDownEvent; // Kept after handling the event.
    BOOL handlingMouseDownEvent;
    NSEvent *keyDownEvent; // Kept after handling the event.

    // A WebHTMLView has a single input context, but we return nil when in non-editable content to avoid making input methods do their work.
    // This state is saved each time selection changes, because computing it causes style recalc, which is not always safe to do.
    BOOL exposeInputContext;

    NSPoint lastScrollPosition;

    WebPluginController *pluginController;
    
    NSString *toolTip;
    NSToolTipTag lastToolTipTag;
    id trackingRectOwner;
    void *trackingRectUserData;
    
    NSTimer *autoscrollTimer;
    NSEvent *autoscrollTriggerEvent;
    
    NSArray *pageRects;

    NSMutableDictionary *highlighters;

#ifdef BUILDING_ON_TIGER
    BOOL nextResponderDisabledOnce;
#endif
    
    WebTextCompletionController *completionController;
    
    BOOL transparentBackground;

    WebHTMLViewInterpretKeyEventsParameters* interpretKeyEventsParameters;
    BOOL receivedNOOP;
    
    WebDataSource *dataSource;
    WebCore::CachedImage* promisedDragTIFFDataSource;
    
    CFRunLoopTimerRef updateMouseoverTimer;

    SEL selectorForDoCommandBySelector;

#ifndef NDEBUG
    BOOL enumeratingSubviews;
#endif
}
- (void)clear;
@end

static NSCellStateValue kit(TriState state)
{
    switch (state) {
        case FalseTriState:
            return NSOffState;
        case TrueTriState:
            return NSOnState;
        case MixedTriState:
            return NSMixedState;
    }
    ASSERT_NOT_REACHED();
    return NSOffState;
}

@implementation WebHTMLViewPrivate

+ (void)initialize
{
    JSC::initializeThreading();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif

    if (!oldSetCursorIMP) {
#ifdef BUILDING_ON_TIGER
        Method setCursorMethod = class_getInstanceMethod([NSCursor class], @selector(set));
#else
        Method setCursorMethod = class_getInstanceMethod([NSWindow class], @selector(_setCursorForMouseLocation:));
#endif
        ASSERT(setCursorMethod);

        oldSetCursorIMP = method_setImplementation(setCursorMethod, (IMP)setCursor);
        ASSERT(oldSetCursorIMP);
    }
    
#ifdef BUILDING_ON_TIGER
    if (!oldResetCursorRectsIMP) {
        Method resetCursorRectsMethod = class_getInstanceMethod([NSWindow class], @selector(resetCursorRects));
        ASSERT(resetCursorRectsMethod);
        oldResetCursorRectsIMP = method_setImplementation(resetCursorRectsMethod, (IMP)resetCursorRects);
        ASSERT(oldResetCursorRectsIMP);
    }
#endif

}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebHTMLViewPrivate class], self))
        return;

    ASSERT(!autoscrollTimer);
    ASSERT(!autoscrollTriggerEvent);
    ASSERT(!updateMouseoverTimer);
    
    [mouseDownEvent release];
    [keyDownEvent release];
    [pluginController release];
    [toolTip release];
    [completionController release];
    [dataSource release];
    [highlighters release];
    if (promisedDragTIFFDataSource)
        promisedDragTIFFDataSource->removeClient(promisedDataClient());

    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();

    if (promisedDragTIFFDataSource)
        promisedDragTIFFDataSource->removeClient(promisedDataClient());

    [super finalize];
}

- (void)clear
{
    [mouseDownEvent release];
    [keyDownEvent release];
    [pluginController release];
    [toolTip release];
    [completionController release];
    [dataSource release];
    [highlighters release];
    if (promisedDragTIFFDataSource)
        promisedDragTIFFDataSource->removeClient(promisedDataClient());

    mouseDownEvent = nil;
    keyDownEvent = nil;
    pluginController = nil;
    toolTip = nil;
    completionController = nil;
    dataSource = nil;
    highlighters = nil;
    promisedDragTIFFDataSource = 0;

#if USE(ACCELERATED_COMPOSITING)
    layerHostingView = nil;
#endif
}

@end

@implementation WebHTMLView (WebHTMLViewFileInternal)

- (DOMRange *)_documentRange
{
    return [[[self _frame] DOMDocument] _documentRange];
}

- (BOOL)_imageExistsAtPaths:(NSArray *)paths
{
    NSEnumerator *enumerator = [paths objectEnumerator];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        NSString *MIMEType = WKGetMIMETypeForExtension([path pathExtension]);
        if (MIMETypeRegistry::isSupportedImageResourceMIMEType(MIMEType))
            return YES;
    }
    
    return NO;
}

- (WebDataSource *)_dataSource
{
    return _private->dataSource;
}

- (WebView *)_webView
{
    return [_private->dataSource _webView];
}

- (WebFrameView *)_frameView
{
    return [[_private->dataSource webFrame] frameView];
}

- (DOMDocumentFragment *)_documentFragmentWithPaths:(NSArray *)paths
{
    DOMDocumentFragment *fragment;
    NSEnumerator *enumerator = [paths objectEnumerator];
    NSMutableArray *domNodes = [[NSMutableArray alloc] init];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        // Non-image file types; _web_userVisibleString is appropriate here because this will
        // be pasted as visible text.
        NSString *url = [[[NSURL fileURLWithPath:path] _webkit_canonicalize] _web_userVisibleString];
        [domNodes addObject:[[[self _frame] DOMDocument] createTextNode: url]];
    }
    
    fragment = [[self _frame] _documentFragmentWithNodesAsParagraphs:domNodes]; 
    
    [domNodes release];
    
    return [fragment firstChild] != nil ? fragment : nil;
}

+ (NSArray *)_excludedElementsForAttributedStringConversion
{
    static NSArray *elements = nil;
    if (elements == nil) {
        elements = [[NSArray alloc] initWithObjects:
            // Omit style since we want style to be inline so the fragment can be easily inserted.
            @"style",
            // Omit xml so the result is not XHTML.
            @"xml", 
            // Omit tags that will get stripped when converted to a fragment anyway.
            @"doctype", @"html", @"head", @"body",
            // Omit deprecated tags.
            @"applet", @"basefont", @"center", @"dir", @"font", @"isindex", @"menu", @"s", @"strike", @"u",
            // Omit object so no file attachments are part of the fragment.
            @"object", nil];
        CFRetain(elements);
    }
    return elements;
}

static NSURL* uniqueURLWithRelativePart(NSString *relativePart)
{
    CFUUIDRef UUIDRef = CFUUIDCreate(kCFAllocatorDefault);
    NSString *UUIDString = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef);
    CFRelease(UUIDRef);
    NSURL *URL = [NSURL URLWithString:[NSString stringWithFormat:@"%@://%@/%@", WebDataProtocolScheme, UUIDString, relativePart]];
    CFRelease(UUIDString);

    return URL;
}

- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard
                                               inContext:(DOMRange *)context
                                          allowPlainText:(BOOL)allowPlainText
{
    NSArray *types = [pasteboard types];
    DOMDocumentFragment *fragment = nil;

    if ([types containsObject:WebArchivePboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:WebArchivePboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;
                                           
    if ([types containsObject:NSFilenamesPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSFilenamesPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;
    
    if ([types containsObject:NSHTMLPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSHTMLPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;
    
    if ([types containsObject:NSRTFDPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSRTFDPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;
    
    if ([types containsObject:NSRTFPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSRTFPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;

    if ([types containsObject:NSTIFFPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSTIFFPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;

    if ([types containsObject:NSPDFPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSPDFPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;

#if defined(BUILDING_ON_TIGER) || defined(BUILDING_ON_LEOPARD)
    if ([types containsObject:NSPICTPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSPICTPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;
#endif

    // Only 10.5 and higher support setting and retrieving pasteboard types with UTIs, but we don't believe
    // that any applications on Tiger put types for which we only have a UTI, like PNG, on the pasteboard.
    if ([types containsObject:(NSString*)kUTTypePNG] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:(NSString*)kUTTypePNG
                                                inContext:context
                                             subresources:0]))
        return fragment;
        
    if ([types containsObject:NSURLPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard 
                                                  forType:NSURLPboardType
                                                inContext:context
                                             subresources:0]))
        return fragment;
        
    if (allowPlainText && [types containsObject:NSStringPboardType] &&
        (fragment = [self _documentFragmentFromPasteboard:pasteboard
                                                  forType:NSStringPboardType
                                                inContext:context
                                             subresources:0])) {
        return fragment;
    }
    
    return nil;
}

- (NSString *)_plainTextFromPasteboard:(NSPasteboard *)pasteboard
{
    NSArray *types = [pasteboard types];
    
    if ([types containsObject:NSStringPboardType])
        return [[pasteboard stringForType:NSStringPboardType] precomposedStringWithCanonicalMapping];
    
    NSAttributedString *attributedString = nil;
    NSString *string;

    if ([types containsObject:NSRTFDPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
    if (attributedString == nil && [types containsObject:NSRTFPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
    if (attributedString != nil) {
        string = [[attributedString string] copy];
        [attributedString release];
        return [string autorelease];
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        string = [[pasteboard propertyListForType:NSFilenamesPboardType] componentsJoinedByString:@"\n"];
        if (string != nil)
            return string;
    }
    
    NSURL *URL;
    
    if ((URL = [NSURL URLFromPasteboard:pasteboard])) {
        string = [URL _web_userVisibleString];
        if ([string length] > 0)
            return string;
    }
    
    return nil;
}

- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText
{
    WebView *webView = [[self _webView] retain];
    [webView _setInsertionPasteboard:pasteboard];

    DOMRange *range = [self _selectedRange];
    DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:pasteboard inContext:range allowPlainText:allowPlainText];
    if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:range givenAction:WebViewInsertActionPasted])
        [[self _frame] _replaceSelectionWithFragment:fragment selectReplacement:NO smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard] matchStyle:NO];

    [webView _setInsertionPasteboard:nil];
    [webView release];
}

- (void)_pasteAsPlainTextWithPasteboard:(NSPasteboard *)pasteboard
{
    WebView *webView = [[self _webView] retain];
    [webView _setInsertionPasteboard:pasteboard];

    NSString *text = [self _plainTextFromPasteboard:pasteboard];
    if ([self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionPasted])
        [[self _frame] _replaceSelectionWithText:text selectReplacement:NO smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard]];

    [webView _setInsertionPasteboard:nil];
    [webView release];
}

- (void)_removeMouseMovedObserverUnconditionally
{
    if (!_private || !_private->observingMouseMovedNotifications)
        return;
    
    [[NSNotificationCenter defaultCenter] removeObserver:self name:WKMouseMovedNotification() object:nil];
    _private->observingMouseMovedNotifications = false;
}

- (void)_removeSuperviewObservers
{
    if (!_private || !_private->observingSuperviewNotifications)
        return;
    
    NSView *superview = [self superview];
    if (!superview || ![self window])
        return;
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSViewFrameDidChangeNotification object:superview];
    [notificationCenter removeObserver:self name:NSViewBoundsDidChangeNotification object:superview];
    
    _private->observingSuperviewNotifications = false;
}

- (void)_removeWindowObservers
{
    if (!_private->observingWindowNotifications)
        return;
    
    NSWindow *window = [self window];
    if (!window)
        return;
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    [notificationCenter removeObserver:self name:NSWindowWillCloseNotification object:window];
    
    _private->observingWindowNotifications = false;
}

- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    DOMNode *child = [fragment firstChild];
    if ([fragment lastChild] == child && [child isKindOfClass:[DOMCharacterData class]])
        return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:[(DOMCharacterData *)child data] replacingDOMRange:range givenAction:action];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertNode:fragment replacingDOMRange:range givenAction:action];
}

- (BOOL)_shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:text replacingDOMRange:range givenAction:action];
}

- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action
{
    return [self _shouldInsertText:text replacingDOMRange:[self _selectedRange] givenAction:action];
}

// Calculate the vertical size of the view that fits on a single page
- (float)_calculatePrintHeight
{
    // Obtain the print info object for the current operation
    NSPrintInfo *pi = [[NSPrintOperation currentOperation] printInfo];
    
    // Calculate the page height in points
    NSSize paperSize = [pi paperSize];
    return paperSize.height - [pi topMargin] - [pi bottomMargin];
}

- (DOMRange *)_selectedRange
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->selection()->toNormalizedRange().get()) : nil;
}

- (BOOL)_shouldDeleteRange:(DOMRange *)range
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor()->shouldDeleteRange(core(range));
}

- (NSView *)_hitViewForEvent:(NSEvent *)event
{
    // Usually, we hack AK's hitTest method to catch all events at the topmost WebHTMLView.  
    // Callers of this method, however, want to query the deepest view instead.
    forceNSViewHitTest = YES;
    NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
    forceNSViewHitTest = NO;    
    return hitView;
}

- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString
{
    // Put HTML on the pasteboard.
    if ([types containsObject:WebArchivePboardType]) {
        if (RefPtr<LegacyWebArchive> coreArchive = LegacyWebArchive::createFromSelection(core([self _frame]))) {
            if (RetainPtr<CFDataRef> data = coreArchive ? coreArchive->rawDataRepresentation() : 0)
                [pasteboard setData:(NSData *)data.get() forType:WebArchivePboardType];
        }
    }
    
    // Put the attributed string on the pasteboard (RTF/RTFD format).
    if ([types containsObject:NSRTFDPboardType]) {
        if (attributedString == nil) {
            attributedString = [self selectedAttributedString];
        }        
        NSData *RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFDData forType:NSRTFDPboardType];
    }        
    if ([types containsObject:NSRTFPboardType]) {
        if (attributedString == nil) {
            attributedString = [self selectedAttributedString];
        }
        if ([attributedString containsAttachments]) {
            attributedString = [attributedString _web_attributedStringByStrippingAttachmentCharacters];
        }
        NSData *RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFData forType:NSRTFPboardType];
    }
    
    // Put plain string on the pasteboard.
    if ([types containsObject:NSStringPboardType]) {
        // Map &nbsp; to a plain old space because this is better for source code, other browsers do it,
        // and because HTML forces you to do this any time you want two spaces in a row.
        NSMutableString *s = [[self selectedString] mutableCopy];
        const unichar NonBreakingSpaceCharacter = 0xA0;
        NSString *NonBreakingSpaceString = [NSString stringWithCharacters:&NonBreakingSpaceCharacter length:1];
        [s replaceOccurrencesOfString:NonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
        [pasteboard setString:s forType:NSStringPboardType];
        [s release];
    }
    
    if ([self _canSmartCopyOrDelete] && [types containsObject:WebSmartPastePboardType]) {
        [pasteboard setData:nil forType:WebSmartPastePboardType];
    }
}

- (void)_setMouseDownEvent:(NSEvent *)event
{
    ASSERT(!event || [event type] == NSLeftMouseDown || [event type] == NSRightMouseDown || [event type] == NSOtherMouseDown);

    if (event == _private->mouseDownEvent)
        return;

    [event retain];
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = event;
}

- (void)_cancelUpdateMouseoverTimer
{
    if (_private->updateMouseoverTimer) {
        CFRunLoopTimerInvalidate(_private->updateMouseoverTimer);
        CFRelease(_private->updateMouseoverTimer);
        _private->updateMouseoverTimer = NULL;
    }
}

- (WebHTMLView *)_topHTMLView
{
    // FIXME: this can fail if the dataSource is nil, which happens when the WebView is tearing down from the window closing.
    WebHTMLView *view = (WebHTMLView *)[[[[_private->dataSource _webView] mainFrame] frameView] documentView];
    ASSERT(!view || [view isKindOfClass:[WebHTMLView class]]);
    return view;
}

- (BOOL)_isTopHTMLView
{
    // FIXME: this should be a cached boolean that doesn't rely on _topHTMLView since that can fail (see _topHTMLView).
    return self == [self _topHTMLView];
}

- (void)_web_setPrintingModeRecursive
{
    [self _setPrinting:YES minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];

#ifndef NDEBUG
    _private->enumeratingSubviews = YES;
#endif

    NSMutableArray *descendantWebHTMLViews = [[NSMutableArray alloc] init];

    [self _web_addDescendantWebHTMLViewsToArray:descendantWebHTMLViews];

    unsigned count = [descendantWebHTMLViews count];
    for (unsigned i = 0; i < count; ++i)
        [[descendantWebHTMLViews objectAtIndex:i] _setPrinting:YES minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];

    [descendantWebHTMLViews release];

#ifndef NDEBUG
    _private->enumeratingSubviews = NO;
#endif
}

- (void)_web_clearPrintingModeRecursive
{
    [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];

#ifndef NDEBUG
    _private->enumeratingSubviews = YES;
#endif

    NSMutableArray *descendantWebHTMLViews = [[NSMutableArray alloc] init];

    [self _web_addDescendantWebHTMLViewsToArray:descendantWebHTMLViews];

    unsigned count = [descendantWebHTMLViews count];
    for (unsigned i = 0; i < count; ++i)
        [[descendantWebHTMLViews objectAtIndex:i] _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];

    [descendantWebHTMLViews release];

#ifndef NDEBUG
    _private->enumeratingSubviews = NO;
#endif
}

- (void)_web_setPrintingModeRecursiveAndAdjustViewSize
{
    [self _setPrinting:YES minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:YES];

#ifndef NDEBUG
    _private->enumeratingSubviews = YES;
#endif

    NSMutableArray *descendantWebHTMLViews = [[NSMutableArray alloc] init];

    [self _web_addDescendantWebHTMLViewsToArray:descendantWebHTMLViews];

    unsigned count = [descendantWebHTMLViews count];
    for (unsigned i = 0; i < count; ++i)
        [[descendantWebHTMLViews objectAtIndex:i] _setPrinting:YES minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:YES];

    [descendantWebHTMLViews release];

#ifndef NDEBUG
    _private->enumeratingSubviews = NO;
#endif
}

@end

@implementation WebHTMLView (WebPrivate)

+ (NSArray *)supportedMIMETypes
{
    return [WebHTMLRepresentation supportedMIMETypes];
}

+ (NSArray *)supportedImageMIMETypes
{
    return [WebHTMLRepresentation supportedImageMIMETypes];
}

+ (NSArray *)supportedNonImageMIMETypes
{
    return [WebHTMLRepresentation supportedNonImageMIMETypes];
}

+ (NSArray *)unsupportedTextMIMETypes
{
    return [NSArray arrayWithObjects:
        @"text/calendar",       // iCal
        @"text/x-calendar",
        @"text/x-vcalendar",
        @"text/vcalendar",
        @"text/vcard",          // vCard
        @"text/x-vcard",
        @"text/directory",
        @"text/ldif",           // Netscape Address Book
        @"text/qif",            // Quicken
        @"text/x-qif",
        @"text/x-csv",          // CSV (for Address Book and Microsoft Outlook)
        @"text/x-vcf",          // vCard type used in Sun affinity app
        @"text/rtf",            // Rich Text Format
        nil];
}

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    // This is a workaround for: <rdar://problem/2981619> NSResponder_Private should include notification for FlagsChanged
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[flagsChangedEvent modifierFlags]
        timestamp:[flagsChangedEvent timestamp]
        windowNumber:[flagsChangedEvent windowNumber]
        context:[flagsChangedEvent context]
        eventNumber:0 clickCount:0 pressure:0];

    // Pretend it's a mouse move.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WKMouseMovedNotification() object:self
        userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (id)_bridge
{
    // This method exists to maintain compatibility with Leopard's Dictionary.app, since it
    // calls _bridge to get access to convertNSRangeToDOMRange: and convertDOMRangeToNSRange:.
    // Return the WebFrame, which implements the compatibility methods. <rdar://problem/6002160>
    return [self _frame];
}

- (void)_updateMouseoverWithFakeEvent
{
    [self _cancelUpdateMouseoverTimer];
    
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    
    [self _updateMouseoverWithEvent:fakeEvent];
}

static void _updateMouseoverTimerCallback(CFRunLoopTimerRef timer, void *info)
{
    WebHTMLView *view = (WebHTMLView *)info;
    
    [view _updateMouseoverWithFakeEvent];
}

- (void)_frameOrBoundsChanged
{
    NSPoint origin = [[self superview] bounds].origin;
    if (!NSEqualPoints(_private->lastScrollPosition, origin)) {
        if (Frame* coreFrame = core([self _frame])) {
            if (FrameView* coreView = coreFrame->view())
                coreView->scrollPositionChanged();
        }
    
        [_private->completionController endRevertingChange:NO moveLeft:NO];
        
        WebView *webView = [self _webView];
        [[webView _UIDelegateForwarder] webView:webView didScrollDocumentInFrameView:[self _frameView]];
    }
    _private->lastScrollPosition = origin;

    if ([self window] && !_private->closed && !_private->updateMouseoverTimer) {
        CFRunLoopTimerContext context = { 0, self, NULL, NULL, NULL };
        
        // Use a 100ms delay so that the synthetic mouse over update doesn't cause cursor thrashing when pages are loading
        // and scrolling rapidly back to back.
        _private->updateMouseoverTimer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 0.1, 0, 0, 0,
                                                              _updateMouseoverTimerCallback, &context);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), _private->updateMouseoverTimer, kCFRunLoopDefaultMode);
    }
    
#if USE(ACCELERATED_COMPOSITING) && defined(BUILDING_ON_LEOPARD)
    [self _updateLayerHostingViewPosition];
#endif
}

- (void)_setAsideSubviews
{
    ASSERT(!_private->subviewsSetAside);
    ASSERT(_private->savedSubviews == nil);
    _private->savedSubviews = _subviews;
#if USE(ACCELERATED_COMPOSITING)
    // We need to keep the layer-hosting view in the subviews, otherwise the layers flash.
    if (_private->layerHostingView) {
        NSArray* newSubviews = [[NSArray alloc] initWithObjects:_private->layerHostingView, nil];
        _subviews = newSubviews;
    } else
        _subviews = nil;
#else
    _subviews = nil;
#endif    
    _private->subviewsSetAside = YES;
 }
 
 - (void)_restoreSubviews
 {
    ASSERT(_private->subviewsSetAside);
#if USE(ACCELERATED_COMPOSITING)
    if (_private->layerHostingView) {
        [_subviews release];
        _subviews = _private->savedSubviews;
    } else {
        ASSERT(_subviews == nil);
        _subviews = _private->savedSubviews;
    }
#else
    ASSERT(_subviews == nil);
    _subviews = _private->savedSubviews;
#endif    
    _private->savedSubviews = nil;
    _private->subviewsSetAside = NO;
}

#ifndef NDEBUG

- (void)didAddSubview:(NSView *)subview
{
    if (_private->enumeratingSubviews)
        LOG(View, "A view of class %s was added during subview enumeration for layout or printing mode change. This view might paint without first receiving layout.", object_getClassName([subview class]));
}

- (void)willRemoveSubview:(NSView *)subview
{
    // Have to null-check _private, since this can be called via -dealloc when
    // cleaning up the the layerHostingView.
    if (_private && _private->enumeratingSubviews)
        LOG(View, "A view of class %s was removed during subview enumeration for layout or printing mode change. We will still do layout or the printing mode change even though this view is no longer in the view hierarchy.", object_getClassName([subview class]));
}

#endif

#ifdef BUILDING_ON_TIGER

// This is called when we are about to draw, but before our dirty rect is propagated to our ancestors.
// That's the perfect time to do a layout, except that ideally we'd want to be sure that we're dirty
// before doing it. As a compromise, when we're opaque we do the layout only when actually asked to
// draw, but when we're transparent we do the layout at this stage so views behind us know that they
// need to be redrawn (in case the layout causes some things to get dirtied).
- (void)_propagateDirtyRectsToOpaqueAncestors
{
    if (![[self _webView] drawsBackground])
        [self _web_layoutIfNeededRecursive];
    [super _propagateDirtyRectsToOpaqueAncestors];
}

#else

- (void)viewWillDraw
{
    // On window close we will be called when the datasource is nil, then hit an assert in _topHTMLView
    // So check if the dataSource is nil before calling [self _isTopHTMLView], this can be removed
    // once the FIXME in _isTopHTMLView is fixed.
    if (_private->dataSource && [self _isTopHTMLView])
        [self _web_layoutIfNeededRecursive];
    [super viewWillDraw];
}

#endif

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (isPrinting) {
        if (!wasInPrintingMode)
            [self _web_setPrintingModeRecursive];
#ifndef BUILDING_ON_TIGER
        else
            [self _web_layoutIfNeededRecursive];
#endif
    } else if (wasInPrintingMode)
        [self _web_clearPrintingModeRecursive];

#ifndef BUILDING_ON_TIGER
    // There are known cases where -viewWillDraw is not called on all views being drawn.
    // See <rdar://problem/6964278> for example. Performing layout at this point prevents us from
    // trying to paint without layout (which WebCore now refuses to do, instead bailing out without
    // drawing at all), but we may still fail to update and regions dirtied by the layout which are
    // not already dirty. 
    if ([self _needsLayout]) {
        LOG_ERROR("View needs layout. Either -viewWillDraw wasn't called or layout was invalidated during the display operation. Performing layout now.");
        [self _web_layoutIfNeededRecursive];
    }
#else
    // Because Tiger does not have viewWillDraw we need to do layout here.
    [self _web_layoutIfNeededRecursive];
    [_subviews makeObjectsPerformSelector:@selector(_propagateDirtyRectsToOpaqueAncestors)];
#endif

    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];

    if (wasInPrintingMode != isPrinting) {
        if (wasInPrintingMode)
            [self _web_setPrintingModeRecursive];
        else
            [self _web_clearPrintingModeRecursive];
    }
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;

    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];

    if (needToSetAsideSubviews) {
        // This helps when we print as part of a larger print process.
        // If the WebHTMLView itself is what we're printing, then we will never have to do this.
        if (isPrinting) {
            if (!wasInPrintingMode)
                [self _web_setPrintingModeRecursive];
#ifndef BUILDING_ON_TIGER
            else
                [self _web_layoutIfNeededRecursive];
#endif
        } else if (wasInPrintingMode)
            [self _web_clearPrintingModeRecursive];

#ifdef BUILDING_ON_TIGER

        // Because Tiger does not have viewWillDraw we need to do layout here.
        NSRect boundsBeforeLayout = [self bounds];
        if (!NSIsEmptyRect(visRect))
            [self _web_layoutIfNeededRecursive];

        // If layout changes the view's bounds, then we need to recompute the visRect.
        // That's because the visRect passed to us was based on the bounds at the time
        // we were called. This method is only displayed to draw "all", so it's safe
        // to just call visibleRect to compute the entire rectangle.
        if (!NSEqualRects(boundsBeforeLayout, [self bounds]))
            visRect = [self visibleRect];

#endif

        [self _setAsideSubviews];
    }

    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];

    if (needToSetAsideSubviews) {
        if (wasInPrintingMode != isPrinting) {
            if (wasInPrintingMode)
                [self _web_setPrintingModeRecursive];
            else
                [self _web_clearPrintingModeRecursive];
        }

        [self _restoreSubviews];
    }
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursive:(BOOL)recurse displayRectIgnoringOpacity:(NSRect)displayRect inContext:(NSGraphicsContext *)context topView:(BOOL)topView
{
#ifdef BUILDING_ON_TIGER 
    // Because Tiger does not have viewWillDraw we need to do layout here.
    [self _web_layoutIfNeededRecursive];
#endif

    [self _setAsideSubviews];
    [super _recursive:recurse displayRectIgnoringOpacity:displayRect inContext:context topView:topView];
    [self _restoreSubviews];
}

- (BOOL)_insideAnotherHTMLView
{
    return self != [self _topHTMLView];
}

- (NSView *)hitTest:(NSPoint)point
{
    // WebHTMLView objects handle all events for objects inside them.
    // To get those events, we prevent hit testing from AppKit.

    // But there are three exceptions to this:
    //   1) For right mouse clicks and control clicks we don't yet have an implementation
    //      that works for nested views, so we let the hit testing go through the
    //      standard NSView code path (needs to be fixed, see bug 4361618).
    //   2) Java depends on doing a hit test inside it's mouse moved handling,
    //      so we let the hit testing go through the standard NSView code path
    //      when the current event is a mouse move (except when we are calling
    //      from _updateMouseoverWithEvent, so we have to use a global,
    //      forceWebHTMLViewHitTest, for that)
    //   3) The acceptsFirstMouse: and shouldDelayWindowOrderingForEvent: methods
    //      both need to figure out which view to check with inside the WebHTMLView.
    //      They use a global to change the behavior of hitTest: so they can get the
    //      right view. The global is forceNSViewHitTest and the method they use to
    //      do the hit testing is _hitViewForEvent:. (But this does not work correctly
    //      when there is HTML overlapping the view, see bug 4361626)
    //   4) NSAccessibilityHitTest relies on this for checking the cursor position.
    //      Our check for that is whether the event is NSFlagsChanged.  This works
    //      for VoiceOver's Control-Option-F5 command (move focus to item under cursor)
    //      and Dictionary's Command-Control-D (open dictionary popup for item under cursor).
    //      This is of course a hack.

    if (_private->closed)
        return nil;

    BOOL captureHitsOnSubviews;
    if (forceNSViewHitTest)
        captureHitsOnSubviews = NO;
    else if (forceWebHTMLViewHitTest)
        captureHitsOnSubviews = YES;
    else {
        NSEvent *event = [[self window] currentEvent];
        captureHitsOnSubviews = !([event type] == NSMouseMoved
            || [event type] == NSRightMouseDown
            || ([event type] == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask) != 0)
            || [event type] == NSFlagsChanged);
    }

    if (!captureHitsOnSubviews) {
        NSView* hitView = [super hitTest:point];
#if USE(ACCELERATED_COMPOSITING)
        if (_private && hitView == _private->layerHostingView)
            hitView = self;
#endif
        return hitView;
    }
    if ([[self superview] mouse:point inRect:[self frame]])
        return self;
    return nil;
}

- (void)_clearLastHitViewIfSelf
{
    if (lastHitView == self)
        lastHitView = nil;
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (tag == 0)
        return;
    
    if (_private && (tag == TRACKING_RECT_TAG)) {
        _private->trackingRectOwner = nil;
        return;
    }
    
    if (_private && (tag == _private->lastToolTipTag)) {
        [super removeTrackingRect:tag];
        _private->lastToolTipTag = 0;
        return;
    }
    
    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    int i;
    for (i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        if (_private != nil) {
            _private->trackingRectOwner = nil;
        }
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseEntered:fakeEvent];
}

- (void)_setToolTip:(NSString *)string
{
    NSString *toolTip = [string length] == 0 ? nil : string;
    NSString *oldToolTip = _private->toolTip;
    if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip : [toolTip isEqualToString:oldToolTip]) {
        return;
    }
    if (oldToolTip) {
        [self _sendToolTipMouseExited];
        [oldToolTip release];
    }
    _private->toolTip = [toolTip copy];
    if (toolTip) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        _private->lastToolTipTag = [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return [[_private->toolTip copy] autorelease];
}

- (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    if (_private->closed)
        return;

    NSView *contentView = [[event window] contentView];
    NSPoint locationForHitTest = [[contentView superview] convertPoint:[event locationInWindow] fromView:nil];
    
    forceWebHTMLViewHitTest = YES;
    NSView *hitView = [contentView hitTest:locationForHitTest];
    forceWebHTMLViewHitTest = NO;
    
    WebHTMLView *view = nil;
    if ([hitView isKindOfClass:[WebHTMLView class]] && ![[(WebHTMLView *)hitView _webView] isHoverFeedbackSuspended])
        view = (WebHTMLView *)hitView;    

    if (view)
        [view retain];

    if (lastHitView != view && lastHitView && [lastHitView _frame]) {
        // If we are moving out of a view (or frame), let's pretend the mouse moved
        // all the way out of that view. But we have to account for scrolling, because
        // WebCore doesn't understand our clipping.
        NSRect visibleRect = [[[[lastHitView _frame] frameView] _scrollView] documentVisibleRect];
        float yScroll = visibleRect.origin.y;
        float xScroll = visibleRect.origin.x;

        NSEvent *event = [NSEvent mouseEventWithType:NSMouseMoved
            location:NSMakePoint(-1 - xScroll, -1 - yScroll)
            modifierFlags:[[NSApp currentEvent] modifierFlags]
            timestamp:[NSDate timeIntervalSinceReferenceDate]
            windowNumber:[[view window] windowNumber]
            context:[[NSApp currentEvent] context]
            eventNumber:0 clickCount:0 pressure:0];
        if (Frame* lastHitCoreFrame = core([lastHitView _frame]))
            lastHitCoreFrame->eventHandler()->mouseMoved(event);
    }

    lastHitView = view;

    if (view) {
        if (Frame* coreFrame = core([view _frame]))
            coreFrame->eventHandler()->mouseMoved(event);

        [view release];
    }
}

// keep in sync with WebPasteboardHelper::insertablePasteboardTypes
+ (NSArray *)_insertablePasteboardTypes
{
    static NSArray *types = nil;
    if (!types) {
        types = [[NSArray alloc] initWithObjects:WebArchivePboardType, NSHTMLPboardType, NSFilenamesPboardType, NSTIFFPboardType, NSPDFPboardType,
#if defined(BUILDING_ON_TIGER) || defined(BUILDING_ON_LEOPARD)
            NSPICTPboardType,
#endif
            NSURLPboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, NSColorPboardType, kUTTypePNG, nil];
        CFRetain(types);
    }
    return types;
}

+ (NSArray *)_selectionPasteboardTypes
{
    // FIXME: We should put data for NSHTMLPboardType on the pasteboard but Microsoft Excel doesn't like our format of HTML (3640423).
    return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
}

- (NSImage *)_dragImageForURL:(NSString*)urlString withLabel:(NSString*)label
{
    BOOL drawURLString = YES;
    BOOL clipURLString = NO, clipLabelString = NO;
    
    if (!label) {
        drawURLString = NO;
        label = urlString;
    }
    
    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DRAG_LINK_LABEL_FONT_SIZE]
                                                           toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize: DRAG_LINK_URL_FONT_SIZE];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MAX_DRAG_LABEL_WIDTH){
        labelSize.width = MAX_DRAG_LABEL_WIDTH;
        clipLabelString = YES;
    }
    
    NSSize imageSize, urlStringSize;
    imageSize.width = labelSize.width + DRAG_LABEL_BORDER_X * 2.0f;
    imageSize.height = labelSize.height + DRAG_LABEL_BORDER_Y * 2.0f;
    if (drawURLString) {
        urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
        imageSize.height += urlStringSize.height;
        if (urlStringSize.width > MAX_DRAG_LABEL_WIDTH) {
            imageSize.width = max(MAX_DRAG_LABEL_WIDTH + DRAG_LABEL_BORDER_X * 2, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP);
            clipURLString = YES;
        } else {
            imageSize.width = max(labelSize.width + DRAG_LABEL_BORDER_X * 2, urlStringSize.width + DRAG_LABEL_BORDER_X * 2);
        }
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];
    
    [[NSColor colorWithDeviceRed: 0.7f green: 0.7f blue: 0.7f alpha: 0.8f] set];
    
    // Drag a rectangle with rounded corners/
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0.0f, 0.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0, imageSize.height - DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2.0f, imageSize.height - DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2.0f, 0.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    
    [path appendBezierPathWithRect: NSMakeRect(DRAG_LABEL_RADIUS, 0.0f, imageSize.width - DRAG_LABEL_RADIUS * 2.0f, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0.0f, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 10.0f, imageSize.height - 2.0f * DRAG_LABEL_RADIUS)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS - 20.0f, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 20.0f, imageSize.height - 2.0f * DRAG_LABEL_RADIUS)];
    [path fill];
    
    NSColor *topColor = [NSColor colorWithDeviceWhite:0.0f alpha:0.75f];
    NSColor *bottomColor = [NSColor colorWithDeviceWhite:1.0f alpha:0.5f];
    if (drawURLString) {
        if (clipURLString)
            urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2.0f) withFont:urlFont];
        
        [urlString _web_drawDoubledAtPoint:NSMakePoint(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y - [urlFont descender]) 
                              withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }
    
    if (clipLabelString)
        label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2.0f) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DRAG_LABEL_BORDER_X, imageSize.height - DRAG_LABEL_BORDER_Y_OFFSET - [labelFont pointSize])
                      withTopColor:topColor bottomColor:bottomColor font:labelFont];
    
    [dragImage unlockFocus];
    
    return dragImage;
}

- (NSImage *)_dragImageForLinkElement:(NSDictionary *)element
{
    NSURL *linkURL = [element objectForKey: WebElementLinkURLKey];
    
    NSString *label = [element objectForKey: WebElementLinkLabelKey];
    NSString *urlString = [linkURL _web_userVisibleString];
    return [self _dragImageForURL:urlString withLabel:label];
}

- (void)pasteboardChangedOwner:(NSPasteboard *)pasteboard
{
    [self setPromisedDragTIFFDataSource:0];
}

- (void)pasteboard:(NSPasteboard *)pasteboard provideDataForType:(NSString *)type
{
    if ([type isEqual:NSRTFDPboardType] && [[pasteboard types] containsObject:WebArchivePboardType]) {
        WebArchive *archive = [[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]];
        [pasteboard _web_writePromisedRTFDFromArchive:archive containsImage:[[pasteboard types] containsObject:NSTIFFPboardType]];
        [archive release];
    } else if ([type isEqual:NSTIFFPboardType] && [self promisedDragTIFFDataSource]) {
        if (Image* image = [self promisedDragTIFFDataSource]->image())
            [pasteboard setData:(NSData *)image->getTIFFRepresentation() forType:NSTIFFPboardType];
        [self setPromisedDragTIFFDataSource:0];
    }
}

- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event 
{ 
    [self autoscroll:event]; 
    [self _startAutoscrollTimer:event]; 
} 

- (WebPluginController *)_pluginController
{
    return _private->pluginController;
}

- (void)_layoutForPrinting
{
    // Set printing mode temporarily so we can adjust the size of the view. This will allow
    // AppKit's pagination code to use the correct height for the page content. Leaving printing
    // mode on indefinitely would interfere with Mail's printing mechanism (at least), so we just
    // turn it off again after adjusting the size.
    [self _web_setPrintingModeRecursiveAndAdjustViewSize];
    [self _web_clearPrintingModeRecursive];
}

- (void)_smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)rangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString
{
    if (!pasteString || !rangeToReplace || ![[self _webView] smartInsertDeleteEnabled]) {
        if (beforeString)
            *beforeString = nil;
        if (afterString)
            *afterString = nil;
        return;
    }
    
    [[self _frame] _smartInsertForString:pasteString replacingRange:rangeToReplace beforeString:beforeString afterString:afterString];
}

- (BOOL)_canSmartReplaceWithPasteboard:(NSPasteboard *)pasteboard
{
    return [[self _webView] smartInsertDeleteEnabled] && [[pasteboard types] containsObject:WebSmartPastePboardType];
}

- (void)_startAutoscrollTimer:(NSEvent *)triggerEvent
{
    if (_private->autoscrollTimer == nil) {
        _private->autoscrollTimer = [[NSTimer scheduledTimerWithTimeInterval:AUTOSCROLL_INTERVAL
            target:self selector:@selector(_autoscroll) userInfo:nil repeats:YES] retain];
        _private->autoscrollTriggerEvent = [triggerEvent retain];
    }
}

// FIXME: _selectionRect is deprecated in favor of selectionRect, which is in protocol WebDocumentSelection.
// We can't remove this yet because it's still in use by Mail.
- (NSRect)_selectionRect
{
    return [self selectionRect];
}

- (void)_stopAutoscrollTimer
{
    NSTimer *timer = _private->autoscrollTimer;
    _private->autoscrollTimer = nil;
    [_private->autoscrollTriggerEvent release];
    _private->autoscrollTriggerEvent = nil;
    [timer invalidate];
    [timer release];
}

- (void)_autoscroll
{
    // Guarantee that the autoscroll timer is invalidated, even if we don't receive
    // a mouse up event.
    BOOL isStillDown = CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft);   
    if (!isStillDown){
        [self _stopAutoscrollTimer];
        return;
    }

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    [self mouseDragged:fakeEvent];
}

- (BOOL)_canEdit
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor()->canEdit();
}

- (BOOL)_canEditRichly
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor()->canEditRichly();
}

- (BOOL)_canAlterCurrentSelection
{
    return [self _hasSelectionOrInsertionPoint] && [self _isEditable];
}

- (BOOL)_hasSelection
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection()->isRange();
}

- (BOOL)_hasSelectionOrInsertionPoint
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection()->isCaretOrRange();
}

- (BOOL)_hasInsertionPoint
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection()->isCaret();
}

- (BOOL)_isEditable
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selection()->isContentEditable();
}

- (BOOL)_transparentBackground
{
    return _private->transparentBackground;
}

- (void)_setTransparentBackground:(BOOL)f
{
    _private->transparentBackground = f;
}

- (NSImage *)_selectionDraggingImage
{
    if ([self _hasSelection]) {
        NSImage *dragImage = core([self _frame])->selectionImage();
        [dragImage _web_dissolveToFraction:WebDragImageAlpha];
        return dragImage;
    }
    return nil;
}

- (NSRect)_selectionDraggingRect
{
    // Mail currently calls this method. We can eliminate it when Mail no longer calls it.
    return [self selectionRect];
}

- (DOMNode *)_insertOrderedList
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor()->insertOrderedList().get()) : nil;
}

- (DOMNode *)_insertUnorderedList
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor()->insertUnorderedList().get()) : nil;
}

- (BOOL)_canIncreaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor()->canIncreaseSelectionListLevel();
}

- (BOOL)_canDecreaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->editor()->canDecreaseSelectionListLevel();
}

- (DOMNode *)_increaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor()->increaseSelectionListLevel().get()) : nil;
}

- (DOMNode *)_increaseSelectionListLevelOrdered
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor()->increaseSelectionListLevelOrdered().get()) : nil;
}

- (DOMNode *)_increaseSelectionListLevelUnordered
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame ? kit(coreFrame->editor()->increaseSelectionListLevelUnordered().get()) : nil;
}

- (void)_decreaseSelectionListLevel
{
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->editor()->decreaseSelectionListLevel();
}

- (void)_setHighlighter:(id<WebHTMLHighlighter>)highlighter ofType:(NSString*)type
{
    if (!_private->highlighters)
        _private->highlighters = [[NSMutableDictionary alloc] init];
    [_private->highlighters setObject:highlighter forKey:type];
}

- (void)_removeHighlighterOfType:(NSString*)type
{
    [_private->highlighters removeObjectForKey:type];
}

- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard
{
    ASSERT([self _hasSelection]);
    NSArray *types = [self pasteboardTypesForSelection];

    // Don't write RTFD to the pasteboard when the copied attributed string has no attachments.
    NSAttributedString *attributedString = [self selectedAttributedString];
    NSMutableArray *mutableTypes = nil;
    if (![attributedString containsAttachments]) {
        mutableTypes = [types mutableCopy];
        [mutableTypes removeObject:NSRTFDPboardType];
        types = mutableTypes;
    }

    [pasteboard declareTypes:types owner:[self _topHTMLView]];
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:attributedString];
    [mutableTypes release];
}

- (void)close
{
    // Check for a nil _private here in case we were created with initWithCoder. In that case, the WebView is just throwing
    // out the archived WebHTMLView and recreating a new one if needed. So close doesn't need to do anything in that case.
    if (!_private || _private->closed)
        return;

    _private->closed = YES;

    [self _cancelUpdateMouseoverTimer];
    [self _clearLastHitViewIfSelf];
    [self _removeMouseMovedObserverUnconditionally];
    [self _removeWindowObservers];
    [self _removeSuperviewObservers];
    [_private->pluginController destroyAllPlugins];
    [_private->pluginController setDataSource:nil];
    // remove tooltips before clearing _private so removeTrackingRect: will work correctly
    [self removeAllToolTips];

#if USE(ACCELERATED_COMPOSITING)
    if (_private->layerHostingView)
        [[self _webView] _stoppedAcceleratedCompositingForFrame:[self _frame]];
#endif

    [_private clear];

    Page* page = core([self _webView]);
    if (page)
        page->dragController()->setDraggingImageURL(KURL());
}

- (BOOL)_hasHTMLDocument
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    Document* document = coreFrame->document();
    return document && document->isHTMLDocument();
}

- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard
                                                 forType:(NSString *)pboardType
                                               inContext:(DOMRange *)context
                                            subresources:(NSArray **)subresources
{
    if (pboardType == WebArchivePboardType) {
        WebArchive *archive = [[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]];
        if (subresources)
            *subresources = [archive subresources];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithArchive:archive];
        [archive release];
        return fragment;
    }
    if (pboardType == NSFilenamesPboardType)
        return [self _documentFragmentWithPaths:[pasteboard propertyListForType:NSFilenamesPboardType]];
        
    if (pboardType == NSHTMLPboardType) {
        NSString *HTMLString = [pasteboard stringForType:NSHTMLPboardType];
        // This is a hack to make Microsoft's HTML pasteboard data work. See 3778785.
        if ([HTMLString hasPrefix:@"Version:"]) {
            NSRange range = [HTMLString rangeOfString:@"<html" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound)
                HTMLString = [HTMLString substringFromIndex:range.location];
        }
        if ([HTMLString length] == 0)
            return nil;
        
        return [[self _frame] _documentFragmentWithMarkupString:HTMLString baseURLString:nil];
    }

    // The _hasHTMLDocument clause here is a workaround for a bug in NSAttributedString: Radar 5052369.
    // If we call _documentFromRange on an XML document we'll get "setInnerHTML: method not found".
    // FIXME: Remove this once bug 5052369 is fixed.
    if ([self _hasHTMLDocument] && pboardType == NSRTFPboardType || pboardType == NSRTFDPboardType) {
        NSAttributedString *string = nil;
        if (pboardType == NSRTFDPboardType)
            string = [[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
        if (string == nil)
            string = [[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
        if (string == nil)
            return nil;
            
        NSDictionary *documentAttributes = [[NSDictionary alloc] initWithObjectsAndKeys:
            [[self class] _excludedElementsForAttributedStringConversion], NSExcludedElementsDocumentAttribute,
            self, @"WebResourceHandler", nil];
        NSArray *s;
        
        BOOL wasDeferringCallbacks = [[self _webView] defersCallbacks];
        if (!wasDeferringCallbacks)
            [[self _webView] setDefersCallbacks:YES];
            
        DOMDocumentFragment *fragment = [string _documentFromRange:NSMakeRange(0, [string length]) 
                                                          document:[[self _frame] DOMDocument] 
                                                documentAttributes:documentAttributes
                                                      subresources:&s];
        if (subresources)
            *subresources = s;
        
        NSEnumerator *e = [s objectEnumerator];
        WebResource *r;
        while ((r = [e nextObject]))
            [[self _dataSource] addSubresource:r];
        
        if (!wasDeferringCallbacks)
            [[self _webView] setDefersCallbacks:NO];
        
        [documentAttributes release];
        [string release];
        return fragment;
    }
    if (pboardType == NSTIFFPboardType) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSTIFFPboardType]
                                                              URL:uniqueURLWithRelativePart(@"image.tiff")
                                                         MIMEType:@"image/tiff" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
    if (pboardType == NSPDFPboardType) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSPDFPboardType]
                                                              URL:uniqueURLWithRelativePart(@"application.pdf")
                                                         MIMEType:@"application/pdf" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
#if defined(BUILDING_ON_TIGER) || defined(BUILDING_ON_LEOPARD)
    if (pboardType == NSPICTPboardType) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSPICTPboardType]
                                                              URL:uniqueURLWithRelativePart(@"image.pict")
                                                         MIMEType:@"image/pict" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
#endif
    // Only 10.5 and higher support setting and retrieving pasteboard types with UTIs, but we don't believe
    // that any applications on Tiger put types for which we only have a UTI, like PNG, on the pasteboard.
    if ([pboardType isEqualToString:(NSString*)kUTTypePNG]) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:(NSString*)kUTTypePNG]
                                                              URL:uniqueURLWithRelativePart(@"image.png")
                                                         MIMEType:@"image/png" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
    if (pboardType == NSURLPboardType) {
        NSURL *URL = [NSURL URLFromPasteboard:pasteboard];
        DOMDocument* document = [[self _frame] DOMDocument];
        ASSERT(document);
        if (!document)
            return nil;
        DOMHTMLAnchorElement *anchor = (DOMHTMLAnchorElement *)[document createElement:@"a"];
        NSString *URLString = [URL _web_originalDataAsString]; // Original data is ASCII-only, so there is no need to precompose.
        if ([URLString length] == 0)
            return nil;
        NSString *URLTitleString = [[pasteboard stringForType:WebURLNamePboardType] precomposedStringWithCanonicalMapping];
        DOMText *text = [document createTextNode:URLTitleString];
        [anchor setHref:URLString];
        [anchor appendChild:text];
        DOMDocumentFragment *fragment = [document createDocumentFragment];
        [fragment appendChild:anchor];
        return fragment;
    }
    if (pboardType == NSStringPboardType)
        return kit(createFragmentFromText(core(context), [[pasteboard stringForType:NSStringPboardType] precomposedStringWithCanonicalMapping]).get());
    return nil;
}

#if ENABLE(NETSCAPE_PLUGIN_API) 
- (void)_pauseNullEventsForAllNetscapePlugins 
{ 
    NSArray *subviews = [self subviews]; 
    unsigned int subviewCount = [subviews count]; 
    unsigned int subviewIndex; 
    
    for (subviewIndex = 0; subviewIndex < subviewCount; subviewIndex++) { 
        NSView *subview = [subviews objectAtIndex:subviewIndex]; 
        if ([subview isKindOfClass:[WebBaseNetscapePluginView class]]) 
            [(WebBaseNetscapePluginView *)subview stopTimers];
    } 
} 
#endif 

#if ENABLE(NETSCAPE_PLUGIN_API) 
- (void)_resumeNullEventsForAllNetscapePlugins 
{ 
    NSArray *subviews = [self subviews]; 
    unsigned int subviewCount = [subviews count]; 
    unsigned int subviewIndex; 
    
    for (subviewIndex = 0; subviewIndex < subviewCount; subviewIndex++) { 
        NSView *subview = [subviews objectAtIndex:subviewIndex]; 
        if ([subview isKindOfClass:[WebBaseNetscapePluginView class]]) 
            [(WebBaseNetscapePluginView *)subview restartTimers]; 
    } 
} 
#endif 

- (BOOL)_isUsingAcceleratedCompositing
{
#if USE(ACCELERATED_COMPOSITING)
    return _private->layerHostingView != nil;
#else
    return NO;
#endif
}

- (NSView *)_compositingLayersHostingView
{
#if USE(ACCELERATED_COMPOSITING)
    return _private->layerHostingView;
#else
    return 0;
#endif
}

@end

@implementation NSView (WebHTMLViewFileInternal)

- (void)_web_addDescendantWebHTMLViewsToArray:(NSMutableArray *)array
{
    unsigned count = [_subviews count];
    for (unsigned i = 0; i < count; ++i) {
        NSView *child = [_subviews objectAtIndex:i];
        if ([child isKindOfClass:[WebHTMLView class]])
            [array addObject:child];
        [child _web_addDescendantWebHTMLViewsToArray:array];
    }
}

@end

@implementation NSMutableDictionary (WebHTMLViewFileInternal)

- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

@end

static bool matchesExtensionOrEquivalent(NSString *filename, NSString *extension)
{
    NSString *extensionAsSuffix = [@"." stringByAppendingString:extension];
    return [filename _webkit_hasCaseInsensitiveSuffix:extensionAsSuffix]
        || ([extension _webkit_isCaseInsensitiveEqualToString:@"jpeg"]
            && [filename _webkit_hasCaseInsensitiveSuffix:@".jpg"]);
}

#ifdef BUILDING_ON_TIGER

// The following is a workaround for
// <rdar://problem/3429631> window stops getting mouse moved events after first tooltip appears
// The trick is to define a category on NSToolTipPanel that implements setAcceptsMouseMovedEvents:.
// Since the category will be searched before the real class, we'll prevent the flag from being
// set on the tool tip panel.

@interface NSToolTipPanel : NSPanel
@end

@interface NSToolTipPanel (WebHTMLViewFileInternal)
@end

@implementation NSToolTipPanel (WebHTMLViewFileInternal)

- (void)setAcceptsMouseMovedEvents:(BOOL)flag
{
    // Do nothing, preventing the tool tip panel from trying to accept mouse-moved events.
}

@end

#endif

@interface NSArray (WebHTMLView)
- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object;
@end

@implementation WebHTMLView

+ (void)initialize
{
    [NSApp registerServicesMenuSendTypes:[[self class] _selectionPasteboardTypes] 
                             returnTypes:[[self class] _insertablePasteboardTypes]];
    JSC::initializeThreading();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
    [self setFocusRingType:NSFocusRingTypeNone];
    
    // Make all drawing go through us instead of subviews.
    [self _setDrawsOwnDescendants:YES];
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->pluginController = [[WebPluginController alloc] initWithDocumentView:self];
    
    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebHTMLView class], self))
        return;

    // We can't assert that close has already been called because
    // this view can be removed from it's superview, even though
    // it could be needed later, so close if needed.
    [self close];
    [_private release];
    _private = nil;
    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();
    // We can't assert that close has already been called because
    // this view can be removed from it's superview, even though
    // it could be needed later, so close if needed.
    [self close];
    [super finalize];
}

// Returns YES if the delegate returns YES (so we should do no more work).
- (BOOL)callDelegateDoCommandBySelectorIfNeeded:(SEL)selector
{
    BOOL callerAlreadyCalledDelegate = _private->selectorForDoCommandBySelector == selector;
    _private->selectorForDoCommandBySelector = 0;
    if (callerAlreadyCalledDelegate)
        return NO;
    WebView *webView = [self _webView];
    return [[webView _editingDelegateForwarder] webView:webView doCommandBySelector:selector];
}

typedef HashMap<SEL, String> SelectorNameMap;

// Map selectors into Editor command names.
// This is not needed for any selectors that have the same name as the Editor command.
static const SelectorNameMap* createSelectorExceptionMap()
{
    SelectorNameMap* map = new HashMap<SEL, String>;

    map->add(@selector(insertNewlineIgnoringFieldEditor:), "InsertNewline");
    map->add(@selector(insertParagraphSeparator:), "InsertNewline");
    map->add(@selector(insertTabIgnoringFieldEditor:), "InsertTab");
    map->add(@selector(pageDown:), "MovePageDown");
    map->add(@selector(pageDownAndModifySelection:), "MovePageDownAndModifySelection");
    map->add(@selector(pageUp:), "MovePageUp");
    map->add(@selector(pageUpAndModifySelection:), "MovePageUpAndModifySelection");

    return map;
}

static String commandNameForSelector(SEL selector)
{
    // Check the exception map first.
    static const SelectorNameMap* exceptionMap = createSelectorExceptionMap();
    SelectorNameMap::const_iterator it = exceptionMap->find(selector);
    if (it != exceptionMap->end())
        return it->second;

    // Remove the trailing colon.
    // No need to capitalize the command name since Editor command names are
    // not case sensitive.
    const char* selectorName = sel_getName(selector);
    size_t selectorNameLength = strlen(selectorName);
    if (selectorNameLength < 2 || selectorName[selectorNameLength - 1] != ':')
        return String();
    return String(selectorName, selectorNameLength - 1);
}

- (Editor::Command)coreCommandBySelector:(SEL)selector
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return Editor::Command();
    return coreFrame->editor()->command(commandNameForSelector(selector));
}

- (Editor::Command)coreCommandByName:(const char*)name
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return Editor::Command();
    return coreFrame->editor()->command(name);
}

- (void)executeCoreCommandBySelector:(SEL)selector
{
    if ([self callDelegateDoCommandBySelectorIfNeeded:selector])
        return;
    [self coreCommandBySelector:selector].execute();
}

- (void)executeCoreCommandByName:(const char*)name
{
    [self coreCommandByName:name].execute();
}

// These commands are forwarded to the Editor object in WebCore.
// Ideally we'd do this for all editing commands; more of the code
// should be moved from here to there, and more commands should be
// added to this list.

// FIXME: Maybe we should set things up so that all these share a single method implementation function.
// The functions are identical.

#define WEBCORE_COMMAND(command) - (void)command:(id)sender { [self executeCoreCommandBySelector:_cmd]; }

WEBCORE_COMMAND(alignCenter)
WEBCORE_COMMAND(alignJustified)
WEBCORE_COMMAND(alignLeft)
WEBCORE_COMMAND(alignRight)
WEBCORE_COMMAND(copy)
WEBCORE_COMMAND(cut)
WEBCORE_COMMAND(delete)
WEBCORE_COMMAND(deleteBackward)
WEBCORE_COMMAND(deleteBackwardByDecomposingPreviousCharacter)
WEBCORE_COMMAND(deleteForward)
WEBCORE_COMMAND(deleteToBeginningOfLine)
WEBCORE_COMMAND(deleteToBeginningOfParagraph)
WEBCORE_COMMAND(deleteToEndOfLine)
WEBCORE_COMMAND(deleteToEndOfParagraph)
WEBCORE_COMMAND(deleteToMark)
WEBCORE_COMMAND(deleteWordBackward)
WEBCORE_COMMAND(deleteWordForward)
WEBCORE_COMMAND(ignoreSpelling)
WEBCORE_COMMAND(indent)
WEBCORE_COMMAND(insertBacktab)
WEBCORE_COMMAND(insertLineBreak)
WEBCORE_COMMAND(insertNewline)
WEBCORE_COMMAND(insertNewlineIgnoringFieldEditor)
WEBCORE_COMMAND(insertParagraphSeparator)
WEBCORE_COMMAND(insertTab)
WEBCORE_COMMAND(insertTabIgnoringFieldEditor)
WEBCORE_COMMAND(makeTextWritingDirectionLeftToRight)
WEBCORE_COMMAND(makeTextWritingDirectionNatural)
WEBCORE_COMMAND(makeTextWritingDirectionRightToLeft)
WEBCORE_COMMAND(moveBackward)
WEBCORE_COMMAND(moveBackwardAndModifySelection)
WEBCORE_COMMAND(moveDown)
WEBCORE_COMMAND(moveDownAndModifySelection)
WEBCORE_COMMAND(moveForward)
WEBCORE_COMMAND(moveForwardAndModifySelection)
WEBCORE_COMMAND(moveLeft)
WEBCORE_COMMAND(moveLeftAndModifySelection)
WEBCORE_COMMAND(moveParagraphBackwardAndModifySelection)
WEBCORE_COMMAND(moveParagraphForwardAndModifySelection)
WEBCORE_COMMAND(moveRight)
WEBCORE_COMMAND(moveRightAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfDocument)
WEBCORE_COMMAND(moveToBeginningOfDocumentAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfLine)
WEBCORE_COMMAND(moveToBeginningOfLineAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfParagraph)
WEBCORE_COMMAND(moveToBeginningOfParagraphAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfSentence)
WEBCORE_COMMAND(moveToBeginningOfSentenceAndModifySelection)
WEBCORE_COMMAND(moveToEndOfDocument)
WEBCORE_COMMAND(moveToEndOfDocumentAndModifySelection)
WEBCORE_COMMAND(moveToEndOfLine)
WEBCORE_COMMAND(moveToEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveToEndOfParagraph)
WEBCORE_COMMAND(moveToEndOfParagraphAndModifySelection)
WEBCORE_COMMAND(moveToEndOfSentence)
WEBCORE_COMMAND(moveToEndOfSentenceAndModifySelection)
WEBCORE_COMMAND(moveToLeftEndOfLine)
WEBCORE_COMMAND(moveToLeftEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveToRightEndOfLine)
WEBCORE_COMMAND(moveToRightEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveUp)
WEBCORE_COMMAND(moveUpAndModifySelection)
WEBCORE_COMMAND(moveWordBackward)
WEBCORE_COMMAND(moveWordBackwardAndModifySelection)
WEBCORE_COMMAND(moveWordForward)
WEBCORE_COMMAND(moveWordForwardAndModifySelection)
WEBCORE_COMMAND(moveWordLeft)
WEBCORE_COMMAND(moveWordLeftAndModifySelection)
WEBCORE_COMMAND(moveWordRight)
WEBCORE_COMMAND(moveWordRightAndModifySelection)
WEBCORE_COMMAND(outdent)
WEBCORE_COMMAND(pageDown)
WEBCORE_COMMAND(pageDownAndModifySelection)
WEBCORE_COMMAND(pageUp)
WEBCORE_COMMAND(pageUpAndModifySelection)
WEBCORE_COMMAND(selectAll)
WEBCORE_COMMAND(selectLine)
WEBCORE_COMMAND(selectParagraph)
WEBCORE_COMMAND(selectSentence)
WEBCORE_COMMAND(selectToMark)
WEBCORE_COMMAND(selectWord)
WEBCORE_COMMAND(setMark)
WEBCORE_COMMAND(subscript)
WEBCORE_COMMAND(superscript)
WEBCORE_COMMAND(swapWithMark)
WEBCORE_COMMAND(transpose)
WEBCORE_COMMAND(underline)
WEBCORE_COMMAND(unscript)
WEBCORE_COMMAND(yank)
WEBCORE_COMMAND(yankAndSelect)

#undef WEBCORE_COMMAND

#define COMMAND_PROLOGUE if ([self callDelegateDoCommandBySelectorIfNeeded:_cmd]) return;

- (IBAction)takeFindStringFromSelection:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _hasSelection]) {
        NSBeep();
        return;
    }

    [NSPasteboard _web_setFindPasteboardString:[self selectedString] withOwner:self];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [pasteboard declareTypes:types owner:[self _topHTMLView]];
    [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    return YES;
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pasteboard
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    if (coreFrame->selection()->isContentRichlyEditable())
        [self _pasteWithPasteboard:pasteboard allowPlainText:YES];
    else
        [self _pasteAsPlainTextWithPasteboard:pasteboard];
    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    BOOL isSendTypeOK = !sendType || ([[self pasteboardTypesForSelection] containsObject:sendType] && [self _hasSelection]);
    BOOL isReturnTypeOK = !returnType || ([[[self class] _insertablePasteboardTypes] containsObject:returnType] && [self _isEditable]);
    if (isSendTypeOK && isReturnTypeOK)
        return self;
    return [[self nextResponder] validRequestorForSendType:sendType returnType:returnType];
}

// jumpToSelection is the old name for what AppKit now calls centerSelectionInVisibleArea. Safari
// was using the old jumpToSelection selector in its menu. Newer versions of Safari will use the
// selector centerSelectionInVisibleArea. We'll leave the old selector in place for two reasons:
// (1) Compatibility between older Safari and newer WebKit; (2) other WebKit-based applications
// might be using the selector, and we don't want to break them.
- (void)jumpToSelection:(id)sender
{
    COMMAND_PROLOGUE

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->revealSelection(ScrollAlignment::alignCenterAlways);
}

- (NSCellStateValue)selectionHasStyle:(CSSStyleDeclaration*)style
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NSOffState;
    return kit(coreFrame->editor()->selectionHasStyle(style));
}

- (BOOL)validateUserInterfaceItemWithoutDelegate:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];
    RefPtr<Frame> frame = core([self _frame]);

    if (!frame)
        return NO;
    
    if (Document* doc = frame->document()) {
        if (doc->isPluginDocument())
            return NO;
        if (doc->isImageDocument()) {            
            if (action == @selector(copy:))
                return frame->loader()->isComplete();
            return NO;
        }
    }

    if (action == @selector(changeSpelling:)
            || action == @selector(_changeSpellingFromMenu:)
            || action == @selector(checkSpelling:)
            || action == @selector(complete:)
            || action == @selector(pasteFont:))
        return [self _canEdit];

    if (action == @selector(showGuessPanel:)) {
#ifndef BUILDING_ON_TIGER
        // Match OS X AppKit behavior for post-Tiger. Don't change Tiger behavior.
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            BOOL panelShowing = [[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible];
            [menuItem setTitle:panelShowing
                ? UI_STRING("Hide Spelling and Grammar", "menu item title")
                : UI_STRING("Show Spelling and Grammar", "menu item title")];
        }
#endif
        return [self _canEdit];
    }
    
    if (action == @selector(changeBaseWritingDirection:)
            || action == @selector(makeBaseWritingDirectionLeftToRight:)
            || action == @selector(makeBaseWritingDirectionRightToLeft:)) {
        NSWritingDirection writingDirection;

        if (action == @selector(changeBaseWritingDirection:)) {
            writingDirection = static_cast<NSWritingDirection>([item tag]);
            if (writingDirection == NSWritingDirectionNatural)
                return NO;
        } else if (action == @selector(makeBaseWritingDirectionLeftToRight:))
            writingDirection = NSWritingDirectionLeftToRight;
        else
            writingDirection = NSWritingDirectionRightToLeft;

        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            RefPtr<CSSStyleDeclaration> style = CSSMutableStyleDeclaration::create();
            ExceptionCode ec;
            style->setProperty("direction", writingDirection == NSWritingDirectionLeftToRight ? "LTR" : "RTL", ec);
            [menuItem setState:frame->editor()->selectionHasStyle(style.get())];
        }
        return [self _canEdit];
    }

    if (action == @selector(makeBaseWritingDirectionNatural:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:NSOffState];
        return NO;
    }

    if (action == @selector(toggleBaseWritingDirection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            RefPtr<CSSStyleDeclaration> style = CSSMutableStyleDeclaration::create();
            ExceptionCode ec;
            style->setProperty("direction", "RTL", ec);
            // Take control of the title of the menu item instead of just checking/unchecking it because
            // a check would be ambiguous.
            [menuItem setTitle:frame->editor()->selectionHasStyle(style.get())
                ? UI_STRING("Left to Right", "Left to Right context menu item")
                : UI_STRING("Right to Left", "Right to Left context menu item")];
        }
        return [self _canEdit];
    } 
    
    if (action == @selector(changeAttributes:)
            || action == @selector(changeColor:)        
            || action == @selector(changeFont:))
        return [self _canEditRichly];
    
    if (action == @selector(capitalizeWord:)
               || action == @selector(lowercaseWord:)
               || action == @selector(uppercaseWord:))
        return [self _hasSelection] && [self _isEditable];

    if (action == @selector(centerSelectionInVisibleArea:)
               || action == @selector(jumpToSelection:)
               || action == @selector(copyFont:))
        return [self _hasSelection] || ([self _isEditable] && [self _hasInsertionPoint]);
    
    if (action == @selector(changeDocumentBackgroundColor:))
        return [[self _webView] isEditable] && [self _canEditRichly];
    
    if (action == @selector(_ignoreSpellingFromMenu:)
            || action == @selector(_learnSpellingFromMenu:)
            || action == @selector(takeFindStringFromSelection:))
        return [self _hasSelection];
    
    if (action == @selector(paste:) || action == @selector(pasteAsPlainText:))
        return frame && (frame->editor()->canDHTMLPaste() || frame->editor()->canPaste());
    
    if (action == @selector(pasteAsRichText:))
        return frame && (frame->editor()->canDHTMLPaste()
            || (frame->editor()->canPaste() && frame->selection()->isContentRichlyEditable()));
    
    if (action == @selector(performFindPanelAction:))
        return NO;
    
    if (action == @selector(_lookUpInDictionaryFromMenu:))
        return [self _hasSelection];

    if (action == @selector(stopSpeaking:))
        return [NSApp isSpeaking];

#ifndef BUILDING_ON_TIGER
    if (action == @selector(toggleGrammarChecking:)) {
        // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must validate 
        // the selector here because we implement it here, and we must implement it here because the AppKit 
        // code checks the first responder.
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isGrammarCheckingEnabled] ? NSOnState : NSOffState];
        return YES;
    }
#endif

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    if (action == @selector(orderFrontSubstitutionsPanel:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            BOOL panelShowing = [[[NSSpellChecker sharedSpellChecker] substitutionsPanel] isVisible];
            [menuItem setTitle:panelShowing
                ? UI_STRING("Hide Substitutions", "menu item title")
                : UI_STRING("Show Substitutions", "menu item title")];
        }
        return [self _canEdit];
    }
    // FIXME 4799134: WebView is the bottleneck for this logic, but we must validate 
    // the selector here because we implement it here, and we must implement it here because the AppKit 
    // code checks the first responder.
    if (action == @selector(toggleSmartInsertDelete:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self smartInsertDeleteEnabled] ? NSOnState : NSOffState];
        return [self _canEdit];
    }
    if (action == @selector(toggleAutomaticQuoteSubstitution:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticQuoteSubstitutionEnabled] ? NSOnState : NSOffState];
        return [self _canEdit];
    }
    if (action == @selector(toggleAutomaticLinkDetection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticLinkDetectionEnabled] ? NSOnState : NSOffState];
        return [self _canEdit];
    }
    if (action == @selector(toggleAutomaticDashSubstitution:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticDashSubstitutionEnabled] ? NSOnState : NSOffState];
        return [self _canEdit];
    }
    if (action == @selector(toggleAutomaticTextReplacement:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticTextReplacementEnabled] ? NSOnState : NSOffState];
        return [self _canEdit];
    }
    if (action == @selector(toggleAutomaticSpellingCorrection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:[self isAutomaticSpellingCorrectionEnabled] ? NSOnState : NSOffState];
        return [self _canEdit];
    }
#endif
    
    Editor::Command command = [self coreCommandBySelector:action];
    if (command.isSupported()) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]])
            [menuItem setState:kit(command.state())];
        return command.isEnabled();
    }

    return YES;
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    // This can be called during teardown when _webView is nil. Return NO when this happens, because CallUIDelegateReturningBoolean
    // assumes the WebVIew is non-nil.
    if (![self _webView])
        return NO;
    BOOL result = [self validateUserInterfaceItemWithoutDelegate:item];
    return CallUIDelegateReturningBoolean(result, [self _webView], @selector(webView:validateUserInterfaceItem:defaultValidation:), item, result);
}

- (BOOL)acceptsFirstResponder
{
    // Don't accept first responder when we first click on this view.
    // We have to pass the event down through WebCore first to be sure we don't hit a subview.
    // Do accept first responder at any other time, for example from keyboard events,
    // or from calls back from WebCore once we begin mouse-down event handling.
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSLeftMouseDown
            && !_private->handlingMouseDownEvent
            && NSPointInRect([event locationInWindow], [self convertRect:[self visibleRect] toView:nil])) {
        return NO;
    }
    return YES;
}

- (BOOL)maintainsInactiveSelection
{
    // This method helps to determine whether the WebHTMLView should maintain
    // an inactive selection when it's not first responder.
    // Traditionally, these views have not maintained such selections,
    // clearing them when the view was not first responder. However,
    // to fix bugs like this one:
    // <rdar://problem/3672088>: "Editable WebViews should maintain a selection even 
    //                            when they're not firstResponder"
    // it was decided to add a switch to act more like an NSTextView.

    if ([[self _webView] maintainsInactiveSelection])
        return YES;

    // Predict the case where we are losing first responder status only to
    // gain it back again. Want to keep the selection in that case.
    id nextResponder = [[self window] _newFirstResponderAfterResigning];
    if ([nextResponder isKindOfClass:[NSScrollView class]]) {
        id contentView = [nextResponder contentView];
        if (contentView)
            nextResponder = contentView;
    }
    if ([nextResponder isKindOfClass:[NSClipView class]]) {
        id documentView = [nextResponder documentView];
        if (documentView)
            nextResponder = documentView;
    }
    if (nextResponder == self)
        return YES;

    Frame* coreFrame = core([self _frame]);
    bool selectionIsEditable = coreFrame && coreFrame->selection()->isContentEditable();
    bool nextResponderIsInWebView = [nextResponder isKindOfClass:[NSView class]]
        && [nextResponder isDescendantOf:[[[self _webView] mainFrame] frameView]];

    return selectionIsEditable && nextResponderIsInWebView;
}

- (void)addMouseMovedObserver
{
    if (!_private->dataSource || ![self _isTopHTMLView] || _private->observingMouseMovedNotifications)
        return;

    // Unless the Dashboard asks us to do this for all windows, keep an observer going only for the key window.
    if (!([[self window] isKeyWindow] 
#if ENABLE(DASHBOARD_SUPPORT)
            || [[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows]
#endif
        ))
        return;

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mouseMovedNotification:)
        name:WKMouseMovedNotification() object:nil];
    [self _frameOrBoundsChanged];
    _private->observingMouseMovedNotifications = true;
}

- (void)removeMouseMovedObserver
{
#if ENABLE(DASHBOARD_SUPPORT)
    // Don't remove the observer if we're running the Dashboard.
    if ([[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows])
        return;
#endif

    [[self _webView] _mouseDidMoveOverElement:nil modifierFlags:0];
    [self _removeMouseMovedObserverUnconditionally];
}

- (void)addSuperviewObservers
{
    if (_private->observingSuperviewNotifications)
        return;

    NSView *superview = [self superview];
    if (!superview || ![self window])
        return;
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(_frameOrBoundsChanged) name:NSViewFrameDidChangeNotification object:superview];
    [notificationCenter addObserver:self selector:@selector(_frameOrBoundsChanged) name:NSViewBoundsDidChangeNotification object:superview];
    
    // In addition to registering for frame/bounds change notifications, call -_frameOrBoundsChanged.
    // It will check the current scroll against the previous layout's scroll.  We need to
    // do this here to catch the case where the WebView is laid out at one size, removed from its
    // window, resized, and inserted into another window.  Our frame/bounds changed notifications
    // will not be sent in that situation, since we only watch for changes while in the view hierarchy.
    [self _frameOrBoundsChanged];
    
    _private->observingSuperviewNotifications = true;
}

- (void)addWindowObservers
{
    if (_private->observingWindowNotifications)
        return;
    
    NSWindow *window = [self window];
    if (!window)
        return;
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(windowDidResignKey:) name:NSWindowDidResignKeyNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(windowWillClose:) name:NSWindowWillCloseNotification object:window];
    
    _private->observingWindowNotifications = true;
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self _removeSuperviewObservers];
}

- (void)viewDidMoveToSuperview
{
    if ([self superview] != nil)
        [self addSuperviewObservers];
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private)
        return;

    // FIXME: Some of these calls may not work because this view may be already removed from it's superview.
    [self _removeMouseMovedObserverUnconditionally];
    [self _removeWindowObservers];
    [self _removeSuperviewObservers];
    [self _cancelUpdateMouseoverTimer];
    
    [[self _pluginController] stopAllPlugins];
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private || _private->closed)
        return;
        
    [self _stopAutoscrollTimer];
    if ([self window]) {
        _private->lastScrollPosition = [[self superview] bounds].origin;
        [self addWindowObservers];
        [self addSuperviewObservers];
        [self addMouseMovedObserver];

        [[self _pluginController] startAllPlugins];

        _private->lastScrollPosition = NSZeroPoint;
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewWillMoveToHostWindow:) withObject:hostWindow];
}

- (void)viewDidMoveToHostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewDidMoveToHostWindow) withObject:nil];
}


- (void)addSubview:(NSView *)view
{
    [super addSubview:view];

    if ([WebPluginController isPlugInView:view])
        [[self _pluginController] addPlugin:view];
}

- (void)willRemoveSubview:(NSView *)subview
{
    if ([WebPluginController isPlugInView:subview])
        [[self _pluginController] destroyPlugin:subview];

    [super willRemoveSubview:subview];
}

- (void)reapplyStyles
{
    if (!_private->needsToApplyStyles)
        return;
    
#ifdef LOG_TIMES
    double start = CFAbsoluteTimeGetCurrent();
#endif

    if (Frame* coreFrame = core([self _frame])) {
        if (FrameView* coreView = coreFrame->view())
            coreView->setMediaType(_private->printing ? "print" : "screen");
        if (Document* document = coreFrame->document())
            document->setPrinting(_private->printing);
        coreFrame->reapplyStyles();
    }
    
#ifdef LOG_TIMES        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}

// Do a layout, but set up a new fixed width for the purposes of doing printing layout.
// minPageWidth==0 implies a non-printing layout
- (void)layoutToMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustViewSize
{
    [self reapplyStyles];
    
    if (![self _needsLayout])
        return;

#ifdef LOG_TIMES        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    LOG(View, "%@ doing layout", self);

    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    if (FrameView* coreView = coreFrame->view()) {
        if (minPageWidth > 0.0)
            coreView->forceLayoutWithPageWidthRange(minPageWidth, maxPageWidth, adjustViewSize);
        else {
            coreView->forceLayout(!adjustViewSize);
            if (adjustViewSize)
                coreView->adjustViewSize();
        }
    }
    
#ifdef LOG_TIMES        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}

- (void)layout
{
    [self layoutToMinimumPageWidth:0.0f maximumPageWidth:0.0f adjustingViewSize:NO];
}

// Deliver mouseup events to the DOM for button 2.
- (void)rightMouseUp:(NSEvent *)event
{
    // There's a chance that if we run a nested event loop the event will be released.
    // Retaining and then autoreleasing prevents that from causing a problem later here or
    // inside AppKit code.
    [[event retain] autorelease];

    [super rightMouseUp:event];

    if (Frame* coreframe = core([self _frame]))
        coreframe->eventHandler()->mouseUp(event);
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    // There's a chance that if we run a nested event loop the event will be released.
    // Retaining and then autoreleasing prevents that from causing a problem later here or
    // inside AppKit code.
    [[event retain] autorelease];

    [_private->completionController endRevertingChange:NO moveLeft:NO];

    RefPtr<Frame> coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;

    Page* page = coreFrame->page();
    if (!page)
        return nil;

    // Match behavior of other browsers by sending a mousedown event for right clicks.
    _private->handlingMouseDownEvent = YES;
    page->contextMenuController()->clearContextMenu();
    coreFrame->eventHandler()->mouseDown(event);
    BOOL handledEvent = coreFrame->eventHandler()->sendContextMenuEvent(event);
    _private->handlingMouseDownEvent = NO;

    if (!handledEvent)
        return nil;

    // Re-get page, since it might have gone away during event handling.
    page = coreFrame->page();
    if (!page)
        return nil;

    ContextMenu* coreMenu = page->contextMenuController()->contextMenu();
    if (!coreMenu)
        return nil;

    NSArray* menuItems = coreMenu->platformDescription();
    if (!menuItems)
        return nil;

    NSUInteger count = [menuItems count];
    if (!count)
        return nil;

    NSMenu* menu = [[[NSMenu alloc] init] autorelease];
    for (NSUInteger i = 0; i < count; i++)
        [menu addItem:[menuItems objectAtIndex:i]];
    return menu;
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return [self searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag startInSelection:NO];
}

- (void)clearFocus
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    Document* document = coreFrame->document();
    if (!document)
        return;
    
    document->setFocusedNode(0);
}

- (BOOL)isOpaque
{
    return [[self _webView] drawsBackground];
}

- (void)setNeedsDisplay:(BOOL)flag
{
    LOG(View, "%@ setNeedsDisplay:%@", self, flag ? @"YES" : @"NO");
    [super setNeedsDisplay:flag];
}

- (void)setNeedsLayout: (BOOL)flag
{
    LOG(View, "%@ setNeedsLayout:%@", self, flag ? @"YES" : @"NO");
    if (!flag)
        return; // There's no way to say you don't need a layout.
    if (Frame* frame = core([self _frame])) {
        if (frame->document() && frame->document()->inPageCache())
            return;
        if (FrameView* view = frame->view())
            view->setNeedsLayout();
    }
}

- (void)setNeedsToApplyStyles: (BOOL)flag
{
    LOG(View, "%@ setNeedsToApplyStyles:%@", self, flag ? @"YES" : @"NO");
    _private->needsToApplyStyles = flag;
}

- (void)drawSingleRect:(NSRect)rect
{
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);
        
    ASSERT([[self superview] isKindOfClass:[WebClipView class]]);

    [(WebClipView *)[self superview] setAdditionalClip:rect];

    @try {
        if ([self _transparentBackground]) {
            [[NSColor clearColor] set];
            NSRectFill (rect);
        }

        [[self _frame] _drawRect:rect contentsOnly:YES];

        WebView *webView = [self _webView];

        // This hack is needed for <rdar://problem/5023545>. We can hit a race condition where drawRect will be
        // called after the WebView has closed. If the client did not properly close the WebView and set the 
        // UIDelegate to nil, then the UIDelegate will be stale and this code will crash. 
        static BOOL version3OrLaterClient = WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_QUICKBOOKS_QUIRK);
        if (version3OrLaterClient)
            [[webView _UIDelegateForwarder] webView:webView didDrawRect:[webView convertRect:rect fromView:self]];

        if (WebNodeHighlight *currentHighlight = [webView currentNodeHighlight])
            [currentHighlight setNeedsUpdateInTargetViewRect:[self convertRect:rect toView:[currentHighlight targetView]]];

        [(WebClipView *)[self superview] resetAdditionalClip];

        [NSGraphicsContext restoreGraphicsState];
    } @catch (NSException *localException) {
        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];
        LOG_ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    }
}

- (void)drawRect:(NSRect)rect
{
    ASSERT_MAIN_THREAD();
    LOG(View, "%@ drawing", self);

    const NSRect *rects;
    NSInteger count;
    [self getRectsBeingDrawn:&rects count:&count];

    BOOL subviewsWereSetAside = _private->subviewsSetAside;
    if (subviewsWereSetAside)
        [self _restoreSubviews];

#ifdef LOG_TIMES
    double start = CFAbsoluteTimeGetCurrent();
#endif

    WebView *webView = [self _webView];
    if ([webView _mustDrawUnionedRect:rect singleRects:rects count:count])
        [self drawSingleRect:rect];
    else
        for (int i = 0; i < count; ++i)
            [self drawSingleRect:rects[i]];

#ifdef LOG_TIMES
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif

    if (subviewsWereSetAside)
        [self _setAsideSubviews];
        
#if USE(ACCELERATED_COMPOSITING)
    if ([webView _needsOneShotDrawingSynchronization]) {
        // Disable screen updates so that any layer changes committed here
        // don't show up on the screen before the window flush at the end
        // of the current window display, but only if a window flush is actually
        // going to happen.
        NSWindow *window = [self window];
        if ([window viewsNeedDisplay])
            [window disableScreenUpdatesUntilFlush];
        
        // Make sure any layer changes that happened as a result of layout
        // via -viewWillDraw are committed.
        [CATransaction flush];
        [webView _setNeedsOneShotDrawingSynchronization:NO];
    }
#endif
}

// Turn off the additional clip while computing our visibleRect.
- (NSRect)visibleRect
{
    if (!([[self superview] isKindOfClass:[WebClipView class]]))
        return [super visibleRect];
        
    WebClipView *clipView = (WebClipView *)[self superview];

    BOOL hasAdditionalClip = [clipView hasAdditionalClip];
    if (!hasAdditionalClip) {
        return [super visibleRect];
    }
    
    NSRect additionalClip = [clipView additionalClip];
    [clipView resetAdditionalClip];
    NSRect visibleRect = [super visibleRect];
    [clipView setAdditionalClip:additionalClip];
    return visibleRect;
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:notification waitUntilDone:NO];
        return;
    }

    NSWindow *keyWindow = [notification object];

    if (keyWindow == [self window])
        [self addMouseMovedObserver];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:notification waitUntilDone:NO];
        return;
    }

    NSWindow *formerKeyWindow = [notification object];

    if (formerKeyWindow == [self window])
        [self removeMouseMovedObserver];

    if (formerKeyWindow == [self window] || formerKeyWindow == [[self window] attachedSheet])
        [_private->completionController endRevertingChange:NO moveLeft:NO];
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:notification waitUntilDone:NO];
        return;
    }

    [_private->completionController endRevertingChange:NO moveLeft:NO];
    [[self _pluginController] destroyAllPlugins];
}

- (void)scrollWheel:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    Frame* frame = core([self _frame]);
    if (!frame || !frame->eventHandler()->wheelEvent(event))
        [super scrollWheel:event];
}

- (BOOL)_isSelectionEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self elementAtPoint:point allowShadowContent:YES] objectForKey:WebElementIsSelectedKey] boolValue];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    
#if ENABLE(DASHBOARD_SUPPORT)
    if ([[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse])
        return YES;
#endif
    
    if (hitHTMLView) {
        bool result = false;
        if (Frame* coreFrame = core([hitHTMLView _frame])) {
            coreFrame->eventHandler()->setActivationEventNumber([event eventNumber]);
            [hitHTMLView _setMouseDownEvent:event];
            if ([hitHTMLView _isSelectionEvent:event])
                result = coreFrame->eventHandler()->eventMayStartDrag(event);
            [hitHTMLView _setMouseDownEvent:nil];
        }
        return result;
    }
    return [hitView acceptsFirstMouse:event];
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    if (hitHTMLView) {
        bool result = false;
        if ([hitHTMLView _isSelectionEvent:event]) {
            if (Frame* coreFrame = core([hitHTMLView _frame])) {
                [hitHTMLView _setMouseDownEvent:event];
                result = coreFrame->eventHandler()->eventMayStartDrag(event);
                [hitHTMLView _setMouseDownEvent:nil];
            }
        }
        return result;
    }
    return [hitView shouldDelayWindowOrderingForEvent:event];
}

- (void)mouseDown:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    RetainPtr<WebHTMLView> protector = self;
    if ([[self inputContext] wantsToHandleMouseEvents] && [[self inputContext] handleMouseEvent:event])
        return;

    _private->handlingMouseDownEvent = YES;

    // Record the mouse down position so we can determine drag hysteresis.
    [self _setMouseDownEvent:event];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        goto done;

    [_private->completionController endRevertingChange:NO moveLeft:NO];

    // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
    // We don't want to pass them along to KHTML a second time.
    if (!([event modifierFlags] & NSControlKeyMask)) {
        _private->ignoringMouseDraggedEvents = NO;

        // Don't do any mouseover while the mouse is down.
        [self _cancelUpdateMouseoverTimer];

        // Let WebCore get a chance to deal with the event. This will call back to us
        // to start the autoscroll timer if appropriate.
        if (Frame* coreframe = core([self _frame]))
            coreframe->eventHandler()->mouseDown(event);
    }

done:
    _private->handlingMouseDownEvent = NO;
}

- (void)dragImage:(NSImage *)dragImage
               at:(NSPoint)at
           offset:(NSSize)offset
            event:(NSEvent *)event
       pasteboard:(NSPasteboard *)pasteboard
           source:(id)source
        slideBack:(BOOL)slideBack
{
    ASSERT(self == [self _topHTMLView]);
    [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;

    [self retain];

    if (!_private->ignoringMouseDraggedEvents) {
        if (Frame* frame = core([self _frame])) {
            if (Page* page = frame->page())
                page->mainFrame()->eventHandler()->mouseDragged(event);
        }
    }

    [self release];
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    ASSERT(![self _webView] || [self _isTopHTMLView]);
    
    Page* page = core([self _webView]);
    if (!page)
        return NSDragOperationNone;

    return (NSDragOperation)page->dragController()->sourceDragOperation();
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    ASSERT(![self _webView] || [self _isTopHTMLView]);
    
    NSPoint windowImageLoc = [[self window] convertScreenToBase:aPoint];
    NSPoint windowMouseLoc = windowImageLoc;
    
    if (Page* page = core([self _webView])) {
        DragController* dragController = page->dragController();
        windowMouseLoc = NSMakePoint(windowImageLoc.x + dragController->dragOffset().x(), windowImageLoc.y + dragController->dragOffset().y());
        dragController->dragEnded();
    }
    
    [[self _frame] _dragSourceEndedAt:windowMouseLoc operation:operation];
    
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;
    
    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // WebCore expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                            location:windowMouseLoc
                                       modifierFlags:[[NSApp currentEvent] modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[[self window] windowNumber]
                                             context:[[NSApp currentEvent] context]
                                         eventNumber:0 clickCount:0 pressure:0];
    [self mouseUp:fakeEvent]; // This will also update the mouseover state.
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSFileWrapper *wrapper = nil;
    NSURL *draggingImageURL = nil;
    
    if (WebCore::CachedImage* tiffResource = [self promisedDragTIFFDataSource]) {
        
        SharedBuffer *buffer = static_cast<CachedResource*>(tiffResource)->data();
        if (!buffer)
            goto noPromisedData;
        
        NSData *data = buffer->createNSData();
        NSURLResponse *response = tiffResource->response().nsURLResponse();
        draggingImageURL = [response URL];
        wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:data] autorelease];
        NSString* filename = [response suggestedFilename];
        NSString* trueExtension(tiffResource->image()->filenameExtension());
        if (!matchesExtensionOrEquivalent(filename, trueExtension))
            filename = [[filename stringByAppendingString:@"."] stringByAppendingString:trueExtension];
        [wrapper setPreferredFilename:filename];
    }
    
noPromisedData:
    
    if (!wrapper) {
        ASSERT(![self _webView] || [self _isTopHTMLView]);
        Page* page = core([self _webView]);
        
        //If a load occurs midway through a drag, the view may be detached, which gives
        //us no ability to get to the original Page, so we cannot access any drag state
        //FIXME: is there a way to recover?
        if (!page) 
            return nil; 
        
        const KURL& imageURL = page->dragController()->draggingImageURL();
        ASSERT(!imageURL.isEmpty());
        draggingImageURL = imageURL;

        wrapper = [[self _dataSource] _fileWrapperForURL:draggingImageURL];
    }
    
    if (wrapper == nil) {
        LOG_ERROR("Failed to create image file.");
        return nil;
    }

    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = [[NSFileManager defaultManager] _webkit_pathWithUniqueFilenameForPath:path];
    if (![wrapper writeToFile:path atomically:NO updateFilenames:YES])
        LOG_ERROR("Failed to create image file via -[NSFileWrapper writeToFile:atomically:updateFilenames:]");
    
    if (draggingImageURL)
        [[NSFileManager defaultManager] _webkit_setMetadataURL:[draggingImageURL absoluteString] referrer:nil atPath:path];
    
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

- (void)mouseUp:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    [self _setMouseDownEvent:nil];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;

    [self retain];

    [self _stopAutoscrollTimer];
    if (Frame* frame = core([self _frame])) {
        if (Page* page = frame->page())
            page->mainFrame()->eventHandler()->mouseUp(event);
    }
    [self _updateMouseoverWithFakeEvent];

    [self release];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    [self _updateMouseoverWithEvent:[[notification userInfo] objectForKey:@"NSEvent"]];
}

// returning YES from this method is the way we tell AppKit that it is ok for this view
// to be in the key loop even when "tab to all controls" is not on.
- (BOOL)needsPanelToBecomeKey
{
    return YES;
}

// Utility function to make sure we don't return anything through the NSTextInput
// API when an editable region is not currently focused.
static BOOL isTextInput(Frame* coreFrame)
{
    return coreFrame && !coreFrame->selection()->isNone() && coreFrame->selection()->isContentEditable();
}

static BOOL isInPasswordField(Frame* coreFrame)
{
    return coreFrame && coreFrame->selection()->isInPasswordField();
}

- (BOOL)becomeFirstResponder
{
    NSSelectionDirection direction = NSDirectSelection;
    if (![[self _webView] _isPerformingProgrammaticFocus])
        direction = [[self window] keyViewSelectionDirection];

    [self _updateFontPanel];
    
    Frame* frame = core([self _frame]);
    if (!frame)
        return YES;

    BOOL exposeInputContext = isTextInput(frame) && !isInPasswordField(frame);
    if (exposeInputContext != _private->exposeInputContext) {
        _private->exposeInputContext = exposeInputContext;
        [NSApp updateWindows];
    }

    frame->editor()->setStartNewKillRingSequence(true);

    Page* page = frame->page();
    if (!page)
        return YES;

    if (![[self _webView] _isPerformingProgrammaticFocus])
        page->focusController()->setFocusedFrame(frame);

    page->focusController()->setFocused(true);

    if (direction == NSDirectSelection)
        return YES;

    if (Document* document = frame->document())
        document->setFocusedNode(0);
    page->focusController()->setInitialFocus(direction == NSSelectingNext ? FocusDirectionForward : FocusDirectionBackward,
                                             frame->eventHandler()->currentKeyboardEvent().get());
    return YES;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        [_private->completionController endRevertingChange:NO moveLeft:NO];
        Frame* coreFrame = core([self _frame]);
        if (!coreFrame)
            return resign;
        Page* page = coreFrame->page();
        if (!page)
            return resign;
        if (![self maintainsInactiveSelection]) { 
            [self deselectAll];
            if (![[self _webView] _isPerformingProgrammaticFocus])
                [self clearFocus];
        }
        
        id nextResponder = [[self window] _newFirstResponderAfterResigning];
        bool nextResponderIsInWebView = [nextResponder isKindOfClass:[NSView class]]
            && [nextResponder isDescendantOf:[[[self _webView] mainFrame] frameView]];
        if (!nextResponderIsInWebView)
            page->focusController()->setFocused(false);
    }
    return resign;
}

- (void)setDataSource:(WebDataSource *)dataSource 
{
    ASSERT(dataSource);
    if (_private->dataSource != dataSource) {
        ASSERT(!_private->closed);
        BOOL hadDataSource = _private->dataSource != nil;

        [dataSource retain];
        [_private->dataSource release];
        _private->dataSource = dataSource;
        [_private->pluginController setDataSource:dataSource];

        if (!hadDataSource)
            [self addMouseMovedObserver];
    }
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

// This is an override of an NSControl method that wants to repaint the entire view when the window resigns/becomes
// key.  WebHTMLView is an NSControl only because it hosts NSCells that are painted by WebCore's Aqua theme
// renderer (and those cells must be hosted by an enclosing NSControl in order to paint properly).
- (void)updateCell:(NSCell*)cell
{
}

// Does setNeedsDisplay:NO as a side effect when printing is ending.
// pageWidth != 0 implies we will relayout to a new width
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize
{
    WebFrame *frame = [self _frame];
    NSArray *subframes = [frame childFrames];
    unsigned n = [subframes count];
    unsigned i;
    for (i = 0; i != n; ++i) {
        WebFrame *subframe = [subframes objectAtIndex:i];
        WebFrameView *frameView = [subframe frameView];
        if ([[subframe _dataSource] _isDocumentHTML]) {
            [(WebHTMLView *)[frameView documentView] _setPrinting:printing minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:adjustViewSize];
        }
    }

    if (printing != _private->printing) {
        [_private->pageRects release];
        _private->pageRects = nil;
        _private->printing = printing;
        if (!printing)
            _private->avoidingPrintOrphan = NO;
        [self setNeedsToApplyStyles:YES];
        [self setNeedsLayout:YES];
        [self layoutToMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
        if (!printing) {
            // Can't do this when starting printing or nested printing won't work, see 3491427.
            [self setNeedsDisplay:NO];
        }
    }
}

- (BOOL)canPrintHeadersAndFooters
{
    return YES;
}

// This is needed for the case where the webview is embedded in the view that's being printed.
// It shouldn't be called when the webview is being printed directly.
- (void)adjustPageHeightNew:(CGFloat *)newBottom top:(CGFloat)oldTop bottom:(CGFloat)oldBottom limit:(CGFloat)bottomLimit
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    if (!wasInPrintingMode)
        [self _setPrinting:YES minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];

    float newBottomFloat = *newBottom;
    if (Frame* frame = core([self _frame])) {
        if (FrameView* view = frame->view())
            view->adjustPageHeight(&newBottomFloat, oldTop, oldBottom, bottomLimit);
    }

#ifdef __LP64__
    // If the new bottom is equal to the old bottom (when both are treated as floats), we just copy
    // oldBottom over to newBottom. This prevents rounding errors that can occur when converting newBottomFloat to a double.
    if (fabs((float)oldBottom - newBottomFloat) <= numeric_limits<float>::epsilon()) 
        *newBottom = oldBottom;
    else
#endif
        *newBottom = newBottomFloat;
    
    if (!wasInPrintingMode) {
        NSPrintOperation *currenPrintOperation = [NSPrintOperation currentOperation];
        if (currenPrintOperation)
            // delay _setPrinting:NO until back to main loop as this method may get called repeatedly
            [self performSelector:@selector(_delayedEndPrintMode:) withObject:currenPrintOperation afterDelay:0];
        else
            // not sure if this is actually ever invoked, it probably shouldn't be
            [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];
    }
}

- (float)_availablePaperWidthForPrintOperation:(NSPrintOperation *)printOperation
{
    NSPrintInfo *printInfo = [printOperation printInfo];
    return [printInfo paperSize].width - [printInfo leftMargin] - [printInfo rightMargin];
}

- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation
{
    float viewWidth = NSWidth([self bounds]);
    if (viewWidth < 1) {
        LOG_ERROR("%@ has no width when printing", self);
        return 1.0f;
    }

    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    float maxShrinkToFitScaleFactor = 1.0f / PrintingMaximumShrinkFactor;
    float shrinkToFitScaleFactor = [self _availablePaperWidthForPrintOperation:printOperation]/viewWidth;
    float shrinkToAvoidOrphan = _private->avoidingPrintOrphan ? (1.0f / PrintingOrphanShrinkAdjustment) : 1.0f;
    return userScaleFactor * max(maxShrinkToFitScaleFactor, shrinkToFitScaleFactor) * shrinkToAvoidOrphan;
}

// FIXME 3491344: This is a secret AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (CGFloat)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return [self _scaleFactorForPrintOperation:printOperation];
}

// This is used for Carbon printing. At some point we might want to make this public API.
- (void)setPageWidthForPrinting:(float)pageWidth
{
    [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];
    [self _setPrinting:YES minimumPageWidth:pageWidth maximumPageWidth:pageWidth adjustViewSize:YES];
}

- (void)_endPrintMode
{
    [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:YES];
    [[self window] setAutodisplay:YES];
}

- (void)_delayedEndPrintMode:(NSPrintOperation *)initiatingOperation
{
    ASSERT_ARG(initiatingOperation, initiatingOperation != nil);
    NSPrintOperation *currentOperation = [NSPrintOperation currentOperation];
    if (initiatingOperation == currentOperation) {
        // The print operation is still underway. We don't expect this to ever happen, hence the assert, but we're
        // being extra paranoid here since the printing code is so fragile. Delay the cleanup
        // further.
        ASSERT_NOT_REACHED();
        [self performSelector:@selector(_delayedEndPrintMode:) withObject:initiatingOperation afterDelay:0];
    } else if ([currentOperation view] == self) {
        // A new print job has started, but it is printing the same WebHTMLView again. We don't expect
        // this to ever happen, hence the assert, but we're being extra paranoid here since the printing code is so
        // fragile. Do nothing, because we don't want to break the print job currently in progress, and
        // the print job currently in progress is responsible for its own cleanup.
        ASSERT_NOT_REACHED();
    } else {
        // The print job that kicked off this delayed call has finished, and this view is not being
        // printed again. We expect that no other print job has started. Since this delayed call wasn't
        // cancelled, beginDocument and endDocument must not have been called, and we need to clean up
        // the print mode here.
        ASSERT(currentOperation == nil);
        [self _endPrintMode];
    }
}

// Return the number of pages available for printing
- (BOOL)knowsPageRange:(NSRangePointer)range
{
    // Must do this explicit display here, because otherwise the view might redisplay while the print
    // sheet was up, using printer fonts (and looking different).
    [self displayIfNeeded];
    [[self window] setAutodisplay:NO];
    
    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    float minLayoutWidth = 0.0f;
    float maxLayoutWidth = 0.0f;
    Frame* frame = core([self _frame]);
    if (!frame)
        return NO;
    if (!frame->document() || !frame->document()->isFrameSet()) {
        float paperWidth = [self _availablePaperWidthForPrintOperation:[NSPrintOperation currentOperation]];
        minLayoutWidth = paperWidth * PrintingMinimumShrinkFactor;
        maxLayoutWidth = paperWidth * PrintingMaximumShrinkFactor;
    }
    [self _setPrinting:YES minimumPageWidth:minLayoutWidth maximumPageWidth:maxLayoutWidth adjustViewSize:YES]; // will relayout
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    // Certain types of errors, including invalid page ranges, can cause beginDocument and
    // endDocument to be skipped after we've put ourselves in print mode (see 4145905). In those cases
    // we need to get out of print mode without relying on any more callbacks from the printing mechanism.
    // If we get as far as beginDocument without trouble, then this delayed request will be cancelled.
    // If not cancelled, this delayed call will be invoked in the next pass through the main event loop,
    // which is after beginDocument and endDocument would be called.
    [self performSelector:@selector(_delayedEndPrintMode:) withObject:printOperation afterDelay:0];
    [[self _webView] _adjustPrintingMarginsForHeaderAndFooter];
    
    // There is a theoretical chance that someone could do some drawing between here and endDocument,
    // if something caused setNeedsDisplay after this point. If so, it's not a big tragedy, because
    // you'd simply see the printer fonts on screen. As of this writing, this does not happen with Safari.

    range->location = 1;
    float totalScaleFactor = [self _scaleFactorForPrintOperation:printOperation];
    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    [_private->pageRects release];
    float fullPageHeight = floorf([self _calculatePrintHeight]/totalScaleFactor);
    NSArray *newPageRects = [[self _frame] _computePageRectsWithPrintWidthScaleFactor:userScaleFactor
                                                                          printHeight:fullPageHeight];
    
    // AppKit gets all messed up if you give it a zero-length page count (see 3576334), so if we
    // hit that case we'll pass along a degenerate 1 pixel square to print. This will print
    // a blank page (with correct-looking header and footer if that option is on), which matches
    // the behavior of IE and Camino at least.
    if ([newPageRects count] == 0)
        newPageRects = [NSArray arrayWithObject:[NSValue valueWithRect:NSMakeRect(0, 0, 1, 1)]];
    else if ([newPageRects count] > 1) {
        // If the last page is a short orphan, try adjusting the print height slightly to see if this will squeeze the
        // content onto one fewer page. If it does, use the adjusted scale. If not, use the original scale.
        float lastPageHeight = NSHeight([[newPageRects lastObject] rectValue]);
        if (lastPageHeight/fullPageHeight < LastPrintedPageOrphanRatio) {
            NSArray *adjustedPageRects = [[self _frame] _computePageRectsWithPrintWidthScaleFactor:userScaleFactor
                                                                                       printHeight:fullPageHeight*PrintingOrphanShrinkAdjustment];
            // Use the adjusted rects only if the page count went down
            if ([adjustedPageRects count] < [newPageRects count]) {
                newPageRects = adjustedPageRects;
                _private->avoidingPrintOrphan = YES;
            }
        }
    }
    
    _private->pageRects = [newPageRects retain];
    
    range->length = [_private->pageRects count];
    
    return YES;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(NSInteger)page
{
    return [[_private->pageRects objectAtIndex:page - 1] rectValue];
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    
    [[self _webView] _drawHeaderAndFooter];
}

- (void)beginDocument
{
    @try {
        // From now on we'll get a chance to call _endPrintMode in either beginDocument or
        // endDocument, so we can cancel the "just in case" pending call.
        [NSObject cancelPreviousPerformRequestsWithTarget:self
                                                 selector:@selector(_delayedEndPrintMode:)
                                                   object:[NSPrintOperation currentOperation]];
        [super beginDocument];
    } @catch (NSException *localException) {
        // Exception during [super beginDocument] means that endDocument will not get called,
        // so we need to clean up our "print mode" here.
        [self _endPrintMode];
    }
}

- (void)endDocument
{
    [super endDocument];
    // Note sadly at this point [NSGraphicsContext currentContextDrawingToScreen] is still NO 
    [self _endPrintMode];
}

- (void)keyDown:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    RetainPtr<WebHTMLView> selfProtector = self;
    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);

    BOOL callSuper = NO;

    [_private->keyDownEvent release];
    _private->keyDownEvent = [event retain];

    BOOL completionPopupWasOpen = _private->completionController && [_private->completionController popupWindowIsOpen];
    Frame* coreFrame = core([self _frame]);
    if (!eventWasSentToWebCore && coreFrame && coreFrame->eventHandler()->keyEvent(event)) {
        // WebCore processed a key event, bail on any preexisting complete: UI
        if (completionPopupWasOpen)
            [_private->completionController endRevertingChange:YES moveLeft:NO];
    } else if (!_private->completionController || ![_private->completionController filterKeyDown:event]) {
        // Not consumed by complete: popup window
        [_private->completionController endRevertingChange:YES moveLeft:NO];
        callSuper = YES;
    }
    if (callSuper)
        [super keyDown:event];
    else
        [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)keyUp:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);

    RetainPtr<WebHTMLView> selfProtector = self;
    Frame* coreFrame = core([self _frame]);
    if (coreFrame && !eventWasSentToWebCore)
        coreFrame->eventHandler()->keyEvent(event);
    else
        [super keyUp:event];
}

- (void)flagsChanged:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->eventHandler()->capsLockStateMayHaveChanged();
    
    RetainPtr<WebHTMLView> selfProtector = self;

    unsigned short keyCode = [event keyCode];

    //Don't make an event from the num lock and function keys
    if (coreFrame && keyCode != 0 && keyCode != 10 && keyCode != 63) {
        coreFrame->eventHandler()->keyEvent(PlatformKeyboardEvent(event));
        return;
    }
        
    [super flagsChanged:event];
}

- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        id accTree = [[self _frame] _accessibilityTree];
        if (accTree)
            return [NSArray arrayWithObject:accTree];
        return nil;
    }
    return [super accessibilityAttributeValue:attributeName];
}

- (id)accessibilityFocusedUIElement
{
    id accTree = [[self _frame] _accessibilityTree];
    if (accTree)
        return [accTree accessibilityFocusedUIElement];
    return self;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    id accTree = [[self _frame] _accessibilityTree];
    if (accTree) {
        NSPoint windowCoord = [[self window] convertScreenToBase:point];
        return [accTree accessibilityHitTest:[self convertPoint:windowCoord fromView:nil]];
    }
    return self;
}

- (id)_accessibilityParentForSubview:(NSView *)subview
{
    id accTree = [[self _frame] _accessibilityTree];
    if (!accTree)
        return self;
    id parent = [accTree _accessibilityParentForSubview:subview];
    if (!parent)
        return self;
    return parent;
}

- (void)centerSelectionInVisibleArea:(id)sender
{
    COMMAND_PROLOGUE

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->revealSelection(ScrollAlignment::alignCenterAlways);
}

- (NSData *)_selectionStartFontAttributesAsRTF
{
    Frame* coreFrame = core([self _frame]);
    NSAttributedString *string = [[NSAttributedString alloc] initWithString:@"x"
        attributes:coreFrame ? coreFrame->fontAttributesForSelectionStart() : nil];
    NSData *data = [string RTFFromRange:NSMakeRange(0, [string length]) documentAttributes:nil];
    [string release];
    return data;
}

- (NSDictionary *)_fontAttributesFromFontPasteboard
{
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    if (fontPasteboard == nil)
        return nil;
    NSData *data = [fontPasteboard dataForType:NSFontPboardType];
    if (data == nil || [data length] == 0)
        return nil;
    // NSTextView does something more efficient by parsing the attributes only, but that's not available in API.
    NSAttributedString *string = [[[NSAttributedString alloc] initWithRTF:data documentAttributes:NULL] autorelease];
    if (string == nil || [string length] == 0)
        return nil;
    return [string fontAttributesInRange:NSMakeRange(0, 1)];
}

- (DOMCSSStyleDeclaration *)_emptyStyle
{
    return [[[self _frame] DOMDocument] createCSSStyleDeclaration];
}

- (NSString *)_colorAsString:(NSColor *)color
{
    NSColor *rgbColor = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    // FIXME: If color is non-nil and rgbColor is nil, that means we got some kind
    // of fancy color that can't be converted to RGB. Changing that to "transparent"
    // might not be great, but it's probably OK.
    if (rgbColor == nil)
        return @"transparent";
    float r = [rgbColor redComponent];
    float g = [rgbColor greenComponent];
    float b = [rgbColor blueComponent];
    float a = [rgbColor alphaComponent];
    if (a == 0)
        return @"transparent";
    if (r == 0 && g == 0 && b == 0 && a == 1)
        return @"black";
    if (r == 1 && g == 1 && b == 1 && a == 1)
        return @"white";
    // FIXME: Lots more named colors. Maybe we could use the table in WebCore?
    if (a == 1)
        return [NSString stringWithFormat:@"rgb(%.0f,%.0f,%.0f)", r * 255, g * 255, b * 255];
    return [NSString stringWithFormat:@"rgba(%.0f,%.0f,%.0f,%f)", r * 255, g * 255, b * 255, a];
}

- (NSString *)_shadowAsString:(NSShadow *)shadow
{
    if (shadow == nil)
        return @"none";
    NSSize offset = [shadow shadowOffset];
    float blurRadius = [shadow shadowBlurRadius];
    if (offset.width == 0 && offset.height == 0 && blurRadius == 0)
        return @"none";
    NSColor *color = [shadow shadowColor];
    if (color == nil)
        return @"none";
    // FIXME: Handle non-integral values here?
    if (blurRadius == 0)
        return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height];
    return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height, blurRadius];
}

- (DOMCSSStyleDeclaration *)_styleFromFontAttributes:(NSDictionary *)dictionary
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSColor *color = [dictionary objectForKey:NSBackgroundColorAttributeName];
    [style setBackgroundColor:[self _colorAsString:color]];

    NSFont *font = [dictionary objectForKey:NSFontAttributeName];
    if (!font) {
        [style setFontFamily:@"Helvetica"];
        [style setFontSize:@"12px"];
        [style setFontWeight:@"normal"];
        [style setFontStyle:@"normal"];
    } else {
        NSFontManager *fm = [NSFontManager sharedFontManager];
        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        [style setFontFamily:[NSString stringWithFormat:@"'%@'", [font familyName]]];
        [style setFontSize:[NSString stringWithFormat:@"%0.fpx", [font pointSize]]];
        // FIXME: Map to the entire range of CSS weight values.
        if ([fm weightOfFont:font] >= MIN_BOLD_WEIGHT)
            [style setFontWeight:@"bold"];
        else
            [style setFontWeight:@"normal"];
        if ([fm traitsOfFont:font] & NSItalicFontMask)
            [style setFontStyle:@"italic"];
        else
            [style setFontStyle:@"normal"];
    }

    color = [dictionary objectForKey:NSForegroundColorAttributeName];
    [style setColor:color ? [self _colorAsString:color] : (NSString *)@"black"];

    NSShadow *shadow = [dictionary objectForKey:NSShadowAttributeName];
    [style setTextShadow:[self _shadowAsString:shadow]];

    int strikethroughInt = [[dictionary objectForKey:NSStrikethroughStyleAttributeName] intValue];

    int superscriptInt = [[dictionary objectForKey:NSSuperscriptAttributeName] intValue];
    if (superscriptInt > 0)
        [style setVerticalAlign:@"super"];
    else if (superscriptInt < 0)
        [style setVerticalAlign:@"sub"];
    else
        [style setVerticalAlign:@"baseline"];
    int underlineInt = [[dictionary objectForKey:NSUnderlineStyleAttributeName] intValue];
    // FIXME: Underline wins here if we have both (see bug 3790443).
    if (strikethroughInt == NSUnderlineStyleNone && underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-khtml-text-decorations-in-effect" value:@"none" priority:@""];
    else if (underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-khtml-text-decorations-in-effect" value:@"line-through" priority:@""];
    else
        [style setProperty:@"-khtml-text-decorations-in-effect" value:@"underline" priority:@""];

    return style;
}

- (void)_applyStyleToSelection:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->applyStyleToSelection(core(style), undoAction);
}

- (void)_applyParagraphStyleToSelection:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->applyParagraphStyleToSelection(core(style), undoAction);
}

- (BOOL)_handleStyleKeyEquivalent:(NSEvent *)event
{
    ASSERT([self _webView]);
    if (![[[self _webView] preferences] respectStandardStyleKeyEquivalents])
        return NO;
    
    if (![self _canEdit])
        return NO;
    
    if (([event modifierFlags] & NSDeviceIndependentModifierFlagsMask) != NSCommandKeyMask)
        return NO;
    
    NSString *string = [event characters];
    if ([string caseInsensitiveCompare:@"b"] == NSOrderedSame) {
        [self executeCoreCommandByName:"ToggleBold"];
        return YES;
    }
    if ([string caseInsensitiveCompare:@"i"] == NSOrderedSame) {
        [self executeCoreCommandByName:"ToggleItalic"];
        return YES;
    }
    
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];

    if ([self _handleStyleKeyEquivalent:event])
        return YES;
    
    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);
    BOOL ret = NO;

    [_private->keyDownEvent release];
    _private->keyDownEvent = [event retain];
    
    [self retain];

    // Pass command-key combos through WebCore if there is a key binding available for
    // this event. This lets web pages have a crack at intercepting command-modified keypresses.
    // But don't do it if we have already handled the event.
    // Pressing Esc results in a fake event being sent - don't pass it to WebCore.
    if (!eventWasSentToWebCore && event == [NSApp currentEvent] && self == [[self window] firstResponder])
        if (Frame* frame = core([self _frame]))
            ret = frame->eventHandler()->keyEvent(event);

    if (!ret)
        ret = [super performKeyEquivalent:event];

    [self release];
    
    return ret;
}

- (void)copyFont:(id)sender
{
    COMMAND_PROLOGUE

    // Put RTF with font attributes on the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    [fontPasteboard declareTypes:[NSArray arrayWithObject:NSFontPboardType] owner:nil];
    [fontPasteboard setData:[self _selectionStartFontAttributesAsRTF] forType:NSFontPboardType];
}

- (void)pasteFont:(id)sender
{
    COMMAND_PROLOGUE

    // Read RTF with font attributes from the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
    [self _applyStyleToSelection:[self _styleFromFontAttributes:[self _fontAttributesFromFontPasteboard]] withUndoAction:EditActionPasteFont];
}

- (void)pasteAsRichText:(id)sender
{
    COMMAND_PROLOGUE

    // Since rich text always beats plain text when both are on the pasteboard, it's not
    // clear how this is different from plain old paste.
    [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:NO];
}

- (NSFont *)_originalFontA
{
    return [[NSFontManager sharedFontManager] fontWithFamily:@"Helvetica" traits:0 weight:STANDARD_WEIGHT size:10.0f];
}

- (NSFont *)_originalFontB
{
    return [[NSFontManager sharedFontManager] fontWithFamily:@"Times" traits:NSFontItalicTrait weight:STANDARD_BOLD_WEIGHT size:12.0f];
}

- (void)_addToStyle:(DOMCSSStyleDeclaration *)style fontA:(NSFont *)a fontB:(NSFont *)b
{
    // Since there's no way to directly ask NSFontManager what style change it's going to do
    // we instead pass two "specimen" fonts to it and let it change them. We then deduce what
    // style change it was doing by looking at what happened to each of the two fonts.
    // So if it was making the text bold, both fonts will be bold after the fact.

    if (a == nil || b == nil)
        return;

    NSFontManager *fm = [NSFontManager sharedFontManager];

    NSFont *oa = [self _originalFontA];

    NSString *aFamilyName = [a familyName];
    NSString *bFamilyName = [b familyName];

    int aPointSize = (int)[a pointSize];
    int bPointSize = (int)[b pointSize];

    int aWeight = [fm weightOfFont:a];
    int bWeight = [fm weightOfFont:b];

    BOOL aIsItalic = ([fm traitsOfFont:a] & NSItalicFontMask) != 0;
    BOOL bIsItalic = ([fm traitsOfFont:b] & NSItalicFontMask) != 0;

    BOOL aIsBold = aWeight > MIN_BOLD_WEIGHT;

    if ([aFamilyName isEqualToString:bFamilyName]) {
        NSString *familyNameForCSS = aFamilyName;

        // The family name may not be specific enough to get us the font specified.
        // In some cases, the only way to get exactly what we are looking for is to use
        // the Postscript name.
        
        // Find the font the same way the rendering code would later if it encountered this CSS.
        NSFontTraitMask traits = aIsItalic ? NSFontItalicTrait : 0;
        int weight = aIsBold ? STANDARD_BOLD_WEIGHT : STANDARD_WEIGHT;
        NSFont *foundFont = [WebFontCache fontWithFamily:aFamilyName traits:traits weight:weight size:aPointSize];

        // If we don't find a font with the same Postscript name, then we'll have to use the
        // Postscript name to make the CSS specific enough.
        if (![[foundFont fontName] isEqualToString:[a fontName]])
            familyNameForCSS = [a fontName];

        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        [style setFontFamily:[NSString stringWithFormat:@"'%@'", familyNameForCSS]];
    }

    int soa = (int)[oa pointSize];
    if (aPointSize == bPointSize)
        [style setFontSize:[NSString stringWithFormat:@"%dpx", aPointSize]];
    else if (aPointSize < soa)
        [style _setFontSizeDelta:@"-1px"];
    else if (aPointSize > soa)
        [style _setFontSizeDelta:@"1px"];

    // FIXME: Map to the entire range of CSS weight values.
    if (aWeight == bWeight)
        [style setFontWeight:aIsBold ? @"bold" : @"normal"];

    if (aIsItalic == bIsItalic)
        [style setFontStyle:aIsItalic ? @"italic" :  @"normal"];
}

- (DOMCSSStyleDeclaration *)_styleFromFontManagerOperation
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSFontManager *fm = [NSFontManager sharedFontManager];

    NSFont *oa = [self _originalFontA];
    NSFont *ob = [self _originalFontB];    
    [self _addToStyle:style fontA:[fm convertFont:oa] fontB:[fm convertFont:ob]];

    return style;
}

- (void)changeFont:(id)sender
{
    COMMAND_PROLOGUE

    [self _applyStyleToSelection:[self _styleFromFontManagerOperation] withUndoAction:EditActionSetFont];
}

- (DOMCSSStyleDeclaration *)_styleForAttributeChange:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSShadow *shadow = [[NSShadow alloc] init];
    [shadow setShadowOffset:NSMakeSize(1, 1)];

    NSDictionary *oa = [NSDictionary dictionaryWithObjectsAndKeys:
        [self _originalFontA], NSFontAttributeName,
        nil];
    NSDictionary *ob = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSColor blackColor], NSBackgroundColorAttributeName,
        [self _originalFontB], NSFontAttributeName,
        [NSColor whiteColor], NSForegroundColorAttributeName,
        shadow, NSShadowAttributeName,
        [NSNumber numberWithInt:NSUnderlineStyleSingle], NSStrikethroughStyleAttributeName,
        [NSNumber numberWithInt:1], NSSuperscriptAttributeName,
        [NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName,
        nil];

    [shadow release];

#if 0

NSObliquenessAttributeName        /* float; skew to be applied to glyphs, default 0: no skew */
    // font-style, but that is just an on-off switch

NSExpansionAttributeName          /* float; log of expansion factor to be applied to glyphs, default 0: no expansion */
    // font-stretch?

NSKernAttributeName               /* float, amount to modify default kerning, if 0, kerning off */
    // letter-spacing? probably not good enough

NSUnderlineColorAttributeName     /* NSColor, default nil: same as foreground color */
NSStrikethroughColorAttributeName /* NSColor, default nil: same as foreground color */
    // text-decoration-color?

NSLigatureAttributeName           /* int, default 1: default ligatures, 0: no ligatures, 2: all ligatures */
NSBaselineOffsetAttributeName     /* float, in points; offset from baseline, default 0 */
NSStrokeWidthAttributeName        /* float, in percent of font point size, default 0: no stroke; positive for stroke alone, negative for stroke and fill (a typical value for outlined text would be 3.0) */
NSStrokeColorAttributeName        /* NSColor, default nil: same as foreground color */
    // need extensions?

#endif
    
    NSDictionary *a = [sender convertAttributes:oa];
    NSDictionary *b = [sender convertAttributes:ob];

    NSColor *ca = [a objectForKey:NSBackgroundColorAttributeName];
    NSColor *cb = [b objectForKey:NSBackgroundColorAttributeName];
    if (ca == cb) {
        [style setBackgroundColor:[self _colorAsString:ca]];
    }

    [self _addToStyle:style fontA:[a objectForKey:NSFontAttributeName] fontB:[b objectForKey:NSFontAttributeName]];

    ca = [a objectForKey:NSForegroundColorAttributeName];
    cb = [b objectForKey:NSForegroundColorAttributeName];
    if (ca == cb) {
        [style setColor:[self _colorAsString:ca]];
    }

    NSShadow *sha = [a objectForKey:NSShadowAttributeName];
    if (sha)
        [style setTextShadow:[self _shadowAsString:sha]];
    else if ([b objectForKey:NSShadowAttributeName] == nil)
        [style setTextShadow:@"none"];

    int sa = [[a objectForKey:NSStrikethroughStyleAttributeName] intValue];
    int sb = [[b objectForKey:NSStrikethroughStyleAttributeName] intValue];
    if (sa == sb) {
        if (sa == NSUnderlineStyleNone)
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"none" priority:@""]; 
            // we really mean "no line-through" rather than "none"
        else
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"line-through" priority:@""];
            // we really mean "add line-through" rather than "line-through"
    }

    sa = [[a objectForKey:NSSuperscriptAttributeName] intValue];
    sb = [[b objectForKey:NSSuperscriptAttributeName] intValue];
    if (sa == sb) {
        if (sa > 0)
            [style setVerticalAlign:@"super"];
        else if (sa < 0)
            [style setVerticalAlign:@"sub"];
        else
            [style setVerticalAlign:@"baseline"];
    }

    int ua = [[a objectForKey:NSUnderlineStyleAttributeName] intValue];
    int ub = [[b objectForKey:NSUnderlineStyleAttributeName] intValue];
    if (ua == ub) {
        if (ua == NSUnderlineStyleNone)
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"none" priority:@""];
            // we really mean "no underline" rather than "none"
        else
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"underline" priority:@""];
            // we really mean "add underline" rather than "underline"
    }

    return style;
}

- (void)changeAttributes:(id)sender
{
    COMMAND_PROLOGUE

    [self _applyStyleToSelection:[self _styleForAttributeChange:sender] withUndoAction:EditActionChangeAttributes];
}

- (DOMCSSStyleDeclaration *)_styleFromColorPanelWithSelector:(SEL)selector
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    ASSERT([style respondsToSelector:selector]);
    [style performSelector:selector withObject:[self _colorAsString:[[NSColorPanel sharedColorPanel] color]]];
    
    return style;
}

- (EditAction)_undoActionFromColorPanelWithSelector:(SEL)selector
{
    if (selector == @selector(setBackgroundColor:))
        return EditActionSetBackgroundColor;    
    return EditActionSetColor;
}

- (void)_changeCSSColorUsingSelector:(SEL)selector inRange:(DOMRange *)range
{
    DOMCSSStyleDeclaration *style = [self _styleFromColorPanelWithSelector:selector];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:range])
        if (Frame* coreFrame = core([self _frame]))
            coreFrame->editor()->applyStyle(core(style), [self _undoActionFromColorPanelWithSelector:selector]);
}

- (void)changeDocumentBackgroundColor:(id)sender
{
    COMMAND_PROLOGUE

    // Mimicking NSTextView, this method sets the background color for the
    // entire document. There is no NSTextView API for setting the background
    // color on the selected range only. Note that this method is currently
    // never called from the UI (see comment in changeColor:).
    // FIXME: this actually has no effect when called, probably due to 3654850. _documentRange seems
    // to do the right thing because it works in startSpeaking:, and I know setBackgroundColor: does the
    // right thing because I tested it with [self _selectedRange].
    // FIXME: This won't actually apply the style to the entire range here, because it ends up calling
    // [frame _applyStyle:], which operates on the current selection. To make this work right, we'll
    // need to save off the selection, temporarily set it to the entire range, make the change, then
    // restore the old selection.
    [self _changeCSSColorUsingSelector:@selector(setBackgroundColor:) inRange:[self _documentRange]];
}

- (void)changeColor:(id)sender
{
    COMMAND_PROLOGUE

    // FIXME: in NSTextView, this method calls changeDocumentBackgroundColor: when a
    // private call has earlier been made by [NSFontFontEffectsBox changeColor:], see 3674493. 
    // AppKit will have to be revised to allow this to work with anything that isn't an 
    // NSTextView. However, this might not be required for Tiger, since the background-color 
    // changing box in the font panel doesn't work in Mail (3674481), though it does in TextEdit.
    [self _applyStyleToSelection:[self _styleFromColorPanelWithSelector:@selector(setColor:)] 
                  withUndoAction:EditActionSetColor];
}

- (void)_changeWordCaseWithSelector:(SEL)selector
{
    if (![self _canEdit])
        return;

    WebFrame *frame = [self _frame];
    [self selectWord:nil];
    NSString *word = [[frame _selectedString] performSelector:selector];
    // FIXME: Does this need a different action context other than "typed"?
    if ([self _shouldReplaceSelectionWithText:word givenAction:WebViewInsertActionTyped])
        [frame _replaceSelectionWithText:word selectReplacement:NO smartReplace:NO];
}

- (void)uppercaseWord:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeWordCaseWithSelector:@selector(uppercaseString)];
}

- (void)lowercaseWord:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeWordCaseWithSelector:@selector(lowercaseString)];
}

- (void)capitalizeWord:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeWordCaseWithSelector:@selector(capitalizedString)];
}

- (void)complete:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _canEdit])
        return;
    if (!_private->completionController)
        _private->completionController = [[WebTextCompletionController alloc] initWithWebView:[self _webView] HTMLView:self];
    [_private->completionController doCompletion];
}

- (void)checkSpelling:(id)sender
{
    COMMAND_PROLOGUE

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->advanceToNextMisspelling();
}

- (void)showGuessPanel:(id)sender
{
    COMMAND_PROLOGUE

    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    NSPanel *spellingPanel = [checker spellingPanel];
#ifndef BUILDING_ON_TIGER
    // Post-Tiger, this menu item is a show/hide toggle, to match AppKit. Leave Tiger behavior alone
    // to match rest of OS X.
    if ([spellingPanel isVisible]) {
        [spellingPanel orderOut:sender];
        return;
    }
#endif
    
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->advanceToNextMisspelling(true);
    [spellingPanel orderFront:sender];
}

- (void)_changeSpellingToWord:(NSString *)newWord
{
    if (![self _canEdit])
        return;

    // Don't correct to empty string.  (AppKit checked this, we might as well too.)
    if (![NSSpellChecker sharedSpellChecker]) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    if ([newWord isEqualToString:@""])
        return;

    if ([self _shouldReplaceSelectionWithText:newWord givenAction:WebViewInsertActionPasted])
        [[self _frame] _replaceSelectionWithText:newWord selectReplacement:YES smartReplace:NO];
}

- (void)changeSpelling:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeSpellingToWord:[[sender selectedCell] stringValue]];
}

- (void)performFindPanelAction:(id)sender
{
    COMMAND_PROLOGUE

    // Implementing this will probably require copying all of NSFindPanel.h and .m.
    // We need *almost* the same thing as AppKit, but not quite.
    LOG_ERROR("unimplemented");
}

- (void)startSpeaking:(id)sender
{
    COMMAND_PROLOGUE

    WebFrame *frame = [self _frame];
    DOMRange *range = [self _selectedRange];
    if (!range || [range collapsed])
        range = [self _documentRange];
    [NSApp speakString:[frame _stringForRange:range]];
}

- (void)stopSpeaking:(id)sender
{
    COMMAND_PROLOGUE

    [NSApp stopSpeaking:sender];
}

- (void)toggleBaseWritingDirection:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _canEdit])
        return;
    
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    WritingDirection direction = RightToLeftWritingDirection;
    switch (coreFrame->baseWritingDirectionForSelectionStart()) {
        case NSWritingDirectionLeftToRight:
            break;
        case NSWritingDirectionRightToLeft:
            direction = LeftToRightWritingDirection;
            break;
        // The writingDirectionForSelectionStart method will never return "natural". It
        // will always return a concrete direction. So, keep the compiler happy, and assert not reached.
        case NSWritingDirectionNatural:
            ASSERT_NOT_REACHED();
            break;
    }

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->setBaseWritingDirection(direction);
}

- (void)changeBaseWritingDirection:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _canEdit])
        return;
    
    NSWritingDirection writingDirection = static_cast<NSWritingDirection>([sender tag]);
    
    // We disable the menu item that performs this action because we can't implement
    // NSWritingDirectionNatural's behavior using CSS.
    ASSERT(writingDirection != NSWritingDirectionNatural);

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->setBaseWritingDirection(writingDirection == NSWritingDirectionLeftToRight ? LeftToRightWritingDirection : RightToLeftWritingDirection);
}

static BOOL writingDirectionKeyBindingsEnabled()
{
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    return YES;
#else
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return [defaults boolForKey:@"NSAllowsBaseWritingDirectionKeyBindings"] || [defaults boolForKey:@"AppleTextDirection"];
#endif
}

- (void)_changeBaseWritingDirectionTo:(NSWritingDirection)direction
{
    if (![self _canEdit])
        return;

    static BOOL bindingsEnabled = writingDirectionKeyBindingsEnabled();

    if (!bindingsEnabled) {
        NSBeep();
        return;
    }

    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->setBaseWritingDirection(direction == NSWritingDirectionLeftToRight ? LeftToRightWritingDirection : RightToLeftWritingDirection);
}

- (void)makeBaseWritingDirectionLeftToRight:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeBaseWritingDirectionTo:NSWritingDirectionLeftToRight];
}

- (void)makeBaseWritingDirectionRightToLeft:(id)sender
{
    COMMAND_PROLOGUE

    [self _changeBaseWritingDirectionTo:NSWritingDirectionRightToLeft];
}

#if defined(BUILDING_ON_TIGER) || defined(BUILDING_ON_LEOPARD)
- (void)changeBaseWritingDirectionToLTR:(id)sender
{
    [self makeBaseWritingDirectionLeftToRight:sender];
}

- (void)changeBaseWritingDirectionToRTL:(id)sender
{
    [self makeBaseWritingDirectionRightToLeft:sender];
}
#endif

- (void)makeBaseWritingDirectionNatural:(id)sender
{
    LOG_ERROR("Sent from %@.", sender);
}

#if 0

// CSS does not have a way to specify an outline font, which may make this difficult to implement.
// Maybe a special case of text-shadow?
- (void)outline:(id)sender;

// This is part of table support, which may be in NSTextView for Tiger.
// It's probably simple to do the equivalent thing for WebKit.
- (void)insertTable:(id)sender;

// This could be important.
- (void)toggleTraditionalCharacterShape:(id)sender;

// I'm not sure what the equivalents of these in the web world are.
- (void)insertLineSeparator:(id)sender;
- (void)insertPageBreak:(id)sender;

// These methods are not implemented in NSTextView yet at the time of this writing.
- (void)changeCaseOfLetter:(id)sender;
- (void)transposeWords:(id)sender;

#endif

#ifndef BUILDING_ON_TIGER

// Override this so that AppKit will send us arrow keys as key down events so we can
// support them via the key bindings mechanism.
- (BOOL)_wantsKeyDownForEvent:(NSEvent *)event
{
    bool haveWebCoreFrame = core([self _frame]);

    // If we have a frame, our keyDown method will handle key bindings after sending
    // the event through the DOM, so ask AppKit not to do its early special key binding
    // mapping. If we don't have a frame, just let things work the normal way without
    // a keyDown.
    return haveWebCoreFrame;
}

#else

// Super-hack alert.
// All this code accomplishes the same thing as the _wantsKeyDownForEvent method above.

// Returns a selector only if called while:
//   1) first responder is self
//   2) handling a key down event
//   3) not yet inside keyDown: method
//   4) key is an arrow key
// The selector is the one that gets sent by -[NSWindow _processKeyboardUIKey] for this key.
- (SEL)_arrowKeyDownEventSelectorIfPreprocessing
{
    NSWindow *w = [self window];
    if ([w firstResponder] != self)
        return NULL;
    NSEvent *e = [w currentEvent];
    if ([e type] != NSKeyDown)
        return NULL;
    if (e == _private->keyDownEvent)
        return NULL;
    NSString *s = [e charactersIgnoringModifiers];
    if ([s length] == 0)
        return NULL;
    switch ([s characterAtIndex:0]) {
        case NSDownArrowFunctionKey:
            return @selector(moveDown:);
        case NSLeftArrowFunctionKey:
            return @selector(moveLeft:);
        case NSRightArrowFunctionKey:
            return @selector(moveRight:);
        case NSUpArrowFunctionKey:
            return @selector(moveUp:);
        default:
            return NULL;
    }
}

// Returns NO instead of YES if called on the selector that the
// _arrowKeyDownEventSelectorIfPreprocessing method returns.
// This should only happen inside -[NSWindow _processKeyboardUIKey],
// and together with the change below should cause that method
// to return NO rather than handling the key.
// Also set a 1-shot flag for the nextResponder check below.
- (BOOL)respondsToSelector:(SEL)selector
{
    if (![super respondsToSelector:selector])
        return NO;
    SEL arrowKeySelector = [self _arrowKeyDownEventSelectorIfPreprocessing];
    if (selector != arrowKeySelector)
        return YES;
    _private->nextResponderDisabledOnce = YES;
    return NO;
}

// Returns nil instead of the next responder if called when the
// one-shot flag is set, and _arrowKeyDownEventSelectorIfPreprocessing
// returns something other than NULL. This should only happen inside
// -[NSWindow _processKeyboardUIKey] and together with the change above
// should cause that method to return NO rather than handling the key.
- (NSResponder *)nextResponder
{
    BOOL disabled = _private->nextResponderDisabledOnce;
    _private->nextResponderDisabledOnce = NO;
    if (disabled && [self _arrowKeyDownEventSelectorIfPreprocessing] != NULL)
        return nil;
    return [super nextResponder];
}

#endif

- (void)_updateControlTints
{
    Frame* frame = core([self _frame]);
    if (!frame)
        return;
    FrameView* view = frame->view();
    if (!view)
        return;
    view->updateControlTints();
}

// Despite its name, this is called at different times than windowDidBecomeKey is.
// It takes into account all the other factors that determine when NSCell draws
// with different tints, so it's the right call to use for control tints. We'd prefer
// to do this with API. <rdar://problem/5136760>
- (void)_windowChangedKeyState
{
    if (pthread_main_np())
        [self _updateControlTints];
    else
        [self performSelectorOnMainThread:@selector(_updateControlTints) withObject:nil waitUntilDone:NO];

    [super _windowChangedKeyState];
}

- (void)otherMouseDown:(NSEvent *)event
{
    if ([event buttonNumber] == 2)
        [self mouseDown:event];
    else
        [super otherMouseDown:event];
}

- (void)otherMouseDragged:(NSEvent *)event
{
    if ([event buttonNumber] == 2)
        [self mouseDragged:event];
    else
        [super otherMouseDragged:event];
}

- (void)otherMouseUp:(NSEvent *)event
{
    if ([event buttonNumber] == 2)
        [self mouseUp:event];
    else
        [super otherMouseUp:event];
}

@end

@implementation NSArray (WebHTMLView)

- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    NSEnumerator *enumerator = [self objectEnumerator];
    WebNetscapePluginView *view;
    while ((view = [enumerator nextObject]) != nil)
        if ([view isKindOfClass:[WebBaseNetscapePluginView class]])
            [view performSelector:selector withObject:object];
#endif
}

@end

@implementation WebHTMLView (WebInternal)

- (void)_selectionChanged
{
    [self _updateSelectionForInputManager];
    [self _updateFontPanel];
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->setStartNewKillRingSequence(true);
}

- (void)_updateFontPanel
{
    // FIXME: NSTextView bails out if becoming or resigning first responder, for which it has ivar flags. Not
    // sure if we need to do something similar.
    
    if (![self _canEdit])
        return;
    
    NSWindow *window = [self window];
    // FIXME: is this first-responder check correct? What happens if a subframe is editable and is first responder?
    if ([NSApp keyWindow] != window || [window firstResponder] != self)
        return;
    
    bool multipleFonts = false;
    NSFont *font = nil;
    if (Frame* coreFrame = core([self _frame])) {
        if (const SimpleFontData* fd = coreFrame->editor()->fontForSelection(multipleFonts))
            font = fd->getNSFont();
    }
    

    // FIXME: for now, return a bogus font that distinguishes the empty selection from the non-empty
    // selection. We should be able to remove this once the rest of this code works properly.
    if (font == nil)
        font = [self _hasSelection] ? [NSFont menuFontOfSize:23] : [NSFont toolTipsFontOfSize:17];
    ASSERT(font != nil);

    [[NSFontManager sharedFontManager] setSelectedFont:font isMultiple:multipleFonts];

    // FIXME: we don't keep track of selected attributes, or set them on the font panel. This
    // appears to have no effect on the UI. E.g., underlined text in Mail or TextEdit is
    // not reflected in the font panel. Maybe someday this will change.
}

- (BOOL)_canSmartCopyOrDelete
{
    if (![[self _webView] smartInsertDeleteEnabled])
        return NO;
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->selectionGranularity() == WordGranularity;
}

- (NSEvent *)_mouseDownEvent
{
    return _private->mouseDownEvent;
}

- (id<WebHTMLHighlighter>)_highlighterForType:(NSString*)type
{
    return [_private->highlighters objectForKey:type];
}

- (WebFrame *)_frame
{
    return [_private->dataSource webFrame];
}

- (void)paste:(id)sender
{
    COMMAND_PROLOGUE

    RetainPtr<WebHTMLView> selfProtector = self;
    RefPtr<Frame> coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    if (coreFrame->editor()->tryDHTMLPaste())
        return; // DHTML did the whole operation
    if (!coreFrame->editor()->canPaste())
        return;
    if (coreFrame->selection()->isContentRichlyEditable())
        [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:YES];
    else
        coreFrame->editor()->pasteAsPlainText();
}

- (void)pasteAsPlainText:(id)sender
{
    COMMAND_PROLOGUE

    if (![self _canEdit])
        return;
    [self _pasteAsPlainTextWithPasteboard:[NSPasteboard generalPasteboard]];
}

- (void)closeIfNotCurrentView
{
    if ([[[self _frame] frameView] documentView] != self)
        [self close];
}

- (DOMDocumentFragment*)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard
{
    return [self _documentFragmentFromPasteboard:pasteboard inContext:nil allowPlainText:NO];
}

#ifndef BUILDING_ON_TIGER

- (BOOL)isGrammarCheckingEnabled
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    return [[self _webView] isGrammarCheckingEnabled];
}

- (void)setGrammarCheckingEnabled:(BOOL)flag
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    [[self _webView] setGrammarCheckingEnabled:flag];
}

- (void)toggleGrammarChecking:(id)sender
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    [[self _webView] toggleGrammarChecking:sender];
}


static CGPoint coreGraphicsScreenPointForAppKitScreenPoint(NSPoint point)
{
    NSArray *screens = [NSScreen screens];
    
    if ([screens count] == 0) {
        // You could theoretically get here if running with no monitor, in which case it doesn't matter
        // much where the "on-screen" point is.
        return CGPointMake(point.x, point.y);
    }
    
    // Flip the y coordinate from the top of the menu bar screen -- see 4636390
    return CGPointMake(point.x, NSMaxY([[screens objectAtIndex:0] frame]) - point.y);
}

#endif

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)

- (void)orderFrontSubstitutionsPanel:(id)sender
{
    COMMAND_PROLOGUE

    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    NSPanel *substitutionsPanel = [checker substitutionsPanel];
    if ([substitutionsPanel isVisible]) {
        [substitutionsPanel orderOut:sender];
        return;
    }
    [substitutionsPanel orderFront:sender];
}

// FIXME 4799134: WebView is the bottleneck for this logic, but we must implement these methods here because
// the AppKit code checks the first responder.

- (BOOL)smartInsertDeleteEnabled
{
    return [[self _webView] smartInsertDeleteEnabled];
}

- (void)setSmartInsertDeleteEnabled:(BOOL)flag
{
    [[self _webView] setSmartInsertDeleteEnabled:flag];
}

- (void)toggleSmartInsertDelete:(id)sender
{
    [[self _webView] toggleSmartInsertDelete:sender];
}

- (BOOL)isAutomaticQuoteSubstitutionEnabled
{
    return [[self _webView] isAutomaticQuoteSubstitutionEnabled];
}

- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticQuoteSubstitutionEnabled:flag];
}

- (void)toggleAutomaticQuoteSubstitution:(id)sender
{
    [[self _webView] toggleAutomaticQuoteSubstitution:sender];
}

- (BOOL)isAutomaticLinkDetectionEnabled
{
    return [[self _webView] isAutomaticLinkDetectionEnabled];
}

- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticLinkDetectionEnabled:flag];
}

- (void)toggleAutomaticLinkDetection:(id)sender
{
    [[self _webView] toggleAutomaticLinkDetection:sender];
}

- (BOOL)isAutomaticDashSubstitutionEnabled
{
    return [[self _webView] isAutomaticDashSubstitutionEnabled];
}

- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticDashSubstitutionEnabled:flag];
}

- (void)toggleAutomaticDashSubstitution:(id)sender
{
    [[self _webView] toggleAutomaticDashSubstitution:sender];
}

- (BOOL)isAutomaticTextReplacementEnabled
{
    return [[self _webView] isAutomaticTextReplacementEnabled];
}

- (void)setAutomaticTextReplacementEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticTextReplacementEnabled:flag];
}

- (void)toggleAutomaticTextReplacement:(id)sender
{
    [[self _webView] toggleAutomaticTextReplacement:sender];
}

- (BOOL)isAutomaticSpellingCorrectionEnabled
{
    return [[self _webView] isAutomaticSpellingCorrectionEnabled];
}

- (void)setAutomaticSpellingCorrectionEnabled:(BOOL)flag
{
    [[self _webView] setAutomaticSpellingCorrectionEnabled:flag];
}

- (void)toggleAutomaticSpellingCorrection:(id)sender
{
    [[self _webView] toggleAutomaticSpellingCorrection:sender];
}

#endif

- (void)_lookUpInDictionaryFromMenu:(id)sender
{
    // Dictionary API will accept a whitespace-only string and display UI as if it were real text,
    // so bail out early to avoid that.
    if ([[[self selectedString] _webkit_stringByTrimmingWhitespace] length] == 0)
        return;

    NSAttributedString *attrString = [self selectedAttributedString];

    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    NSRect rect = coreFrame->selectionBounds();

#ifndef BUILDING_ON_TIGER
    NSDictionary *attributes = [attrString fontAttributesInRange:NSMakeRange(0,1)];
    NSFont *font = [attributes objectForKey:NSFontAttributeName];
    if (font)
        rect.origin.y += [font ascender];
#endif

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    [self showDefinitionForAttributedString:attrString atPoint:rect.origin];
    return;
#endif

    // We soft link to get the function that displays the dictionary (either pop-up window or app) to avoid the performance
    // penalty of linking to another framework. This function changed signature as well as framework between Tiger and Leopard,
    // so the two cases are handled separately.

#ifdef BUILDING_ON_TIGER
    typedef OSStatus (*ServiceWindowShowFunction)(id inWordString, NSRect inWordBoundary, UInt16 inLineDirection);
    const char *frameworkPath = "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/LangAnalysis.framework/LangAnalysis";
    const char *functionName = "DCMDictionaryServiceWindowShow";
#else
    typedef void (*ServiceWindowShowFunction)(id unusedDictionaryRef, id inWordString, CFRange selectionRange, id unusedFont, CGPoint textOrigin, Boolean verticalText, id unusedTransform);
    const char *frameworkPath = "/System/Library/Frameworks/Carbon.framework/Frameworks/HIToolbox.framework/HIToolbox";
    const char *functionName = "HIDictionaryWindowShow";
#endif

    static bool lookedForFunction = false;
    static ServiceWindowShowFunction dictionaryServiceWindowShow = NULL;

    if (!lookedForFunction) {
        void* langAnalysisFramework = dlopen(frameworkPath, RTLD_LAZY);
        ASSERT(langAnalysisFramework);
        if (langAnalysisFramework)
            dictionaryServiceWindowShow = (ServiceWindowShowFunction)dlsym(langAnalysisFramework, functionName);
        lookedForFunction = true;
    }

    ASSERT(dictionaryServiceWindowShow);
    if (!dictionaryServiceWindowShow) {
        NSLog(@"Couldn't find the %s function in %s", functionName, frameworkPath); 
        return;
    }

#ifdef BUILDING_ON_TIGER
    // FIXME: must check for right-to-left here
    NSWritingDirection writingDirection = NSWritingDirectionLeftToRight;

    // FIXME: the dictionary API expects the rect for the first line of selection. Passing
    // the rect for the entire selection, as we do here, positions the pop-up window near
    // the bottom of the selection rather than at the selected word.
    rect = [self convertRect:rect toView:nil];
    rect.origin = [[self window] convertBaseToScreen:rect.origin];
    NSData *data = [attrString RTFFromRange:NSMakeRange(0, [attrString length]) documentAttributes:nil];
    dictionaryServiceWindowShow(data, rect, (writingDirection == NSWritingDirectionRightToLeft) ? 1 : 0);
#else
    // The HIDictionaryWindowShow function requires the origin, in CG screen coordinates, of the first character of text in the selection.
    // FIXME 4945808: We approximate this in a way that works well when a single word is selected, and less well in some other cases
    // (but no worse than we did in Tiger)
    NSPoint windowPoint = [self convertPoint:rect.origin toView:nil];
    NSPoint screenPoint = [[self window] convertBaseToScreen:windowPoint];

    dictionaryServiceWindowShow(nil, attrString, CFRangeMake(0, [attrString length]), nil, 
                                coreGraphicsScreenPointForAppKitScreenPoint(screenPoint), false, nil);
#endif
}

- (void)_hoverFeedbackSuspendedChanged
{
    [self _updateMouseoverWithFakeEvent];
}

- (BOOL)_interceptEditingKeyEvent:(KeyboardEvent*)event shouldSaveCommand:(BOOL)shouldSave
{
    // Ask AppKit to process the key event -- it will call back with either insertText or doCommandBySelector.
    WebHTMLViewInterpretKeyEventsParameters parameters;
    parameters.eventWasHandled = false;
    parameters.shouldSaveCommand = shouldSave;
    // If we're intercepting the initial IM call we assume that the IM has consumed the event, 
    // and only change this assumption if one of the NSTextInput/Responder callbacks is used.
    // We assume the IM will *not* consume hotkey sequences
    parameters.consumedByIM = !event->metaKey() && shouldSave;

    if (const PlatformKeyboardEvent* platformEvent = event->keyEvent()) {
        NSEvent *macEvent = platformEvent->macEvent();
        if ([macEvent type] == NSKeyDown && [_private->completionController filterKeyDown:macEvent])
            return true;
        
        if ([macEvent type] == NSFlagsChanged)
            return false;
        
        parameters.event = event;
        _private->interpretKeyEventsParameters = &parameters;
        _private->receivedNOOP = NO;
        const Vector<KeypressCommand>& commands = event->keypressCommands();
        bool hasKeypressCommand = !commands.isEmpty();

        // FIXME: interpretKeyEvents doesn't match application key equivalents (such as Cmd+A),
        // and sends noop: for those. As a result, we don't handle those from within WebCore,
        // but send a full sequence of DOM events, including an unneeded keypress.
        if (parameters.shouldSaveCommand || !hasKeypressCommand)
            [self interpretKeyEvents:[NSArray arrayWithObject:macEvent]];
        else {
            size_t size = commands.size();
            // Are there commands that would just cause text insertion if executed via Editor?
            // WebKit doesn't have enough information about mode to decide how they should be treated, so we leave it upon WebCore
            // to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
            // (e.g. Tab that inserts a Tab character, or Enter).
            bool haveTextInsertionCommands = false;
            for (size_t i = 0; i < size; ++i) {
                if ([self coreCommandBySelector:NSSelectorFromString(commands[i].commandName)].isTextInsertion())
                    haveTextInsertionCommands = true;
            }
            if (!haveTextInsertionCommands || platformEvent->type() == PlatformKeyboardEvent::Char) {
                for (size_t i = 0; i < size; ++i) {
                    if (commands[i].commandName == "insertText:")
                        [self insertText:commands[i].text];
                    else
                        [self doCommandBySelector:NSSelectorFromString(commands[i].commandName)];
                }
            }
        }
        _private->interpretKeyEventsParameters = 0;
    }
    return (!_private->receivedNOOP && parameters.eventWasHandled) || parameters.consumedByIM;
}

- (WebCore::CachedImage*)promisedDragTIFFDataSource
{
    return _private->promisedDragTIFFDataSource;
}

- (void)setPromisedDragTIFFDataSource:(WebCore::CachedImage*)source
{
    if (source)
        source->addClient(promisedDataClient());
    
    if (_private->promisedDragTIFFDataSource)
        _private->promisedDragTIFFDataSource->removeClient(promisedDataClient());
    _private->promisedDragTIFFDataSource = source;
}

#undef COMMAND_PROLOGUE

- (void)_layoutIfNeeded
{
    ASSERT(!_private->subviewsSetAside);

    if (_private->needsToApplyStyles || [self _needsLayout])
        [self layout];
}

- (void)_web_layoutIfNeededRecursive
{
    [self _layoutIfNeeded];

#ifndef NDEBUG
    _private->enumeratingSubviews = YES;
#endif

    NSMutableArray *descendantWebHTMLViews = [[NSMutableArray alloc] init];

    [self _web_addDescendantWebHTMLViewsToArray:descendantWebHTMLViews];

    unsigned count = [descendantWebHTMLViews count];
    for (unsigned i = 0; i < count; ++i)
        [[descendantWebHTMLViews objectAtIndex:i] _layoutIfNeeded];

    [descendantWebHTMLViews release];

#ifndef NDEBUG
    _private->enumeratingSubviews = NO;
#endif
}

- (void) _destroyAllWebPlugins
{
    [[self _pluginController] destroyAllPlugins];
}

- (BOOL)_needsLayout
{
    return [[self _frame] _needsLayout];
}

#if USE(ACCELERATED_COMPOSITING)
- (void)attachRootLayer:(CALayer*)layer
{
    if (!_private->layerHostingView) {
        NSView* hostingView = [[NSView alloc] initWithFrame:[self bounds]];
#if !defined(BUILDING_ON_LEOPARD)
        [hostingView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
#endif
        [self addSubview:hostingView];
        [hostingView release];
        // hostingView is owned by being a subview of self
        _private->layerHostingView = hostingView;
        [[self _webView] _startedAcceleratedCompositingForFrame:[self _frame]];
    }

    // Make a container layer, which will get sized/positioned by AppKit and CA.
    CALayer* viewLayer = [CALayer layer];

#if defined(BUILDING_ON_LEOPARD)
    // Turn off default animations.
    NSNull *nullValue = [NSNull null];
    NSDictionary *actions = [NSDictionary dictionaryWithObjectsAndKeys:
                             nullValue, @"anchorPoint",
                             nullValue, @"bounds",
                             nullValue, @"contents",
                             nullValue, @"contentsRect",
                             nullValue, @"opacity",
                             nullValue, @"position",
                             nullValue, @"sublayerTransform",
                             nullValue, @"sublayers",
                             nullValue, @"transform",
                             nil];
    [viewLayer setStyle:[NSDictionary dictionaryWithObject:actions forKey:@"actions"]];
#endif

    [_private->layerHostingView setLayer:viewLayer];
    [_private->layerHostingView setWantsLayer:YES];
    
    // Parent our root layer in the container layer
    [viewLayer addSublayer:layer];
    
#if defined(BUILDING_ON_LEOPARD)
    [self _updateLayerHostingViewPosition];
#endif
}

- (void)detachRootLayer
{
    if (_private->layerHostingView) {
        [_private->layerHostingView setLayer:nil];
        [_private->layerHostingView setWantsLayer:NO];
        [_private->layerHostingView removeFromSuperview];
        _private->layerHostingView = nil;
        [[self _webView] _stoppedAcceleratedCompositingForFrame:[self _frame]];
    }
}

#if defined(BUILDING_ON_LEOPARD)
// This method is necessary on Leopard to work around <rdar://problem/7067892>.
- (void)_updateLayerHostingViewPosition
{
    if (!_private->layerHostingView)
        return;
    
    const CGFloat maxHeight = 2048;
    NSRect layerViewFrame = [self bounds];

    if (layerViewFrame.size.height > maxHeight) {
        CGFloat documentHeight = layerViewFrame.size.height;
            
        // Clamp the size of the view to <= maxHeight to avoid the bug.
        layerViewFrame.size.height = maxHeight;
        NSRect visibleRect = [[self enclosingScrollView] documentVisibleRect];
        
        // Place the top of the layer-hosting view at the top of the visibleRect.
        CGFloat topOffset = NSMinY(visibleRect);
        layerViewFrame.origin.y = topOffset;

        // Compensate for the moved view by adjusting the sublayer transform on the view's layer (using flipped coords).
        CGFloat bottomOffset = documentHeight - layerViewFrame.size.height - topOffset;
        [[_private->layerHostingView layer] setSublayerTransform:CATransform3DMakeTranslation(0, -bottomOffset, 0)];
    }

    [_private->layerHostingView _updateLayerGeometryFromView];  // Workaround for <rdar://problem/7071636>
    [_private->layerHostingView setFrame:layerViewFrame];
}
#endif // defined(BUILDING_ON_LEOPARD)
#endif // USE(ACCELERATED_COMPOSITING)

@end

@implementation WebHTMLView (WebNSTextInputSupport)

- (NSArray *)validAttributesForMarkedText
{
    static NSArray *validAttributes;
    if (!validAttributes) {
        validAttributes = [[NSArray alloc] initWithObjects:
            NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName,
            NSMarkedClauseSegmentAttributeName, NSTextInputReplacementRangeAttributeName, nil];
        // NSText also supports the following attributes, but it's
        // hard to tell which are really required for text input to
        // work well; I have not seen any input method make use of them yet.
        //     NSFontAttributeName, NSForegroundColorAttributeName,
        //     NSBackgroundColorAttributeName, NSLanguageAttributeName.
        CFRetain(validAttributes);
    }
    LOG(TextInput, "validAttributesForMarkedText -> (...)");
    return validAttributes;
}

- (NSTextInputContext *)inputContext
{
    return _private->exposeInputContext ? [super inputContext] : nil;
}

- (NSAttributedString *)textStorage
{
    if (!_private->exposeInputContext) {
        LOG(TextInput, "textStorage -> nil");
        return nil;
    }
    NSAttributedString *result = [self attributedSubstringFromRange:NSMakeRange(0, UINT_MAX)];
    
    LOG(TextInput, "textStorage -> \"%@\"", result ? [result string] : @"");
    
    // We have to return an empty string rather than null to prevent TSM from calling -string
    return result ? result : [[[NSAttributedString alloc] initWithString:@""] autorelease];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
    NSWindow *window = [self window];
    WebFrame *frame = [self _frame];

    if (window)
        thePoint = [window convertScreenToBase:thePoint];
    thePoint = [self convertPoint:thePoint fromView:nil];

    DOMRange *range = [frame _characterRangeAtPoint:thePoint];
    if (!range) {
        LOG(TextInput, "characterIndexForPoint:(%f, %f) -> NSNotFound", thePoint.x, thePoint.y);
        return NSNotFound;
    }
    
    unsigned result = [frame _convertDOMRangeToNSRange:range].location;
    LOG(TextInput, "characterIndexForPoint:(%f, %f) -> %u", thePoint.x, thePoint.y, result);
    return result;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
{    
    WebFrame *frame = [self _frame];
    
    // Just to match NSTextView's behavior. Regression tests cannot detect this;
    // to reproduce, use a test application from http://bugs.webkit.org/show_bug.cgi?id=4682
    // (type something; try ranges (1, -1) and (2, -1).
    if ((theRange.location + theRange.length < theRange.location) && (theRange.location + theRange.length != 0))
        theRange.length = 0;
    
    DOMRange *range = [frame _convertNSRangeToDOMRange:theRange];
    if (!range) {
        LOG(TextInput, "firstRectForCharacterRange:(%u, %u) -> (0, 0, 0, 0)", theRange.location, theRange.length);
        return NSMakeRect(0, 0, 0, 0);
    }
    
    ASSERT([range startContainer]);
    ASSERT([range endContainer]);
    
    NSRect resultRect = [frame _firstRectForDOMRange:range];
    resultRect = [self convertRect:resultRect toView:nil];

    NSWindow *window = [self window];
    if (window)
        resultRect.origin = [window convertBaseToScreen:resultRect.origin];
    
    LOG(TextInput, "firstRectForCharacterRange:(%u, %u) -> (%f, %f, %f, %f)", theRange.location, theRange.length, resultRect.origin.x, resultRect.origin.y, resultRect.size.width, resultRect.size.height);
    return resultRect;
}

- (NSRange)selectedRange
{
    if (!isTextInput(core([self _frame]))) {
        LOG(TextInput, "selectedRange -> (NSNotFound, 0)");
        return NSMakeRange(NSNotFound, 0);
    }
    NSRange result = [[self _frame] _selectedNSRange];

    LOG(TextInput, "selectedRange -> (%u, %u)", result.location, result.length);
    return result;
}

- (NSRange)markedRange
{
    WebFrame *webFrame = [self _frame];
    Frame* coreFrame = core(webFrame);
    if (!coreFrame)
        return NSMakeRange(0, 0);
    NSRange result = [webFrame _convertToNSRange:coreFrame->editor()->compositionRange().get()];

    LOG(TextInput, "markedRange -> (%u, %u)", result.location, result.length);
    return result;
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)nsRange
{
    WebFrame *frame = [self _frame];
    Frame* coreFrame = core(frame);
    if (!isTextInput(coreFrame) || isInPasswordField(coreFrame)) {
        LOG(TextInput, "attributedSubstringFromRange:(%u, %u) -> nil", nsRange.location, nsRange.length);
        return nil;
    }
    DOMRange *domRange = [frame _convertNSRangeToDOMRange:nsRange];
    if (!domRange) {
        LOG(TextInput, "attributedSubstringFromRange:(%u, %u) -> nil", nsRange.location, nsRange.length);
        return nil;
    }

    NSAttributedString *result = [NSAttributedString _web_attributedStringFromRange:core(domRange)];
    
    // [NSAttributedString(WebKitExtras) _web_attributedStringFromRange:]  insists on inserting a trailing 
    // whitespace at the end of the string which breaks the ATOK input method.  <rdar://problem/5400551>
    // To work around this we truncate the resultant string to the correct length.
    if ([result length] > nsRange.length) {
        ASSERT([result length] == nsRange.length + 1);
        ASSERT([[result string] characterAtIndex:nsRange.length] == '\n' || [[result string] characterAtIndex:nsRange.length] == ' ');
        result = [result attributedSubstringFromRange:NSMakeRange(0, nsRange.length)];
    }
    LOG(TextInput, "attributedSubstringFromRange:(%u, %u) -> \"%@\"", nsRange.location, nsRange.length, [result string]);
    return result;
}

// test for 10.4 because of <rdar://problem/4243463>
#ifdef BUILDING_ON_TIGER
- (long)conversationIdentifier
{
    return (long)self;
}
#else
- (NSInteger)conversationIdentifier
{
    return (NSInteger)self;
}
#endif

- (BOOL)hasMarkedText
{
    Frame* coreFrame = core([self _frame]);
    BOOL result = coreFrame && coreFrame->editor()->hasComposition();
    LOG(TextInput, "hasMarkedText -> %u", result);
    return result;
}

- (void)unmarkText
{
    LOG(TextInput, "unmarkText");

    // Use pointer to get parameters passed to us by the caller of interpretKeyEvents.
    WebHTMLViewInterpretKeyEventsParameters* parameters = _private->interpretKeyEventsParameters;
    _private->interpretKeyEventsParameters = 0;

    if (parameters) {
        parameters->eventWasHandled = YES;
        parameters->consumedByIM = NO;
    }
    
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->confirmComposition();
}

static void extractUnderlines(NSAttributedString *string, Vector<CompositionUnderline>& result)
{
    int length = [[string string] length];

    int i = 0;
    while (i < length) {
        NSRange range;
        NSDictionary *attrs = [string attributesAtIndex:i longestEffectiveRange:&range inRange:NSMakeRange(i, length - i)];

        if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
            Color color = Color::black;
            if (NSColor *colorAttr = [attrs objectForKey:NSUnderlineColorAttributeName])
                color = colorFromNSColor([colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
            result.append(CompositionUnderline(range.location, NSMaxRange(range), color, [style intValue] > 1));
        }

        i = range.location + range.length;
    }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString

    LOG(TextInput, "setMarkedText:\"%@\" selectedRange:(%u, %u)", isAttributedString ? [string string] : string, newSelRange.location, newSelRange.length);

    // Use pointer to get parameters passed to us by the caller of interpretKeyEvents.
    WebHTMLViewInterpretKeyEventsParameters* parameters = _private->interpretKeyEventsParameters;
    _private->interpretKeyEventsParameters = 0;

    if (parameters) {
        parameters->eventWasHandled = YES;
        parameters->consumedByIM = NO;
    }
    
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    if (![self _isEditable])
        return;

    Vector<CompositionUnderline> underlines;
    NSString *text = string;

    if (isAttributedString) {
        unsigned markedTextLength = [(NSString *)string length];
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:NULL inRange:NSMakeRange(0, markedTextLength)];
        LOG(TextInput, "    ReplacementRange: %@", rangeString);
        // The AppKit adds a 'secret' property to the string that contains the replacement range.
        // The replacement range is the range of the the text that should be replaced with the new string.
        if (rangeString)
            [[self _frame] _selectNSRange:NSRangeFromString(rangeString)];

        text = [string string];
        extractUnderlines(string, underlines);
    }

    coreFrame->editor()->setComposition(text, underlines, newSelRange.location, NSMaxRange(newSelRange));
}

- (void)doCommandBySelector:(SEL)selector
{
    LOG(TextInput, "doCommandBySelector:\"%s\"", sel_getName(selector));

    // Use pointer to get parameters passed to us by the caller of interpretKeyEvents.
    // The same call to interpretKeyEvents can do more than one command.
    WebHTMLViewInterpretKeyEventsParameters* parameters = _private->interpretKeyEventsParameters;
    if (parameters)
        parameters->consumedByIM = NO;

    if (selector == @selector(noop:)) {
        _private->receivedNOOP = YES;
        return;
    }

    KeyboardEvent* event = parameters ? parameters->event : 0;
    bool shouldSaveCommand = parameters && parameters->shouldSaveCommand;

    if (event && shouldSaveCommand)
        event->keypressCommands().append(KeypressCommand(NSStringFromSelector(selector)));
    else {
        // Make sure that only direct calls to doCommandBySelector: see the parameters by setting to 0.
        _private->interpretKeyEventsParameters = 0;

        bool eventWasHandled;

        WebView *webView = [self _webView];
        if ([[webView _editingDelegateForwarder] webView:webView doCommandBySelector:selector])
            eventWasHandled = true;
        else {
            Editor::Command command = [self coreCommandBySelector:selector];
            if (command.isSupported())
                eventWasHandled = command.execute(event);
            else {
                // If WebKit does not support this command, we need to pass the selector to super.
                _private->selectorForDoCommandBySelector = selector;

                // The sink does two things: 1) Tells us if the responder went unhandled, and
                // 2) prevents any NSBeep; we don't ever want to beep here.
                WebResponderChainSink *sink = [[WebResponderChainSink alloc] initWithResponderChain:self];
                [super doCommandBySelector:selector];
                eventWasHandled = ![sink receivedUnhandledCommand];
                [sink detach];
                [sink release];

                _private->selectorForDoCommandBySelector = 0;
            }
        }

        if (parameters)
            parameters->eventWasHandled = eventWasHandled;

        // Restore the parameters so that other calls to doCommandBySelector: see them,
        // and other commands can participate in setting the "eventWasHandled" flag.
        _private->interpretKeyEventsParameters = parameters;
    }
}

- (void)insertText:(id)string
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString

    LOG(TextInput, "insertText:\"%@\"", isAttributedString ? [string string] : string);

    WebHTMLViewInterpretKeyEventsParameters* parameters = _private->interpretKeyEventsParameters;
    _private->interpretKeyEventsParameters = 0;
    if (parameters)
        parameters->consumedByIM = NO;

    // We don't support inserting an attributed string but input methods don't appear to require this.
    RefPtr<Frame> coreFrame = core([self _frame]);
    NSString *text;
    bool isFromInputMethod = coreFrame && coreFrame->editor()->hasComposition();
    if (isAttributedString) {
        text = [string string];
        // We deal with the NSTextInputReplacementRangeAttributeName attribute from NSAttributedString here
        // simply because it is used by at least one Input Method -- it corresonds to the kEventParamTextInputSendReplaceRange
        // event in TSM.  This behaviour matches that of -[WebHTMLView setMarkedText:selectedRange:] when it receives an
        // NSAttributedString
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:NULL inRange:NSMakeRange(0, [text length])];
        LOG(TextInput, "    ReplacementRange: %@", rangeString);
        if (rangeString) {
            [[self _frame] _selectNSRange:NSRangeFromString(rangeString)];
            isFromInputMethod = YES;
        }
    } else
        text = string;

    bool eventHandled = false;
    if ([text length]) {
        KeyboardEvent* event = parameters ? parameters->event : 0;

        // insertText can be called from an input method or from normal key event processing
        // If its from normal key event processing, we may need to save the action to perform it later.
        // If its from an input method, then we should go ahead and insert the text now.  
        // We assume it's from the input method if we have marked text.
        // FIXME: In theory, this could be wrong for some input methods, so we should try to find
        // another way to determine if the call is from the input method
        bool shouldSaveCommand = parameters && parameters->shouldSaveCommand;
        if (event && shouldSaveCommand && !isFromInputMethod) {
            event->keypressCommands().append(KeypressCommand("insertText:", text));
            _private->interpretKeyEventsParameters = parameters;
            return;
        }
        
        String eventText = text;
        eventText.replace(NSBackTabCharacter, NSTabCharacter); // same thing is done in KeyEventMac.mm in WebCore
        if (coreFrame && coreFrame->editor()->canEdit()) {
            if (!coreFrame->editor()->hasComposition())
                eventHandled = coreFrame->editor()->insertText(eventText, event);
            else {
                eventHandled = true;
                coreFrame->editor()->confirmComposition(eventText);
            }
        }
    }
    
    if (!parameters)
        return;
    
    if (isFromInputMethod) {
        // Allow doCommandBySelector: to be called after insertText: by resetting interpretKeyEventsParameters
        _private->interpretKeyEventsParameters = parameters;
        parameters->consumedByIM = YES;
        return;
    }
    
    parameters->eventWasHandled = eventHandled;
}

- (void)_updateSelectionForInputManager
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;

    BOOL exposeInputContext = isTextInput(coreFrame) && !isInPasswordField(coreFrame);
    if (exposeInputContext != _private->exposeInputContext) {
        _private->exposeInputContext = exposeInputContext;
        // Let AppKit cache a potentially changed input context.
        // WebCore routinely sets the selection to None when editing, and IMs become unhappy when an input context suddenly turns nil, see bug 26009.
        if (!coreFrame->selection()->isNone())
            [NSApp updateWindows];
    }

    if (!coreFrame->editor()->hasComposition())
        return;

    if (coreFrame->editor()->ignoreCompositionSelectionChange())
        return;

    unsigned start;
    unsigned end;
    if (coreFrame->editor()->getCompositionSelection(start, end))
        [[NSInputManager currentInputManager] markedTextSelectionChanged:NSMakeRange(start, end - start) client:self];
    else {
        coreFrame->editor()->confirmCompositionWithoutDisturbingSelection();
        [[NSInputManager currentInputManager] markedTextAbandoned:self];
    }
}

@end

@implementation WebHTMLView (WebDocumentPrivateProtocols)

- (NSRect)selectionRect
{
    if ([self _hasSelection])
        return core([self _frame])->selectionBounds();
    return NSZeroRect;
}

- (NSArray *)selectionTextRects
{
    if (![self _hasSelection])
        return nil;
    
    Vector<FloatRect> list;
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->selectionTextRects(list, Frame::RespectTransforms);

    unsigned size = list.size();
    NSMutableArray *result = [[[NSMutableArray alloc] initWithCapacity:size] autorelease];
    for (unsigned i = 0; i < size; ++i)
        [result addObject:[NSValue valueWithRect:list[i]]];
    
    return result;
}

- (NSView *)selectionView
{
    return self;
}

- (NSImage *)selectionImageForcingBlackText:(BOOL)forceBlackText
{
    if ([self _hasSelection])
        return core([self _frame])->selectionImage(forceBlackText);
    return nil;
}

- (NSRect)selectionImageRect
{
    if ([self _hasSelection])
        return core([self _frame])->selectionBounds();
    return NSZeroRect;
}

- (NSArray *)pasteboardTypesForSelection
{
    if ([self _canSmartCopyOrDelete]) {
        NSMutableArray *types = [[[[self class] _selectionPasteboardTypes] mutableCopy] autorelease];
        [types addObject:WebSmartPastePboardType];
        return types;
    } else {
        return [[self class] _selectionPasteboardTypes];
    }
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:nil];
}

- (void)selectAll
{
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->selection()->selectAll();
}

- (void)deselectAll
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    coreFrame->selection()->clear();
}

- (NSString *)string
{
    return [[self _frame] _stringForRange:[self _documentRange]];
}

- (NSAttributedString *)_attributeStringFromDOMRange:(DOMRange *)range
{
    NSAttributedString *attributedString;
#if !LOG_DISABLED        
    double start = CFAbsoluteTimeGetCurrent();
#endif    
    attributedString = [[[NSAttributedString alloc] _initWithDOMRange:range] autorelease];
#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "creating attributed string from selection took %f seconds.", duration);
#endif
    return attributedString;
}

- (NSAttributedString *)attributedString
{
    DOMDocument *document = [[self _frame] DOMDocument];
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[document _documentRange]];
    if (!attributedString) {
        Document* coreDocument = core(document);
        attributedString = [NSAttributedString _web_attributedStringFromRange:Range::create(coreDocument, coreDocument, 0, coreDocument, coreDocument->childNodeCount()).get()];
    }
    return attributedString;
}

- (NSString *)selectedString
{
    return [[self _frame] _selectedString];
}

- (NSAttributedString *)selectedAttributedString
{
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[self _selectedRange]];
    if (!attributedString) {
        Frame* coreFrame = core([self _frame]);
        if (coreFrame) {
            RefPtr<Range> range = coreFrame->selection()->selection().toNormalizedRange();
            attributedString = [NSAttributedString _web_attributedStringFromRange:range.get()];
        }
    }
    return attributedString;
}

- (BOOL)supportsTextEncoding
{
    return YES;
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection
{
    if (![string length])
        return NO;
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->findString(string, forward, caseFlag, wrapFlag, startInSelection);
}

@end

@implementation WebHTMLView (WebDocumentInternalProtocols)

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    return [self elementAtPoint:point allowShadowContent:NO];
}

- (NSDictionary *)elementAtPoint:(NSPoint)point allowShadowContent:(BOOL)allow
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;
    return [[[WebElementDictionary alloc] initWithHitTestResult:coreFrame->eventHandler()->hitTestResultAtPoint(IntPoint(point), allow)] autorelease];
}

- (NSUInteger)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag limit:(NSUInteger)limit
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return 0;
    return coreFrame->markAllMatchesForText(string, caseFlag, limit);
}

- (void)setMarkedTextMatchesAreHighlighted:(BOOL)newValue
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    coreFrame->setMarkedTextMatchesAreHighlighted(newValue);
}

- (BOOL)markedTextMatchesAreHighlighted
{
    Frame* coreFrame = core([self _frame]);
    return coreFrame && coreFrame->markedTextMatchesAreHighlighted();
}

- (void)unmarkAllTextMatches
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    Document* document = coreFrame->document();
    if (!document)
        return;
    document->removeMarkers(DocumentMarker::TextMatch);
}

- (NSArray *)rectsForTextMatches
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return [NSArray array];
    Document* document = coreFrame->document();
    if (!document)
        return [NSArray array];

    Vector<IntRect> rects = document->renderedRectsForMarkers(DocumentMarker::TextMatch);
    unsigned count = rects.size();
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:count];
    for (unsigned index = 0; index < count; ++index)
        [result addObject:[NSValue valueWithRect:rects[index]]];    
    return result;
}

@end

// This is used by AppKit and is included here so that WebDataProtocolScheme is only defined once.
@implementation NSURL (WebDataURL)

+ (NSURL *)_web_uniqueWebDataURL
{
    CFUUIDRef UUIDRef = CFUUIDCreate(kCFAllocatorDefault);
    NSString *UUIDString = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef);
    CFRelease(UUIDRef);
    NSURL *URL = [NSURL URLWithString:[NSString stringWithFormat:@"%@://%@", WebDataProtocolScheme, UUIDString]];
    CFRelease(UUIDString);
    return URL;
}

@end

@implementation WebResponderChainSink

- (id)initWithResponderChain:(NSResponder *)chain
{
    self = [super init];
    _lastResponderInChain = chain;
    while (NSResponder *next = [_lastResponderInChain nextResponder])
        _lastResponderInChain = next;
    [_lastResponderInChain setNextResponder:self];
    return self;
}

- (void)detach
{
    [_lastResponderInChain setNextResponder:nil];
    _lastResponderInChain = nil;
}

- (BOOL)receivedUnhandledCommand
{
    return _receivedUnhandledCommand;
}

- (void)noResponderFor:(SEL)selector
{
    _receivedUnhandledCommand = YES;
}

- (void)doCommandBySelector:(SEL)selector
{
    _receivedUnhandledCommand = YES;
}

- (BOOL)tryToPerform:(SEL)action with:(id)object
{
    _receivedUnhandledCommand = YES;
    return YES;
}

@end
