/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#ifndef __PROXY_CONNECTION__
#define __PROXY_CONNECTION__

#include <glib.h>
#include <gio/gio.h>

#include <stdio.h>
#include <string.h>

#define MAX_INBOUND_CONNECTIONS 512
#define MAX_REQUEST_HEADER_SIZE 2048
#define MAX_HOSTNAME_LENGTH 128
#define MAX_RESPONSE_BUFFERS 16384

typedef enum _Protocol {
    HTTP,
    UNDEFINED,
} Protocol;

/*
typedef struct _Buffer {
    glong sequenceNumber;
} Buffer;
*/

typedef struct _OutboundConnection {
    GSocketClient* client;
    GSocketConnection *connection;
    GIOChannel *channel;
    guint bytesWrittenCount;
    gchar* responseBuffers[MAX_RESPONSE_BUFFERS];
    gsize responseBuffersSize[MAX_RESPONSE_BUFFERS];
    guint responseBuffersNum;
    guint responseBuffersSent;
} OutboundConnection;

typedef struct _InboundConnection {
    GMutex mutex;
    GSocketConnection *connection;
    GIOChannel *channel;
    guint lineReadCount;
    Protocol protocol;
    gchar destinationHost[MAX_HOSTNAME_LENGTH];
    gchar requestHeaderBuffer[MAX_REQUEST_HEADER_SIZE+1]; // We want to keep the last byte in the buffer zero to use as a string terminator
    gsize requestHeaderOffset;
    OutboundConnection* out;
    guint bytesWrittenCount;
    gfloat maxBandwidthKbps;
    gint minDelayMs;
    gint64 lastSendTime;
} InboundConnection;

//typedef InboundConnection* UserTable[MAX_INBOUND_CONNECTIONS];

OutboundConnection*
newOutboundConnection (GSocketClient *client)
{
    OutboundConnection *out = (OutboundConnection*) g_malloc (sizeof(OutboundConnection));
    out->client = client;
    out->bytesWrittenCount = 0;
    out->channel = NULL;
    out->connection = NULL;
    memset (out->responseBuffers, 0, sizeof(out->responseBuffers));
    memset (out->responseBuffersSize, 0, sizeof(out->responseBuffersSize));
    out->responseBuffersNum = 0;
    out->responseBuffersSent = 0;
    return out;
}

InboundConnection*
newInboundConnection (GSocketConnection* connection, GIOChannel *channel)
{
    InboundConnection* user = (InboundConnection*) g_malloc (sizeof(InboundConnection));
    g_mutex_init (&user->mutex);
    user->out = NULL;
    user->connection = connection;
    user->channel = channel;
    user->lineReadCount = 0;
    user->protocol = UNDEFINED;
    //user.destinationHost[0] = '\0';
    memset (user->destinationHost, 0, MAX_HOSTNAME_LENGTH);
    memset (user->requestHeaderBuffer, 0, sizeof(user->requestHeaderBuffer));
    user->requestHeaderOffset = 0;
    user->maxBandwidthKbps = -1;
    user->minDelayMs = -1;
    user->lastSendTime = -1;
    user->bytesWrittenCount = 0;
    return user;
}

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

#endif