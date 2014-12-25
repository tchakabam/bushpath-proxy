#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/connection.h"

static void
setup (void **state)
{

}

static void
teardown (void **state)
{

}

static void
testNewUser ()
{
	InboundConnection* ic = newInboundConnection ((GSocketConnection*) 0x1, (GIOChannel*) 0x2);

  /*
  assert_true (ic->connection == (GSocketConnection*) 0x1);
  assert_true (ic->sourceChannel == 0x2);
  assert_true (ic->out == NULL);
  */

  ic->out = newOutboundConnection((GSocketClient*) 0x3);

  /*
  assert_true (ic->out == 0x3);
  */
}

int
main (int argc, char **argv)
{
  const UnitTest tests[] = {
    unit_test_setup_teardown (testNewUser, setup, teardown),
  };

  g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL,
    g_log_default_handler, NULL);

  // We never ever want anything with warnings to succeed
  g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);

  return run_tests (tests);
}