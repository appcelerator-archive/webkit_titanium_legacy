/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DumpRenderTree.h"
#import "LayoutTestController.h"

#import "EditingDelegate.h"
#import "PolicyDelegate.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebKit/DOMDocument.h>
#import <WebKit/DOMElement.h>
#import <WebKit/WebApplicationCache.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebDatabaseManagerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryPrivate.h>
#import <WebKit/WebInspector.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebSecurityOriginPrivate.h>
#import <WebKit/WebTypesInternal.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface CommandValidationTarget : NSObject <NSValidatedUserInterfaceItem>
{
    SEL _action;
}
- (id)initWithAction:(SEL)action;
@end

@implementation CommandValidationTarget

- (id)initWithAction:(SEL)action
{
    self = [super init];
    if (!self)
        return nil;

    _action = action;
    return self;
}

- (SEL)action
{
    return _action;
}

- (NSInteger)tag
{
    return 0;
}

@end

LayoutTestController::~LayoutTestController()
{
}

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    RetainPtr<CFStringRef> urlCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, url));

    if (!disallowedURLs)
        disallowedURLs = CFSetCreateMutable(kCFAllocatorDefault, 0, NULL);

    // Canonicalize the URL
    NSURLRequest* request = [NSURLRequest requestWithURL:[NSURL URLWithString:(NSString *)urlCF.get()]];
    request = [NSURLProtocol canonicalRequestForRequest:request];

    CFSetAddValue(disallowedURLs, [request URL]);
}

void LayoutTestController::clearAllDatabases()
{
    [[WebDatabaseManager sharedWebDatabaseManager] deleteAllDatabases];
}

void LayoutTestController::clearBackForwardList()
{
    WebBackForwardList *backForwardList = [[mainFrame webView] backForwardList];
    WebHistoryItem *item = [[backForwardList currentItem] retain];

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    int capacity = [backForwardList capacity];
    [backForwardList setCapacity:0];
    [backForwardList setCapacity:capacity];
    [backForwardList addItem:item];
    [backForwardList goToItem:item];
    [item release];
}

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();
    return JSStringCreateWithCFString((CFStringRef)[nameNS _web_decodeHostName]);
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();
    return JSStringCreateWithCFString((CFStringRef)[nameNS _web_encodeHostName]);
}

void LayoutTestController::display()
{
    displayWebView();
}

void LayoutTestController::keepWebHistory()
{
    if (![WebHistory optionalSharedHistory]) {
        WebHistory *history = [[WebHistory alloc] init];
        [WebHistory setOptionalSharedHistory:history];
        [history release];
    }
}

size_t LayoutTestController::webHistoryItemCount()
{
    return [[[WebHistory optionalSharedHistory] allItems] count];
}

void LayoutTestController::notifyDone()
{
    if (m_waitToDump && !topLoadingFrame && !WorkQueue::shared()->count())
        dump();
    m_waitToDump = false;
}

JSStringRef LayoutTestController::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    return JSStringRetain(url); // Do nothing on mac.
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    RetainPtr<CFStringRef> urlCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, url));
    NSString *urlNS = (NSString *)urlCF.get();

    NSURL *nsurl = [NSURL URLWithString:urlNS relativeToURL:[[[mainFrame dataSource] response] URL]];
    NSString* nsurlString = [nsurl absoluteString];

    JSRetainPtr<JSStringRef> absoluteURL(Adopt, JSStringCreateWithUTF8CString([nsurlString UTF8String]));
    WorkQueue::shared()->queue(new LoadItem(absoluteURL.get(), target));
}

void LayoutTestController::setAcceptsEditing(bool newAcceptsEditing)
{
    [(EditingDelegate *)[[mainFrame webView] editingDelegate] setAcceptsEditing:newAcceptsEditing];
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long size)
{
    [WebApplicationCache setMaximumSize:size];
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
{
    [[[mainFrame webView] preferences] setAuthorAndUserStylesEnabled:flag];
}

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    if (setDelegate) {
        [policyDelegate setPermissive:permissive];
        [[mainFrame webView] setPolicyDelegate:policyDelegate];
    } else
        [[mainFrame webView] setPolicyDelegate:nil];
}

void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{    
    WebSecurityOrigin *origin = [[WebSecurityOrigin alloc] initWithURL:[NSURL URLWithString:@"file:///"]];
    [origin setQuota:quota];
    [origin release];
}

void LayoutTestController::setIconDatabaseEnabled(bool iconDatabaseEnabled)
{
    // FIXME: Workaround <rdar://problem/6480108>
    static WebIconDatabase* sharedWebIconDatabase = NULL;
    if (!sharedWebIconDatabase) {
        if (!iconDatabaseEnabled)
            return;
        sharedWebIconDatabase = [WebIconDatabase sharedIconDatabase];
        if ([sharedWebIconDatabase isEnabled] == iconDatabaseEnabled)
            return;
    }
    [sharedWebIconDatabase setEnabled:iconDatabaseEnabled];
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool profilingEnabled)
{
    [[[mainFrame webView] preferences] setDeveloperExtrasEnabled:profilingEnabled];
    [[[mainFrame webView] inspector] setJavaScriptProfilingEnabled:profilingEnabled];
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    NSView *documentView = [[mainFrame frameView] documentView];
    
    NSResponder *firstResponder = flag ? documentView : nil;
    [[[mainFrame webView] window] makeFirstResponder:firstResponder];
}

void LayoutTestController::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    [[[mainFrame webView] preferences] setPrivateBrowsingEnabled:privateBrowsingEnabled];
}

void LayoutTestController::setXSSAuditorEnabled(bool enabled)
{
    [[[mainFrame webView] preferences] setXSSAuditorEnabled:enabled];
}

void LayoutTestController::setPopupBlockingEnabled(bool popupBlockingEnabled)
{
    [[[mainFrame webView] preferences] setJavaScriptCanOpenWindowsAutomatically:!popupBlockingEnabled];
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool cycles)
{
    [[mainFrame webView] setTabKeyCyclesThroughElements:cycles];
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    [[mainFrame webView] _setDashboardBehavior:WebDashboardBehaviorUseBackwardCompatibilityMode to:flag];
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    [[WebPreferences standardPreferences] setUserStyleSheetEnabled:flag];
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    RetainPtr<CFStringRef> pathCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, path));
    NSURL *url = [NSURL URLWithString:(NSString *)pathCF.get()];
    [[WebPreferences standardPreferences] setUserStyleSheetLocation:url];
}

void LayoutTestController::dispatchPendingLoadRequests()
{
    [[mainFrame webView] _dispatchPendingLoadRequests];
}

void LayoutTestController::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    RetainPtr<CFStringRef> urlString(AdoptCF, JSStringCopyCFString(0, jsURL));
    ::setPersistentUserStyleSheetLocation(urlString.get());
}

void LayoutTestController::clearPersistentUserStyleSheet()
{
    ::setPersistentUserStyleSheetLocation(0);
}

void LayoutTestController::setWindowIsKey(bool windowIsKey)
{
    m_windowIsKey = windowIsKey;
    [[mainFrame webView] _updateActiveState];
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool flag)
{
    [[mainFrame webView] setSmartInsertDeleteEnabled:flag];
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool flag)
{
    [[mainFrame webView] setSelectTrailingWhitespaceEnabled:flag];
}

static const CFTimeInterval waitToDumpWatchdogInterval = 10.0;

static void waitUntilDoneWatchdogFired(CFRunLoopTimerRef timer, void* info)
{
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    fprintf(stderr, "%s", message);
    fprintf(stdout, "%s", message);
    dump();
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog) {
        waitToDumpWatchdog = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + waitToDumpWatchdogInterval, 0, 0, 0, waitUntilDoneWatchdogFired, NULL);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), waitToDumpWatchdog, kCFRunLoopCommonModes);
    }
}

int LayoutTestController::windowCount()
{
    return CFArrayGetCount(openWindowsRef);
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef id)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, id));
    NSString *idNS = (NSString *)idCF.get();
    
    DOMElement *element = [[mainFrame DOMDocument] getElementById:idNS];
    id rep = [[mainFrame dataSource] representation];
    
    if ([rep class] == [WebHTMLRepresentation class])
        return [(WebHTMLRepresentation *)rep elementDoesAutoComplete:element];

    return false;
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef value)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();

    RetainPtr<CFStringRef> valueCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, value));
    NSString *valueNS = (NSString *)valueCF.get();

    [[mainFrame webView] _executeCoreCommandByName:nameNS value:valueNS];
}

void LayoutTestController::setCacheModel(int cacheModel)
{
    [[WebPreferences standardPreferences] setCacheModel:cacheModel];
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = reinterpret_cast<const NSString *>(nameCF.get());

    // Accept command strings with capital letters for first letter without trailing colon.
    if (![nameNS hasSuffix:@":"] && [nameNS length]) {
        nameNS = [[[[nameNS substringToIndex:1] lowercaseString]
            stringByAppendingString:[nameNS substringFromIndex:1]]
            stringByAppendingString:@":"];
    }

    SEL selector = NSSelectorFromString(nameNS);
    RetainPtr<CommandValidationTarget> target(AdoptNS, [[CommandValidationTarget alloc] initWithAction:selector]);
    id validator = [NSApp targetForAction:selector to:[mainFrame webView] from:target.get()];
    if (!validator)
        return false;
    if (![validator respondsToSelector:selector])
        return false;
    if (![validator respondsToSelector:@selector(validateUserInterfaceItem:)])
        return true;
    return [validator validateUserInterfaceItem:target.get()];
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, elementId));
    NSString *idNS = (NSString *)idCF.get();
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, animationName));
    NSString *nameNS = (NSString *)nameCF.get();
    
    return [mainFrame _pauseAnimation:nameNS onNode:[[mainFrame DOMDocument] getElementById:idNS] atTime:time];
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, elementId));
    NSString *idNS = (NSString *)idCF.get();
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, propertyName));
    NSString *nameNS = (NSString *)nameCF.get();
    
    return [mainFrame _pauseTransitionOfProperty:nameNS onNode:[[mainFrame DOMDocument] getElementById:idNS] atTime:time];
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    return [mainFrame _numberOfActiveAnimations];
}

void LayoutTestController::waitForPolicyDelegate()
{
    setWaitToDump(true);
    [policyDelegate setControllerToNotifyDone:this];
    [[mainFrame webView] setPolicyDelegate:policyDelegate];
}
