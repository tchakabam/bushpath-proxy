#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "api.h"

#include "tcp_client.h"
#include "tcp_service.h"
#include "throttler.h"
#include "http_parser.h"

#define USE_EXPERIMENTAL_TCP FALSE
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 6900

#define GST_CAT_DEFAULT bp_proxy_api_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

GError* InitGStreamer (int* argc, char** argv)
{
    // Init GStreamer
    GError* error = NULL;
    if (!gst_init_check (NULL, NULL, &error)) {
        GST_WARNING ("Could not init GStreamer!");
    }

    GST_DEBUG_CATEGORY_INIT (bp_proxy_api_debug, "bp_proxy_api", 0,
      "Bushpath Proxy API");

    return error;
}

BushpathProxyOptions
BushpathProxyOptions_New (gchar* bindAddress, guint16 port)
{
    BushpathProxyOptions options;
    options.bindAddress = strdup(bindAddress);
    options.port = port;
    return options;
}

void
BushpathProxyOptions_Free (BushpathProxyOptions* options)
{
    g_free (options->bindAddress);
}

void
BushpathProxy_HeaderComplete (GstElement* element, gpointer* proxy);

void
BushpathProxy_SetPipelineState (BushpathProxy *proxy, GstState state)
{
    GstStateChangeReturn stateChangeRet;
    GstElement* pipeline;

    pipeline = proxy->outPipeline;
    stateChangeRet = gst_element_set_state (GST_ELEMENT (pipeline), state);
    switch (stateChangeRet) {
    case GST_STATE_CHANGE_FAILURE:
        GST_ERROR ("Failed to change state: %" GST_PTR_FORMAT, pipeline);
        break;
    case GST_STATE_CHANGE_SUCCESS:
        GST_INFO ("Succeeded to change state: %" GST_PTR_FORMAT, pipeline);
        break;
    case GST_STATE_CHANGE_ASYNC:
        GST_DEBUG ("Async state change return: %" GST_PTR_FORMAT, pipeline);
        break;
    case GST_STATE_CHANGE_NO_PREROLL:
        GST_DEBUG ("No-preroll state change return: %" GST_PTR_FORMAT, pipeline);
        break;
    }

    ///*
    pipeline = proxy->inPipeline;
    stateChangeRet = gst_element_set_state (GST_ELEMENT (pipeline), state);
    switch (stateChangeRet) {
    case GST_STATE_CHANGE_FAILURE:
        GST_ERROR ("Failed to change state: %" GST_PTR_FORMAT, pipeline);
        break;
    case GST_STATE_CHANGE_SUCCESS:
        GST_INFO ("Succeeded to change state: %" GST_PTR_FORMAT, pipeline);
        break;
    case GST_STATE_CHANGE_ASYNC:
        GST_DEBUG ("Async state change return: %" GST_PTR_FORMAT, pipeline);
        break;
    case GST_STATE_CHANGE_NO_PREROLL:
        GST_DEBUG ("No-preroll state change return: %" GST_PTR_FORMAT, pipeline);
        break;
    }
    //*/
}

BushpathProxy*
BushpathProxy_New (GMainContext *context, BushpathProxyOptions options, int *argc, char **argv)
{
    gchar* url;
    GError* error;
    BushpathProxy *proxy;

    GST_DEBUG ("Validating input ...");

    g_return_val_if_fail (context, NULL);

    if (options.bindAddress == NULL) {
        options.bindAddress = "127.0.0.1";
    }

    GST_DEBUG ("Allocating memory ...");

    proxy = (BushpathProxy*) g_malloc(sizeof(BushpathProxy));
    memset (proxy, 0, sizeof(BushpathProxy));

    GST_DEBUG ("Launching proxy ...");

    proxy->options = options;
    proxy->options.bindAddress = strdup(options.bindAddress);
    proxy->context = context;

    // Launch GStreamer
    error = InitGStreamer(argc, argv);
    if (error) {
        GST_ERROR ("Error initializing GStreamer: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    // Register elements on GStreamer factory
    bp_http_parser_setup ();
    bp_throttler_setup ();
    bp_tcp_service_setup ();
    bp_tcp_client_setup ();

    // Create elements
    if (USE_EXPERIMENTAL_TCP) {
        GST_WARNING ("Using experimental TCP elements!");
        proxy->tcpServiceSrc = gst_element_factory_make ("bp_tcpservice", "tcpServiceSrc");
        proxy->tcpServiceSink = gst_element_factory_make ("bp_tcpservice", "tcpServiceSink");
        proxy->tcpClientSrc = gst_element_factory_make ("bp_tcpclient", "tcpClientSrc");
        proxy->tcpClientSink = gst_element_factory_make ("bp_tcpclient", "tcpClientSink");
    } else {
        proxy->tcpServiceSrc = gst_element_factory_make ("tcpserversrc", "tcpServiceSrc");
        proxy->tcpServiceSink = gst_element_factory_make ("tcpserversink", "tcpServiceSink");
        proxy->tcpClientSrc = gst_element_factory_make ("tcpclientsrc", "tcpClientSrc");
        proxy->tcpClientSink = gst_element_factory_make ("tcpclientsink", "tcpClientSink");
    }

    proxy->inThrottler = gst_element_factory_make ("bp_throttler", "inThrottler");
    proxy->outThrottler = gst_element_factory_make ("bp_throttler", "outThrottler");
    proxy->httpParserIn = gst_element_factory_make ("bp_http_parser", "httpParser");
    proxy->httpParserOut = gst_element_factory_make ("bp_http_parser", "httpParser");

    GST_DEBUG ("Elements created. Configuring ...");

    // FIXME: use 'host' instead of 'address' as property in xp tcp elements
    if (USE_EXPERIMENTAL_TCP) {
        g_object_set (proxy->tcpServiceSrc, "mode", BP_MODE_SRC_ONLY, NULL);
        g_object_set (proxy->tcpServiceSink, "mode", BP_MODE_SINK_ONLY, NULL);
        g_object_set (proxy->tcpClientSrc, "mode", BP_MODE_SRC_ONLY, NULL);
        g_object_set (proxy->tcpClientSink, "mode", BP_MODE_SINK_ONLY, NULL);
        // FIXME: set default host and port here instead of relying on internal defaults
    } else {
        g_object_set (proxy->tcpServiceSrc, "host", DEFAULT_HOST, "port", DEFAULT_PORT, NULL);
        g_object_set (proxy->tcpServiceSink, "host", DEFAULT_HOST, "port", DEFAULT_PORT, NULL);
    }

    GST_DEBUG ("Building pipeline ...");

    // Create pipelines
    proxy->outPipeline = gst_pipeline_new ("proxyOut");
    proxy->inPipeline = gst_pipeline_new ("proxyIn");

    gst_bin_add_many (GST_BIN (proxy->outPipeline),
        proxy->tcpServiceSrc,
        proxy->httpParserOut,
        proxy->outThrottler,
    NULL);

    gst_bin_add_many (GST_BIN (proxy->inPipeline),
        proxy->httpParserIn,
        proxy->inThrottler,
        proxy->tcpServiceSink,
    NULL);

    if (!gst_element_link_many (
        // outbind pipeline
        proxy->tcpServiceSrc,
        proxy->httpParserOut,
        proxy->outThrottler,
    NULL)) {
        GST_ERROR ("Cannot link outbound pipeline elements!");
        return NULL;
    }

    if (!gst_element_link_many (
        // inbound pipeline
        proxy->httpParserIn,
        proxy->inThrottler,
        proxy->tcpServiceSink,
    NULL)) {
        GST_ERROR ("Cannot link inbound pipeline elements");
        return NULL;
    }

    GST_DEBUG ("Pipelines linked!");

    // Attach signals
    g_signal_connect(proxy->httpParserIn, "header-complete", G_CALLBACK(BushpathProxy_HeaderComplete), proxy);
    g_signal_connect(proxy->httpParserOut, "header-complete", G_CALLBACK(BushpathProxy_HeaderComplete), proxy);

    GST_DEBUG ("Playing ...");

    BushpathProxy_SetPipelineState (proxy, GST_STATE_PAUSED);

    return proxy;
}

void
BushpathProxy_ConnectClient (BushpathProxy* p, gchar* address, guint portNum)
{
    GstStateChangeReturn stateChangeRet;

    gst_bin_add_many (GST_BIN (p->outPipeline),
        p->tcpClientSink,
    NULL);

    if (!gst_element_link_many (
        // outbind pipeline
        p->outThrottler,
        p->tcpClientSink,
    NULL)) {
        GST_ERROR ("Cannot link outbound pipeline elements!");
        return;
    }

///*
    gst_bin_add_many (GST_BIN (p->inPipeline),
        p->tcpClientSrc,
    NULL);

    if (!gst_element_link_many (
        // inbound pipeline
        p->tcpClientSrc,
        p->httpParserIn,
    NULL)) {
        GST_ERROR ("Cannot link inbound pipeline elements");
        return;
    }
//*/

    // FIXME: use 'host' instead of 'address' as property in xp tcp elements
    if (USE_EXPERIMENTAL_TCP) {
        g_object_set (p->tcpClientSrc,
            "address", address,
            "port", portNum,
        NULL);
        g_object_set (p->tcpClientSink,
            "address", address,
            "port", portNum,
         NULL);
    } else {
        ///*
        g_object_set (p->tcpClientSrc,
            "host", address,
            "port", portNum,
        NULL);
        //*/
        g_object_set (p->tcpClientSink,
            "host", address,
            "port", portNum,
        NULL);
    }

    BushpathProxy_SetPipelineState (p, GST_STATE_PLAYING);

    GST_DEBUG ("Client setup performed");
}

void
BushpathProxy_HeaderComplete (GstElement* element, gpointer* proxy)
{
    gsize len, lenHost;
    gchar* host, *address, *port = NULL;
    BushpathProxy* p = (BushpathProxy*) proxy;
    guint portNum;

    GST_DEBUG ("Header complete signal received");

    // Get full HTTP host string
    g_object_get (element, "host", &host, NULL);
    lenHost = strlen (host);
    if (lenHost == 0) {
        GST_ERROR ("Could not parse host name (length is zero) !");
        g_free (host);
        return;
    }
    GST_DEBUG ("Host is: %s", host);

    // Parse out host & port
    port = g_strstr_len (host, lenHost, ":");
    if (port == NULL) {
        port = "80";
        address = g_strdup (host);
    }
    len = strlen(port);
    // at least the collon and one number
    g_return_if_fail (len > 2);
    port = g_strdup (++port); // new alloc
    address = g_strndup (host, lenHost - len);
    g_return_if_fail (address && port);

    // Convert port string to int
    portNum = (guint) strtoul (port, NULL, 10);
    g_return_if_fail (portNum > 0);
    GST_DEBUG ("Set host: %s; Set port: %u", address, portNum);

    // Connect client elements
    BushpathProxy_ConnectClient (p, address, portNum);

    // Cleanup
    g_free (address);
    g_free (port);
    g_free (host);
}

void
BushpathProxy_Free (BushpathProxy* proxy)
{
    GST_DEBUG ("Destroying proxy ...");

    g_free (proxy->options.bindAddress);
    g_free (proxy);

    gst_object_unref (proxy->inPipeline);
    gst_object_unref (proxy->outPipeline);
    gst_object_unref (proxy->tcpServiceSrc);
    gst_object_unref (proxy->tcpServiceSink);
    gst_object_unref (proxy->tcpClientSrc);
    gst_object_unref (proxy->tcpClientSink);
    gst_object_unref (proxy->inThrottler);
    gst_object_unref (proxy->outThrottler);
    gst_object_unref (proxy->httpParserIn);
    gst_object_unref (proxy->httpParserOut);

    GST_DEBUG ("Proxy free'd.");
}
