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

typedef struct _BP_Connection {
    GSocketClient* client;
    GSocketConnection *inConnection;
    GSocketConnection *outConnection;
} BP_Connection;

//typedef InboundConnection* UserTable[MAX_INBOUND_CONNECTIONS];

BP_Connection*
newConnection (GSocketClient *client, GSocketConnection* in, GSocketConnection* out)
{
    BP_Connection *c = g_new (BP_Connection, 1);
    c->client = client;
    c->inConnection = in;
    c->outConnection = out;
    return c;
}

void
freeConnection (BP_Connection* connection)
{
    g_free (connection);
}

#endif