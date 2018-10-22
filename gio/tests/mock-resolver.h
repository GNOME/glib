#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define MOCK_TYPE_RESOLVER (mock_resolver_get_type())
G_DECLARE_FINAL_TYPE (MockResolver, mock_resolver, MOCK, RESOLVER, GResolver)

MockResolver *mock_resolver_new (void);
void mock_resolver_set_ipv4_delay (MockResolver *self, guint delay);
void mock_resolver_set_ipv4_results (MockResolver *self, GList *results);
void mock_resolver_set_ipv4_error (MockResolver *self, GError *error);
void mock_resolver_set_ipv6_delay (MockResolver *self, guint delay);
void mock_resolver_set_ipv6_results (MockResolver *self, GList *results);
void mock_resolver_set_ipv6_error (MockResolver *self, GError *error);
G_END_DECLS
