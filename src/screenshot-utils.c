/* screenshot-utils.c - common functions for GNOME Screenshot
 *
 * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 */

#include "config.h"

#include "screenshot-utils.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "screenshot-backend-portal.h"
#include "screenshot-backend-shell.h"

#ifdef HAVE_X11
#include "screenshot-backend-x11.h"
#include <gdk/gdkx.h>
#endif

gboolean
screenshot_platform_is_x11 (void)
{
#ifdef HAVE_X11
  return GDK_IS_X11_DISPLAY (gdk_display_get_default ());
#else
  return FALSE;
#endif
}

static ScreenshotBackend *
make_backend (const gchar *name)
{
  if (g_strcmp0 (name, "portal") == 0)
    return screenshot_backend_portal_new ();

  if (g_strcmp0 (name, "shell") == 0)
    return screenshot_backend_shell_new ();

#ifdef HAVE_X11
  if (g_strcmp0 (name, "x11") == 0)
    return screenshot_backend_x11_new ();
#endif

  return NULL;
}

GdkPixbuf *
screenshot_get_pixbuf (GdkRectangle *rectangle)
{
  const gchar *order[4] = { NULL, NULL, NULL, NULL };
  const gchar *forced;
  gint i;

  forced = g_getenv ("BETTER_SCREENSHOT_BACKEND");

  /* Kept so that the option documented by GNOME Screenshot still does what it
   * used to: bypass the compositor and read the X server directly.
   */
  if (forced == NULL && g_getenv ("GNOME_SCREENSHOT_FORCE_FALLBACK") != NULL)
    forced = "x11";

  if (forced != NULL)
    {
      order[0] = forced;
    }
  else if (screenshot_platform_is_x11 ())
    {
      /* Preferred wherever it works: no permission prompt, and the only
       * backend that can draw the pointer into a single-monitor capture.
       */
      order[0] = "x11";
      order[1] = "portal";
      order[2] = "shell";
    }
  else
    {
      /* Under Wayland the portal is the only route open to us. The
       * compositor's own org.gnome.Shell.Screenshot interface accepts a fixed
       * list of callers that this application is deliberately not part of, but
       * it is still tried last in case of a build that is on that list.
       */
      order[0] = "portal";
      order[1] = "shell";
    }

  for (i = 0; i < 4 && order[i] != NULL; i++)
    {
      g_autoptr (ScreenshotBackend) backend = make_backend (order[i]);
      GdkPixbuf *screenshot;

      if (backend == NULL)
        continue;

      screenshot = screenshot_backend_get_pixbuf (backend, rectangle);
      if (screenshot != NULL)
        return screenshot;

      if (order[i + 1] != NULL)
        g_message ("The %s backend did not work, trying %s.", order[i], order[i + 1]);
      else
        g_message ("The %s backend did not work.", order[i]);
    }

  return NULL;
}

typedef struct
{
  ScreenshotResponseFunc callback;
  gpointer user_data;
} DialogData;

static void
response_cb (GtkDialog  *dialog,
             gint        response_id,
             DialogData *data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (data->callback)
    data->callback (response_id, data->user_data);

  g_free (data);
}

void
screenshot_show_dialog (GtkWindow              *parent,
                        GtkMessageType          message_type,
                        GtkButtonsType          buttons_type,
                        const gchar            *message,
                        const gchar            *detail,
                        ScreenshotResponseFunc  callback,
                        gpointer                user_data)
{
  DialogData *data;
  GtkWidget *dialog;
  GtkWindowGroup *group;

  g_return_if_fail ((parent == NULL) || (GTK_IS_WINDOW (parent)));
  g_return_if_fail (message != NULL);

  data = g_new0 (DialogData, 1);
  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   message_type,
                                   buttons_type,
                                   "%s", message);

  if (detail)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", detail);

  if (parent)
    {
      group = gtk_window_get_group (parent);
      if (group != NULL)
        gtk_window_group_add_window (group, GTK_WINDOW (dialog));
    }

  data->callback = callback;
  data->user_data = user_data;

  if (callback)
    g_signal_connect (dialog, "response", G_CALLBACK (response_cb), data);

  gtk_window_present (GTK_WINDOW (dialog));
}

void
screenshot_display_help (GtkWindow *parent)
{
  g_autoptr(GError) error = NULL;

  gtk_show_uri_on_window (parent,
                          "help:gnome-help/screen-shot-record",
                          gtk_get_current_event_time (),
                          &error);

  if (error)
    screenshot_show_dialog (parent,
                            GTK_MESSAGE_ERROR,
                            GTK_BUTTONS_OK,
                            _("Error loading the help page"),
                            error->message,
                            NULL,
                            NULL);
}
