/* screenshot-interactive-dialog.h - Interactive options dialog
 *
 * Copyright (C) 2001 Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2006 Emmanuele Bassi <ebassi@gnome.org>
 * Copyright (C) 2008, 2011 Cosimo Cecchi <cosimoc@gnome.org>
 * Copyright (C) 2013 Nils Dagsson Moskopp <nils@dieweltistgarnichtso.net>
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

#include <glib/gi18n.h>

#include "screenshot-config.h"
#include "screenshot-interactive-dialog.h"
#include "screenshot-monitors.h"
#include "screenshot-utils.h"

typedef enum {
  SCREENSHOT_MODE_SCREEN,
  SCREENSHOT_MODE_WINDOW,
  SCREENSHOT_MODE_SELECTION,
} ScreenshotMode;

struct _ScreenshotInteractiveDialog
{
  HdyApplicationWindow parent_instance;

  GtkWidget *listbox;
  GtkWidget *monitor;
  GtkWidget *monitor_row;
  GtkWidget *pointer;
  GtkWidget *pointer_row;
  GtkAdjustment *delay_adjustment;
  GtkWidget *window;
  GtkWidget *selection;
};

G_DEFINE_TYPE (ScreenshotInteractiveDialog, screenshot_interactive_dialog, HDY_TYPE_APPLICATION_WINDOW)

enum {
  SIGNAL_CAPTURE,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
set_mode (ScreenshotInteractiveDialog *self,
          ScreenshotMode               mode)
{
  gboolean take_window_shot = (mode == SCREENSHOT_MODE_WINDOW);
  gboolean take_area_shot = (mode == SCREENSHOT_MODE_SELECTION);

  gtk_widget_set_sensitive (self->pointer_row,
                            !take_area_shot && screenshot_platform_is_x11 ());

  /* Picking a monitor only means something when capturing a screen: a window
   * shot follows the window, and a selection carries its own rectangle.
   */
  gtk_widget_set_sensitive (self->monitor_row, mode == SCREENSHOT_MODE_SCREEN);

  screenshot_config->take_window_shot = take_window_shot;
  screenshot_config->take_area_shot = take_area_shot;
}

static void
screen_toggled_cb (GtkToggleButton             *button,
                   ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    set_mode (self, SCREENSHOT_MODE_SCREEN);
}

static void
window_toggled_cb (GtkToggleButton             *button,
                   ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    set_mode (self, SCREENSHOT_MODE_WINDOW);
}

static void
selection_toggled_cb (GtkToggleButton             *button,
                      ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    set_mode (self, SCREENSHOT_MODE_SELECTION);
}

static void
monitor_changed_cb (GtkComboBox                 *combo,
                    ScreenshotInteractiveDialog *self)
{
  gint active = gtk_combo_box_get_active (combo);

  if (active < 0)
    return;

  /* The monitors occupy positions 0..n-1 in the list order they were added,
   * so the position is the monitor index; the trailing entry is the whole
   * desktop.
   */
  if (active >= screenshot_monitors_get_n ())
    screenshot_config->monitor_index = SCREENSHOT_MONITOR_ALL;
  else
    screenshot_config->monitor_index = active;
}

static void
populate_monitors (ScreenshotInteractiveDialog *self)
{
  GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT (self->monitor);
  gint i, n, active;

  n = screenshot_monitors_get_n ();

  /* With a single monitor the choice is meaningless: the whole desktop and
   * that monitor are the same picture.
   */
  gtk_widget_set_visible (self->monitor_row, n > 1);

  for (i = 0; i < n; i++)
    {
      g_autofree gchar *label = screenshot_monitors_get_label (i);

      gtk_combo_box_text_append_text (combo, label);
    }

  /* Deliberately last: capturing every monitor at once is the behaviour this
   * dialog exists to make optional, so it must never be what is preselected.
   */
  gtk_combo_box_text_append_text (combo, _("Whole Desktop (All Monitors)"));

  active = screenshot_config->monitor_index;
  if (active < 0 || active >= n)
    active = n;

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);
}

static void
delay_spin_value_changed_cb (GtkSpinButton               *button,
                             ScreenshotInteractiveDialog *self)
{
  screenshot_config->delay = gtk_spin_button_get_value_as_int (button);
}

static void
include_pointer_toggled_cb (GtkSwitch                   *toggle,
                            ScreenshotInteractiveDialog *self)
{
  screenshot_config->include_pointer = gtk_switch_get_active (toggle);
  gtk_switch_set_state (toggle, gtk_switch_get_active (toggle));
}

static void
capture_button_clicked_cb (GtkButton                   *button,
                           ScreenshotInteractiveDialog *self)
{
  g_signal_emit (self, signals[SIGNAL_CAPTURE], 0);
}

static void
screenshot_interactive_dialog_class_init (ScreenshotInteractiveDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  signals[SIGNAL_CAPTURE] =
    g_signal_new ("capture",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/k-nowicki/BetterScreenshot/ui/screenshot-interactive-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, listbox);
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, monitor);
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, monitor_row);
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, pointer);
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, pointer_row);
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, delay_adjustment);
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, window);
  gtk_widget_class_bind_template_child (widget_class, ScreenshotInteractiveDialog, selection);
  gtk_widget_class_bind_template_callback (widget_class, screen_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, selection_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, monitor_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, delay_spin_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, include_pointer_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, capture_button_clicked_cb);
}

static void
screenshot_interactive_dialog_init (ScreenshotInteractiveDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  if (screenshot_config->take_window_shot)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->window), TRUE);

  if (screenshot_config->take_area_shot)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->selection), TRUE);

  populate_monitors (self);

  /* Under Wayland the capture goes through the desktop portal, which returns
   * one image of the whole desktop and offers no window or cursor options.
   * Leaving the controls live would let the user ask for something that
   * silently will not happen.
   */
  if (!screenshot_platform_is_x11 ())
    {
      gtk_widget_set_sensitive (self->window, FALSE);
      gtk_widget_set_tooltip_text (self->window,
                                   _("Capturing a single window is only available on X11."));
      gtk_widget_set_tooltip_text (self->pointer_row,
                                   _("Showing the pointer is only available on X11."));
    }

  gtk_widget_set_sensitive (self->monitor_row, !screenshot_config->take_window_shot &&
                                               !screenshot_config->take_area_shot);
  gtk_widget_set_sensitive (self->pointer_row,
                            !screenshot_config->take_area_shot &&
                            screenshot_platform_is_x11 ());
  gtk_switch_set_active (GTK_SWITCH (self->pointer), screenshot_config->include_pointer);

  gtk_adjustment_set_value (self->delay_adjustment, (gdouble) screenshot_config->delay);
}

ScreenshotInteractiveDialog *
screenshot_interactive_dialog_new (GtkApplication *app)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (app), NULL);

  return g_object_new (SCREENSHOT_TYPE_INTERACTIVE_DIALOG,
                       "application", app,
                       NULL);
}
