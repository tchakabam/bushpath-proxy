/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#include <glib.h>
#include <gio/gio.h>

#include "user_table.h"

#ifndef __PROXY_PROXY__
#define __PROXY_PROXY__

typedef struct _BushpathProxy {
    guint listenPort;
    gchar *listenHost;
    GInetAddress *inetAddress;
    GSocketAddress *socketAddress;
    GSocketService *service;
    GSocketClient *client;
    GMainLoop *loop;
    UserTable users;
} BushpathProxy;

#endif
