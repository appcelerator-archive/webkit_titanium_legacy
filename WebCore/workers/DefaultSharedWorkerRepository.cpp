/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SHARED_WORKERS)

#include "DefaultSharedWorkerRepository.h"

#include "ActiveDOMObject.h"
#include "Document.h"
#include "GenericWorkerTask.h"
#include "MessagePort.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "SharedWorker.h"
#include "SharedWorkerContext.h"
#include "SharedWorkerRepository.h"
#include "SharedWorkerThread.h"
#include "WorkerLoaderProxy.h"
#include "WorkerReportingProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerScriptLoaderClient.h"
#include <wtf/HashSet.h>
#include <wtf/Threading.h>

namespace WebCore {

class SharedWorkerProxy : public ThreadSafeShared<SharedWorkerProxy>, public WorkerLoaderProxy, public WorkerReportingProxy {
public:
    static PassRefPtr<SharedWorkerProxy> create(const String& name, const KURL& url, PassRefPtr<SecurityOrigin> origin) { return adoptRef(new SharedWorkerProxy(name, url, origin)); }

    void setThread(PassRefPtr<SharedWorkerThread> thread) { m_thread = thread; }
    SharedWorkerThread* thread() { return m_thread.get(); }
    bool isClosing() const { return m_closing; }
    KURL url() const { return m_url.copy(); }
    String name() const { return m_name.copy(); }
    bool matches(const String& name, PassRefPtr<SecurityOrigin> origin) const { return name == m_name && origin->equal(m_origin.get()); }

    // WorkerLoaderProxy
    virtual void postTaskToLoader(PassRefPtr<ScriptExecutionContext::Task>);
    virtual void postTaskForModeToWorkerContext(PassRefPtr<ScriptExecutionContext::Task>, const String&);

    // WorkerReportingProxy
    virtual void postExceptionToWorkerObject(const String& errorMessage, int lineNumber, const String& sourceURL);
    virtual void postConsoleMessageToWorkerObject(MessageDestination, MessageSource, MessageType, MessageLevel, const String& message, int lineNumber, const String& sourceURL);
    virtual void workerContextClosed();
    virtual void workerContextDestroyed();

    // Updates the list of the worker's documents, per section 4.5 of the WebWorkers spec.
    void addToWorkerDocuments(ScriptExecutionContext*);

    bool isInWorkerDocuments(Document* document) { return m_workerDocuments.contains(document); }

    // Removes a detached document from the list of worker's documents. May set the closing flag if this is the last document in the list.
    void documentDetached(Document*);

private:
    SharedWorkerProxy(const String& name, const KURL&, PassRefPtr<SecurityOrigin>);
    void close();

    bool m_closing;
    String m_name;
    KURL m_url;
    // The thread is freed when the proxy is destroyed, so we need to make sure that the proxy stays around until the SharedWorkerContext exits.
    RefPtr<SharedWorkerThread> m_thread;
    RefPtr<SecurityOrigin> m_origin;
    HashSet<Document*> m_workerDocuments;
    // Ensures exclusive access to the worker documents. Must not grab any other locks (such as the DefaultSharedWorkerRepository lock) while holding this one.
    Mutex m_workerDocumentsLock;
};

SharedWorkerProxy::SharedWorkerProxy(const String& name, const KURL& url, PassRefPtr<SecurityOrigin> origin)
    : m_closing(false)
    , m_name(name.copy())
    , m_url(url.copy())
    , m_origin(origin)
{
    // We should be the sole owner of the SecurityOrigin, as we will free it on another thread.
    ASSERT(m_origin->hasOneRef());
}

void SharedWorkerProxy::postTaskToLoader(PassRefPtr<ScriptExecutionContext::Task> task)
{
    MutexLocker lock(m_workerDocumentsLock);

    if (isClosing())
        return;

    // If we aren't closing, then we must have at least one document.
    ASSERT(m_workerDocuments.size());

    // Just pick an arbitrary active document from the HashSet and pass load requests to it.
    // FIXME: Do we need to deal with the case where the user closes the document mid-load, via a shadow document or some other solution?
    Document* document = *(m_workerDocuments.begin());
    document->postTask(task);
}

void SharedWorkerProxy::postTaskForModeToWorkerContext(PassRefPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    if (isClosing())
        return;
    ASSERT(m_thread);
    m_thread->runLoop().postTaskForMode(task, mode);
}

static void postExceptionTask(ScriptExecutionContext* context, const String& errorMessage, int lineNumber, const String& sourceURL)
{
    context->reportException(errorMessage, lineNumber, sourceURL);
}

void SharedWorkerProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    MutexLocker lock(m_workerDocumentsLock);
    for (HashSet<Document*>::iterator iter = m_workerDocuments.begin(); iter != m_workerDocuments.end(); ++iter)
        (*iter)->postTask(createCallbackTask(&postExceptionTask, errorMessage, lineNumber, sourceURL));
}

static void postConsoleMessageTask(ScriptExecutionContext* document, MessageDestination destination, MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    document->addMessage(destination, source, type, level, message, lineNumber, sourceURL);
}

void SharedWorkerProxy::postConsoleMessageToWorkerObject(MessageDestination destination, MessageSource source, MessageType type, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    MutexLocker lock(m_workerDocumentsLock);
    for (HashSet<Document*>::iterator iter = m_workerDocuments.begin(); iter != m_workerDocuments.end(); ++iter)
        (*iter)->postTask(createCallbackTask(&postConsoleMessageTask, destination, source, type, level, message, lineNumber, sourceURL));
}

void SharedWorkerProxy::workerContextClosed()
{
    if (isClosing())
        return;
    close();
}

void SharedWorkerProxy::workerContextDestroyed()
{
    // The proxy may be freed by this call, so do not reference it any further.
    DefaultSharedWorkerRepository::instance().removeProxy(this);
}

void SharedWorkerProxy::addToWorkerDocuments(ScriptExecutionContext* context)
{
    // Nested workers are not yet supported, so passed-in context should always be a Document.
    ASSERT(context->isDocument());
    ASSERT(!isClosing());
    MutexLocker lock(m_workerDocumentsLock);
    Document* document = static_cast<Document*>(context);
    m_workerDocuments.add(document);
}

void SharedWorkerProxy::documentDetached(Document* document)
{
    if (isClosing())
        return;
    // Remove the document from our set (if it's there) and if that was the last document in the set, mark the proxy as closed.
    MutexLocker lock(m_workerDocumentsLock);
    m_workerDocuments.remove(document);
    if (!m_workerDocuments.size())
        close();
}

void SharedWorkerProxy::close()
{
    ASSERT(!isClosing());
    m_closing = true;
    // Stop the worker thread - the proxy will stay around until we get workerThreadExited() notification.
    if (m_thread)
        m_thread->stop();
}

class SharedWorkerConnectTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<SharedWorkerConnectTask> create(PassOwnPtr<MessagePortChannel> channel)
    {
        return adoptRef(new SharedWorkerConnectTask(channel));
    }

private:
    SharedWorkerConnectTask(PassOwnPtr<MessagePortChannel> channel)
        : m_channel(channel)
    {
    }

    virtual void performTask(ScriptExecutionContext* scriptContext)
    {
        RefPtr<MessagePort> port = MessagePort::create(*scriptContext);
        port->entangle(m_channel.release());
        ASSERT(scriptContext->isWorkerContext());
        WorkerContext* workerContext = static_cast<WorkerContext*>(scriptContext);
        ASSERT(workerContext->isSharedWorkerContext());
        workerContext->toSharedWorkerContext()->dispatchConnect(port);
    }

    OwnPtr<MessagePortChannel> m_channel;
};

// Loads the script on behalf of a worker.
class SharedWorkerScriptLoader : public RefCounted<SharedWorkerScriptLoader>, public ActiveDOMObject, private WorkerScriptLoaderClient {
public:
    SharedWorkerScriptLoader(PassRefPtr<SharedWorker>, PassOwnPtr<MessagePortChannel>, PassRefPtr<SharedWorkerProxy>);
    void load(const KURL&);

private:
    // WorkerScriptLoaderClient callback
    virtual void notifyFinished();

    RefPtr<SharedWorker> m_worker;
    OwnPtr<MessagePortChannel> m_port;
    RefPtr<SharedWorkerProxy> m_proxy;
    OwnPtr<WorkerScriptLoader> m_scriptLoader;
};

SharedWorkerScriptLoader::SharedWorkerScriptLoader(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, PassRefPtr<SharedWorkerProxy> proxy)
    : ActiveDOMObject(worker->scriptExecutionContext(), this)
    , m_worker(worker)
    , m_port(port)
    , m_proxy(proxy)
{
}

void SharedWorkerScriptLoader::load(const KURL& url)
{
    // Mark this object as active for the duration of the load.
    ASSERT(!hasPendingActivity());
    m_scriptLoader = new WorkerScriptLoader();
    m_scriptLoader->loadAsynchronously(scriptExecutionContext(), url, DenyCrossOriginRequests, this);

    // Stay alive until the load finishes.
    setPendingActivity(this);
}

void SharedWorkerScriptLoader::notifyFinished()
{
    // Hand off the just-loaded code to the repository to start up the worker thread.
    if (m_scriptLoader->failed())
        m_worker->dispatchLoadErrorEvent();
    else
        DefaultSharedWorkerRepository::instance().workerScriptLoaded(*m_proxy, scriptExecutionContext()->userAgent(m_scriptLoader->url()), m_scriptLoader->script(), m_port.release());

    // This frees this object - must be the last action in this function.
    unsetPendingActivity(this);
}

DefaultSharedWorkerRepository& DefaultSharedWorkerRepository::instance()
{
    AtomicallyInitializedStatic(DefaultSharedWorkerRepository*, instance = new DefaultSharedWorkerRepository);
    return *instance;
}

void DefaultSharedWorkerRepository::workerScriptLoaded(SharedWorkerProxy& proxy, const String& userAgent, const String& workerScript, PassOwnPtr<MessagePortChannel> port)
{
    MutexLocker lock(m_lock);
    if (proxy.isClosing())
        return;

    // Another loader may have already started up a thread for this proxy - if so, just send a connect to the pre-existing thread.
    if (!proxy.thread()) {
        RefPtr<SharedWorkerThread> thread = SharedWorkerThread::create(proxy.name(), proxy.url(), userAgent, workerScript, proxy, proxy);
        proxy.setThread(thread);
        thread->start();
    }
    proxy.thread()->runLoop().postTask(SharedWorkerConnectTask::create(port));
}

void SharedWorkerRepository::connect(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const KURL& url, const String& name, ExceptionCode& ec)
{
    DefaultSharedWorkerRepository::instance().connectToWorker(worker, port, url, name, ec);
}

void SharedWorkerRepository::documentDetached(Document* document)
{
    DefaultSharedWorkerRepository::instance().documentDetached(document);
}

bool SharedWorkerRepository::hasSharedWorkers(Document* document)
{
    return DefaultSharedWorkerRepository::instance().hasSharedWorkers(document);
}

bool DefaultSharedWorkerRepository::hasSharedWorkers(Document* document)
{
    MutexLocker lock(m_lock);
    for (unsigned i = 0; i < m_proxies.size(); i++) {
        if (m_proxies[i]->isInWorkerDocuments(document))
            return true;
    }
    return false;
}

void DefaultSharedWorkerRepository::removeProxy(SharedWorkerProxy* proxy)
{
    MutexLocker lock(m_lock);
    for (unsigned i = 0; i < m_proxies.size(); i++) {
        if (proxy == m_proxies[i].get()) {
            m_proxies.remove(i);
            return;
        }
    }
}

void DefaultSharedWorkerRepository::documentDetached(Document* document)
{
    MutexLocker lock(m_lock);
    for (unsigned i = 0; i < m_proxies.size(); i++)
        m_proxies[i]->documentDetached(document);
}

void DefaultSharedWorkerRepository::connectToWorker(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const KURL& url, const String& name, ExceptionCode& ec)
{
    MutexLocker lock(m_lock);
    ASSERT(worker->scriptExecutionContext()->securityOrigin()->canAccess(SecurityOrigin::create(url).get()));
    // Fetch a proxy corresponding to this SharedWorker.
    RefPtr<SharedWorkerProxy> proxy = getProxy(name, url);
    proxy->addToWorkerDocuments(worker->scriptExecutionContext());
    if (proxy->url() != url) {
        // Proxy already existed under alternate URL - return an error.
        ec = URL_MISMATCH_ERR;
        return;
    }
    // If proxy is already running, just connect to it - otherwise, kick off a loader to load the script.
    if (proxy->thread())
        proxy->thread()->runLoop().postTask(SharedWorkerConnectTask::create(port));
    else {
        RefPtr<SharedWorkerScriptLoader> loader = adoptRef(new SharedWorkerScriptLoader(worker, port.release(), proxy.release()));
        loader->load(url);
    }
}

// Creates a new SharedWorkerProxy or returns an existing one from the repository. Must only be called while the repository mutex is held.
PassRefPtr<SharedWorkerProxy> DefaultSharedWorkerRepository::getProxy(const String& name, const KURL& url)
{
    // Look for an existing worker, and create one if it doesn't exist.
    // Items in the cache are freed on another thread, so copy the URL before creating the origin, to make sure no references to external strings linger.
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url.copy());
    for (unsigned i = 0; i < m_proxies.size(); i++) {
        if (!m_proxies[i]->isClosing() && m_proxies[i]->matches(name, origin))
            return m_proxies[i];
    }
    // Proxy is not in the repository currently - create a new one.
    RefPtr<SharedWorkerProxy> proxy = SharedWorkerProxy::create(name, url, origin.release());
    m_proxies.append(proxy);
    return proxy.release();
}

DefaultSharedWorkerRepository::DefaultSharedWorkerRepository()
{
}

DefaultSharedWorkerRepository::~DefaultSharedWorkerRepository()
{
}

} // namespace WebCore

#endif // ENABLE(SHARED_WORKERS)
