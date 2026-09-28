/* screenshot-clipboard.h - Keep a screenshot on the clipboard after exit
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

/* Internal argument that turns the binary into the clipboard helper. It is
 * checked in main() before GApplication runs, so it never shows up in --help.
 */
#define SCREENSHOT_CLIPBOARD_HELPER_ARG "--serve-clipboard"

gboolean screenshot_clipboard_hand_over (GdkPixbuf *pixbuf);
int      screenshot_clipboard_serve     (void);

G_END_DECLS
