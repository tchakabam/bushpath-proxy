/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#ifndef __PROXY_TRANSPORT__
#define __PROXY_TRANSPORT__

#include <glib.h>
#include <gio/gio.h>

#include "connection.h"

#define SOCKET_RECEIVE_POLL_PERIOD 1
#define SOCKET_RECEIVER_BUFFER_SIZE 2048

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
        g_error_free(error);
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

#endif
