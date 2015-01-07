#pragma once

#include <glib.h>
#include <gst/gst.h>

typedef struct _BPThrottlerBuffer
{
	gint64 arrivalTimeMs;
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
