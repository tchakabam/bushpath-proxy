#include "tcp_service.h"

#define GST_CAT_DEFAULT bptcpservice_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define bptcpservice_parent_class parent_class
G_DEFINE_TYPE (BPTCPService, bptcpservice, GST_TYPE_ELEMENT);

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_PORT,
  PROP_REMOTE_PORT,
  PROP_REMOTE_ADDRESS,
};

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

gboolean
BP_TCPService_IncomingConnection (GSocketService *service,
              GSocketConnection *incomingConnection,
              GObject *source_object,
              gpointer user_data);
gboolean
BP_TCPService_IncomingConnectionReceive (GIOChannel *sourceChannel,
            GIOCondition idCond,
            gpointer user_data);

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

  g_object_class_install_property (gobject_class, PROP_PORT,
	g_param_spec_uint ("port", "Port", "Port to listen at", 1, 65535, 6900, G_PARAM_WRITABLE)
  );

  g_object_class_install_property (gobject_class, PROP_ADDRESS,
	g_param_spec_string ("address", "Address", "Bind address", "127.0.0.1", G_PARAM_WRITABLE)
  );

  g_object_class_install_property (gobject_class, PROP_REMOTE_PORT,
	g_param_spec_uint ("remote-port", "Port", "Remote port", 0, 65535, 0, G_PARAM_READABLE)
  );

  g_object_class_install_property (gobject_class, PROP_REMOTE_ADDRESS,
	g_param_spec_string ("remote-address", "Address", "Remote address", "", G_PARAM_READABLE)
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
	gchar *url;

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->remotePort = 0;
  filter->remoteAddress = g_string_new(NULL);

  filter->port = 6900;
  filter->address = g_string_new(NULL);
  g_string_assign (filter->address, "127.0.0.1");

  filter->service = g_socket_service_new();

  // create network address
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

  g_object_get (filter->socketAddress, "port", &filter->port, NULL);
  url = g_inet_address_to_string (filter->inetAddress);

  g_message ("Local IP/Port: %s:%u", url, filter->port);

  g_free (url);
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

gboolean
BP_TCPService_IncomingConnection (GSocketService *service,
              GSocketConnection *incomingConnection,
              GObject *source_object,
              gpointer user_data)
{
  BPTCPService *filter = BP_TCPSERVICE (user_data);

  GSocket *socket = g_socket_connection_get_socket(incomingConnection);
  GSocketAddress *sockaddr = g_socket_connection_get_remote_address(incomingConnection, NULL);
  GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(sockaddr));
  guint16 port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(sockaddr));

  gint fd;
  GIOChannel *channel;

  g_return_val_if_fail (socket, FALSE);
  g_object_ref (incomingConnection);

  g_string_assign(filter->remoteAddress, g_inet_address_to_string(addr));
  filter->remotePort = port;

  g_message ("New connection from %s:%d", filter->remoteAddress->str, (int) port);
  g_message ("Attaching IO channel watch ...");

  fd = g_socket_get_fd(socket);
  channel = g_io_channel_unix_new(fd);
  g_return_val_if_fail (channel, FALSE);

  // Add IO watch with pointer to connection handle as user data for callback
  g_io_add_watch(channel, G_IO_IN, (GIOFunc) BP_TCPService_IncomingConnectionReceive, filter);
  g_io_channel_unref (channel);

  g_message ("IO channel watch attached.");

  // We only want to accept one connection for now
	return FALSE;
}

gboolean
BP_TCPService_IncomingConnectionReceive (GIOChannel *channel,
														            GIOCondition idCond,
														            gpointer user_data)
{
  BPTCPService *filter = BP_TCPSERVICE (user_data);
  gboolean res = TRUE;
  gsize bufferSize = 4096;
  gchar* data = (gchar*) g_malloc (bufferSize);
  gsize bytesRead = 0;
  GError* error = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstBuffer *buffer;
  GstMapInfo bufferMap;
  //GString* tmpBuffer = g_string_new (NULL);

  GIOStatus readStatus = g_io_channel_read_chars (channel, data, bufferSize, &bytesRead, &error);

  switch (readStatus) {
  case G_IO_STATUS_NORMAL:
  case G_IO_STATUS_EOF:
  	buffer = gst_buffer_new_and_alloc (bytesRead);
  	if (!gst_buffer_map(buffer, &bufferMap, GST_MAP_WRITE)) {
  		g_error ("Memory allocation error !!");
  		return FALSE;
  	}

    //g_string_append_len(tmpBuffer, data, bytesRead);
  	memcpy (bufferMap.data, data, bytesRead);

  	gst_buffer_unmap(buffer, &bufferMap);
  	ret = gst_pad_push (filter->srcpad, buffer);

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