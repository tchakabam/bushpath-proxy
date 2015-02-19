#include "tcp_service.h"

#define READ_BUFFER_SIZE 4096

#define GST_CAT_DEFAULT bptcpservice_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define bptcpservice_parent_class parent_class
G_DEFINE_TYPE (BPTCPService, bptcpservice, GST_TYPE_ELEMENT);

typedef struct _Connection {
  GSocketService *service;
  GSocketConnection *incomingConnection;
} Connection;

static GData *connectionPool = NULL;

static void
ServerPool_Init ()
{
  g_datalist_init (&connectionPool);
}

static void
ServerPool_AddInstance (GQuark uid,
              GSocketService *service,
              GSocketConnection *incomingConnection)
{
  Connection *handle = g_slice_new0 (Connection);
  handle->service = service;
  handle->incomingConnection = incomingConnection;
  g_datalist_id_set_data(&connectionPool, uid, handle);
}

// Element signals
enum
{
  SIG_CONNECTED,
  /* add more above */
  SIG_COUNT
};

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_PORT,
  PROP_REMOTE_PORT,
  PROP_REMOTE_ADDRESS,
  PROP_MODE,
  PROP_READY,
};

static guint signals[SIG_COUNT] = { 0 };

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

static void bptcpservice_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void bptcpservice_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstFlowReturn bptcpservice_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

static GstFlowReturn
BP_TCPService_Chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

static gboolean
BP_TCPService_IncomingConnection (GSocketService *service,
            GSocketConnection *incomingConnection,
            GObject *source_object,
            gpointer user_data);
static gboolean
BP_TCPService_IncomingConnectionReceive (GIOChannel *sourceChannel,
            GIOCondition idCond,
            gpointer user_data);

static void
BP_TCPService_BindListener (BPTCPService *filter);

void
bp_tcp_service_setup (void)
{
  GST_DEBUG_CATEGORY_INIT (bptcpservice_debug, "bp_tcpservice", 0,
      "Bushpath TCP service");

  gst_element_register (NULL, "bp_tcpservice", GST_RANK_NONE,
      TYPE_BP_TCPSERVICE);
}

/* initialize the bp_tcpservice's class */
static void
bptcpservice_class_init (BPTCPServiceClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = bptcpservice_set_property;
  gobject_class->get_property = bptcpservice_get_property;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));

  gst_element_class_set_static_metadata (gstelement_class,
      "BP TCP Service", "Source/Network",
      "BP TCP Service",
      "Stephan Hesse <disparat@gmail.com");

  g_object_class_install_property (gobject_class, PROP_MODE,
  g_param_spec_int ("mode", "Mode", "Operation mode (use sink and/or source)", 0, 2, 0, G_PARAM_WRITABLE)
  );

  g_object_class_install_property (gobject_class, PROP_READY,
  g_param_spec_boolean ("ready", "Ready", "Ready flag", FALSE, G_PARAM_WRITABLE)
  );

  g_object_class_install_property (gobject_class, PROP_PORT,
	g_param_spec_uint ("port", "Port", "Port to listen at", 1, 65535, 6900, G_PARAM_READWRITE)
  );

  g_object_class_install_property (gobject_class, PROP_ADDRESS,
	g_param_spec_string ("address", "Address", "Bind address", "127.0.0.1", G_PARAM_READWRITE)
  );

  g_object_class_install_property (gobject_class, PROP_REMOTE_PORT,
	g_param_spec_uint ("remote-port", "Port", "Remote port", 0, 65535, 0, G_PARAM_READWRITE)
  );

  g_object_class_install_property (gobject_class, PROP_REMOTE_ADDRESS,
	g_param_spec_string ("remote-address", "Address", "Remote address", "", G_PARAM_READWRITE)
  );

  // Signals
  signals[SIG_CONNECTED] =
      g_signal_new (
        "connected",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (BPTCPServiceClass, connected),
        NULL, NULL,
        g_cclosure_marshal_generic, G_TYPE_NONE,
        0
      );
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
bptcpservice_init (BPTCPService * filter)
{
  if (!connectionPool) {
    ServerPool_Init ();
  }

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(bptcpservice_chain));

  filter->mode = BP_MODE_SINK_AND_SRC; // default

  filter->remotePort = 0;
  filter->remoteAddress = g_string_new(NULL);

  filter->port = 6900;
  filter->address = g_string_new(NULL);
  g_string_assign (filter->address, "127.0.0.1");

  filter->service = NULL;
  filter->inetAddress = NULL;
  filter->socketAddress = NULL;
  filter->ready = FALSE;
  filter->init = FALSE;
}

static void
bptcpservice_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  BPTCPService *filter = BP_TCPSERVICE (object);

  switch (prop_id) {
    case PROP_PORT:
      filter->port = g_value_get_uint (value);
      break;
    case PROP_ADDRESS:
      g_string_assign (filter->address, g_value_get_string (value));
      break;
    case PROP_REMOTE_PORT:
      if (filter->mode != BP_MODE_SINK_ONLY) {
        GST_WARNING ("Attempt to set remote port while not in sink-only mode");
        return;
      }
      filter->remotePort = g_value_get_uint (value);
      break;
    case PROP_REMOTE_ADDRESS:
      if (filter->mode != BP_MODE_SINK_ONLY) {
        GST_WARNING ("Attempt to set remote address while not in sink-only mode");
        return;
      }
      g_string_assign (filter->remoteAddress, g_value_get_string (value));
      break;
    case PROP_MODE:
      filter->mode = g_value_get_int (value);
      break;
    case PROP_READY: {
      filter->ready = g_value_get_boolean (value);
      if (filter->ready && !filter->init && filter->mode != BP_MODE_SINK_ONLY) {
        BP_TCPService_BindListener (filter);
      }
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
bptcpservice_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  BPTCPService *filter = BP_TCPSERVICE (object);

  switch (prop_id) {
    case PROP_PORT:
      g_value_set_uint (value, filter->port);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, filter->address->str);
      break;
  	case PROP_REMOTE_ADDRESS:
  		g_value_set_string (value, filter->remoteAddress->str);
  		break;
  	case PROP_REMOTE_PORT:
  		g_value_set_uint (value, filter->remotePort);
  		break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
bptcpservice_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret;
  BPTCPService *filter;

  filter = BP_TCPSERVICE (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

static GstFlowReturn
bptcpservice_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  return BP_TCPService_Chain (pad, parent, buf);
}

static void
BP_TCPService_UpdateUID (BPTCPService *filter)
{
  GString* uid = g_string_new (NULL);
  g_string_printf (uid, "%s:%d<->%s:%d", filter->address->str, filter->port,
                                          filter->remoteAddress->str, filter->remotePort);
  g_message ("Setting connection UID");
  filter->uid = g_quark_from_string (uid->str);
  g_string_free (uid, TRUE);
}

static void
BP_TCPService_ValidateBind (BPTCPService *filter)
{
  guint port;
  g_object_get (filter->socketAddress, "port", &port, NULL);
  if (port != filter->port) {
    g_warning ("Port used by server is not port that was requested for bind!");
    filter->port = port;
  }

  gchar* url = g_inet_address_to_string (filter->inetAddress);
  if (strcmp(url, filter->address->str)) {
    g_warning ("Address used by server is not address that was requested for bind!");
    g_string_assign(filter->address, url);
  }
  g_free (url);
}

static void
BP_TCPService_BindListener (BPTCPService *filter)
{
  // Don't bind if we're sink-only
  g_return_if_fail (filter->ready && !filter->init && filter->mode != BP_MODE_SINK_ONLY);

  filter->service = g_socket_service_new();
  filter->inetAddress = g_inet_address_new_from_string(filter->address->str);
  filter->socketAddress = g_inet_socket_address_new(filter->inetAddress, filter->port);

  // add server socket to service and attach listener
  g_socket_listener_add_address(G_SOCKET_LISTENER(filter->service),
                                                  filter->socketAddress,
                                                  G_SOCKET_TYPE_STREAM,
                                                  G_SOCKET_PROTOCOL_TCP,
                                                  NULL, NULL, NULL);

  g_message ("Starting IO service...");

  // Connect socket to IO service
  g_socket_service_start(filter->service);

  g_message ("IO service is up");
  g_message ("Attaching signals ...");

  // Pass proxy instance as user data
  g_signal_connect(filter->service, "incoming", G_CALLBACK(BP_TCPService_IncomingConnection), filter);

  g_message ("Attached!");

  BP_TCPService_ValidateBind (filter);

  g_message ("Local IP/Port: %s:%u", filter->address->str, filter->port);

  filter->init = TRUE;
}

static gboolean
BP_TCPService_IncomingConnection (GSocketService *service,
              GSocketConnection *incomingConnection,
              GObject *source_object,
              gpointer user_data)
{
  BPTCPService *filter = BP_TCPSERVICE (user_data);

  GSocket *socket = g_socket_connection_get_socket(incomingConnection);
  GSocketAddress *sockaddr = g_socket_connection_get_remote_address(incomingConnection, NULL);
  GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(sockaddr));
  gchar* remoteHost = g_inet_address_to_string(addr);
  gint fd = g_socket_get_fd(socket);
  GIOChannel *channel = g_io_channel_unix_new(fd);

  if (filter->mode == BP_MODE_SINK_ONLY) {
    GST_ERROR ("Attempt to accept connection but in sink-only mode");
    return FALSE;
  }

  // Ref connection
  g_object_ref (incomingConnection);

  // Get remote host & port
  filter->remotePort = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(sockaddr));
  g_string_assign(filter->remoteAddress, remoteHost);
  g_free (remoteHost);

  g_message ("New connection from %s:%d", filter->remoteAddress->str, (int) filter->remotePort);
  g_message ("Attaching IO channel watch ...");

  // Add IO watch with pointer to connection handle as user data for callback
  g_io_add_watch(channel, G_IO_IN, (GIOFunc) BP_TCPService_IncomingConnectionReceive, filter);
  g_io_channel_unref (channel);

  g_message ("Registering connection");

  BP_TCPService_UpdateUID (filter);

  ServerPool_AddInstance (filter->uid, filter->service, incomingConnection);

	return FALSE;
}

static gboolean
BP_TCPService_IncomingConnectionReceive (GIOChannel *channel,
														            GIOCondition idCond,
														            gpointer user_data)
{

  BPTCPService *filter = BP_TCPSERVICE (user_data);
  gboolean res = TRUE;
  gsize bufferSize = READ_BUFFER_SIZE;
  gchar buffer[READ_BUFFER_SIZE];
  gsize bytesRead = 0;
  GError* error = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstBuffer *gstBuffer;
  GstMapInfo bufferMap;
  GIOStatus readStatus;
  //GString* tmpBuffer = g_string_new (NULL);

  if (filter->mode == BP_MODE_SINK_ONLY) {
    GST_ERROR ("Attempt to read data but in sink-only mode");
    return FALSE;
  }

  readStatus = g_io_channel_read_chars (channel, buffer, bufferSize, &bytesRead, &error);

  switch (readStatus) {
  case G_IO_STATUS_NORMAL:
  case G_IO_STATUS_EOF:
  	gstBuffer = gst_buffer_new_and_alloc (bytesRead);
  	if (!gst_buffer_map(gstBuffer, &bufferMap, GST_MAP_WRITE)) {
  		g_error ("Memory allocation error !!");
  		return FALSE;
  	}

  	memcpy (bufferMap.data, buffer, bytesRead);

  	gst_buffer_unmap(gstBuffer, &bufferMap);
  	ret = gst_pad_push (filter->srcpad, gstBuffer);

  	if (ret != GST_FLOW_OK) {
  		g_warning ("Flow return was %d", (int) ret);
  	}

  	break;
  case G_IO_STATUS_AGAIN:
  case G_IO_STATUS_ERROR:
  	break;
  }

  return TRUE;
}

static GstFlowReturn
BP_TCPService_Chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  BPTCPService *filter = BP_TCPSERVICE (parent);
  Connection *conn;

  if (filter->mode == BP_MODE_SRC_ONLY) {
    GST_ERROR ("Attempt to write to sink pad but in source-only mode");
    return GST_FLOW_ERROR;
  }

  BP_TCPService_UpdateUID (filter);

  conn = g_datalist_id_get_data (&connectionPool, filter->uid);

  return GST_FLOW_OK;
}