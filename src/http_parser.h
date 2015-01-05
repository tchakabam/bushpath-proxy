/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#pragma once

 #ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define TYPE_BP_HTTPPARSER            (bp_http_parser_get_type())
#define BP_HTTPPARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),TYPE_BP_HTTPPARSER,BP_HTTPParser))
#define IS_BP_HTTPPARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),TYPE_BP_HTTPPARSER))
#define BP_HTTPPARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,TYPE_BP_HTTPPARSER,BP_HTTPParserClass))
#define IS_BP_HTTPPARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,TYPE_SKIPPY_LOCALRC))
#define BP_HTTPPARSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,TYPE_BP_HTTPPARSER,BP_HTTPParserClass))

#define MAX_BUFFER_SIZE 32752

#define GST_CAT_DEFAULT bp_http_parser_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _BP_HTTPParser {
  GstBin parent;

  // src pad
  GstPad *srcpad;
  // sink pad
  GstPad *sinkpad;
  // props
  GString* host;
  gboolean foundHTTP;
  gboolean headerComplete;
  // ...
  guint lineCount;

  GString* lineBuffer;

} BP_HTTPParser;

typedef struct _BP_HTTPParserClass {
  GstBinClass parent_class;
} BP_HTTPParserClass;

G_END_DECLS

enum
{
  PROP_0,
  PROP_HOST,
  PROP_EMPTY_LINE
};

G_DEFINE_TYPE (BP_HTTPParser, bp_http_parser, GST_TYPE_BIN);

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
bp_http_parser_plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template plugin' with your description
   */
  GST_DEBUG_CATEGORY_INIT (bp_http_parser_debug, "plugin",
      0, "BushpathProxy HTTP parser debug");

  return gst_element_register (plugin, "plugin", GST_RANK_NONE,
      TYPE_BP_HTTPPARSER);
}

#define PACKAGE "BushpathProxy"

/* gstreamer looks for this structure to register plugins
 *
 * exchange the string 'Template plugin' with your plugin description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    plugin,
    "Template plugin",
    bp_http_parser_plugin_init,
    "0.0.0",
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void
bp_http_parser_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  BP_HTTPParser* parser = BP_HTTPPARSER (object);

  switch (prop_id)
  {
  case PROP_HOST:
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
bp_http_parser_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  BP_HTTPParser* parser = BP_HTTPPARSER (object);

  switch (prop_id)
  {
  case PROP_HOST:
    g_value_set_string(value, parser->host->str);
    break;
  case PROP_EMPTY_LINE:
    g_value_set_boolean(value, parser->headerComplete);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static GstStateChangeReturn
bp_http_parser_change_state (GstElement * element, GstStateChange transition)
{
    return GST_ELEMENT_CLASS (bp_http_parser_parent_class)->change_state (element, transition);
}

static void
bp_http_parser_finalize (GObject * object)
{

}

static gboolean
bp_http_parser_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  return TRUE;
}

void BP_HTTPParser_ReadConnectionHeader (BP_HTTPParser* parser, gboolean*res);
void BP_HTTPParser_Parse (BP_HTTPParser* parser, GstBuffer * buffer, gboolean* res);

static GstFlowReturn
bp_http_parser_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
    BP_HTTPParser* parser = BP_HTTPPARSER (parent);
    gboolean res = TRUE;

    if (pad != parser->sinkpad) {
      return GST_FLOW_ERROR;
    }

    BP_HTTPParser_Parse (parser, buffer, &res);

    if (!res) {
      // we still forward stuff even if it's bad ...
      gst_pad_push (parser->srcpad, buffer);
      // but we signal an error to the upstream elements
      return GST_FLOW_ERROR;
    }

    return gst_pad_push (parser->srcpad, buffer);
}

static void
bp_http_parser_class_init (BP_HTTPParserClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gstbin_class = (GstBinClass *) klass;

  gstelement_class = (GstElementClass *) klass;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Bushpath Proxy HTTP parser", "Network/Protocol/Analyzer",
      "An analyzer for HTTP traffic data", "Stephan Hesse <stephan@soundcloud.com>");

  gstelement_class->change_state = bp_http_parser_change_state;

  gobject_class = (GObjectClass *) klass;
  gobject_class->finalize = bp_http_parser_finalize;
  gobject_class->set_property = bp_http_parser_set_property;
  gobject_class->get_property = bp_http_parser_get_property;

  g_object_class_install_property (
    gobject_class,
    PROP_HOST,
    g_param_spec_string ("host", "Host", "Host found in request header", "", G_PARAM_READABLE)
  );

  g_object_class_install_property (
    gobject_class,
    PROP_HOST,
    g_param_spec_boolean ("empty", "Empty", "Empty line found in request header", FALSE, G_PARAM_READABLE)
  );

}

static void
bp_http_parser_init (BP_HTTPParser * parser)
{
  parser->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_event_function (parser->sinkpad,
                              GST_DEBUG_FUNCPTR(bp_http_parser_sink_event));

  gst_pad_set_chain_function (parser->sinkpad,
                              GST_DEBUG_FUNCPTR(bp_http_parser_chain));

  GST_PAD_SET_PROXY_CAPS (parser->sinkpad);
  gst_element_add_pad (GST_ELEMENT (parser), parser->sinkpad);

  parser->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  GST_PAD_SET_PROXY_CAPS (parser->srcpad);
  gst_element_add_pad (GST_ELEMENT (parser), parser->srcpad);

  parser->host = g_string_new(NULL);
  parser->foundHTTP = FALSE;
  parser->lineCount = 0;
  parser->lineBuffer = g_string_new(NULL);
  parser->headerComplete = FALSE;
}

void
BP_HTTPParser_ParseRequestLine (BP_HTTPParser* parser, gchar* s)
{
    // Let's try to detect HTTP
    gboolean isHttp = FALSE;
    // Split up the line in space-sign-tokens
    gchar** tokens = g_strsplit (s, " ", 3);

    g_return_if_fail (tokens != NULL);

    do {
        // If we get this the vector is zero length
        if (tokens[0] == NULL) {
            break;
        }
        if (strcmp(tokens[0], "GET") != 0) {
            break;
        }
        // If we get this the vector is ended
        if (tokens[1] == NULL) {
            break;
        }
         // If we get this the vector is ended
        if (tokens[2] == NULL) {
            break;
        }
        if (!g_str_has_prefix (tokens[2], "HTTP/1.1")) {
            break;
        }
        // If we get this the vector is ended
        if (tokens[3] != NULL) {
            break;
        }

        parser->foundHTTP = TRUE;

    } while (FALSE);

    if (!parser->foundHTTP) {
        g_message ("Not HTTP");
        return;
    }

    g_message ("It's HTTP :)");
}

void
BP_HTTPParser_ParseHeaderLine (BP_HTTPParser* parser, gchar* s)
{
    // Let's try to detect HTTP
    // Split up the line in space-sign-tokens
    int len;
    gchar** tokens = g_strsplit (s, " ", 2);

    g_return_if_fail (tokens);

    do {
        // If we get this the vector is zero length
        if (tokens[0] == NULL) {
            break;
        }
        if (strcmp(tokens[0], "Host:") != 0) {
            break;
        }
        // If we get this the vector is ended
        if (tokens[1] == NULL) {
            break;
        }

        // If we get this the vector is ended
        if (tokens[2] != NULL) {
            break;
        }

        // TODO: Replace this by guard for max header line length
        /*
        if (strlen(tokens[1]) > MAX_HOSTNAME_LENGTH) {
            g_warning ("Parsed host name (%s) length exceeds max length %d", tokens[1], MAX_HOSTNAME_LENGTH);
            break;
        }
        */

        // Length should be more than two chars (with termination character)
        len = strlen(tokens[1]);
        if (len <= 2) {
            g_warning ("Parsed host name (%s) is too short", tokens[1]);
            break;
        }

        // truncate at last letter which is linebreak
        tokens[1][len-2] = '\0';
        // fix for unicode strings
        tokens[1][len-1] = '\0';
        // copy to user data
        g_string_assign (parser->host, tokens[1]);
        // free vector
        g_strfreev (tokens);
        // log
        g_message ("Destination host is: %s", parser->host);

    } while (FALSE);
}

void
BP_HTTPParser_ParseLine (BP_HTTPParser* parser, gchar* s)
{
    // This is the first line so we can detect the application layer protocol
    switch (parser->lineCount) {
        case 1: {
            BP_HTTPParser_ParseRequestLine (parser, s);
            break;
        }
        default: {
            if (parser->host->len) {
                BP_HTTPParser_ParseHeaderLine (parser, s);
            }
            break;
        }
    }
}

void
BP_HTTPParser_ReadConnectionHeader (BP_HTTPParser* parser, gboolean*res)
{
    /*
    The request message consists of the following:

    A request line, for example GET /images/logo.png HTTP/1.1, which requests a resource called /images/logo.png from the server.
    Request header fields, such as Accept-Language: en
    An empty line.
    An optional message body.
    The request line and other header fields must each end with <CR><LF> (that is, a carriage return character followed by a line feed character). The empty line must consist of only <CR><LF> and no other whitespace.[24] In the HTTP/1.1 protocol, all header fields except Host are optional.

    A request line containing only the path name is accepted by servers to maintain compatibility with HTTP clients before the HTTP/1.0 specification in RFC 1945.[25]
    */

    const gchar* termLF = NULL;
    const gchar* termCR = NULL;
    gchar* line = NULL;
    gsize lineLength;

    do {

        termCR = strstr(parser->lineBuffer->str, "\r");
        termLF = strstr(parser->lineBuffer->str, "\n");

        // No CR-LF sequence found yet in buffer
        if (!termCR || !termLF) {
            g_message ("No CR-LF sequence found.");
            break;
        }

        // Validate it's an actual sequence
        if (termCR != termLF -1) {
          g_error ("Invalid HTTP header line: LF char is not followed directly by LF char.");
          *res = FALSE;
          return;
        }

        // Set line length and copy the line into our temp buffer and remove from main buffer.
        lineLength = parser->lineBuffer->str - termLF + 1;
        line = (gchar*) g_malloc (lineLength + 1);
        line[lineLength] = '\0';
        memcpy (line, parser->lineBuffer->str, lineLength);
        g_string_erase (parser->lineBuffer, 0, lineLength);
        g_message ("Read line of length: %u; %s", lineLength, line);

        if (lineLength == 0) {
          parser->headerComplete = TRUE;
        }

        // Increment line read count
        parser->lineCount++;

        // Do HTTP request header parsing
        BP_HTTPParser_ParseLine (parser, line);

        // Free allocated memory
        g_free (line);

    } while (TRUE);

    *res = TRUE;
    return;
}

void
BP_HTTPParser_Parse (BP_HTTPParser* parser, GstBuffer * buffer, gboolean* res)
{
    GstMapInfo inputMap;
    const gchar* inData;
    gsize inLen;

    if (parser->headerComplete) {
      return;
    }

    if (!gst_buffer_map (buffer, &inputMap, GST_MAP_READ))
    {
      *res = FALSE;
      return;
    }

    inData = (const gchar*) inputMap.data;
    inLen = inputMap.size;
    g_string_append_len(parser->lineBuffer, inData, inLen);
    BP_HTTPParser_ReadConnectionHeader (parser, res);
    gst_buffer_unmap (buffer, &inputMap);
}