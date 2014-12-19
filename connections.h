#ifndef __PROXY_CONNECTIONS__
#define __PROXY_CONNECTIONS__

#include <glib.h>
#include <gio/gio.h>

#include "user_table.h"
#include "http_parser.h"

#define DEFAULT_PORT 80
#define SOCKET_RECEIVE_POLL_PERIOD 1
#define SOCKET_RECEIVER_BUFFER_SIZE 2048

gchar*
getSocketRemoteIP (GSocketConnection* conn, GError** error)
{
    return g_inet_address_to_string (
        G_INET_ADDRESS(
            g_inet_socket_address_get_address (
                G_INET_SOCKET_ADDRESS(
                    g_socket_connection_get_remote_address (
                        conn, error
                    )
                )
            )
        )
    );
}

guint16
getSocketRemotePort (GSocketConnection* conn, GError** error)
{
    return g_inet_socket_address_get_port (
                G_INET_SOCKET_ADDRESS(
                    g_socket_connection_get_remote_address (
                        conn, error
                    )
                )
            );
}

gboolean
inboundConnectionHeaderParsed (InboundConnection * user)
{
    return user->protocol == HTTP && strlen(user->destinationHost) > 0;
}

gboolean
sendInboundData (gpointer data)
{
    g_return_val_if_fail (data, FALSE);

    InboundConnection* user = (InboundConnection*) data;
    OutboundConnection* out = user->out;
    GSocket* socket = g_socket_connection_get_socket (user->connection);
    GError* error = NULL;
    gint64 now = g_get_monotonic_time (); // microseconds
    g_return_val_if_fail(user->lastSendTime >= 0, FALSE);
    gfloat timeDiffSeconds = (now - user->lastSendTime) / 1000000.0;
    gfloat maxBandwidth = user->maxBandwidthKbps;
    gsize maxBytes;
    gssize size;
    guint count = 0;
    guint bytesSent = 0;
    gchar* buffer;

    if (maxBandwidth < 0) {
        maxBandwidth = (out->responseBuffersNum) * 8*SOCKET_RECEIVER_BUFFER_SIZE / SOCKET_RECEIVE_POLL_PERIOD; // bytes / ms = kbps

    }
    g_return_val_if_fail (maxBandwidth >= 0, FALSE);
    if (maxBandwidth != 0) {
        //return TRUE;
        g_message ("Max bandwidth is %d kbps", (int) maxBandwidth);
    }

    maxBytes = ((maxBandwidth * 1000.0) / 8) * timeDiffSeconds;
    g_return_val_if_fail (maxBytes >= 0, FALSE);
    if (maxBytes != 0) {
        //return TRUE;
        g_message ("Will send %d bytes maximum", (int) maxBytes);
    }

    g_return_val_if_fail (socket, FALSE);

    //g_message ("Sent %d of %d response buffers", (int) out.responseBuffersSent, (int) out.responseBuffersNum);

    for (int i=out->responseBuffersSent;i<out->responseBuffersNum;i++)
    {
        buffer = out->responseBuffers[i];
        size = out->responseBuffersSize[i];

        g_return_val_if_fail (buffer, FALSE);
        g_return_val_if_fail (size, FALSE);

        // Skip this call if we can't send less than maxBytes
        if (size > maxBytes) {
            g_message ("Holding in %d bytes of data", (int) size);
            return TRUE;
        }

        g_socket_send (socket, buffer, size, NULL, &error);
        user->lastSendTime = g_get_monotonic_time();
        bytesSent += size;

        g_message ("Sent %d bytes to user", (int) size);

        // Allocated in receiveOutboundData
        g_free (buffer);

        out->responseBuffersSent++;
        // Clear/reset the response buffer vector if we're done
        // with all of them
        if (out->responseBuffersSent == out->responseBuffersNum) {
            memset (out->responseBuffers, 0, sizeof(out->responseBuffers));
            memset (out->responseBuffersSize, 0, sizeof(out->responseBuffersSize));
            out->responseBuffersSent = 0;
            out->responseBuffersNum = 0;
        }

        if (bytesSent >= maxBytes) {
            out->bytesWrittenCount += bytesSent;
            return TRUE;
        }
    }

    g_message ("Nothing sent");

    return TRUE;
}

gboolean
receiveOutboundData (gpointer data)
{
    g_return_val_if_fail (data, FALSE);

    InboundConnection* user = (InboundConnection*) data;
    OutboundConnection* out = user->out;
    GSocket* socket = g_socket_connection_get_socket (out->connection);
    gchar buffer[SOCKET_RECEIVER_BUFFER_SIZE+1];
    gssize size;
    GError* error = NULL;

    g_return_val_if_fail(socket, FALSE);

    size = g_socket_receive (socket,
                    buffer,
                    SOCKET_RECEIVER_BUFFER_SIZE,
                    NULL,
                    &error);

    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
            return TRUE;
        }
        g_warning ("Error receiving data from socket (%s): %s", user->destinationHost, error->message);
        g_object_unref(error);
        return FALSE;
    }

    if (size <= 0) {
        g_warning ("Fatal error reading from socket");
        return FALSE;
    }

    g_message ("Received %d bytes from socket (%s)", (int) size, user->destinationHost);

    g_return_val_if_fail (out->responseBuffersNum <= MAX_RESPONSE_BUFFERS, FALSE);
    if (out->responseBuffersNum == MAX_RESPONSE_BUFFERS) {
        g_warning ("Response buffer full, dropping data!!");
    }

    g_return_val_if_fail (out->responseBuffersNum < MAX_RESPONSE_BUFFERS, FALSE);

    // Free'd in sendInboundData()
    out->responseBuffers[out->responseBuffersNum] = (gchar*) g_malloc(size);

    out->responseBuffersSize[out->responseBuffersNum] = size;
    memcpy(out->responseBuffers[out->responseBuffersNum], buffer, size);
    out->responseBuffersNum++;

    g_message ("Inbound connection has written %d bytes", (int) user->bytesWrittenCount);

    if (user->bytesWrittenCount == 0) {
        g_message ("Resetting bitrate counter");
        user->lastSendTime = g_get_monotonic_time();
    }

    //buffer[size] = '\0';
    //printf("\n%s\n", buffer);

    return TRUE;
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
ensureOpenOutboundConnection (InboundConnection * user, gboolean *res)
{
    guint16 port = DEFAULT_PORT;
    GError *error = NULL;
    gchar *address = NULL;
    GSocket* socket;

    g_message ("ensureOpenOutboundConnection()");

    // We need to create the outbound connection if it's NULL
    if (user->out->connection) {
        return;
    }

    *res = FALSE;
    g_return_if_fail(user->destinationHost && strlen(user->destinationHost));

    g_message ("Connectiong to host ... : %s", user->destinationHost);

    //FIXME: Rather use g_socket_client_connect_to_uri ?
    user->out->connection = g_socket_client_connect_to_host (user->out->client, user->destinationHost, port, NULL, &error);

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

    g_free (address);

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
        g_timeout_add(SOCKET_RECEIVE_POLL_PERIOD, sendInboundData, user);
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
