#pragma once

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

  gint64 bandwidth;
  gint64 delayMs;

  gint64 currentTimeMs;
  gint64 newestBufferTimeMs;
  gint64 oldestBufferTimeMs;

  gint64 lastPushTimeMs;

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

void bp_throttler_setup (void);

G_END_DECLS
