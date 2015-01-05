#include <glib.h>
#include <gio/gio.h>

#ifndef __PROXY_API__
#define __PROXY_API__

typedef struct _BushpathProxyOptions {
	guint16 port;
	gchar* bindAddress;
} BushpathProxyOptions;

typedef BushpathProxyOptions BPOptions;

typedef struct _BushpathProxy {
    GMainContext *context;
    BushpathProxyOptions options;
} BushpathProxy;

BushpathProxy*
BushpathProxy_New (GMainContext *loop, BushpathProxyOptions options);

void
BushpathProxy_Destroy (BushpathProxy* proxy);

#endif
