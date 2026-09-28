/* screenshot-clipboard.c - Keep a screenshot on the clipboard after exit
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

/* Clipboard contents belong to the process that offered them: pasting asks
 * that process for the data. Better Screenshot exits right after a capture,
 * and GNOME has no clipboard manager that keeps images (mutter's built-in one
 * keeps only text), so an image set by the application itself vanishes the
 * moment it quits.
 *
 * The application therefore re-executes itself as a small helper, pipes the
 * image to it as PNG and exits as usual. The helper owns the clipboard until
 * something else is copied, then quits, just as xclip does.
 *
 * The helper is deliberately not a GApplication: a lingering primary instance
 * would receive the next launch of the application instead of letting it
 * start afresh.
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "screenshot-clipboard.h"

gboolean
screenshot_clipboard_hand_over (GdkPixbuf *pixbuf)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) helper = NULL;
  g_autofree gchar *self_path = NULL;
  g_autofree gchar *png = NULL;
  gsize png_size;
  GOutputStream *input;

  self_path = g_file_read_link ("/proc/self/exe", &error);
  if (self_path == NULL)
    {
      g_warning ("Unable to copy the screenshot to the clipboard: %s",
                 error->message);
      return FALSE;
    }

  /* Speed matters more than size here: the data only travels down a pipe. */
  if (!gdk_pixbuf_save_to_buffer (pixbuf, &png, &png_size, "png", &error,
                                  "compression", "1", NULL))
    {
      g_warning ("Unable to copy the screenshot to the clipboard: %s",
                 error->message);
      return FALSE;
    }

  helper = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE, &error,
                             self_path, SCREENSHOT_CLIPBOARD_HELPER_ARG, NULL);
  if (helper == NULL)
    {
      g_warning ("Unable to copy the screenshot to the clipboard: %s",
                 error->message);
      return FALSE;
    }

  /* The helper reads everything before doing anything else, so a blocking
   * write cannot deadlock. The helper is not waited for: it outlives us.
   */
  input = g_subprocess_get_stdin_pipe (helper);
  if (!g_output_stream_write_all (input, png, png_size, NULL, NULL, &error) ||
      !g_output_stream_close (input, NULL, &error))
    {
      g_warning ("Unable to copy the screenshot to the clipboard: %s",
                 error->message);
      return FALSE;
    }

  return TRUE;
}

static GdkPixbuf *
read_pixbuf_from_stdin (GError **error)
{
  g_autoptr(GdkPixbufLoader) loader = gdk_pixbuf_loader_new ();
  guchar buffer[65536];
  gssize n_read;

  while ((n_read = read (STDIN_FILENO, buffer, sizeof buffer)) != 0)
    {
      if (n_read < 0)
        {
          if (errno == EINTR)
            continue;

          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                       "%s", g_strerror (errno));
          gdk_pixbuf_loader_close (loader, NULL);
          return NULL;
        }

      if (!gdk_pixbuf_loader_write (loader, buffer, n_read, error))
        {
          gdk_pixbuf_loader_close (loader, NULL);
          return NULL;
        }
    }

  if (!gdk_pixbuf_loader_close (loader, error))
    return NULL;

  return g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader));
}

static void
clipboard_get_cb (GtkClipboard     *clipboard,
                  GtkSelectionData *selection_data,
                  guint             info,
                  gpointer          user_data)
{
  gtk_selection_data_set_pixbuf (selection_data, user_data);
}

static void
clipboard_clear_cb (GtkClipboard *clipboard,
                    gpointer      user_data)
{
  /* Something else was copied: nobody can paste our image any more. */
  gtk_main_quit ();
}

int
screenshot_clipboard_serve (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  GtkTargetList *target_list;
  GtkTargetEntry *targets;
  GtkClipboard *clipboard;
  gint n_targets;
  gboolean owned;

  pixbuf = read_pixbuf_from_stdin (&error);
  if (pixbuf == NULL)
    {
      g_printerr ("Clipboard helper: unable to read the screenshot: %s\n",
                  error->message);
      return EXIT_FAILURE;
    }

  /* A Wayland client may only set the clipboard while it has keyboard focus,
   * which a helper without windows never has. X11 has no such rule, and under
   * Wayland GNOME forwards the X11 clipboard of Xwayland clients.
   */
  gdk_set_allowed_backends ("x11");

  if (!gtk_init_check (NULL, NULL))
    {
      g_printerr ("Clipboard helper: unable to open the X11 display\n");
      return EXIT_FAILURE;
    }

  target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (target_list, 0, TRUE);
  targets = gtk_target_table_new_from_list (target_list, &n_targets);
  gtk_target_list_unref (target_list);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);
  owned = gtk_clipboard_set_with_data (clipboard, targets, n_targets,
                                       clipboard_get_cb, clipboard_clear_cb,
                                       pixbuf);
  gtk_target_table_free (targets, n_targets);

  if (!owned)
    {
      g_printerr ("Clipboard helper: unable to take the clipboard\n");
      return EXIT_FAILURE;
    }

  gtk_main ();

  return EXIT_SUCCESS;
}
