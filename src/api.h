#include <glib.h>

#ifndef __PROXY_API__
#define __PROXY_API__

typedef struct _BushpathProxy BushpathProxy;

BushpathProxy*
BushpathProxy_New (GMainLoop *loop);

void
BushpathProxy_Destroy (BushpathProxy* proxy);

#endif
