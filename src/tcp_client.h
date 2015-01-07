#pragma once

#include <string.h>

#include <gio/gio.h>
#include <glib.h>
#include <gst/gst.h>


G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define TYPE_BP_TCPCLIENT \
  (bptcpclient_get_type())
#define BP_TCPCLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),TYPE_BP_TCPCLIENT,BPTCPClient))
#define BP_TCPCLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),TYPE_BP_TCPCLIENT,BPTCPClientClass))
#define IS_BP_TCPCLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),TYPE_BP_TCPCLIENT))
#define IS_BP_TCPCLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),TYPE_BP_TCPCLIENT))

typedef struct _BPTCPClient      BPTCPClient;
typedef struct _BPTCPClientClass BPTCPClientClass;

struct _BPTCPClient
{
  GstElement element;

  GString* remoteAddress;
  guint remotePort;

  GstPad *sinkpad;

  GSocketClient *client;

  GSocketConnection *connection;
  GIOChannel *channel;
  gboolean connected;
};

struct _BPTCPClientClass
{
  GstElementClass parent_class;
};

GType
bptcpclient_get_type (void);

void
bp_tcp_client_setup (void);

G_END_DECLS