/*

BUSHPATH PROXY

@author: Stephan Hesse <stephan@soundcloud.com> <disparat@gmail.com>

Copyright (c) 2014, 2015 Stephan Hesse

Read included LICENSE for third-party usage.

*/

#include "api.h"

int main(int argc, char **argv)
{
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    BushpathProxyOptions options = BushpathProxyOptions_New ("127.0.0.1", 8080);
    BushpathProxy* proxy = BushpathProxy_New  (g_main_loop_get_context(loop), options);

    g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL,
        g_log_default_handler, NULL);

    // Run main
    g_main_loop_run(loop);

    return 0;
}

