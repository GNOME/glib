/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __G_SIGNAL_H__
#define __G_SIGNAL_H__


#include	<gobject/gclosure.h>
#include	<gobject/gvalue.h>
#include	<gobject/gparam.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* --- typedefs --- */
typedef struct _GSignalQuery          	 GSignalQuery;
typedef struct _GSignalInvocationHint 	 GSignalInvocationHint;
typedef	GClosureMarshal			 GSignalCMarshaller;
typedef gboolean (*GSignalEmissionHook) (GSignalInvocationHint *ihint,
					 guint			n_param_values,
					 const GValue	       *param_values);
typedef gboolean (*GSignalAccumulator)	(GSignalInvocationHint *ihint,
					 GValue		       *return_accu,
					 const GValue	       *return_value);


/* --- run & match types --- */
typedef enum
{
  G_SIGNAL_RUN_FIRST	= 1 << 0,
  G_SIGNAL_RUN_LAST	= 1 << 1,
  G_SIGNAL_RUN_CLEANUP	= 1 << 2,
  G_SIGNAL_NO_RECURSE	= 1 << 3,
  G_SIGNAL_DETAILED	= 1 << 4,
  G_SIGNAL_ACTION	= 1 << 5,
  G_SIGNAL_NO_HOOKS	= 1 << 6
#define	G_SIGNAL_FLAGS_MASK  0x7f
} GSignalFlags;
typedef enum
{
  G_SIGNAL_MATCH_ID	   = 1 << 0,
  G_SIGNAL_MATCH_DETAIL	   = 1 << 1,
  G_SIGNAL_MATCH_CLOSURE   = 1 << 2,
  G_SIGNAL_MATCH_FUNC	   = 1 << 3,
  G_SIGNAL_MATCH_DATA	   = 1 << 4,
  G_SIGNAL_MATCH_UNBLOCKED = 1 << 5
#define	G_SIGNAL_MATCH_MASK  0x3f
} GSignalMatchType;


/* --- signal information --- */
struct _GSignalInvocationHint
{
  guint		signal_id;
  GQuark	detail;
  GSignalFlags	run_type;
};
struct _GSignalQuery
{
  guint		signal_id;
  const gchar  *signal_name;
  GType		itype;
  GSignalFlags	signal_flags;
  GType		return_type;
  guint		n_params;
  const GType  *param_types;
};


/* --- signals --- */
guint	g_signal_newv			(const gchar		*signal_name,
					 GType			 itype,
					 GSignalFlags		 signal_flags,
					 GClosure		*class_closure,
					 GSignalAccumulator	 accumulator,
					 GSignalCMarshaller	 c_marshaller,
					 GType			 return_type,
					 guint			 n_params,
					 GType			*param_types);
void	g_signal_emitv			(const GValue		*instance_and_params,
					 guint			 signal_id,
					 GQuark			 detail,
					 GValue			*return_value);
guint	g_signal_lookup			(const gchar		*name,
					 GType			 itype);
gchar*	g_signal_name			(guint			 signal_id);
void	g_signal_query			(guint			 signal_id,
					 GSignalQuery		*query);
guint*	g_signal_list_ids		(GType			 itype,
					 guint			*n_ids);

/* --- signal handlers --- */
guint	 g_signal_connect_closure	(gpointer		 instance,
					 guint			 signal_id,
					 GQuark			 detail,
					 GClosure		*closure,
					 gboolean		 after);
void	 g_signal_handler_disconnect	(gpointer		 instance,
					 guint			 handler_id);
void	 g_signal_handler_block		(gpointer		 instance,
					 guint			 handler_id);
void	 g_signal_handler_unblock	(gpointer		 instance,
					 guint			 handler_id);
guint	 g_signal_handler_find		(gpointer		 instance,
					 GSignalMatchType	 mask,
					 guint			 signal_id,
					 GQuark			 detail,
					 GClosure		*closure,
					 gpointer		 func,
					 gpointer		 data);
gboolean g_signal_has_handler_pending	(gpointer		 instance,
					 guint			 signal_id,
					 GQuark			 detail,
					 gboolean		 may_be_blocked);


/* --- signal emissions --- */
void	g_signal_stop_emission		(gpointer		 instance,
					 guint			 signal_id,
					 GQuark			 detail);
guint	g_signal_add_emission_hook_full	(guint			 signal_id,
					 GClosure		*closure);
void	g_signal_remove_emission_hook	(guint			 signal_id,
					 guint			 hook_id);

/*< private >*/
void	g_signal_handlers_destroy	(gpointer		instance);
void	g_signals_destroy		(GType			itype);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_SIGNAL_H__ */
