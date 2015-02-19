#include "tcp_client.h"

#define READ_BUFFER_SIZE 4096

#define GST_CAT_DEFAULT bptcpclient_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define bptcpclient_parent_class parent_class
G_DEFINE_TYPE (BPTCPClient, bptcpclient, GST_TYPE_ELEMENT);

enum
{
  PROP_0,
  PROP_DESTINATION_ADDRESS,
  PROP_DESTINATION_PORT
};

static void bptcpclient_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void bptcpclient_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean bptcpclient_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static gboolean bptcpclient_src_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn bptcpclient_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

void
bp_tcp_client_setup (void)
{
  GST_DEBUG_CATEGORY_INIT (bptcpclient_debug, "bp_tcpclient", 0,
      "Bushpath TCP client");

  gst_element_register (NULL, "bp_tcpclient", GST_RANK_NONE,
      TYPE_BP_TCPCLIENT);
}

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

/* GObject vmethod implementations */

/* initialize the bp_tcpclient's class */
static void
bptcpclient_class_init (BPTCPClientClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = bptcpclient_set_property;
  gobject_class->get_property = bptcpclient_get_property;

  g_object_class_install_property (gobject_class, PROP_DESTINATION_ADDRESS,
      g_param_spec_string ("address", "Address", "Remote address",
          "", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_DESTINATION_PORT,
      g_param_spec_uint ("port", "Port", "Remote port",
          0, 65535, 0, G_PARAM_WRITABLE));

  gst_element_class_set_details_simple(gstelement_class,
    "BPTCPClient",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    " <<user@hostname.org>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
bptcpclient_init (BPTCPClient * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(bptcpclient_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(bptcpclient_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_event_function (filter->srcpad,
                              GST_DEBUG_FUNCPTR(bptcpclient_src_event));
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->client = g_socket_client_new();

  filter->connection = NULL;
  filter->channel = NULL;
  filter->connected = FALSE;

  filter->remoteAddress = g_string_new (NULL);
  filter->remotePort = 0;
}

gchar*
getSocketRemoteIP (GSocketConnection* conn, GError** error)
{
    return g_inet_address_to_string (
        G_INET_ADDRESS(
            g_inet_socket_address_get_address (
                G_INET_SOCKET_ADDRESS(
                    g_socket_connection_get_remote_address (
                        conn, error
                    )
                )
            )
        )
    );
}

guint16
getSocketRemotePort (GSocketConnection* conn, GError** error)
{
    return g_inet_socket_address_get_port (
                G_INET_SOCKET_ADDRESS(
                    g_socket_connection_get_remote_address (
                        conn, error
                    )
                )
            );
}

gboolean
BP_TCPClient_HangUp (GIOChannel *channel,
                                        GIOCondition idCond,
                                        gpointer user_data)
{
  return TRUE;
}

gboolean
BP_TCPClient_Error (GIOChannel *channel,
                                        GIOCondition idCond,
                                        gpointer user_data)
{
  return TRUE;
}

gboolean
BP_TCPClient_InvalidRequest (GIOChannel *channel,
                                        GIOCondition idCond,
                                        gpointer user_data)
{
  return TRUE;
}

gboolean
BP_TCPClient_Read (GIOChannel *channel,
                                        GIOCondition idCond,
                                        gpointer user_data)
{
  BPTCPClient *filter = BP_TCPCLIENT (user_data);
  gchar buffer[READ_BUFFER_SIZE];
  gsize bufferSize = READ_BUFFER_SIZE;
  gsize bytesRead = 0;
  GError *error = NULL;
  GIOStatus status;
  gboolean ret = TRUE;
  GstBuffer *gstBuffer;
  GstMapInfo bufferMap;

  g_assert (channel == filter->channel);

  status = g_io_channel_read_chars (channel, buffer, bufferSize, &bytesRead, &error);
  if (status != G_IO_STATUS_NORMAL) {
    g_warning ("Channel read status is %d", (int) status);
  }
  if (error != NULL) {
    g_warning ("Error reading from channel: %s", error->message);
    g_error_free (error);
    ret = FALSE;
  }

  g_message ("Read %d bytes", (int) bytesRead);

  switch (status) {
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

  return ret;
}

gboolean
BP_TCPClient_Write (BPTCPClient* filter, guint8* data, gsize size)
{
  GError* error = NULL;
  gsize bytesWritten = 0;
  GIOStatus status;
  gboolean ret = TRUE;

  status = g_io_channel_write_chars (filter->channel, (const gchar*) data, size, &bytesWritten, &error);
  if (status != G_IO_STATUS_NORMAL) {
    g_warning ("Channel write status is %d", (int) status);
  }
  if (error != NULL) {
    g_warning ("Error writing to channel: %s", error->message);
    g_error_free (error);
    ret = FALSE;
  }

  status = g_io_channel_flush (filter->channel, &error);
  if (status != G_IO_STATUS_NORMAL) {
    g_warning ("Channel flush status is %d", (int) status);
  }
  if (error != NULL) {
    g_warning ("Error flushing channel: %s", error->message);
    g_error_free (error);
    ret = FALSE;
  }

  g_message ("Wrote %d bytes", (int) bytesWritten);

  return TRUE;
}

GError*
BP_TCPClient_Connect (BPTCPClient* filter)
{
  GError *error = NULL;
  GSocket* socket;
  GIOChannel *channel;
  gchar* address;
  guint port;

  g_return_val_if_fail(filter->remoteAddress && filter->remoteAddress->str && strlen(filter->remoteAddress->str), error);
  g_return_val_if_fail(filter->remotePort > 0, error);
  g_return_val_if_fail(!filter->connected, error);

  g_message ("Connectiong to host ... : %s", filter->remoteAddress->str);

  //FIXME: Rather use g_socket_client_connect_to_uri
  filter->connection = g_socket_client_connect_to_host (filter->client, filter->remoteAddress->str, filter->remotePort, NULL, &error);

  g_message ("Validating connection ...");

  if (error) {
      g_warning ("Failed to create outbound connection to host %s: %s", filter->remoteAddress->str, error->message);
      return error;
  }
  g_return_val_if_fail (filter->connection, error);

  address = getSocketRemoteIP(filter->connection, &error);
  port = getSocketRemotePort(filter->connection, &error);

  g_message( "Connected to %s (%s) on port %d", filter->remoteAddress->str, address, (int) port);

  // Force some socket config
  socket = g_socket_connection_get_socket (filter->connection);
  g_return_val_if_fail (socket, error);
  g_object_set (socket, "blocking", FALSE, NULL);

  // Get IO channel
  filter->channel = channel = g_io_channel_unix_new(g_socket_get_fd(socket));
  filter->connected = TRUE;

  g_io_add_watch(channel, G_IO_HUP, BP_TCPClient_HangUp, filter);
  g_io_add_watch(channel, G_IO_ERR, BP_TCPClient_Error, filter);
  g_io_add_watch(channel, G_IO_NVAL, BP_TCPClient_InvalidRequest, filter);
  g_io_add_watch(channel, G_IO_IN, BP_TCPClient_Read, filter);

  g_free (address);

  return error;
}

static void
bptcpclient_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  //g_message ("bptcpclient_set_property");

  BPTCPClient *filter = BP_TCPCLIENT (object);

  switch (prop_id) {
    case PROP_DESTINATION_ADDRESS:
      g_string_assign (filter->remoteAddress, g_value_get_string(value));
      GST_DEBUG_OBJECT (filter, "Set destination address: %s", filter->remoteAddress->str);
      break;
    case PROP_DESTINATION_PORT:
      filter->remotePort = g_value_get_uint(value);
      GST_DEBUG_OBJECT (filter, "Set destination address: %u", (guint) filter->remotePort);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  if (!filter->connected && filter->remotePort > 0 && strlen(filter->remoteAddress->str)) {
    BP_TCPClient_Connect (filter);
  }
}

static void
bptcpclient_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  BPTCPClient *filter = BP_TCPCLIENT (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
bptcpclient_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  BPTCPClient *filter = BP_TCPCLIENT (parent);
  gboolean ret = TRUE;

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

static gboolean
bptcpclient_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  BPTCPClient *filter = BP_TCPCLIENT (parent);
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
bptcpclient_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  BPTCPClient *filter = BP_TCPCLIENT (parent);
  GstMapInfo bufferMap;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!filter->connected) {
    BP_TCPClient_Connect(filter);
  }

  g_return_val_if_fail(filter->connected, GST_FLOW_ERROR);

  gst_buffer_map (buf, &bufferMap, GST_MAP_READ);

  if (!BP_TCPClient_Write (filter, bufferMap.data, bufferMap.size)) {
    ret = GST_FLOW_ERROR;
  }

  gst_buffer_unmap (buf, &bufferMap);
  gst_buffer_unref (buf);

  return ret;
}



