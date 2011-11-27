/*
 * Copyright © 2011 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_MENU_MARKUP_H__
#define __G_MENU_MARKUP_H__

#include <gio/gmenu.h>

G_BEGIN_DECLS

void                    g_menu_markup_parser_start                      (GMarkupParseContext  *context,
                                                                         const gchar          *domain,
                                                                         GHashTable           *objects);
GHashTable *            g_menu_markup_parser_end                        (GMarkupParseContext  *context);

void                    g_menu_markup_parser_start_menu                 (GMarkupParseContext  *context,
                                                                         const gchar          *domain,
                                                                         GHashTable           *objects);
GMenu *                 g_menu_markup_parser_end_menu                   (GMarkupParseContext  *context);

void                    g_menu_markup_print_stderr                      (GMenuModel           *model);
GString *               g_menu_markup_print_string                      (GString              *string,
                                                                         GMenuModel           *model,
                                                                         gint                  indent,
                                                                         gint                  tabstop);

G_END_DECLS

#endif /* __G_MENU_MARKUP_H__ */
