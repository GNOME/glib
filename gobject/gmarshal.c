/* Note: This file is no longer generated.  See the comment in gmarshal.list */

#include "gvalue.h"
#include "gclosure.h"

#ifdef G_ENABLE_DEBUG
#define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v)
#define g_marshal_value_peek_char(v)     g_value_get_char (v)
#define g_marshal_value_peek_uchar(v)    g_value_get_uchar (v)
#define g_marshal_value_peek_int(v)      g_value_get_int (v)
#define g_marshal_value_peek_uint(v)     g_value_get_uint (v)
#define g_marshal_value_peek_long(v)     g_value_get_long (v)
#define g_marshal_value_peek_ulong(v)    g_value_get_ulong (v)
#define g_marshal_value_peek_int64(v)    g_value_get_int64 (v)
#define g_marshal_value_peek_uint64(v)   g_value_get_uint64 (v)
#define g_marshal_value_peek_enum(v)     g_value_get_enum (v)
#define g_marshal_value_peek_flags(v)    g_value_get_flags (v)
#define g_marshal_value_peek_float(v)    g_value_get_float (v)
#define g_marshal_value_peek_double(v)   g_value_get_double (v)
#define g_marshal_value_peek_string(v)   (char*) g_value_get_string (v)
#define g_marshal_value_peek_param(v)    g_value_get_param (v)
#define g_marshal_value_peek_boxed(v)    g_value_get_boxed (v)
#define g_marshal_value_peek_pointer(v)  g_value_get_pointer (v)
#define g_marshal_value_peek_object(v)   g_value_get_object (v)
#define g_marshal_value_peek_variant(v)  g_value_get_variant (v)
#else /* !G_ENABLE_DEBUG */
/* WARNING: This code accesses GValues directly, which is UNSUPPORTED API.
 *          Do not access GValues directly in your code. Instead, use the
 *          g_value_get_*() functions
 */
#define g_marshal_value_peek_boolean(v)  (v)->data[0].v_int
#define g_marshal_value_peek_char(v)     (v)->data[0].v_int
#define g_marshal_value_peek_uchar(v)    (v)->data[0].v_uint
#define g_marshal_value_peek_int(v)      (v)->data[0].v_int
#define g_marshal_value_peek_uint(v)     (v)->data[0].v_uint
#define g_marshal_value_peek_long(v)     (v)->data[0].v_long
#define g_marshal_value_peek_ulong(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_int64(v)    (v)->data[0].v_int64
#define g_marshal_value_peek_uint64(v)   (v)->data[0].v_uint64
#define g_marshal_value_peek_enum(v)     (v)->data[0].v_long
#define g_marshal_value_peek_flags(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_float(v)    (v)->data[0].v_float
#define g_marshal_value_peek_double(v)   (v)->data[0].v_double
#define g_marshal_value_peek_string(v)   (v)->data[0].v_pointer
#define g_marshal_value_peek_param(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_boxed(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_pointer(v)  (v)->data[0].v_pointer
#define g_marshal_value_peek_object(v)   (v)->data[0].v_pointer
#define g_marshal_value_peek_variant(v)  (v)->data[0].v_pointer
#endif /* !G_ENABLE_DEBUG */


/* VOID:VOID (./gmarshal.list:6) */
void
g_cclosure_marshal_VOID__VOID (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:BOOLEAN (./gmarshal.list:7) */
void
g_cclosure_marshal_VOID__BOOLEAN (GClosure     *closure,
                                  GValue       *return_value G_GNUC_UNUSED,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                  gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:CHAR (./gmarshal.list:8) */
void
g_cclosure_marshal_VOID__CHAR (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:UCHAR (./gmarshal.list:9) */
void
g_cclosure_marshal_VOID__UCHAR (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:INT (./gmarshal.list:10) */
void
g_cclosure_marshal_VOID__INT (GClosure     *closure,
                              GValue       *return_value G_GNUC_UNUSED,
                              guint         n_param_values,
                              const GValue *param_values,
                              gpointer      invocation_hint G_GNUC_UNUSED,
                              gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:UINT (./gmarshal.list:11) */
void
g_cclosure_marshal_VOID__UINT (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:LONG (./gmarshal.list:12) */
void
g_cclosure_marshal_VOID__LONG (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:ULONG (./gmarshal.list:13) */
void
g_cclosure_marshal_VOID__ULONG (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:ENUM (./gmarshal.list:14) */
void
g_cclosure_marshal_VOID__ENUM (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:FLAGS (./gmarshal.list:15) */
void
g_cclosure_marshal_VOID__FLAGS (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:FLOAT (./gmarshal.list:16) */
void
g_cclosure_marshal_VOID__FLOAT (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:DOUBLE (./gmarshal.list:17) */
void
g_cclosure_marshal_VOID__DOUBLE (GClosure     *closure,
                                 GValue       *return_value G_GNUC_UNUSED,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint G_GNUC_UNUSED,
                                 gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:STRING (./gmarshal.list:18) */
void
g_cclosure_marshal_VOID__STRING (GClosure     *closure,
                                 GValue       *return_value G_GNUC_UNUSED,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint G_GNUC_UNUSED,
                                 gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:PARAM (./gmarshal.list:19) */
void
g_cclosure_marshal_VOID__PARAM (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:BOXED (./gmarshal.list:20) */
void
g_cclosure_marshal_VOID__BOXED (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:POINTER (./gmarshal.list:21) */
void
g_cclosure_marshal_VOID__POINTER (GClosure     *closure,
                                  GValue       *return_value G_GNUC_UNUSED,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                  gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:OBJECT (./gmarshal.list:22) */
void
g_cclosure_marshal_VOID__OBJECT (GClosure     *closure,
                                 GValue       *return_value G_GNUC_UNUSED,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint G_GNUC_UNUSED,
                                 gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:VARIANT (./gmarshal.list:23) */
void
g_cclosure_marshal_VOID__VARIANT (GClosure     *closure,
                                  GValue       *return_value G_GNUC_UNUSED,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                  gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* VOID:UINT,POINTER (./gmarshal.list:26) */
void
g_cclosure_marshal_VOID__UINT_POINTER (GClosure     *closure,
                                       GValue       *return_value G_GNUC_UNUSED,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint G_GNUC_UNUSED,
                                       gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* BOOL:FLAGS (./gmarshal.list:27) */
void
g_cclosure_marshal_BOOLEAN__FLAGS (GClosure     *closure,
                                   GValue       *return_value G_GNUC_UNUSED,
                                   guint         n_param_values,
                                   const GValue *param_values,
                                   gpointer      invocation_hint G_GNUC_UNUSED,
                                   gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* STRING:OBJECT,POINTER (./gmarshal.list:28) */
void
g_cclosure_marshal_STRING__OBJECT_POINTER (GClosure     *closure,
                                           GValue       *return_value G_GNUC_UNUSED,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint G_GNUC_UNUSED,
                                           gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

/* BOOL:BOXED,BOXED (./gmarshal.list:29) */
void
g_cclosure_marshal_BOOLEAN__BOXED_BOXED (GClosure     *closure,
                                         GValue       *return_value G_GNUC_UNUSED,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint G_GNUC_UNUSED,
                                         gpointer      marshal_data)
{
  g_cclosure_marshal_generic (closure, return_value, n_param_values, param_values,
			      invocation_hint, marshal_data);
}

