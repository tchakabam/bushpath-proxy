#include <stdio.h>
#include <string.h>
/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#include <glib.h>
#include <gio/gio.h>

#include "api.h"
#include "transport.h"
#include "io_handlers.h"

BushpathProxy*
BushpathProxy_New (GMainContext *context, BushpathProxyOptions options)
{
    gchar* url;

    g_message ("Validating input ...");

    g_return_val_if_fail (context, NULL);
    g_return_val_if_fail (options.bindAddress, NULL);

    g_message ("Allocating memory ...");

    BushpathProxy *proxy = (BushpathProxy*) g_malloc(sizeof(BushpathProxy));
    memset (proxy, 0, sizeof(BushpathProxy));

    g_message ("Launching proxy ...");

    proxy->options = options;
    proxy->options.bindAddress = strdup(options.bindAddress);

    proxy->context = context;
    // start service, client, and main loop
    proxy->service = g_socket_service_new();
    proxy->client = g_socket_client_new();
    // create network address
    proxy->inetAddress = g_inet_address_new_from_string(proxy->options.bindAddress);
    proxy->socketAddress = g_inet_socket_address_new(proxy->inetAddress, proxy->options.port);

    // add server socket to service and attach listener
    g_socket_listener_add_address(G_SOCKET_LISTENER(proxy->service),
                                                            proxy->socketAddress,
                                                                            G_SOCKET_TYPE_STREAM,
                                                                            G_SOCKET_PROTOCOL_TCP,
                                                                            NULL, NULL, NULL);

    g_message ("Starting IO service...");
    // Connect socket to IO service
    g_socket_service_start(proxy->service);
    g_message ("IO service is up");
    g_message ("Attaching signals ...");
    // Pass proxy instance as user data
    g_signal_connect(proxy->service, "incoming", G_CALLBACK(onNewConnection), proxy);
    g_message ("Attached!");

    g_object_get (proxy->socketAddress, "port", &proxy->options.port, NULL);
    url = g_inet_address_to_string (proxy->inetAddress);
    g_message ("Local IP/Port: %s:%u", url, proxy->options.port);
    g_free (url);

    return proxy;
}

void
BushpathProxy_Destroy (BushpathProxy* proxy)
{
    g_message ("Destroying proxy ...");

    g_object_unref (proxy->inetAddress);
    g_object_unref (proxy->socketAddress);
    g_object_unref (proxy->client);
    g_object_unref (proxy->service);

    // TODO: Free / unref user connection data

    g_free (proxy->options.bindAddress);
    g_free (proxy);

    g_message ("Proxy free'd.");
}
