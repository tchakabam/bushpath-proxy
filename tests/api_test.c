#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include <curl/curl.h>

#include "../src/api.h"

#define PROXY_HOST "127.0.0.1"
#define PROXY_PORT 8080

GMainLoop* loop = NULL;
GThread* thread = NULL;
CURL* curl = NULL;
BushpathProxy* proxy = NULL;
BushpathProxyOptions options;
gchar *responseBuffer = NULL;
gsize responseBufferSize = 0;

static gpointer
runMainLoop (gpointer data)
{
  // Run
  g_return_val_if_fail (loop, NULL);
  g_main_loop_run(loop);

  return NULL;
}

static void
teardown (void **state)
{
  assert_non_null (loop);

  g_return_if_fail (loop);
  g_return_if_fail (thread);

  g_main_loop_quit (loop);
  g_main_loop_unref (loop);
  loop = NULL;

  g_thread_join (thread);
  thread = NULL;

  BushpathProxy_Destroy (proxy);
  proxy = NULL;

  curl_easy_cleanup (curl);
  if (responseBuffer) {
    g_free (responseBuffer);
    responseBuffer = NULL;
  }
  responseBufferSize = 0;
  curl = NULL;
}

gboolean
timeout (gpointer data)
{
  g_message ("Test harness timeout!");
  teardown (NULL);
  return FALSE;
}

static void
setup (void **state)
{
  g_return_if_fail (!loop);
  g_return_if_fail (!proxy);
  g_return_if_fail (!thread);
  g_return_if_fail (!curl);

  curl_global_init (CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  responseBuffer = NULL;

  options.port = PROXY_PORT;
  options.bindAddress = PROXY_HOST;

  loop = g_main_loop_new(NULL, FALSE);
  proxy = BushpathProxy_New  (g_main_loop_get_context(loop), options);
  thread = g_thread_new ("api_test", runMainLoop, NULL);

  // Poll main loop until it's running
  while (!g_main_loop_is_running(loop)) {
    g_usleep (1);
  }

  g_message ("MAIN LOOP RUNNING");

  /*
  g_timeout_add_seconds (5,
                         timeout,
                         NULL);
                         */
}

size_t curlWrite(char *data, size_t size, size_t nmemb, void *context)
{
  // g_return_val_if_fail (responseBuffer == NULL, 0);
  // g_return_val_if_fail (responseBufferSize == 0, 0);

  gsize bufferSize = size*nmemb;
  gchar* tmp;
  gsize offset;

  if (responseBuffer != NULL) {
    g_return_val_if_fail (responseBufferSize > 0, 0);
    offset = responseBufferSize;
    responseBufferSize += bufferSize;
    tmp = g_malloc (responseBufferSize);
    memcpy (tmp, responseBuffer, offset);
    memcpy (tmp, data + offset, bufferSize);
    g_free (responseBuffer);
    responseBuffer = tmp;
  } else {
    responseBuffer = (gchar*) g_malloc (bufferSize);
    responseBufferSize = bufferSize;
    memcpy (responseBuffer, data, bufferSize);
  }

  return bufferSize;
}

static CURLcode
curlDoRequest (const gchar *url)
{
  CURLcode error;
  glong proxyPort = PROXY_PORT;
  gchar* proxyHost = PROXY_HOST;

  curl_easy_reset (curl);
  curl_easy_setopt (curl, CURLOPT_URL, url);
  curl_easy_setopt (curl, CURLOPT_PROXY, proxyHost);
  curl_easy_setopt (curl, CURLOPT_PROXYPORT, proxyPort);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curlWrite);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, NULL);
  error = curl_easy_perform (curl);
  if (error != CURLE_OK) {
    g_warning ("CURL error: %s", curl_easy_strerror(error));
  }
  return error;
}

static void
testReset ()
{
  assert_non_null (loop);

  BushpathProxy_Destroy (proxy);
  proxy = BushpathProxy_New  (g_main_loop_get_context(loop), options);
}

static void
testOneRequest ()
{
  curlDoRequest ("http://localhost:8000/tests/fixtures/140kb.jpg");
  //curlDoRequest ("http://www.example.com");

  g_message ("Received %d bytes", (int) responseBufferSize);

  assert_int_equal (responseBufferSize, 140233);
}

int
main (int argc, char **argv)
{
  const UnitTest tests[] = {
    //unit_test_setup_teardown (testReset, setup, teardown),
    unit_test_setup_teardown (testOneRequest, setup, teardown),
  };

  g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL,
    g_log_default_handler, NULL);

  // We never ever want anything with warnings to succeed
  g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);

  return run_tests (tests);
}