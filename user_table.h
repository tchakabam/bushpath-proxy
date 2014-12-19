#ifndef __PROXY_USER_TABLE__
#define __PROXY_USER_TABLE__

#include <glib.h>
#include <gio/gio.h>

#include <stdio.h>
#include <string.h>

#define MAX_INBOUND_CONNECTIONS 512
#define MAX_REQUEST_HEADER_SIZE 2048
#define MAX_HOSTNAME_LENGTH 128
#define MAX_RESPONSE_BUFFERS 16384
#define USER_TABLE_LENGTH sizeof(UserTable)/sizeof(InboundConnection)
#define ITERATE_ON_USER_TABLE for(int i=0;i<USER_TABLE_LENGTH;i++)

typedef enum _Protocol {
    HTTP,
    UNDEFINED,
} Protocol;

typedef struct _Packet {

} Packet;

typedef struct _OutboundConnection {
    GSocketClient* client;
    GSocketConnection *connection;
    GIOChannel *destChannel;
    guint bytesWrittenCount;
    gchar* responseBuffers[MAX_RESPONSE_BUFFERS];
    gsize responseBuffersSize[MAX_RESPONSE_BUFFERS];
    guint responseBuffersNum;
    guint responseBuffersSent;
} OutboundConnection;

typedef struct _InboundConnection {
    GSocketConnection *connection;
    GIOChannel *sourceChannel;
    gboolean enabled;
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

typedef InboundConnection UserTable[MAX_INBOUND_CONNECTIONS];

OutboundConnection*
newOutboundConnection (GSocketClient *client)
{
    OutboundConnection *out = (OutboundConnection*) g_malloc (sizeof(OutboundConnection));
    out->client = client;
    out->bytesWrittenCount = 0;
    out->destChannel = NULL;
    out->connection = NULL;
    memset (out->responseBuffers, 0, sizeof(out->responseBuffers));
    memset (out->responseBuffersSize, 0, sizeof(out->responseBuffersSize));
    out->responseBuffersNum = 0;
    out->responseBuffersSent = 0;
    return out;
}

InboundConnection
newUser (gboolean enabled)
{
    InboundConnection user;
    user.out = NULL;
    user.connection = NULL;
    user.sourceChannel = NULL;
    user.enabled = enabled;
    user.lineReadCount = 0;
    user.protocol = UNDEFINED;
    //user.destinationHost[0] = '\0';
    memset (user.destinationHost, 0, MAX_HOSTNAME_LENGTH);
    memset (user.requestHeaderBuffer, 0, sizeof(user.requestHeaderBuffer));
    user.requestHeaderOffset = 0;
    user.maxBandwidthKbps = 32;
    user.minDelayMs = -1;
    user.lastSendTime = -1;
    user.bytesWrittenCount = 0;
    return user;
}

InboundConnection *
findUser (UserTable users, GSocketConnection *connection, GIOChannel *sourceChannel)
{
    ITERATE_ON_USER_TABLE
    {
        if (users[i].connection == connection && users[i].sourceChannel == sourceChannel)
        {
            return &users[i];
        }
    }
    return NULL;
}

gboolean
addUser (UserTable users, InboundConnection user)
{
    ITERATE_ON_USER_TABLE
    {
        if (!users[i].enabled) {
            users[i] = user;
            return TRUE;
        }
    }

    g_warning ("No more space for user connection data");

    return FALSE;
}

void
resetUserTable (UserTable users)
{
    InboundConnection nullUser = newUser (FALSE);
    ITERATE_ON_USER_TABLE
    {
        users[i] = nullUser;
    }
}

#endif