#include "throttler.h"
#include "throttler_buffer.h"

#define GST_CAT_DEFAULT bpthrottler_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define bpthrottler_parent_class parent_class
G_DEFINE_TYPE (BPThrottler, bpthrottler, GST_TYPE_ELEMENT);

#define GetMonotonicTimeMs() g_get_monotonic_time ()/1000

enum
{
  PROP_0,
  PROP_BANDWIDTH,
  PROP_DELAY,
};

static void bpthrottler_set_property (GObject * object, guint prop_id,
                                      const GValue * value, GParamSpec * pspec);
static void bpthrottler_get_property (GObject * object, guint prop_id,
                                      GValue * value, GParamSpec * pspec);

static gboolean bpthrottler_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn bpthrottler_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

gboolean BP_Throttler_ThreadNotifier (gpointer data);
gpointer BP_Throttler_Thread (gpointer data);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
                                                            GST_PAD_SINK,
                                                            GST_PAD_ALWAYS,
                                                            GST_STATIC_CAPS ("ANY")
                                                            );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
                                                            GST_PAD_SRC,
                                                            GST_PAD_ALWAYS,
                                                            GST_STATIC_CAPS ("ANY")
                                                            );

void
bp_throttler_setup (void)
{
  GST_DEBUG_CATEGORY_INIT (bpthrottler_debug, "bp_throttler", 0,
      "Bushpath data link throttler");

  gst_element_register (NULL, "bp_throttler", GST_RANK_NONE,
      TYPE_BP_THROTTLER);
}

/* initialize the bp_throttler's class */
static void
bpthrottler_class_init (BPThrottlerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = bpthrottler_set_property;
  gobject_class->get_property = bpthrottler_get_property;

  g_object_class_install_property (gobject_class, PROP_BANDWIDTH,
    g_param_spec_int ("bandwidth", "Bandwidth", "Maximum bandwidth in bits/second", 0, INT_MAX, INT_MAX, G_PARAM_READWRITE)
   );

  g_object_class_install_property (
  	gobject_class,
  	PROP_DELAY,
    g_param_spec_int ("delay", "Delay", "Delay in milliseconds", 0, INT_MAX, 0, G_PARAM_READWRITE)
   );

  gst_element_class_set_details_simple(gstelement_class,
    "BPThrottler",
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
bpthrottler_init (BPThrottler * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(bpthrottler_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(bpthrottler_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->bandwidth = INT_MAX;
  filter->delayMs = 0;

  filter->lastPushTimeMs = GetMonotonicTimeMs();

  filter->oldestBufferTimeMs = -1;
  filter->newestBufferTimeMs = -1;
  filter->currentTimeMs = -1;
  filter->queue = g_queue_new();

  g_mutex_init (&filter->mutex);
  g_cond_init (&filter->cond);

  filter->running = TRUE;
  filter->waiting = FALSE;
  filter->scheduled = FALSE;
  filter->thread = g_thread_new ("BP_Throttler", BP_Throttler_Thread, filter);
}

static void
bpthrottler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  BPThrottler *filter = BP_THROTTLER (object);

  g_mutex_lock(&filter->mutex);

  switch (prop_id) {
    case PROP_BANDWIDTH:
      filter->bandwidth = (gint64) g_value_get_int (value);
      break;
    case PROP_DELAY:
      filter->delayMs = (gint64) g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  // If we change any parameter we re-schedule the worker immediately
  g_timeout_add(0, BP_Throttler_ThreadNotifier, filter);

  g_mutex_unlock(&filter->mutex);
}

static void
bpthrottler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  BPThrottler *filter = BP_THROTTLER (object);

  g_mutex_lock(&filter->mutex);

  switch (prop_id) {
    case PROP_BANDWIDTH:
      g_value_set_float (value, filter->bandwidth);
      break;
    case PROP_DELAY:
      g_value_set_float (value, filter->delayMs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock(&filter->mutex);
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
bpthrottler_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret;
  BPThrottler *filter;

  filter = BP_THROTTLER (parent);

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

gboolean BP_Throttler_ThreadNotifier (gpointer data);

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
bpthrottler_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  BPThrottler *filter = BP_THROTTLER (parent);
  gint64 nowMs = GetMonotonicTimeMs();

  g_mutex_lock(&filter->mutex);

  GST_DEBUG ("Got %" GST_PTR_FORMAT, buf);

  // enqueue the buffer
  g_queue_push_tail (
  	filter->queue,
  	newBPThrottlerBuffer(buf, nowMs)
  );

  filter->newestBufferTimeMs = nowMs;

  if (filter->waiting && !filter->scheduled) {
    g_timeout_add(0, BP_Throttler_ThreadNotifier, filter);
  }

  g_mutex_unlock(&filter->mutex);

  return GST_FLOW_OK;
}

void BP_Throttler_ThreadLoop (gpointer data, gpointer user_data)
{
  // data is the queue element we are operating on in this loop iteration
  BPThrottlerBuffer *buffer = (BPThrottlerBuffer*) data;
  BPThrottler *filter = BP_THROTTLER (user_data);
  gint64 nowMs = GetMonotonicTimeMs();
  gsize bytes;
  gsize maxBytes;
  GstFlowReturn ret;

  GST_DEBUG ("Thread loop run");

  // We skip all because a previous was too new (we can't push stuff out-of-order)
  // (blocks the foreach-loop here, kind off acts like a break in a while)
  if (filter->currentTimeMs == -1) {
  	return;
  }

  // Set oldest buffer timestamp that we considered
	filter->oldestBufferTimeMs = buffer->arrivalTimeMs;

  // Buffer is still too new to be sent out
  if (buffer->arrivalTimeMs + filter->delayMs > filter->currentTimeMs) {
  	filter->currentTimeMs = -1;
  	return;
  }

  GST_DEBUG ("Got %" GST_PTR_FORMAT, buffer->data);

  // Constrain source bandwidth (and skip anything after on this queue run if we exceed it for now)
  bytes = gst_buffer_get_size (buffer->data);
  maxBytes = (filter->bandwidth / 8.0f) * ((nowMs - filter->lastPushTimeMs) / 1000.0f);
  if (bytes > maxBytes) {
    filter->currentTimeMs = -1;
    GST_DEBUG ("Holding data in (max %d bytes)", (int) maxBytes);
    return;
  }

  GST_DEBUG ("Pushing %d bytes of data through (max is %d bytes) ...", (int) bytes, (int) maxBytes);

  // Push data on the source pad
  ret = gst_pad_push (filter->srcpad, buffer->data);
  if (ret != GST_FLOW_OK) {
  	GST_WARNING ("Flow return value is: %s", gst_flow_get_name (ret));
    gst_buffer_ref (buffer->data);
    return;
  }

  // Set last push time
  filter->lastPushTimeMs = GetMonotonicTimeMs();

  GST_DEBUG ("Push time: %ld", (long) filter->lastPushTimeMs);

  // Element we pop off here should always be the head element (we can't push stuff out-of-order)
  g_assert ( buffer == g_queue_pop_head (filter->queue) );

  freeBPThrottlerBuffer (buffer);
}

gboolean BP_Throttler_ThreadNotifier (gpointer data)
{
  BPThrottler *filter = BP_THROTTLER (data);
  g_mutex_lock(&filter->mutex);
  filter->scheduled = FALSE;
  g_cond_signal (&filter->cond);
  g_mutex_unlock(&filter->mutex);
  return FALSE;
}

gpointer BP_Throttler_Thread (gpointer data)
{
  BPThrottler *filter = BP_THROTTLER (data);
  gint64 timeoutMs;

  g_mutex_lock(&filter->mutex);

  GST_DEBUG ("Throttler thread running ...");

  while (filter->running) {
  	// Note: g_cond_wait takes care of lockinng/unlocking mutex
  	//g_mutex_unlock(&filter->mutex);
  	// wait until we are triggered
    filter->waiting = TRUE;
    GST_DEBUG ("Throttler waiting ...");
  	g_cond_wait (&filter->cond, &filter->mutex);
    filter->waiting = FALSE;
  	//g_mutex_lock(&filter->mutex);

  	// set latest loop run timestamp
  	filter->currentTimeMs = GetMonotonicTimeMs();

    GST_DEBUG ("Running at %ld seconds ...", (long) filter->currentTimeMs / 1000);

  	// jump back if the queue is empty
  	if (g_queue_is_empty (filter->queue)) {
      GST_DEBUG ("Queue empty");
  		continue;
  	}

  	// run thread loop
  	g_queue_foreach(filter->queue, BP_Throttler_ThreadLoop, filter);

    // set timeout for next run
    g_assert (filter->oldestBufferTimeMs != -1);
    // real delay = time in the future when to send - current time = when the packet arrived + applied delay - current time
    timeoutMs = (filter->oldestBufferTimeMs + filter->delayMs) - GetMonotonicTimeMs();
    // we're late, let's do it now!
    if (timeoutMs < 0) {
      timeoutMs = 0;
    }

    GST_DEBUG ("Timeout: %ld ms", (long) timeoutMs);

  	// we want to be triggered again when the next buffer needs to be pushed out
  	g_timeout_add((guint) timeoutMs, BP_Throttler_ThreadNotifier, filter);
    filter->scheduled = TRUE;
  }

  g_mutex_unlock(&filter->mutex);

  return NULL;
}
