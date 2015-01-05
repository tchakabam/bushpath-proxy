#include <glib.h>
#include <gio/gio.h>

#include "connection.h"
#include "http_parser.h"

#ifndef __PROXY_HTTP_HANDLERS__
#define __PROXY_HTTP_HANDLERS__

#define DEFAULT_PORT 80

gboolean
inboundConnectionHeaderParsed (InboundConnection * user)
{
    return user->protocol == HTTP && strlen(user->destinationHost) > 0;
}

void
ensureOpenOutboundConnection (InboundConnection * user, gboolean *res)
{
    guint16 port = DEFAULT_PORT;
    GError *error = NULL;
    gchar *address = NULL;
    GSocket* socket;
    gint fd;
    GIOChannel *channel;

    g_message ("ensureOpenOutboundConnection()");

    // We need to create the outbound connection if it's NULL
    if (user->out->connection) {
        return;
    }

    *res = FALSE;
    g_return_if_fail(user->destinationHost && strlen(user->destinationHost));

    g_message ("Client %p", user->out->client);

    g_return_if_fail(user->out->client);

    g_message ("Connectiong to host ... : %s", user->destinationHost);

    //FIXME: Rather use g_socket_client_connect_to_uri ?
    user->out->connection = g_socket_client_connect_to_host (user->out->client, user->destinationHost, port, NULL, &error);

    g_message ("Validating connection ...");

    if (error) {
        g_warning ("Failed to create outbound connection to host %s: %s", user->destinationHost, error->message);
        g_object_unref (error);
    }

    g_return_if_fail (!error);
    g_return_if_fail (user->out->connection);

    address = getSocketRemoteIP(user->out->connection, &error);
    port = getSocketRemotePort(user->out->connection, &error);

    g_message( "Connected to %s (%s) on port %d", user->destinationHost, address, (int) port);

    // Force some socket config
    socket = g_socket_connection_get_socket (user->out->connection);
    g_return_if_fail (socket);
    g_object_set (socket, "blocking", FALSE, NULL);

    // Get IO channel
    fd = g_socket_get_fd(socket);
    channel = g_io_channel_unix_new(fd);
    user->out->channel = channel;

    g_free (address);
    *res = TRUE;
}

void
forwardHeaderBuffer (InboundConnection * user, gboolean *res)
{
    GSocket* socket = NULL;
    GError* error = NULL;
    guint size;

    *res = FALSE;

    g_message ("Will now forward buffered connection header ...");

    g_return_if_fail (user->requestHeaderBuffer);

#if 1
    printf ("\n%s\n", user->requestHeaderBuffer);
#endif

    g_return_if_fail (user->out->connection);
    socket = g_socket_connection_get_socket (user->out->connection);
    g_return_if_fail (socket);

    size = strlen(user->requestHeaderBuffer);
    g_socket_send (socket, user->requestHeaderBuffer, size, NULL, &error);

    if (error) {
        g_warning ("Failed to send headers: %s", error->message);
        g_object_unref (error);
    }

    user->out->bytesWrittenCount += size;
    user->requestHeaderOffset = 0;
    user->lastSendTime = g_get_monotonic_time();

    *res = TRUE;
}

void
forwardData (InboundConnection * user, gboolean *res)
{
    ///*
    if (user->requestHeaderOffset == 0) {
        *res = FALSE;
        return;
    }
    //*/

    g_message ("Forwarding ...");

    // opens a connection if not connected yet
    ensureOpenOutboundConnection (user, res);
    g_return_if_fail (res);

    // First we have to send the header stuff we buffered
    if (user->requestHeaderOffset > 0) {
        forwardHeaderBuffer (user, res);
        g_return_if_fail (res);
        // set up timeout to receive data for this user
        g_timeout_add(SOCKET_RECEIVE_POLL_PERIOD, receiveOutboundData, user);
        g_timeout_add(SOCKET_RECEIVE_POLL_PERIOD+1, sendInboundData, user);
    }
}

void
analyzeData (InboundConnection *user, gboolean *res)
{
    g_message ("Analyzing ...");

    readConnectionHeader (user, res);
    g_return_if_fail (res);
    // Poll on each connection header read for data
    // to forward already (maybe we wont get called again)
    if (inboundConnectionHeaderParsed (user)) {
        // forward any data
        forwardData (user, res);
        g_return_if_fail (res);
    }
}

#endif