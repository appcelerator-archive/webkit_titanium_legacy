/*
 *  Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitWebSourceGStreamer.h"

#include "CString.h"
#include "Document.h"
#include "GOwnPtr.h"
#include "GRefPtr.h"
#include "Noncopyable.h"
#include "NotImplemented.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/missing-plugins.h>

using namespace WebCore;

class StreamingClient : public Noncopyable, public ResourceHandleClient {
    public:
        StreamingClient(WebKitWebSrc*);
        virtual ~StreamingClient();

        virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse&);
        virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
        virtual void didReceiveData(ResourceHandle*, const char*, int, int);
        virtual void didFinishLoading(ResourceHandle*);
        virtual void didFail(ResourceHandle*, const ResourceError&);
        virtual void wasBlocked(ResourceHandle*);
        virtual void cannotShowURL(ResourceHandle*);

    private:
        WebKitWebSrc* m_src;
};

#define WEBKIT_WEB_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_SRC, WebKitWebSrcPrivate))
struct _WebKitWebSrcPrivate {
    GstAppSrc* appsrc;
    GstPad* srcpad;
    gchar* uri;

    RefPtr<WebCore::Frame> frame;

    StreamingClient* client;
    RefPtr<ResourceHandle> resourceHandle;

    guint64 offset;
    guint64 size;
    gboolean seekable;
    gboolean paused;

    guint64 requestedOffset;

    guint needDataID;
    guint enoughDataID;
    guint seekID;

    // icecast stuff
    gboolean iradioMode;
    gchar* iradioName;
    gchar* iradioGenre;
    gchar* iradioUrl;
    gchar* iradioTitle;
};

enum {
    PROP_IRADIO_MODE = 1,
    PROP_IRADIO_NAME,
    PROP_IRADIO_GENRE,
    PROP_IRADIO_URL,
    PROP_IRADIO_TITLE
};

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
                                                                  GST_PAD_SRC,
                                                                  GST_PAD_ALWAYS,
                                                                  GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(webkit_web_src_debug);
#define GST_CAT_DEFAULT webkit_web_src_debug

static void webKitWebSrcUriHandlerInit(gpointer gIface,
                                            gpointer ifaceData);

static void webKitWebSrcFinalize(GObject* object);
static void webKitWebSrcSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* pspec);
static void webKitWebSrcGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* pspec);
static GstStateChangeReturn webKitWebSrcChangeState(GstElement* element, GstStateChange transition);

static void webKitWebSrcNeedDataCb(GstAppSrc* appsrc, guint length, gpointer userData);
static void webKitWebSrcEnoughDataCb(GstAppSrc* appsrc, gpointer userData);
static gboolean webKitWebSrcSeekDataCb(GstAppSrc* appsrc, guint64 offset, gpointer userData);

static void webKitWebSrcStop(WebKitWebSrc* src, bool resetRequestedOffset);

static GstAppSrcCallbacks appsrcCallbacks = {
    webKitWebSrcNeedDataCb,
    webKitWebSrcEnoughDataCb,
    webKitWebSrcSeekDataCb,
    { 0 }
};

static void doInit(GType gtype)
{
    static const GInterfaceInfo uriHandlerInfo = {
        webKitWebSrcUriHandlerInit,
        0, 0
    };

    GST_DEBUG_CATEGORY_INIT(webkit_web_src_debug, "webkitwebsrc", 0, "websrc element");
    g_type_add_interface_static(gtype, GST_TYPE_URI_HANDLER,
                                &uriHandlerInfo);
}

GST_BOILERPLATE_FULL(WebKitWebSrc, webkit_web_src, GstBin, GST_TYPE_BIN, doInit);

static void webkit_web_src_base_init(gpointer klass)
{
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(eklass,
                                       gst_static_pad_template_get(&srcTemplate));
    gst_element_class_set_details_simple(eklass,
                                         (gchar*) "WebKit Web source element",
                                         (gchar*) "Source",
                                         (gchar*) "Handles HTTP/HTTPS uris",
                                         (gchar*) "Sebastian Dröge <sebastian.droege@collabora.co.uk>");
}

static void webkit_web_src_class_init(WebKitWebSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    oklass->finalize = webKitWebSrcFinalize;
    oklass->set_property = webKitWebSrcSetProperty;
    oklass->get_property = webKitWebSrcGetProperty;

    // icecast stuff
    g_object_class_install_property(oklass,
                                    PROP_IRADIO_MODE,
                                    g_param_spec_boolean("iradio-mode",
                                                         "iradio-mode",
                                                         "Enable internet radio mode (extraction of shoutcast/icecast metadata)",
                                                         FALSE,
                                                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass,
                                    PROP_IRADIO_NAME,
                                    g_param_spec_string("iradio-name",
                                                        "iradio-name",
                                                        "Name of the stream",
                                                        0,
                                                        (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass,
                                    PROP_IRADIO_GENRE,
                                    g_param_spec_string("iradio-genre",
                                                        "iradio-genre",
                                                        "Genre of the stream",
                                                        0,
                                                        (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
                                                        
    g_object_class_install_property(oklass,
                                    PROP_IRADIO_URL,
                                    g_param_spec_string("iradio-url",
                                                        "iradio-url",
                                                        "Homepage URL for radio stream",
                                                        0,
                                                        (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass,
                                    PROP_IRADIO_TITLE,
                                    g_param_spec_string("iradio-title",
                                                        "iradio-title",
                                                        "Name of currently playing song",
                                                        0,
                                                        (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    eklass->change_state = webKitWebSrcChangeState;

    g_type_class_add_private(klass, sizeof(WebKitWebSrcPrivate));
}

static void webkit_web_src_init(WebKitWebSrc* src,
                                WebKitWebSrcClass* gKlass)
{
    GstPadTemplate* padTemplate = gst_static_pad_template_get(&srcTemplate);
    GstPad* targetpad;
    WebKitWebSrcPrivate* priv = WEBKIT_WEB_SRC_GET_PRIVATE(src);

    src->priv = priv;

    priv->client = new StreamingClient(src);

    priv->srcpad = gst_ghost_pad_new_no_target_from_template("src",
                                                             padTemplate);

    gst_element_add_pad(GST_ELEMENT(src), priv->srcpad);

    priv->appsrc = GST_APP_SRC(gst_element_factory_make("appsrc", 0));
    if (!priv->appsrc) {
        GST_ERROR_OBJECT(src, "Failed to create appsrc");
        return;
    }

    gst_bin_add(GST_BIN(src), GST_ELEMENT(priv->appsrc));

    targetpad = gst_element_get_static_pad(GST_ELEMENT(priv->appsrc), "src");
    gst_ghost_pad_set_target(GST_GHOST_PAD(priv->srcpad), targetpad);
    gst_object_unref(targetpad);

    gst_app_src_set_callbacks(priv->appsrc, &appsrcCallbacks, src, 0);
    gst_app_src_set_emit_signals(priv->appsrc, FALSE);
    gst_app_src_set_stream_type(priv->appsrc, GST_APP_STREAM_TYPE_SEEKABLE);

    // 512k is a abitrary number but we should choose a value
    // here to not pause/unpause the SoupMessage too often and
    // to make sure there's always some data available for
    // GStreamer to handle.
    gst_app_src_set_max_bytes(priv->appsrc, 512 * 1024);

    webKitWebSrcStop(src, true);
}

static void webKitWebSrcFinalize(GObject* object)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(object);
    WebKitWebSrcPrivate* priv = src->priv;

    delete priv->client;

    g_free(priv->uri);

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, ((GObject* )(src)));
}

static void webKitWebSrcSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* pspec)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(object);
    WebKitWebSrcPrivate* priv = src->priv;

    switch (propID) {
    case PROP_IRADIO_MODE:
        priv->iradioMode = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
}

static void webKitWebSrcGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* pspec)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(object);
    WebKitWebSrcPrivate* priv = src->priv;

    switch (propID) {
    case PROP_IRADIO_MODE:
        g_value_set_boolean(value, priv->iradioMode);
        break;
    case PROP_IRADIO_NAME:
        g_value_set_string(value, priv->iradioName);
        break;
    case PROP_IRADIO_GENRE:
        g_value_set_string(value, priv->iradioGenre);
        break;
    case PROP_IRADIO_URL:
        g_value_set_string(value, priv->iradioUrl);
        break;
    case PROP_IRADIO_TITLE:
        g_value_set_string(value, priv->iradioTitle);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
}


static void webKitWebSrcStop(WebKitWebSrc* src, bool resetRequestedOffset)
{
    WebKitWebSrcPrivate* priv = src->priv;

    if (priv->resourceHandle) {
        priv->resourceHandle->cancel();
        priv->resourceHandle.release();
    }
    priv->resourceHandle = 0;

    if (priv->frame)
        priv->frame.release();

    if (priv->needDataID)
        g_source_remove(priv->needDataID);
    priv->needDataID = 0;

    if (priv->enoughDataID)
        g_source_remove(priv->enoughDataID);
    priv->enoughDataID = 0;

    if (priv->seekID)
        g_source_remove(priv->seekID);
    priv->seekID = 0;

    priv->paused = FALSE;

    g_free(priv->iradioName);
    priv->iradioName = 0;

    g_free(priv->iradioGenre);
    priv->iradioGenre = 0;

    g_free(priv->iradioUrl);
    priv->iradioUrl = 0;

    g_free(priv->iradioTitle);
    priv->iradioTitle = 0;

    if (priv->appsrc)
        gst_app_src_set_caps(priv->appsrc, 0);

    priv->offset = 0;
    priv->size = 0;
    priv->seekable = FALSE;

    if (resetRequestedOffset)
        priv->requestedOffset = 0;

    GST_DEBUG_OBJECT(src, "Stopped request");
}

static bool webKitWebSrcStart(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;

    if (!priv->uri) {
        GST_ERROR_OBJECT(src, "No URI provided");
        return false;
    }
    
    KURL url = KURL(KURL(), priv->uri);

    ResourceRequest request(url);
    request.setTargetType(ResourceRequestBase::TargetIsMedia);
    request.setAllowCookies(true);

    // Let Apple web servers know we want to access their nice movie trailers.
    if (!g_ascii_strcasecmp("movies.apple.com", url.host().utf8().data()))
        request.setHTTPUserAgent("Quicktime/7.2.0");

    if (priv->frame) {
        Document* document = priv->frame->document();
        if (document)
            request.setHTTPReferrer(document->documentURI());

        FrameLoader* loader = priv->frame->loader();
        if (loader)
            loader->addExtraFieldsToSubresourceRequest(request);
    }

    if (priv->requestedOffset) {
        GOwnPtr<gchar> val;

        val.set(g_strdup_printf("bytes=%" G_GUINT64_FORMAT "-", priv->requestedOffset));
        request.setHTTPHeaderField("Range", val.get());
    }

    if (priv->iradioMode)
        request.setHTTPHeaderField("icy-metadata", "1");

    // Needed to use DLNA streaming servers
    request.setHTTPHeaderField("transferMode.dlna", "Streaming");

    priv->resourceHandle = ResourceHandle::create(request, priv->client, 0, false, false, false);
    if (!priv->resourceHandle) {
        GST_ERROR_OBJECT(src, "Failed to create ResourceHandle");
        return false;
    }

    GST_DEBUG_OBJECT(src, "Started request");

    return true;
}

static GstStateChangeReturn webKitWebSrcChangeState(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    WebKitWebSrc* src = WEBKIT_WEB_SRC(element);
    WebKitWebSrcPrivate* priv = src->priv;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!priv->appsrc) {
            gst_element_post_message(element,
                                     gst_missing_element_message_new(element, "appsrc"));
            GST_ELEMENT_ERROR(src, CORE, MISSING_PLUGIN, (0), ("no appsrc"));
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(ret == GST_STATE_CHANGE_FAILURE)) {
        GST_DEBUG_OBJECT(src, "State change failed");
        return ret;
    }

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        GST_DEBUG_OBJECT(src, "READY->PAUSED");
        if (!webKitWebSrcStart(src))
            ret = GST_STATE_CHANGE_FAILURE;
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_DEBUG_OBJECT(src, "PAUSED->READY");
        webKitWebSrcStop(src, true);
        break;
    default:
        break;
    }

    return ret;
}

// uri handler interface

static GstURIType webKitWebSrcUriGetType(void)
{
    return GST_URI_SRC;
}

static gchar** webKitWebSrcGetProtocols(void)
{
    static gchar* protocols[] = {(gchar*) "http", (gchar*) "https", 0 };

    return protocols;
}

static const gchar* webKitWebSrcGetUri(GstURIHandler* handler)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(handler);
    WebKitWebSrcPrivate* priv = src->priv;

    return priv->uri;
}

static gboolean webKitWebSrcSetUri(GstURIHandler* handler, const gchar* uri)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(handler);
    WebKitWebSrcPrivate* priv = src->priv;

    if (GST_STATE(src) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
        return FALSE;
    }

    g_free(priv->uri);
    priv->uri = 0;

    if (!uri)
        return TRUE;

    SoupURI* soupUri = soup_uri_new(uri);

    if (!soupUri || !SOUP_URI_VALID_FOR_HTTP(soupUri)) {
        GST_ERROR_OBJECT(src, "Invalid URI '%s'", uri);
        soup_uri_free(soupUri);
        return FALSE;
    }

    priv->uri = soup_uri_to_string(soupUri, FALSE);
    soup_uri_free(soupUri);

    return TRUE;
}

static void webKitWebSrcUriHandlerInit(gpointer gIface, gpointer ifaceData)
{
    GstURIHandlerInterface* iface = (GstURIHandlerInterface *) gIface;

    iface->get_type = webKitWebSrcUriGetType;
    iface->get_protocols = webKitWebSrcGetProtocols;
    iface->get_uri = webKitWebSrcGetUri;
    iface->set_uri = webKitWebSrcSetUri;
}

// appsrc callbacks

static gboolean webKitWebSrcNeedDataMainCb(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;

    ResourceHandleInternal* d = priv->resourceHandle->getInternal();
    if (d->m_msg)
        soup_session_unpause_message(ResourceHandle::defaultSession(), d->m_msg);

    priv->paused = FALSE;
    priv->needDataID = 0;
    return FALSE;
}

static void webKitWebSrcNeedDataCb(GstAppSrc* appsrc, guint length, gpointer userData)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(userData);
    WebKitWebSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Need more data: %u", length);
    if (priv->needDataID || !priv->paused)
        return;

    priv->needDataID = g_timeout_add_full(G_PRIORITY_HIGH, 0, (GSourceFunc) webKitWebSrcNeedDataMainCb, gst_object_ref(src), (GDestroyNotify) gst_object_unref);
}

static gboolean webKitWebSrcEnoughDataMainCb(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;

    ResourceHandleInternal* d = priv->resourceHandle->getInternal();
    soup_session_pause_message(ResourceHandle::defaultSession(), d->m_msg);
    
    priv->paused = TRUE;
    priv->enoughDataID = 0;
    return FALSE;
}

static void webKitWebSrcEnoughDataCb(GstAppSrc* appsrc, gpointer userData)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(userData);
    WebKitWebSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Have enough data");
    if (priv->enoughDataID || priv->paused)
        return;

    priv->enoughDataID = g_timeout_add_full(G_PRIORITY_HIGH, 0, (GSourceFunc) webKitWebSrcEnoughDataMainCb, gst_object_ref(src), (GDestroyNotify) gst_object_unref);
}

static gboolean webKitWebSrcSeekMainCb(WebKitWebSrc* src)
{
    webKitWebSrcStop(src, false);
    webKitWebSrcStart(src);

    return FALSE;
}

static gboolean webKitWebSrcSeekDataCb(GstAppSrc* appsrc, guint64 offset, gpointer userData)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(userData);
    WebKitWebSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Seeking to offset: %" G_GUINT64_FORMAT, offset);
    if (offset == priv->offset)
        return TRUE;

    if (!priv->seekable)
        return FALSE;
    if (offset > priv->size)
        return FALSE;

    GST_DEBUG_OBJECT(src, "Doing range-request seek");
    priv->requestedOffset = offset;
    if (priv->seekID)
        g_source_remove(priv->seekID);
    priv->seekID = g_timeout_add_full(G_PRIORITY_HIGH, 0, (GSourceFunc) webKitWebSrcSeekMainCb, gst_object_ref(src), (GDestroyNotify) gst_object_unref);
    
    return TRUE;
}

void webKitWebSrcSetFrame(WebKitWebSrc* src, WebCore::Frame* frame)
{
    WebKitWebSrcPrivate* priv = src->priv;

    priv->frame = frame;
}

StreamingClient::StreamingClient(WebKitWebSrc* src) : m_src(src)
{

}

StreamingClient::~StreamingClient()
{

}

void StreamingClient::willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse&)
{
}

void StreamingClient::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    WebKitWebSrcPrivate* priv = m_src->priv;

    GST_DEBUG_OBJECT(m_src, "Received response: %d", response.httpStatusCode());

    // If we seeked we need 206 == PARTIAL_CONTENT
    if (priv->requestedOffset && response.httpStatusCode() != 206) {
        GST_ELEMENT_ERROR(m_src, RESOURCE, READ, (0), (0));
        webKitWebSrcStop(m_src, true);
        return;
    }

    long long length = response.expectedContentLength();
    if (length > 0) {
        length += priv->requestedOffset;
        gst_app_src_set_size(priv->appsrc, length);
    }

    priv->size = length >= 0 ? length : 0;
    priv->seekable = length > 0 && g_ascii_strcasecmp("none", response.httpHeaderField("Accept-Ranges").utf8().data());

    // icecast stuff
    String value = response.httpHeaderField("icy-metaint");
    if (!value.isEmpty()) {
        gchar* endptr = 0;
        gint64 icyMetaInt = g_ascii_strtoll(value.utf8().data(), &endptr, 10);
            
        if (endptr && *endptr == '\0' && icyMetaInt > 0) {
            GstCaps* caps = gst_caps_new_simple("application/x-icy", "metadata-interval", G_TYPE_INT, (gint) icyMetaInt, NULL);

            gst_app_src_set_caps(priv->appsrc, caps);
            gst_caps_unref(caps);
        }
    }

    GstTagList* tags = gst_tag_list_new();
    value = response.httpHeaderField("icy-name");
    if (!value.isEmpty()) {
        g_free(priv->iradioName);
        priv->iradioName = g_strdup(value.utf8().data());
        g_object_notify(G_OBJECT(m_src), "iradio-name");
        gst_tag_list_add(tags, GST_TAG_MERGE_REPLACE, GST_TAG_ORGANIZATION, priv->iradioName, NULL);
    }
    value = response.httpHeaderField("icy-genre");
    if (!value.isEmpty()) {
        g_free(priv->iradioGenre);
        priv->iradioGenre = g_strdup(value.utf8().data());
        g_object_notify(G_OBJECT(m_src), "iradio-genre");
        gst_tag_list_add(tags, GST_TAG_MERGE_REPLACE, GST_TAG_GENRE, priv->iradioGenre, NULL);
    }
    value = response.httpHeaderField("icy-url");
    if (!value.isEmpty()) {
        g_free(priv->iradioUrl);
        priv->iradioUrl = g_strdup(value.utf8().data());
        g_object_notify(G_OBJECT(m_src), "iradio-url");
        gst_tag_list_add(tags, GST_TAG_MERGE_REPLACE, GST_TAG_LOCATION, priv->iradioUrl, NULL);
    }
    value = response.httpHeaderField("icy-title");
    if (!value.isEmpty()) {
        g_free(priv->iradioTitle);
        priv->iradioTitle = g_strdup(value.utf8().data());
        g_object_notify(G_OBJECT(m_src), "iradio-title");
        gst_tag_list_add(tags, GST_TAG_MERGE_REPLACE, GST_TAG_TITLE, priv->iradioTitle, NULL);
    }

    if (gst_tag_list_is_empty(tags))
        gst_tag_list_free(tags);
    else
        gst_element_found_tags_for_pad(GST_ELEMENT(m_src), m_src->priv->srcpad, tags);
}

void StreamingClient::didReceiveData(ResourceHandle* handle, const char* data, int length, int lengthReceived)
{
    WebKitWebSrcPrivate* priv = m_src->priv;

    GST_LOG_OBJECT(m_src, "Have %d bytes of data", length);

    if (priv->seekID || handle != priv->resourceHandle) {
        GST_DEBUG_OBJECT(m_src, "Seek in progress, ignoring data");
        return;
    }

    GstBuffer* buffer = gst_buffer_new_and_alloc(length);

    memcpy(GST_BUFFER_DATA(buffer), data, length);
    GST_BUFFER_OFFSET(buffer) = priv->offset;
    priv->offset += length;
    GST_BUFFER_OFFSET_END(buffer) = priv->offset;

    GstFlowReturn ret = gst_app_src_push_buffer(priv->appsrc, buffer);
    if (ret != GST_FLOW_OK && ret != GST_FLOW_UNEXPECTED)
        GST_ELEMENT_ERROR(m_src, CORE, FAILED, (0), (0));
}

void StreamingClient::didFinishLoading(ResourceHandle*)
{
    GST_DEBUG_OBJECT(m_src, "Have EOS");
    gst_app_src_end_of_stream(m_src->priv->appsrc);
}

void StreamingClient::didFail(ResourceHandle*, const ResourceError& error)
{
    GST_ERROR_OBJECT(m_src, "Have failure: %s", error.localizedDescription().utf8().data());
    GST_ELEMENT_ERROR(m_src, RESOURCE, FAILED, ("%s", error.localizedDescription().utf8().data()), (0));
    gst_app_src_end_of_stream(m_src->priv->appsrc);
}

void StreamingClient::wasBlocked(ResourceHandle*)
{
}

void StreamingClient::cannotShowURL(ResourceHandle*)
{
}

