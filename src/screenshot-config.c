/* screenshot-config.h - Holds current configuration for better-screenshot
 *
 * Copyright (C) 2001 Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2006 Emmanuele Bassi <ebassi@gnome.org>
 * Copyright (C) 2008, 2011 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#include "config.h"

#include <errno.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "screenshot-config.h"
#include "screenshot-monitors.h"

#define DELAY_KEY               "delay"
#define INCLUDE_POINTER_KEY     "include-pointer"
#define INCLUDE_ICC_PROFILE     "include-icc-profile"
#define AUTO_SAVE_DIRECTORY_KEY "auto-save-directory"
#define LAST_SAVE_DIRECTORY_KEY "last-save-directory"
#define DEFAULT_FILE_TYPE_KEY   "default-file-type"

/* The remembered monitor lives in its own key file rather than in GSettings.
 * Adding a key to the schema would mean shipping and compiling a modified
 * schema alongside the binary, and reading a key the installed schema does not
 * define makes GLib abort the process. A key file keeps installation to a
 * single binary and cannot desynchronise.
 */
#define MONITOR_CONFIG_DIR      "better-screenshot"
#define MONITOR_CONFIG_FILE     "monitor.ini"
#define MONITOR_CONFIG_GROUP    "Monitor"
#define MONITOR_CONFIG_KEY      "connector"

ScreenshotConfig *screenshot_config;

static gchar *
monitor_config_path (void)
{
  return g_build_filename (g_get_user_config_dir (),
                           MONITOR_CONFIG_DIR, MONITOR_CONFIG_FILE, NULL);
}

static gchar *
monitor_config_load (void)
{
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autofree gchar *path = monitor_config_path ();

  /* A missing file simply means nothing has been remembered yet. */
  if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, NULL))
    return NULL;

  return g_key_file_get_string (keyfile, MONITOR_CONFIG_GROUP,
                                MONITOR_CONFIG_KEY, NULL);
}

static void
monitor_config_store (const gchar *connector)
{
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(GError) error = NULL;
  g_autofree gchar *path = monitor_config_path ();
  g_autofree gchar *dir = NULL;

  g_assert (connector != NULL);

  dir = g_path_get_dirname (path);

  if (g_mkdir_with_parents (dir, 0700) != 0)
    {
      g_warning ("Unable to create %s: %s", dir, g_strerror (errno));
      return;
    }

  g_key_file_set_string (keyfile, MONITOR_CONFIG_GROUP,
                         MONITOR_CONFIG_KEY, connector);
  g_key_file_set_comment (keyfile, MONITOR_CONFIG_GROUP, MONITOR_CONFIG_KEY,
                          " Connector name of the last monitor captured,"
                          " or \"*\" for the whole desktop.", NULL);

  if (!g_key_file_save_to_file (keyfile, path, &error))
    g_warning ("Unable to remember the selected monitor: %s", error->message);
}

static void
screenshot_store_monitor_config (void)
{
  const gchar *connector;

  if (screenshot_config->monitor_index == SCREENSHOT_MONITOR_ALL)
    connector = SCREENSHOT_MONITOR_ALL_CONNECTOR;
  else
    connector = screenshot_monitors_get_connector (screenshot_config->monitor_index);

  /* A monitor GDK cannot name is a monitor we could not find again next time;
   * keeping the previous value beats storing something unusable.
   */
  if (connector == NULL)
    return;

  monitor_config_store (connector);
}

void
screenshot_load_config (void)
{
  ScreenshotConfig *config;

  config = g_slice_new0 (ScreenshotConfig);

  config->settings = g_settings_new ("io.github.k-nowicki.BetterScreenshot");
  config->save_dir =
    g_settings_get_string (config->settings,
                           LAST_SAVE_DIRECTORY_KEY);
  config->delay =
    g_settings_get_int (config->settings,
                        DELAY_KEY);
  config->include_pointer =
    g_settings_get_boolean (config->settings,
                            INCLUDE_POINTER_KEY);
  config->file_type =
    g_settings_get_string (config->settings,
                           DEFAULT_FILE_TYPE_KEY);
  config->include_icc_profile =
    g_settings_get_boolean (config->settings,
                            INCLUDE_ICC_PROFILE);

  config->take_window_shot = FALSE;
  config->take_area_shot = FALSE;

  /* Called from GApplication::startup after the parent chain has run, so GDK
   * is already up and the connector can be resolved to an index right away.
   */
  config->monitor_connector = monitor_config_load ();
  config->monitor_index = screenshot_monitors_resolve_saved (config->monitor_connector);

  screenshot_config = config;
}

void
screenshot_save_config (void)
{
  ScreenshotConfig *c = screenshot_config;

  g_assert (c != NULL);

  /* if we were not started up in interactive mode, avoid
   * overwriting these settings.
   */
  if (!c->interactive)
    return;

  g_settings_set_boolean (c->settings,
                          INCLUDE_POINTER_KEY, c->include_pointer);

  g_settings_set_int (c->settings, DELAY_KEY, c->delay);

  screenshot_store_monitor_config ();
}

gboolean
screenshot_config_parse_command_line (gboolean clipboard_arg,
                                      gboolean window_arg,
                                      gboolean area_arg,
                                      gboolean include_border_arg,
                                      gboolean disable_border_arg,
                                      gboolean include_pointer_arg,
                                      const gchar *border_effect_arg,
                                      guint delay_arg,
                                      const gchar *monitor_arg,
                                      gboolean interactive_arg,
                                      const gchar *file_arg)
{
  if (window_arg && area_arg)
    {
      g_printerr (_("Conflicting options: --window and --area should not be "
                    "used at the same time.\n"));
      return FALSE;
    }

  if (monitor_arg != NULL && (window_arg || area_arg))
    {
      g_printerr (_("Conflicting options: --monitor cannot be combined with "
                    "--window or --area.\n"));
      return FALSE;
    }

  screenshot_config->interactive = interactive_arg;

  if (include_border_arg)
    g_warning ("Option --include-border is deprecated and will be removed in "
               "better-screenshot 3.38.0. Window border is always included.");
  if (disable_border_arg)
    g_warning ("Option --remove-border is deprecated and will be removed in "
               "better-screenshot 3.38.0. Window border is always included.");
  if (border_effect_arg != NULL)
    g_warning ("Option --border-effect is deprecated and will be removed in "
               "better-screenshot 3.38.0. No effect will be used.");

  if (screenshot_config->interactive)
    {
      if (clipboard_arg)
        g_warning ("Option --clipboard is ignored in interactive mode.");
      if (include_pointer_arg)
        g_warning ("Option --include-pointer is ignored in interactive mode.");
      if (file_arg)
        g_warning ("Option --file is ignored in interactive mode.");

      if (delay_arg > 0)
        screenshot_config->delay = delay_arg;

      /* There is no Save As dialog any more, so the last-used folder is not a
       * meaningful destination: save where unattended captures go.
       */
      g_free (screenshot_config->save_dir);
      screenshot_config->save_dir =
        g_settings_get_string (screenshot_config->settings,
                               AUTO_SAVE_DIRECTORY_KEY);
    }
  else
    {
      g_free (screenshot_config->save_dir);
      screenshot_config->save_dir =
        g_settings_get_string (screenshot_config->settings,
                               AUTO_SAVE_DIRECTORY_KEY);

      screenshot_config->delay = delay_arg;
      screenshot_config->include_pointer = include_pointer_arg;
      screenshot_config->copy_to_clipboard = clipboard_arg;
      if (file_arg != NULL)
        screenshot_config->file = g_file_new_for_commandline_arg (file_arg);

      /* Outside interactive mode the remembered monitor is deliberately not
       * applied: a bare "better-screenshot" must keep capturing everything, or
       * existing scripts and key bindings would silently change behaviour.
       * --monitor is the explicit opt-in.
       */
      screenshot_config->monitor_index = SCREENSHOT_MONITOR_ALL;
    }

  if (monitor_arg != NULL)
    {
      gint index = screenshot_monitors_parse_spec (monitor_arg);

      if (index == SCREENSHOT_MONITOR_UNKNOWN)
        {
          g_autofree gchar *summary = screenshot_monitors_get_summary ();

          g_printerr (_("No such monitor: “%s”. Connected monitors are %s.\n"),
                      monitor_arg, summary);
          return FALSE;
        }

      screenshot_config->monitor_index = index;
    }

  screenshot_config->take_window_shot = window_arg;
  screenshot_config->take_area_shot = area_arg;

  return TRUE;
}
