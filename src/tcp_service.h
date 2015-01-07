#pragma once

#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define TYPE_BP_TCPSERVICE \
  (bptcpservice_get_type())
#define BP_TCPSERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),TYPE_BP_TCPSERVICE,BPTCPService))
#define BP_TCPSERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),TYPE_BP_TCPSERVICE,BPTCPServiceClass))
#define IS_BP_TCPSERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),TYPE_BP_TCPSERVICE))
#define IS_BP_TCPSERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),TYPE_BP_TCPSERVICE))

typedef struct _BPTCPService      BPTCPService;
typedef struct _BPTCPServiceClass BPTCPServiceClass;

struct _BPTCPService
{
  GstElement element;

  GstPad *srcpad;

  guint port;
  GString *address;

 	guint remotePort;
 	GString *remoteAddress;

	GInetAddress *inetAddress;
	GSocketAddress *socketAddress;
	GSocketService *service;
};

struct _BPTCPServiceClass
{
  GstElementClass parent_class;
};

GType bptcpservice_get_type (void);

void bp_tcp_service_setup (void);

G_END_DECLS

