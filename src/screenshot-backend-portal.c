/* screenshot-backend-portal.c - XDG desktop portal backend
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

/* org.gnome.Shell.Screenshot is restricted to a fixed list of callers, which a
 * renamed fork is not on, so under Wayland the only route left is the portal.
 * It hands back one image of the whole desktop and offers no area, window or
 * cursor options, so a monitor capture is done by cropping that image here.
 */

#include "config.h"

#include "screenshot-backend-portal.h"

#include "screenshot-config.h"
#include "screenshot-monitors.h"

#include <glib/gstdio.h>

/* Long enough for the user to answer the portal's permission prompt. */
#define PORTAL_TIMEOUT_SECONDS 120

struct _ScreenshotBackendPortal
{
  GObject parent_instance;
};

static void screenshot_backend_portal_backend_init (ScreenshotBackendInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ScreenshotBackendPortal, screenshot_backend_portal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SCREENSHOT_TYPE_BACKEND, screenshot_backend_portal_backend_init))

typedef struct
{
  GMainLoop *loop;
  gchar *uri;
  gboolean failed;
} PortalCall;

static void
response_cb (GDBusConnection *connection,
             const gchar     *sender_name,
             const gchar     *object_path,
             const gchar     *interface_name,
             const gchar     *signal_name,
             GVariant        *parameters,
             gpointer         user_data)
{
  PortalCall *call = user_data;
  g_autoptr(GVariant) results = NULL;
  guint32 code = 1;

  g_variant_get (parameters, "(u@a{sv})", &code, &results);

  /* 0 succeeded, 1 cancelled by the user, 2 ended some other way. */
  if (code != 0 || !g_variant_lookup (results, "uri", "s", &call->uri))
    call->failed = TRUE;

  g_main_loop_quit (call->loop);
}

static gboolean
timeout_cb (gpointer user_data)
{
  PortalCall *call = user_data;

  g_warning ("The screenshot portal did not respond within %d seconds.",
             PORTAL_TIMEOUT_SECONDS);
  call->failed = TRUE;
  g_main_loop_quit (call->loop);

  return G_SOURCE_REMOVE;
}

/* The portal always returns the whole desktop, in device pixels. Monitor
 * geometry is in logical pixels, so derive the ratio from the image itself
 * rather than trusting a scale factor that may be fractional.
 */
static GdkPixbuf *
crop_to_rectangle (GdkPixbuf    *full,
                   GdkRectangle *rectangle)
{
  GdkRectangle desktop, want;
  g_autoptr(GdkPixbuf) sub = NULL;
  gdouble scale = 1.0;

  if (rectangle == NULL)
    return g_object_ref (full);

  if (screenshot_monitors_get_desktop_bounds (&desktop) && desktop.width > 0)
    scale = (gdouble) gdk_pixbuf_get_width (full) / (gdouble) desktop.width;

  want.x = (gint) ((rectangle->x - desktop.x) * scale + 0.5);
  want.y = (gint) ((rectangle->y - desktop.y) * scale + 0.5);
  want.width = (gint) (rectangle->width * scale + 0.5);
  want.height = (gint) (rectangle->height * scale + 0.5);

  /* A compositor may report a desktop larger than the image it returns. */
  want.x = CLAMP (want.x, 0, gdk_pixbuf_get_width (full) - 1);
  want.y = CLAMP (want.y, 0, gdk_pixbuf_get_height (full) - 1);
  want.width = MIN (want.width, gdk_pixbuf_get_width (full) - want.x);
  want.height = MIN (want.height, gdk_pixbuf_get_height (full) - want.y);

  if (want.width <= 0 || want.height <= 0)
    {
      g_warning ("The selected monitor lies outside the captured desktop.");
      return g_object_ref (full);
    }

  sub = gdk_pixbuf_new_subpixbuf (full, want.x, want.y, want.width, want.height);

  /* Detach from the full-desktop pixels so the large image can be freed. */
  return gdk_pixbuf_copy (sub);
}

static GdkPixbuf *
screenshot_backend_portal_get_pixbuf (ScreenshotBackend *backend,
                                      GdkRectangle      *rectangle)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GdkPixbuf) full = NULL;
  g_autofree gchar *token = NULL;
  g_autofree gchar *sender = NULL;
  g_autofree gchar *handle = NULL;
  g_autofree gchar *path = NULL;
  GDBusConnection *connection;
  GVariantBuilder options;
  PortalCall call = { NULL, NULL, FALSE };
  guint subscription, timeout;

  connection = g_application_get_dbus_connection (g_application_get_default ());
  if (connection == NULL)
    return NULL;

  if (screenshot_config->take_window_shot)
    g_warning ("The screenshot portal cannot capture a single window; "
               "capturing the whole screen instead.");

  /* The Response signal can arrive before the method call returns, so the
   * Request path is derived up front and subscribed to first. It is built the
   * way the portal specifies: the unique name without its leading colon and
   * with dots turned into underscores.
   */
  token = g_strdup_printf ("better_screenshot_%u", g_random_int ());
  sender = g_strdup (g_dbus_connection_get_unique_name (connection) + 1);
  g_strdelimit (sender, ".", '_');
  handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s",
                            sender, token);

  call.loop = g_main_loop_new (NULL, FALSE);
  subscription = g_dbus_connection_signal_subscribe (connection,
                                                     "org.freedesktop.portal.Desktop",
                                                     "org.freedesktop.portal.Request",
                                                     "Response",
                                                     handle,
                                                     NULL,
                                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                                     response_cb,
                                                     &call,
                                                     NULL);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token",
                         g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "interactive",
                         g_variant_new_boolean (FALSE));
  g_variant_builder_add (&options, "{sv}", "modal",
                         g_variant_new_boolean (FALSE));

  reply = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.portal.Desktop",
                                       "/org/freedesktop/portal/desktop",
                                       "org.freedesktop.portal.Screenshot",
                                       "Screenshot",
                                       g_variant_new ("(sa{sv})", "", &options),
                                       G_VARIANT_TYPE ("(o)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &error);

  if (error != NULL)
    {
      g_warning ("Unable to ask the screenshot portal: %s", error->message);
      g_dbus_connection_signal_unsubscribe (connection, subscription);
      g_main_loop_unref (call.loop);

      return NULL;
    }

  timeout = g_timeout_add_seconds (PORTAL_TIMEOUT_SECONDS, timeout_cb, &call);
  g_main_loop_run (call.loop);
  g_source_remove (timeout);

  g_dbus_connection_signal_unsubscribe (connection, subscription);
  g_main_loop_unref (call.loop);

  if (call.failed || call.uri == NULL)
    {
      g_free (call.uri);

      return NULL;
    }

  path = g_filename_from_uri (call.uri, NULL, &error);
  g_free (call.uri);

  if (path == NULL)
    {
      g_warning ("The portal returned an unusable location: %s", error->message);

      return NULL;
    }

  full = gdk_pixbuf_new_from_file (path, &error);

  /* The portal writes a temporary file that it expects the caller to remove. */
  g_unlink (path);

  if (full == NULL)
    {
      g_warning ("Unable to read the screenshot the portal produced: %s",
                 error->message);

      return NULL;
    }

  return crop_to_rectangle (full, rectangle);
}

static void
screenshot_backend_portal_class_init (ScreenshotBackendPortalClass *klass)
{
}

static void
screenshot_backend_portal_init (ScreenshotBackendPortal *self)
{
}

static void
screenshot_backend_portal_backend_init (ScreenshotBackendInterface *iface)
{
  iface->get_pixbuf = screenshot_backend_portal_get_pixbuf;
}

ScreenshotBackend *
screenshot_backend_portal_new (void)
{
  return g_object_new (SCREENSHOT_TYPE_BACKEND_PORTAL, NULL);
}
