#include <gio/gio.h>

int main(int argc, char *argv[])
{
  /* Verify we can link on gio symbols */
  GApplication *app = g_application_new (NULL, 0);
  g_object_unref (app);
  return 0;
}
