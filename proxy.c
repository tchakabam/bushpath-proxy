
// Compile-and-run-bang-line:

//&!/usr/bin/gcc -I/Library/Frameworks/GStreamer.framework/Headers/ -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgnutls -o proxy proxy.c && ./proxy

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "connections.h"

gchar *buffer;

guint listenPort = 8080;
gchar *listenHost = "127.0.0.1";
GInetAddress *inetAddress = NULL;
GSocketAddress *socketAddress = NULL;
GSocketService *service = NULL;
GSocketClient *client = NULL;
GMainLoop *loop = NULL;
UserTable users;

gboolean
onNetworkRead(GIOChannel *sourceChannel,
            GIOCondition idCond,
            gpointer userData) // Should be GSocketConnection *connection
{
    GSocketConnection *connection = userData;
    InboundConnection * user = findUser (users, connection, sourceChannel);
    gboolean res = TRUE;
    gboolean forward = TRUE;

    if (!user) {
        g_object_unref (userData);
        g_error ("Failed to retrieve user connection data from table!!!");
        return FALSE;
    }

    g_message ("Network read: Found connection/IO-channel in user table mapped @ %p", user);

    // TODO: We only parse on header per connection!
    // So this means we can't multiplex several user HTTP connections
    // over one inbound proxy connection.
    if (!inboundConnectionHeaderParsed (user)) {
        g_message ("Have not parsed header for this connection yet.");
        forward = FALSE;
    }

    switch (forward)
    {
    case FALSE:
        analyzeData (user, &res);
        break;
    case TRUE:
        forwardData (user, &res);
        break;
    }

    if (!res) {
        g_message ("Destroying connection");
        // Drop last reference on connection
        g_object_unref (userData);
    }

    return res;
}

gboolean
onNewConnection(GSocketService *service,
              GSocketConnection *connection,
              GObject *source_object,
              gpointer user_data)
{
    GSocket *socket = g_socket_connection_get_socket(connection);
    GSocketAddress *sockaddr = g_socket_connection_get_remote_address(connection, NULL);
    GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(sockaddr));
    guint16 port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(sockaddr));
    gint fd;
    GIOChannel *channel;
    InboundConnection user;

    g_object_ref (connection);

    g_message ("New connection from %s:%d", g_inet_address_to_string(addr), port);

    fd = g_socket_get_fd(socket);
    channel = g_io_channel_unix_new(fd);

    // Add IO watch with pointer to connection handle as user data for callback
    g_io_add_watch(channel, G_IO_IN, (GIOFunc) onNetworkRead, connection);

    user = newUser (TRUE);
    user.sourceChannel = channel;
    user.connection = connection;
    user.out = newOutboundConnection (client);
    addUser (users, user);

    return TRUE;
}

void
init ()
{
    gchar* url;

    g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL,
        g_log_default_handler, NULL);

    g_message ("Launching proxy");

    g_message ("Init look-up-tables ...");

    resetUserTable (users);

    g_message ("Done.");

    // start service, client, and main loop
    service = g_socket_service_new();
    client = g_socket_client_new();
    loop = g_main_loop_new(NULL, FALSE);

    // create network address
    inetAddress = g_inet_address_new_from_string(listenHost);
    socketAddress = g_inet_socket_address_new(inetAddress, listenPort);

    // add server socket to service and attach listener
    g_socket_listener_add_address(G_SOCKET_LISTENER(service), socketAddress, G_SOCKET_TYPE_STREAM,
                                                                            G_SOCKET_PROTOCOL_TCP,
                                                                            NULL, NULL, NULL);

    g_message ("Starting IO service...");

    // Connect socket to IO service
    g_socket_service_start(service);

    g_message ("IO service is up");
    g_message ("Attaching signals ...");

    g_signal_connect(service, "incoming", G_CALLBACK(onNewConnection), NULL);

    g_message ("Attached!");

    g_object_get (socketAddress, "port", &listenPort, NULL);

    url = g_inet_address_to_string (inetAddress);

    g_message ("Local IP/Port: %s:%u", url, listenPort);

    // Run main
    g_main_loop_run(loop);

    // Clean up
    g_object_unref(inetAddress);
    g_object_unref(socketAddress);

    g_free (url);
}

// TODO: destroy ()

int main(int argc, char **argv)
{
    init ();

    return 0;
}

