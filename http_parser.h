#ifndef __PROXY_HTTPPARSER__
#define __PROXY_HTTPPARSER__

#include <stdio.h>
#include <string.h>

#include "user_table.h"

void
parseRequestLine (InboundConnection * user, gchar* s)
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

        isHttp = TRUE;

    } while (FALSE);

    if (!isHttp) {
        g_message ("Not HTTP");
        return;
    }

    g_message ("It's HTTP :)");

    user->protocol = HTTP;
}

void
parseDestinationHost (InboundConnection * user, gchar* s)
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

        if (strlen(tokens[1]) > MAX_HOSTNAME_LENGTH) {
            g_warning ("Parsed host name (%s) length exceeds max length %d", tokens[1], MAX_HOSTNAME_LENGTH);
            break;
        }

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
        strcpy (user->destinationHost, tokens[1]);
        // free vector
        g_strfreev (tokens);
        // log
        g_message ("Destination host is: %s", user->destinationHost);

    } while (FALSE);
}

void
detectHttpConnection (InboundConnection * user, gchar* s)
{
    g_return_if_fail (user != NULL);

    // This is the first line so we can detect the application layer protocol
    switch (user->lineReadCount) {
        case 1: {
            parseRequestLine (user, s);
            break;
        }
        default: {
            if (strlen(user->destinationHost) == 0) {
                parseDestinationHost (user, s);
            }
            break;
        }
    }
}

void
readConnectionHeader (InboundConnection * user, gboolean *res)
{
    gchar *s = NULL;
    gsize length = -1;
    gsize termPos = -1;
    GIOStatus ret;
    GError *error = NULL;

    while (*res) {

        ret = g_io_channel_read_line(user->sourceChannel, &s, &length, &termPos, &error);

        switch (ret) {
        case G_IO_STATUS_ERROR: {
            g_warning ("IO status error reading from socket: %s", error->message);
            // Remove the event source
            *res = FALSE;
            break;
            }
        // Connection closed (probably by remote)
        case G_IO_STATUS_EOF: {
            g_message ("Source EOF'd");
            *res = FALSE;
            break;
            }
        case G_IO_STATUS_NORMAL:
            break;
        case G_IO_STATUS_AGAIN:
            break;
        }

        if (length == 0) {
            break;
        }

        g_message ("Read line of length: %u; termination character position:%u\n\n%s", length, termPos, s);

        // Increment line read count
        user->lineReadCount++;

        // Do HTTP request header parsing
        detectHttpConnection (user, s);

        // After first pass we must have detected a valid protocol
        if (user->protocol != HTTP) {
            g_error ("Couldn't detect HTTP");
            break;
        }

        // Finally let's write this stuff into a buffer to send it on later
        if (user->requestHeaderOffset + length > MAX_REQUEST_HEADER_SIZE) {
            break;
        }
        memcpy(user->requestHeaderBuffer + user->requestHeaderOffset, s, length);
        user->requestHeaderOffset += length;
    }

    if (s) {
        g_free (s);
    }
}

#endif