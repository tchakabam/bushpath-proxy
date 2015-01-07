#include <stdio.h>
#include <string.h>
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

GError* InitGStreamer (int* argc, char** argv)
{
    // Init GStreamer
    GError* error = NULL;
    if (!gst_init_check (NULL, NULL, &error)) {
        g_warning ("Could not init GStreamer!");
    }
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
BushpathProxy_HeaderComplete (GstElement* element);

BushpathProxy*
BushpathProxy_New (GMainContext *context, BushpathProxyOptions options, int *argc, char **argv)
{
    gchar* url;
    GError* error;
    GstElement *tcpService, *tcpClient, *throttler, *httpParser;
    GstElement *pipeline;
    GstStateChangeReturn stateChangeRet;

    g_message ("Validating input ...");

    g_return_val_if_fail (context, NULL);

    if (options.bindAddress == NULL) {
        options.bindAddress = "127.0.0.1";
    }

    g_message ("Allocating memory ...");

    BushpathProxy *proxy = (BushpathProxy*) g_malloc(sizeof(BushpathProxy));
    memset (proxy, 0, sizeof(BushpathProxy));

    g_message ("Launching proxy ...");

    proxy->options = options;
    proxy->options.bindAddress = strdup(options.bindAddress);
    proxy->context = context;

    // Launch GStreamer
    error = InitGStreamer(argc, argv);
    if (error) {
        g_error ("Error initializing GStreamer: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    // Register elements on GStreamer factory
    bp_http_parser_setup ();
    bp_throttler_setup ();
    bp_tcp_service_setup ();
    bp_tcp_client_setup ();

    // Create elements
    tcpService = gst_element_factory_make ("bp_tcpservice", NULL);
    tcpClient = gst_element_factory_make ("bp_tcpclient", NULL);
    throttler = gst_element_factory_make ("bp_throttler", NULL);
    httpParser = gst_element_factory_make ("bp_http_parser", NULL);

    // Link pipeline
    pipeline = gst_pipeline_new ("bp_proxy");
    gst_bin_add_many (GST_BIN (pipeline), tcpService, httpParser, throttler, tcpClient, NULL);
    if (!gst_element_link_many (tcpService, httpParser, throttler, tcpClient, NULL)) {
        g_error ("Cannot link gstreamer elements");
        return NULL;
    }

    g_message ("Elements created and linked!");

    // Attach signals
    g_signal_connect(httpParser, "header-complete", G_CALLBACK(BushpathProxy_HeaderComplete), NULL);

    stateChangeRet = gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
    if (stateChangeRet != GST_STATE_CHANGE_SUCCESS) {
        g_warning ("State change returned %d", (int) stateChangeRet);
    }

    g_message ("State change performed with result %d", (int) stateChangeRet);

    return proxy;
}

void
BushpathProxy_HeaderComplete (GstElement* element)
{
    g_message ("Header complete signal received");
}

void
BushpathProxy_Free (BushpathProxy* proxy)
{
    g_message ("Destroying proxy ...");

    g_free (proxy->options.bindAddress);
    g_free (proxy);

    g_message ("Proxy free'd.");
}
