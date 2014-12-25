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

#ifndef __PROXY_IO_HANDLERS__
#define __PROXY_IO_HANDLERS__

typedef struct _IOHandlerData {
	BushpathProxy* proxy;
	InboundConnection* connection;
} IOHandlerData;

IOHandlerData*
newIOHandlerData (BushpathProxy* proxy, InboundConnection* connection)
{
	IOHandlerData* handlerData = (IOHandlerData*) g_malloc(sizeof(IOHandlerData));
	handlerData->proxy = proxy;
	handlerData->connection = connection;
	return handlerData;
}

void
destroyIOHandlerData (IOHandlerData* data)
{
	g_free(data);
}

gboolean
onNetworkRead(GIOChannel *sourceChannel,
            GIOCondition idCond,
            gpointer userData) // IOHandlerData*
{
    IOHandlerData* handlerData = (IOHandlerData*) userData;
    InboundConnection* user = (InboundConnection*) handlerData->connection;
    gboolean res = TRUE;
    gboolean forward = TRUE;

    if (!user) {
        destroyIOHandlerData(handlerData);
        g_error ("Failed to retrieve user connection data from table!!!");
        return FALSE;
    }

    g_message ("Network read: User data mapped @ %p", user);

    // TODO: We only parse on header per connection!
    // So this means we can't multiplex several user HTTP connections
    // over one inbound proxy connection.
    if (!inboundConnectionHeaderParsed (user)) {
        g_message ("Have not parsed header for this connection yet.");
        forward = FALSE;
    }

    switch (forward)
    {
    case FALSE:
        analyzeData (user, &res);
        break;
    case TRUE:
        forwardData (user, &res);
        break;
    }

    if (!res) {
        g_message ("Destroying connection");
        // Drop last reference on connection
        g_object_unref (userData);
    }

    return res;
}

gboolean
onNewConnection(GSocketService *service,
              GSocketConnection *connection,
              GObject *source_object,
              gpointer user_data)
{
	BushpathProxy* proxy = (BushpathProxy*) user_data;

    GSocket *socket = g_socket_connection_get_socket(connection);
    GSocketAddress *sockaddr = g_socket_connection_get_remote_address(connection, NULL);
    GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(sockaddr));
    guint16 port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(sockaddr));
    gint fd;
    GIOChannel *channel;
    InboundConnection *user;

    g_object_ref (connection);

    g_message ("New connection from %s:%d", g_inet_address_to_string(addr), (int) port);

    g_return_val_if_fail (socket, FALSE);

    g_message ("Getting IO channel ...");

    fd = g_socket_get_fd(socket);
    channel = g_io_channel_unix_new(fd);

    g_return_val_if_fail (channel, FALSE);

    g_message ("Attaching IO watch");

    user = newInboundConnection (connection, channel);
    user->out = newOutboundConnection (proxy->client);

    // Add IO watch with pointer to connection handle as user data for callback
    g_io_add_watch(channel, G_IO_IN, (GIOFunc) onNetworkRead, newIOHandlerData(proxy, user));

    return TRUE;
}

#endif