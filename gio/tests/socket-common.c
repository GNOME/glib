/* #included into both socket-client.c and socket-server.c */

static const char *unix_socket_address_types[] = {
  "invalid",
  "anonymous",
  "path",
  "abstract",
  "padded"
};

static char *
socket_address_to_string (GSocketAddress *address)
{
  char *res;

  if (G_IS_INET_SOCKET_ADDRESS (address))
    {
      GInetAddress *inet_address;
      char *str;
      int port;

      inet_address = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));
      str = g_inet_address_to_string (inet_address);
      port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (address));
      res = g_strdup_printf ("%s:%d", str, port);
      g_free (str);
    }
  else if (G_IS_UNIX_SOCKET_ADDRESS (address))
    {
      GUnixSocketAddress *uaddr = G_UNIX_SOCKET_ADDRESS (address);

      res = g_strdup_printf ("%s:%s",
			     unix_socket_address_types[g_unix_socket_address_get_address_type (uaddr)],
			     g_unix_socket_address_get_path (uaddr));
    }

  return res;
}

GSocketAddress *
socket_address_from_string (const char *name)
{
  int i, len;

  for (i = 0; i < G_N_ELEMENTS (unix_socket_address_types); i++)
    {
      len = strlen (unix_socket_address_types[i]);
      if (!strncmp (name, unix_socket_address_types[i], len) &&
	  name[len] == ':')
	{
	  return g_unix_socket_address_new_with_type (name + len + 1, -1,
						      (GUnixSocketAddressType)i);
	}
    }
  return NULL;
}
