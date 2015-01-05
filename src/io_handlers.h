/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#include <glib.h>
#include <gio/gio.h>

#include "api.h"
#include "connection.h"
//#include "http_handlers.h"

#ifndef __PROXY_IO_HANDLERS__
#define __PROXY_IO_HANDLERS__

typedef struct _BP_IOHandlerData {
	BushpathProxy* proxy;
	BP_Connection* connection;
} BP_IOHandlerData;

BP_IOHandlerData*
newIOHandlerData (BushpathProxy* proxy, BP_Connection* connection)
{
	BP_IOHandlerData* handlerData = g_new (BP_IOHandlerData, 1);
	handlerData->proxy = proxy;
	handlerData->connection = connection;
	return handlerData;
}

void
freeIOHandlerData (BP_IOHandlerData* data)
{
	g_free(data);
}

gboolean
BP_IncomingConnectionReceive (GIOChannel *sourceChannel,
            GIOCondition idCond,
            gpointer userData) // IOHandlerData*
{
    BP_IOHandlerData* handlerData = (BP_IOHandlerData*) userData;
    BP_Connection* conn = handlerData->connection;
    gboolean res = TRUE;
    gboolean forward = TRUE;

    if (!conn) {
        freeIOHandlerData(handlerData);
        g_error ("Failed to retrieve user connection data from table!!!");
        return FALSE;
    }

    g_message ("Network read: User data mapped @ %p", conn);

    if (!res) {
        g_message ("Error processing incoming data");
    }

    return res;
}

gboolean
BushpathProxy_IncomingConnection (GSocketService *service,
              GSocketConnection *incomingConnection,
              GObject *source_object,
              gpointer user_data)
{
	BushpathProxy* proxy = (BushpathProxy*) user_data;

    GSocket *socket = g_socket_connection_get_socket(incomingConnection);
    GSocketAddress *sockaddr = g_socket_connection_get_remote_address(incomingConnection, NULL);
    GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(sockaddr));
    guint16 port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(sockaddr));
    gint fd;
    GIOChannel *channel;
    BP_Connection *conn;

    g_return_val_if_fail (socket, FALSE);
    g_object_ref (incomingConnection);

    g_message ("New connection from %s:%d", g_inet_address_to_string(addr), (int) port);
    g_message ("Attaching IO channel watch ...");

    conn = newConnection (proxy->client, incomingConnection, NULL);
    fd = g_socket_get_fd(socket);
    channel = g_io_channel_unix_new(fd);
    g_return_val_if_fail (channel, FALSE);

    // Add IO watch with pointer to connection handle as user data for callback
    g_io_add_watch(channel, G_IO_IN, (GIOFunc) BP_IncomingConnectionReceive, newIOHandlerData(proxy, conn));
    g_io_channel_unref (channel);

    return TRUE;
}

#endif