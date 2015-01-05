#pragma once

 #ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define TYPE_BP_THROTTLER \
  (bpthrottler_get_type())
#define BP_THROTTLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),TYPE_BP_THROTTLER,BPThrottler))
#define BP_THROTTLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),TYPE_BP_THROTTLER,BPThrottlerClass))
#define IS_BP_THROTTLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),TYPE_BP_THROTTLER))
#define IS_BP_THROTTLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),TYPE_BP_THROTTLER))

typedef struct _BPThrottler      BPThrottler;
typedef struct _BPThrottlerClass BPThrottlerClass;

struct _BPThrottler
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gfloat bandwidth;
  gfloat delayMs;

  gfloat currentTimeMs;
  gfloat newestBufferTimeMs;
  gfloat oldestBufferTimeMs;

  gfloat lastPushTimeMs;

  GQueue* queue;

  GMutex mutex;
  GCond cond;
  GThread* thread;
  gboolean running;
  gboolean waiting;
  gboolean scheduled;
};

struct _BPThrottlerClass
{
  GstElementClass parent_class;
};

GType bpthrottler_get_type (void);

G_END_DECLS

GST_DEBUG_CATEGORY_STATIC (bpthrottler_debug);
#define GST_CAT_DEFAULT bpthrottler_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_BANDWIDTH,
  PROP_DELAY,
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
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

#define bpthrottler_parent_class parent_class
G_DEFINE_TYPE (BPThrottler, bpthrottler, GST_TYPE_ELEMENT);

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
bp_throttler_init (GstPlugin * bp_throttler)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template bp_throttler' with your description
   */
  GST_DEBUG_CATEGORY_INIT (bpthrottler_debug, "bp_throttler",
      0, "Template bp_throttler");

  return gst_element_register (bp_throttler, "bp_throttler", GST_RANK_NONE,
      TYPE_BP_THROTTLER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "bp_throttler"
#endif

/* gstreamer looks for this structure to register bp_throttlers
 *
 * exchange the string 'Template bp_throttler' with your bp_throttler description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bp_throttler,
    "bp_throttler",
    bp_throttler_init,
    "0.0.0",
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)

#define GetMonotonicTimeMs() g_get_monotonic_time()/1000.0f

static void bpthrottler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void bpthrottler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean bpthrottler_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn bpthrottler_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

gpointer BP_Throttler_Thread (gpointer data);

typedef struct _BPThrottlerBuffer
{
	gfloat arrivalTimeMs;
	GstBuffer* data;
} BPThrottlerBuffer;

static BPThrottlerBuffer* newBPThrottlerBuffer(GstBuffer* gstBuffer, gint64 arrivalTime)
{
	BPThrottlerBuffer* buffer = (BPThrottlerBuffer*) g_malloc(sizeof(BPThrottlerBuffer));
	gst_buffer_ref (gstBuffer);
	buffer->data = gstBuffer;
	buffer->arrivalTimeMs = arrivalTime;
	return buffer;
}

static void freeBPThrottlerBuffer(BPThrottlerBuffer* buffer)
{
	gst_buffer_unref (buffer->data);
	g_free (buffer);
}

/* GObject vmethod implementations */

gboolean BP_Throttler_ThreadNotifier (gpointer data);

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
    g_param_spec_float ("bandwidth", "Bandwidth", "Maximum bandwidth in bits/second", 0, FLT_MAX, FLT_MAX, G_PARAM_READABLE)
   );

  g_object_class_install_property (
  	gobject_class,
  	PROP_DELAY,
    g_param_spec_float ("delay", "Delay", "Delay in milliseconds", 0, FLT_MAX, 0, G_PARAM_READABLE)
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

  filter->bandwidth = FLT_MAX;
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
      filter->bandwidth = g_value_get_float (value);
      break;
    case PROP_DELAY:
      filter->delayMs = g_value_get_float (value);
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
  gfloat nowMs = GetMonotonicTimeMs();

  g_mutex_lock(&filter->mutex);

  // enqueue the buffer
  g_queue_insert_after(
  	filter->queue,
  	NULL,
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
  gfloat currentTimeMs = filter->currentTimeMs;
  gfloat nowMs = GetMonotonicTimeMs();
  gsize bytes;
  gsize maxBytes;
  GstFlowReturn ret;

  // We skip all because a previous was too new (we can't push stuff out-of-order)
  // (blocks the foreach-loop here, kind off acts like a break in a while)
  if (filter->currentTimeMs == -1) {
  	return;
  }

  // Set oldest buffer timestamp that we considered
	filter->oldestBufferTimeMs = buffer->arrivalTimeMs;

  // Buffer is still too new to be sent out
  if (buffer->arrivalTimeMs + filter->delayMs > currentTimeMs) {
  	filter->currentTimeMs = -1;
  	return;
  }

  // Constrain source bandwidth (and skip anything after on this queue run if we exceed it for now)
  bytes = gst_buffer_get_size (buffer->data);
  maxBytes = (filter->bandwidth / 8.0f) * ((nowMs - filter->lastPushTimeMs) / 1000.0f);
  if (bytes > maxBytes) {
    filter->currentTimeMs = -1;
    return;
  }

  // Push data on the source pad
  ret = gst_pad_push (filter->srcpad, buffer->data);
  if (ret != GST_FLOW_OK) {
  	g_warning ("Flow return value is %u", ret);
  }

  // Set last push time
  filter->lastPushTimeMs = GetMonotonicTimeMs();

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
  gfloat timeoutMs;

  g_mutex_lock(&filter->mutex);

  while (filter->running) {
  	// Note: g_cond_wait takes care of lockinng/unlocking mutex
  	//g_mutex_unlock(&filter->mutex);
  	// wait until we are triggered
    filter->waiting = TRUE;
  	g_cond_wait (&filter->cond, &filter->mutex);
    filter->waiting = FALSE;
  	//g_mutex_lock(&filter->mutex);

  	// set latest loop run timestamp
  	filter->currentTimeMs = GetMonotonicTimeMs();

  	// jump back if the queue is empty
  	if (g_queue_is_empty (filter->queue)) {
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

  	// we want to be triggered again when the next buffer needs to be pushed out
  	g_timeout_add(timeoutMs, BP_Throttler_ThreadNotifier, filter);
    filter->scheduled = TRUE;
  }

  g_mutex_unlock(&filter->mutex);

  return NULL;
}
