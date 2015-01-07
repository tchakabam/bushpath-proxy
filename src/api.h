#include <glib.h>
#include <gio/gio.h>

#pragma once

typedef struct _BushpathProxyOptions {
	guint16 port;
	gchar* bindAddress;
} BushpathProxyOptions;

typedef BushpathProxyOptions BPOptions;

typedef struct _BushpathProxy {
    GMainContext *context;
    BushpathProxyOptions options;
} BushpathProxy;

BushpathProxyOptions
BushpathProxyOptions_New (gchar* bindAddress, guint16 port);

void
BushpathProxyOptions_Free (BushpathProxyOptions* options);

BushpathProxy*
BushpathProxy_New (GMainContext *context, BushpathProxyOptions options,
							int *argc, char **argv);

void
BushpathProxy_Free (BushpathProxy* proxy);

