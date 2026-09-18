/* screenshot-monitors.c - Physical monitor enumeration helpers
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

#include <glib/gi18n.h>

#include "screenshot-monitors.h"

static GdkMonitor *
get_monitor (gint index)
{
  GdkDisplay *display = gdk_display_get_default ();

  if (display == NULL || index < 0)
    return NULL;

  return gdk_display_get_monitor (display, index);
}

gint
screenshot_monitors_get_n (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  if (display == NULL)
    return 0;

  return gdk_display_get_n_monitors (display);
}

gint
screenshot_monitors_get_primary_index (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkMonitor *primary;
  gint i, n;

  if (display == NULL)
    return SCREENSHOT_MONITOR_ALL;

  n = gdk_display_get_n_monitors (display);
  if (n <= 0)
    return SCREENSHOT_MONITOR_ALL;

  primary = gdk_display_get_primary_monitor (display);

  for (i = 0; i < n; i++)
    {
      if (gdk_display_get_monitor (display, i) == primary)
        return i;
    }

  /* Nothing is flagged as primary, which happens on some Wayland compositors.
   * The first monitor is a better guess than capturing every screen.
   */
  return 0;
}

const gchar *
screenshot_monitors_get_connector (gint index)
{
  GdkMonitor *monitor = get_monitor (index);
  const gchar *model;

  if (monitor == NULL)
    return NULL;

  /* GTK3 has no gdk_monitor_get_connector(); on both X11 and Wayland the
   * connector name ("DP-5") is reported as the monitor model. It is the only
   * identifier here that stays stable when monitors are replugged, which is
   * why it, rather than the index, is what gets persisted.
   */
  model = gdk_monitor_get_model (monitor);

  if (model == NULL || *model == '\0')
    return NULL;

  return model;
}

gchar *
screenshot_monitors_get_label (gint index)
{
  GdkMonitor *monitor = get_monitor (index);
  GdkRectangle geometry;
  const gchar *connector;

  if (monitor == NULL)
    return NULL;

  gdk_monitor_get_geometry (monitor, &geometry);
  connector = screenshot_monitors_get_connector (index);

  if (connector == NULL)
    {
      /* Translators: shown in the monitor list when the connector name is
       * unavailable. %d is the monitor number, then its width and height.
       */
      return g_strdup_printf (_("Monitor %d — %d × %d"),
                              index + 1, geometry.width, geometry.height);
    }

  if (gdk_monitor_is_primary (monitor))
    {
      /* Translators: an entry in the monitor list. %s is a connector name
       * such as "HDMI-0", followed by the monitor width and height.
       */
      return g_strdup_printf (_("%s — %d × %d (primary)"),
                              connector, geometry.width, geometry.height);
    }

  /* Translators: an entry in the monitor list. %s is a connector name such
   * as "DP-5", followed by the monitor width and height.
   */
  return g_strdup_printf (_("%s — %d × %d"),
                          connector, geometry.width, geometry.height);
}

gint
screenshot_monitors_find_by_connector (const gchar *connector)
{
  gint i, n;

  if (connector == NULL || *connector == '\0')
    return SCREENSHOT_MONITOR_UNKNOWN;

  if (g_strcmp0 (connector, SCREENSHOT_MONITOR_ALL_CONNECTOR) == 0)
    return SCREENSHOT_MONITOR_ALL;

  n = screenshot_monitors_get_n ();

  for (i = 0; i < n; i++)
    {
      if (g_strcmp0 (screenshot_monitors_get_connector (i), connector) == 0)
        return i;
    }

  return SCREENSHOT_MONITOR_UNKNOWN;
}

gint
screenshot_monitors_resolve_saved (const gchar *saved_connector)
{
  gint index;

  /* No stored preference yet: start on the primary monitor rather than on the
   * whole desktop, which is the case this fork exists to avoid.
   */
  if (saved_connector == NULL || *saved_connector == '\0')
    return screenshot_monitors_get_primary_index ();

  index = screenshot_monitors_find_by_connector (saved_connector);

  /* The remembered monitor was unplugged, powered off or docked elsewhere. */
  if (index == SCREENSHOT_MONITOR_UNKNOWN)
    return screenshot_monitors_get_primary_index ();

  return index;
}

/* Accepts either an index ("1") or a connector name ("DP-5"). The latter is
 * what gets persisted and what survives replugging, so it is the better thing
 * to put in a key binding.
 */
gint
screenshot_monitors_parse_spec (const gchar *spec)
{
  gchar *end = NULL;
  gint64 index;

  if (spec == NULL || *spec == '\0')
    return SCREENSHOT_MONITOR_UNKNOWN;

  index = g_ascii_strtoll (spec, &end, 10);

  /* Consumed the whole string, so it was a plain number. */
  if (end != spec && *end == '\0')
    {
      if (index < 0 || index >= screenshot_monitors_get_n ())
        return SCREENSHOT_MONITOR_UNKNOWN;

      return (gint) index;
    }

  return screenshot_monitors_find_by_connector (spec);
}

gchar *
screenshot_monitors_get_summary (void)
{
  GString *summary = g_string_new (NULL);
  gint i, n;

  n = screenshot_monitors_get_n ();

  for (i = 0; i < n; i++)
    {
      g_autofree gchar *label = screenshot_monitors_get_label (i);

      if (i > 0)
        g_string_append (summary, ", ");

      g_string_append_printf (summary, "%d: %s", i, label);
    }

  return g_string_free (summary, FALSE);
}

gboolean
screenshot_monitors_get_geometry (gint          index,
                                  GdkRectangle *geometry)
{
  GdkMonitor *monitor = get_monitor (index);

  if (monitor == NULL)
    return FALSE;

  /* Logical ("application") coordinates. Both consumers work in that space:
   * the X11 backend passes them to gdk_pixbuf_get_from_window(), and the Shell
   * backend passes them to org.gnome.Shell.Screenshot.ScreenshotArea(). No
   * scale-factor conversion belongs here.
   */
  gdk_monitor_get_geometry (monitor, geometry);

  return TRUE;
}

/* The union of every monitor, in logical coordinates: the area a full-desktop
 * capture covers.
 */
gboolean
screenshot_monitors_get_desktop_bounds (GdkRectangle *bounds)
{
  gint i, n;

  n = screenshot_monitors_get_n ();
  if (n <= 0)
    return FALSE;

  for (i = 0; i < n; i++)
    {
      GdkRectangle geometry;

      if (!screenshot_monitors_get_geometry (i, &geometry))
        continue;

      if (i == 0)
        *bounds = geometry;
      else
        gdk_rectangle_union (bounds, &geometry, bounds);
    }

  return TRUE;
}
