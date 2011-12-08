#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static void
activate (GApplication *application)
{
  g_application_hold (application);
  g_print ("activated\n");
  g_application_release (application);
}

static void
show_help (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       data)
{
  g_print ("Want help, eh ?!\n");
}

static void
show_about (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  g_print ("Not much to say, really.\nJust a stupid example\n");
}

static void
quit_app (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  g_print ("Quitting...\n");
  g_application_release (g_application_get_default ());
}

static GActionEntry entries[] = {
  { "help",  show_help,  NULL, NULL, NULL },
  { "about", show_about, NULL, NULL, NULL },
  { "quit",  quit_app,   NULL, NULL, NULL }
};

static void
add_actions (GApplication *app)
{
  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   entries, G_N_ELEMENTS (entries),
                                   NULL);
}

static void
add_menu (GApplication *app)
{
  GMenu *menu;

  menu = g_menu_new ();

  g_menu_append (menu, "Help", "help");
  g_menu_append (menu, "About Example", "about");
  g_menu_append (menu, "Quit", "quit");

  g_application_set_app_menu (app, G_MENU_MODEL (menu));

  g_object_unref (menu);
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication", 0);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  add_actions (app);
  add_menu (app);

  g_application_hold (app);

  status = g_application_run (app, argc, argv);

  g_object_unref (app);

  return status;
}
