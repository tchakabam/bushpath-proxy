/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#pragma once

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

  void (*header_complete) (GstElement *element);

} BP_HTTPParserClass;

GType
bp_http_parser_get_type (void);

void
bp_http_parser_setup (void);

G_END_DECLS
