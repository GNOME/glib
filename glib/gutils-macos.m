#include "config.h"

#ifndef HAVE_COCOA
#error "Can only build gutils-macos.m on MacOS"
#endif

#include <Cocoa/Cocoa.h>
#include "gutils.h"
#include "gstrfuncs.h"

static gchar *
find_folder (NSSearchPathDirectory type)
{
  gchar *filename;
  NSString *path;
  NSArray *paths;

  paths = NSSearchPathForDirectoriesInDomains (type, NSUserDomainMask, YES);
  path = [paths firstObject];
  if (path == nil)
    {
      return NULL;
    }

  filename = g_strdup ([path UTF8String]);

  return filename;
}

void
load_user_special_dirs_macos(gchar **table)
{
  table[G_USER_DIRECTORY_DESKTOP] = find_folder (NSDesktopDirectory);
  table[G_USER_DIRECTORY_DOCUMENTS] = find_folder (NSDocumentDirectory);
  table[G_USER_DIRECTORY_DOWNLOAD] = find_folder (NSDownloadsDirectory);
  table[G_USER_DIRECTORY_MUSIC] = find_folder (NSMusicDirectory);
  table[G_USER_DIRECTORY_PICTURES] = find_folder (NSPicturesDirectory);
  table[G_USER_DIRECTORY_PUBLIC_SHARE] = find_folder (NSSharedPublicDirectory);
  table[G_USER_DIRECTORY_TEMPLATES] = NULL;
  table[G_USER_DIRECTORY_VIDEOS] = find_folder (NSMoviesDirectory);
}