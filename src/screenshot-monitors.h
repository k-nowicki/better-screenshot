/* screenshot-monitors.h - Physical monitor enumeration helpers
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Capture every connected monitor, i.e. the whole root window. */
#define SCREENSHOT_MONITOR_ALL (-1)

/* The requested monitor is not currently connected. */
#define SCREENSHOT_MONITOR_UNKNOWN (-2)

/* Stored in the config file to mean SCREENSHOT_MONITOR_ALL. Connector names
 * never contain this character, so it cannot collide with a real monitor.
 */
#define SCREENSHOT_MONITOR_ALL_CONNECTOR "*"

gint         screenshot_monitors_get_n             (void);
gint         screenshot_monitors_get_primary_index (void);
const gchar *screenshot_monitors_get_connector     (gint          index);
gchar       *screenshot_monitors_get_label         (gint          index);
gint         screenshot_monitors_find_by_connector (const gchar  *connector);
gint         screenshot_monitors_resolve_saved     (const gchar  *saved_connector);
gint         screenshot_monitors_parse_spec        (const gchar  *spec);
gchar       *screenshot_monitors_get_summary       (void);
gboolean     screenshot_monitors_get_geometry      (gint          index,
                                                    GdkRectangle *geometry);
gboolean     screenshot_monitors_get_desktop_bounds (GdkRectangle *bounds);

G_END_DECLS
