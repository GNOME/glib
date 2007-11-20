/* GLib testing framework examples
 * Copyright (C) 2007 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __G_TESTFRAMEWORK_H__
#define __G_TESTFRAMEWORK_H__

#include <glib.h>

G_BEGIN_DECLS;

typedef struct GTestCase  GTestCase;
typedef struct GTestSuite GTestSuite;

/* assertion API */
#define g_assert_cmpstr(s1, cmp, s2)    do { const char *__s1 = (s1), *__s2 = (s2); \
                                             if (g_strcmp0 (__s1, __s2) cmp 0) ; else \
                                               g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #s1 " " #cmp " " #s2, __s1, #cmp, __s2); } while (0)
#define g_assert_cmpint(n1, cmp, n2)    do { gint64 __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); } while (0)
#define g_assert_cmpuint(n1, cmp, n2)   do { guint64 __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); } while (0)
#define g_assert_cmphex(n1, cmp, n2)    do { guint64 __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'x'); } while (0)
#define g_assert_cmpfloat(n1,cmp,n2)    do { long double __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'f'); } while (0)
int     g_strcmp0                       (const char     *str1,
                                         const char     *str2);
//      g_assert(condition)             /*...*/
//      g_assert_not_reached()          /*...*/

/* report performance results */
void    g_test_minimized_result         (double          minimized_quantity,
                                         const char     *format,
                                         ...) G_GNUC_PRINTF (2, 3);
void    g_test_maximized_result         (double          maximized_quantity,
                                         const char     *format,
                                         ...) G_GNUC_PRINTF (2, 3);

/* initialize testing framework */
void    g_test_init                     (int            *argc,
                                         char         ***argv,
                                         ...);
/* run all tests under toplevel suite (path: /) */
int     g_test_run                      (void);
/* hook up a simple test function under test path */
void    g_test_add_func                 (const char     *testpath,
                                         void          (*test_func) (void));
/* hook up a test with fixture under test path */
#define g_test_add(testpath, Fixture, fsetup, ftest, fteardown) \
                                        ((void (*) (const char*,        \
                                                    gsize,              \
                                                    void (*) (Fixture*),   \
                                                    void (*) (Fixture*),   \
                                                    void (*) (Fixture*)))  \
                                         (void*) g_test_add_vtable) \
                                          (testpath, sizeof (Fixture), fsetup, ftest, fteardown)
/* measure test timings */
void    g_test_timer_start              (void);
double  g_test_timer_elapsed            (void); // elapsed seconds
double  g_test_timer_last               (void); // repeat last elapsed() result

/* automatically g_free or g_object_unref upon teardown */
void    g_test_queue_free               (gpointer gfree_pointer);
void    g_test_queue_unref              (gpointer gobjectunref_pointer);

/* test traps are guards used around forked tests */
typedef enum {
  G_TEST_TRAP_SILENCE_STDOUT    = 1 << 7,
  G_TEST_TRAP_SILENCE_STDERR    = 1 << 8,
  G_TEST_TRAP_INHERIT_STDIN     = 1 << 9,
} GTestTrapFlags;
gboolean g_test_trap_fork               (guint64              usec_timeout,
                                         GTestTrapFlags       test_trap_flags);
gboolean g_test_trap_has_passed         (void);
gboolean g_test_trap_reached_timeout    (void);
#define  g_test_trap_assert_passed()            g_test_trap_assertions (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, 1, 0, 0, 0)
#define  g_test_trap_assert_failed()            g_test_trap_assertions (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, 0, 1, 0, 0)
#define  g_test_trap_assert_stdout(soutpattern) g_test_trap_assertions (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, 0, 0, soutpattern, 0)
#define  g_test_trap_assert_stderr(serrpattern) g_test_trap_assertions (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, 0, 0, 0, serrpattern)

/* provide seed-able random numbers for tests */
#define  g_test_rand_bit()              (0 != (g_test_rand_int() & (1 << 15)))
gint32   g_test_rand_int                (void);
gint32   g_test_rand_int_range          (gint32          begin,
                                         gint32          end);
double   g_test_rand_double             (void);
double   g_test_rand_double_range       (double          range_start,
                                         double          range_end);

/* semi-internal API */
GTestCase*    g_test_create_case        (const char     *test_name,
                                         gsize           data_size,
                                         void          (*data_setup) (void),
                                         void          (*data_test) (void),
                                         void          (*data_teardown) (void));
GTestSuite*   g_test_create_suite       (const char     *suite_name);
GTestSuite*   g_test_get_root           (void);
void          g_test_suite_add          (GTestSuite     *suite,
                                         GTestCase      *test_case);
void          g_test_suite_add_suite    (GTestSuite     *suite,
                                         GTestSuite     *nestedsuite);
int           g_test_run_suite          (GTestSuite     *suite);

/* internal ABI */
void    g_test_trap_assertions          (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         gboolean        must_pass,
                                         gboolean        must_fail,
                                         const char     *stdout_pattern,
                                         const char     *stderr_pattern);
void    g_assertion_message             (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *message);
void    g_assertion_message_expr        (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr);
void    g_assertion_message_cmpstr      (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr,
                                         const char     *arg1,
                                         const char     *cmp,
                                         const char     *arg2);
void    g_assertion_message_cmpnum      (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr,
                                         long double     arg1,
                                         const char     *cmp,
                                         long double     arg2,
                                         char            numtype);
void    g_test_add_vtable               (const char     *testpath,
                                         gsize           data_size,
                                         void          (*data_setup)    (void),
                                         void          (*data_test)     (void),
                                         void          (*data_teardown) (void));

/* internal logging API */
typedef enum {
  G_TEST_LOG_NONE,
  G_TEST_LOG_ERROR,             // s:msg
  G_TEST_LOG_START_BINARY,      // s:binaryname s:seed
  G_TEST_LOG_LIST_CASE,         // s:testpath
  G_TEST_LOG_SKIP_CASE,         // s:testpath
  G_TEST_LOG_START_CASE,        // s:testpath
  G_TEST_LOG_STOP_CASE,         // d:status d:nforks d:elapsed
  G_TEST_LOG_MIN_RESULT,        // s:blurb d:result
  G_TEST_LOG_MAX_RESULT,        // s:blurb d:result
} GTestLogType;

typedef struct {
  GTestLogType  log_type;
  guint         n_strings;
  gchar       **strings; // NULL terminated
  guint         n_nums;
  long double  *nums;
} GTestLogMsg;
typedef struct {
  /*< private >*/
  GString     *data;
  GSList      *msgs;
} GTestLogBuffer;

const char*     g_test_log_type_name    (GTestLogType    log_type);
GTestLogBuffer* g_test_log_buffer_new   (void);
void            g_test_log_buffer_free  (GTestLogBuffer *tbuffer);
void            g_test_log_buffer_push  (GTestLogBuffer *tbuffer,
                                         guint           n_bytes,
                                         const guint8   *bytes);
GTestLogMsg*    g_test_log_buffer_pop   (GTestLogBuffer *tbuffer);
void            g_test_log_msg_free     (GTestLogMsg    *tmsg);

G_END_DECLS;

#endif /* __G_TESTFRAMEWORK_H__ */
