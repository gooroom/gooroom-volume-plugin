/*
 * Origianl work Copyright (c) 2014-2017 Andrzej <andrzejr@xfce.org>
 *                             2017      Viktor Odintsev <zakhams@gmail.com>
 *                             2017      Matthieu Mota <matthieumota@gmail.com>
 *
 * Modified work Copyright (c) 2017 Gooroom Project Team
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <keybinder.h>
#include <canberra-gtk.h>

#include <libnotify/notify.h>

#include "volume-plugin.h"

#include "pulseaudio-config.h"
#include "pulseaudio-volume.h"

#define PANEL_TRAY_ICON_SIZE            22

#define VOLUME_PLUGIN_RAISE_VOLUME_KEY  "XF86AudioRaiseVolume"
#define VOLUME_PLUGIN_LOWER_VOLUME_KEY  "XF86AudioLowerVolume"
#define VOLUME_PLUGIN_MUTE_KEY          "XF86AudioMute"




struct _VolumePluginClass
{
  XfcePanelPluginClass __parent__;
};

/* plugin structure */
struct _VolumePlugin
{
	XfcePanelPlugin   __parent__;

	PulseaudioConfig  *config;
	PulseaudioVolume  *volume;

	GtkWidget         *button;
	GtkWidget         *img_tray;
	GtkWidget         *scale;
	GtkWidget         *popup_window;
	GtkWidget         *popup_vol_icon;

	NotifyNotification* notification;

	const gchar       *icon_name;
};


/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (VolumePlugin, volume_plugin)

static void
notify_notification (NotifyNotification *notification,
                     const gchar        *icon,
                     gint                value)
{
    GdkPixbuf *pix = NULL;

	pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                    icon, 32,
                                    GTK_ICON_LOOKUP_FORCE_SIZE,
                                    NULL);

	notify_notification_set_image_from_pixbuf (notification, pix);
	notify_notification_set_hint_int32 (notification, "value", value);
	notify_notification_show (notification, NULL);
}

static void
notify_volume_notification (NotifyNotification *notification,
                            gboolean            muted,
                            gdouble             volume)
{
	const gchar *icon;

	if (muted) {
		icon = "gooroom-volume-muted-symbolic";
	} else {
		if (volume== 0) {
			icon = "gooroom-volume-low-symbolic";
		} else if (volume < 34) {
			icon = "gooroom-volume-low-symbolic";
		} else if (volume < 67) {
			icon = "gooroom-volume-medium-symbolic";
		} else {
			icon = "gooroom-volume-high-symbolic";
		}
	}

	notify_notification (notification, icon, volume);
}

static void
volume_plugin_volume_key_pressed (const char *keystring, gpointer data)
{
	VolumePlugin *plugin      = VOLUME_PLUGIN (data);
	gdouble       volume      = pulseaudio_volume_get_volume (plugin->volume);
	gdouble       volume_step = pulseaudio_config_get_volume_step (plugin->config) / 100.0;
	gboolean      muted       = pulseaudio_volume_get_muted (plugin->volume);
	gdouble new_volume;

	if (muted) {
		new_volume = volume;
	} else {
		if (g_strcmp0 (keystring, VOLUME_PLUGIN_RAISE_VOLUME_KEY) == 0)
			new_volume = MIN (volume + volume_step, MAX (volume, 1.0));
		else if (strcmp (keystring, VOLUME_PLUGIN_LOWER_VOLUME_KEY) == 0)
			new_volume = volume - volume_step;
		else
			return;
	}

	/* Play a sound! */
	ca_gtk_play_for_widget (GTK_WIDGET (plugin), 0,
			CA_PROP_EVENT_ID, "audio-volume-change",
			NULL);

	pulseaudio_volume_set_volume (plugin->volume, new_volume);

	notify_volume_notification (plugin->notification, muted, new_volume * 100.0);
}

static void
volume_plugin_mute_pressed (const char *keystring, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	gdouble volume = pulseaudio_volume_get_volume (plugin->volume);
	gboolean muted = pulseaudio_volume_get_muted (plugin->volume);

	pulseaudio_volume_set_muted (plugin->volume, !muted);

	/* Play a sound! */
	ca_gtk_play_for_widget (GTK_WIDGET (plugin), 0,
			CA_PROP_EVENT_ID, "audio-volume-change",
			NULL);

	notify_volume_notification (plugin->notification, !muted, volume * 100.0);

	pulseaudio_volume_toggle_muted (plugin->volume);
}

static gboolean
volume_plugin_bind_keys (VolumePlugin *plugin)
{
	g_return_val_if_fail (IS_VOLUME_PLUGIN (plugin), FALSE);

	gboolean success;

	success = (keybinder_bind (VOLUME_PLUGIN_LOWER_VOLUME_KEY, volume_plugin_volume_key_pressed, plugin) &&
			keybinder_bind (VOLUME_PLUGIN_RAISE_VOLUME_KEY, volume_plugin_volume_key_pressed, plugin) &&
			keybinder_bind (VOLUME_PLUGIN_MUTE_KEY, volume_plugin_mute_pressed, plugin));

	if (!success)
		g_warning ("Could not have grabbed volume control keys. Is another volume control application (xfce4-volumed) running?");

	return success;
}

static void
volume_plugin_unbind_keys (VolumePlugin *plugin)
{
	g_return_if_fail (IS_VOLUME_PLUGIN (plugin));

	keybinder_unbind (VOLUME_PLUGIN_LOWER_VOLUME_KEY, volume_plugin_volume_key_pressed);
	keybinder_unbind (VOLUME_PLUGIN_RAISE_VOLUME_KEY, volume_plugin_volume_key_pressed);
	keybinder_unbind (VOLUME_PLUGIN_MUTE_KEY, volume_plugin_mute_pressed);
}

static void
volume_plugin_bind_keys_cb (PulseaudioConfig *config, gpointer data)
{
	g_return_if_fail (IS_VOLUME_PLUGIN (data));

	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	if (pulseaudio_config_get_enable_keyboard_shortcuts (config))
		volume_plugin_bind_keys (plugin);
	else
		volume_plugin_unbind_keys (plugin);
}

static void
popup_volume_button_icon_update (VolumePlugin *plugin)
{
	gboolean     muted;
	gboolean     connected;
	const gchar *icon_name;

	connected = pulseaudio_volume_get_connected (plugin->volume);
	muted = pulseaudio_volume_get_muted (plugin->volume);

	if (!connected || muted) {
		icon_name = "gooroom-volume-muted-symbolic";
	} else {
		gdouble volume = pulseaudio_volume_get_volume (plugin->volume);

		if (volume <= 0.0)
			icon_name = "gooroom-volume-zero-symbolic";
		else if (volume <= 0.3)
			icon_name = "gooroom-volume-low-symbolic";
		else if (volume <= 0.7)
			icon_name = "gooroom-volume-medium-symbolic";
		else
			icon_name = "gooroom-volume-high-symbolic";
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (plugin->popup_vol_icon), icon_name, GTK_ICON_SIZE_BUTTON);
}

static void
volume_button_icon_update (VolumePlugin *plugin, gboolean force_update)
{
	gboolean     connected;
	gboolean     muted;
	const gchar *icon_name;

	muted = pulseaudio_volume_get_muted (plugin->volume);
	connected = pulseaudio_volume_get_connected (plugin->volume);

	if (!connected || muted) {
		icon_name = "gooroom-volume-muted-panel-symbolic";
	} else {
		gdouble volume = pulseaudio_volume_get_volume (plugin->volume);
		if (volume <= 0.0)
			icon_name = "gooroom-volume-zero-panel-symbolic";
		else if (volume <= 0.3)
			icon_name = "gooroom-volume-low-panel-symbolic";
		else if (volume <= 0.7)
			icon_name = "gooroom-volume-medium-panel-symbolic";
		else
			icon_name = "gooroom-volume-high-panel-symbolic";
	}

	if (force_update || icon_name != plugin->icon_name) {
		plugin->icon_name = icon_name;

		GdkPixbuf *pix = NULL;
		pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                     icon_name, PANEL_TRAY_ICON_SIZE, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

		if (pix) {
			gtk_image_set_from_pixbuf (GTK_IMAGE (plugin->img_tray), pix);
			g_object_unref (G_OBJECT (pix));
		}
	}
}

static void
on_mute_button_clicked (GtkToggleButton *button, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	gboolean muted = pulseaudio_volume_get_muted (plugin->volume);

	gtk_widget_set_sensitive (plugin->scale, muted);
	pulseaudio_volume_set_muted (plugin->volume, !muted);

	popup_volume_button_icon_update (plugin);

	/* Play a sound! */
	ca_gtk_play_for_widget (GTK_WIDGET (plugin), 0,
			CA_PROP_EVENT_ID, "audio-volume-change",
			NULL);
}

static gboolean
on_scale_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	/* Play a sound! */
	ca_gtk_play_for_widget (GTK_WIDGET (plugin), 0,
			CA_PROP_EVENT_ID, "audio-volume-change",
			NULL);

	return FALSE;
}

static void
on_scale_value_changed (GtkRange *range, gpointer data)
{
	gdouble new_volume;
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	if (plugin->notification)
		notify_notification_close (plugin->notification, NULL);

	new_volume = gtk_range_get_value (GTK_RANGE (range));
	pulseaudio_volume_set_volume (plugin->volume, new_volume / 100.0);
}

static void
on_volume_changed (PulseaudioVolume *volume, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	g_signal_handlers_block_by_func (G_OBJECT (plugin->scale), on_scale_value_changed, data);

	gtk_range_set_value (GTK_RANGE (plugin->scale), pulseaudio_volume_get_volume (volume) * 100.0);

	popup_volume_button_icon_update (plugin);

	g_signal_handlers_unblock_by_func (G_OBJECT (plugin->scale), on_scale_value_changed, data);

	volume_button_icon_update (plugin, FALSE);
}

static void
on_popup_window_realized (GtkWidget *widget, gpointer data)
{
	gint x, y;
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (plugin), widget, plugin->button, &x, &y);
	gtk_window_move (GTK_WINDOW (widget), x, y);
}

static gboolean
on_popup_window_closed (gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	gtk_widget_destroy (plugin->popup_window);
	plugin->popup_window = NULL;
	plugin->popup_vol_icon = NULL;

	xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (plugin), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), FALSE);

	return TRUE;
}

static gboolean
on_popup_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape) {
		on_popup_window_closed (data);
		return TRUE;
	}

	return FALSE;
}

static gboolean
close_popup_window (gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	if (plugin->popup_window != NULL)
		on_popup_window_closed (plugin);

	return FALSE;
}

static GtkWidget *
popup_volume_window (VolumePlugin *plugin)
{
	GtkWidget *window;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_UTILITY);
	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW (window), TRUE);
	gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
	gtk_window_stick(GTK_WINDOW (window));
	gtk_window_set_screen (GTK_WINDOW (window), gtk_widget_get_screen (GTK_WIDGET (plugin)));

	gtk_widget_add_events (window, GDK_KEY_PRESS_MASK);

	GtkWidget *main_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), main_vbox);

	GtkWidget *ebox = gtk_event_box_new ();
	gtk_widget_set_name (ebox, "panel-popup-window-frame");
	gtk_box_pack_start (GTK_BOX (main_vbox), ebox, FALSE, FALSE, 0);

	GtkWidget *alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);
	gtk_container_add (GTK_CONTAINER (ebox), alignment);

	GtkWidget *title = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (title), _("<big><b>Volume Control</b></big>"));
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_container_add (GTK_CONTAINER (alignment), title);

	GtkWidget *hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
	gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

	GtkWidget *button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus (button, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	plugin->popup_vol_icon = gtk_image_new_from_icon_name ("gooroom-volume-muted-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (button), plugin->popup_vol_icon);

	plugin->scale = gtk_hscale_new_with_range (0.0, 100.0, 1.0);
	gtk_widget_set_can_focus (plugin->scale, FALSE);
	gtk_widget_set_size_request (plugin->scale, 224, -1);
	gtk_range_set_inverted (GTK_RANGE (plugin->scale), FALSE);
	gtk_scale_set_draw_value (GTK_SCALE (plugin->scale), FALSE);
	gtk_range_set_round_digits (GTK_RANGE (plugin->scale), 0);
	gtk_box_pack_start (GTK_BOX (hbox), plugin->scale, TRUE, TRUE, 0);

	gboolean muted = pulseaudio_volume_get_muted (plugin->volume);

	const gchar *icon_name = muted ? "gooroom-volume-muted-symbolic" : "gooroom-volume-high-symbolic";

	gtk_image_set_from_icon_name (GTK_IMAGE (plugin->popup_vol_icon), icon_name, GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_sensitive (plugin->scale, !muted);

	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (on_mute_button_clicked), plugin);
	g_signal_connect (G_OBJECT (plugin->scale), "value-changed", G_CALLBACK (on_scale_value_changed), plugin);
	g_signal_connect (G_OBJECT (plugin->scale), "button-release-event", G_CALLBACK (on_scale_button_release_event), plugin);

	g_signal_connect (G_OBJECT (window), "realize", G_CALLBACK (on_popup_window_realized), plugin);
	g_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK (on_popup_key_press_event), plugin);
	g_signal_connect_swapped (G_OBJECT (window), "delete-event", G_CALLBACK (on_popup_window_closed), plugin);
	g_signal_connect_swapped (G_OBJECT (window), "focus-out-event", G_CALLBACK (on_popup_window_closed), plugin);

	if (pulseaudio_volume_get_connected (plugin->volume))
		on_volume_changed (plugin->volume, plugin);
	else
		gtk_widget_set_sensitive (window, FALSE);

	gtk_widget_show_all (window);

	xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (plugin), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), TRUE);

	if (plugin->notification)
		notify_notification_close (plugin->notification, NULL);

	return window;
}

static gboolean
on_volume_button_icon_update_timeout (gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	volume_button_icon_update (plugin, TRUE);

	g_signal_connect (G_OBJECT (plugin->volume), "volume-changed", G_CALLBACK (on_volume_changed), plugin);

	return FALSE;
}


static gboolean
on_volume_button_pressed (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	if (event->button == 1 || event->button == 2) {
		if (event->type == GDK_BUTTON_PRESS) {
			if (plugin->popup_window != NULL) {
				on_popup_window_closed (plugin);
			} else {
				plugin->popup_window = popup_volume_window (plugin);
			}

			return TRUE;
		}
	}

	/* bypass GTK_TOGGLE_BUTTON's handler and go directly to the plugin's one */
	return (*GTK_WIDGET_CLASS (volume_plugin_parent_class)->button_press_event) (GTK_WIDGET (plugin), event);
}

static void
volume_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (panel_plugin);

	/* release keybindings */
	volume_plugin_unbind_keys (plugin);

	close_popup_window (plugin);
}

static gboolean
volume_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                            gint             size)
{
	if (xfce_panel_plugin_get_mode (panel_plugin) == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL) {
		gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), -1, size);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), size, -1);
	}

	return TRUE;
}

static void
volume_plugin_mode_changed (XfcePanelPlugin *plugin, XfcePanelPluginMode mode)
{
	volume_plugin_size_changed (plugin, xfce_panel_plugin_get_size (plugin));
}

static void
volume_plugin_init (VolumePlugin *plugin)
{
	plugin->volume         = NULL;
	plugin->button         = NULL;
	plugin->img_tray       = NULL;
	plugin->scale          = NULL;
	plugin->popup_window   = NULL;
	plugin->popup_vol_icon = NULL;
	plugin->notification   = NULL;

	plugin->button = xfce_panel_create_toggle_button ();
	xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
	gtk_container_add (GTK_CONTAINER (plugin), plugin->button);

	GdkPixbuf *pix = NULL;
	pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                    "gooroom-volume-high-panel-symbolic", PANEL_TRAY_ICON_SIZE,
                                    GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

	if (pix) {
		plugin->img_tray = gtk_image_new_from_pixbuf (pix);
		gtk_container_add (GTK_CONTAINER (plugin->button), plugin->img_tray);
		g_object_unref (G_OBJECT (pix));
	}

	gtk_widget_show_all (plugin->button);

	g_signal_connect (G_OBJECT (plugin->button), "button-press-event", G_CALLBACK (on_volume_button_pressed), plugin);
}

static void
volume_plugin_construct (XfcePanelPlugin *panel_plugin)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (panel_plugin);

	xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	plugin->config = pulseaudio_config_new (xfce_panel_plugin_get_property_base (XFCE_PANEL_PLUGIN (plugin)));
	plugin->volume = pulseaudio_volume_new (plugin->config);

	notify_init ("gooroom-volume-plugin");

	plugin->notification = notify_notification_new ("gooroom-volumed-plugin", NULL, NULL);

	/* Initialize libkeybinder */
	keybinder_init ();

	g_signal_connect (G_OBJECT (plugin->config), "notify::enable-keyboard-shortcuts", G_CALLBACK (volume_plugin_bind_keys_cb), plugin);
	if (pulseaudio_config_get_enable_keyboard_shortcuts (plugin->config))
		volume_plugin_bind_keys (plugin);
	else
		volume_plugin_unbind_keys (plugin);

	g_timeout_add (200, (GSourceFunc) on_volume_button_icon_update_timeout, plugin);
}

static void
volume_plugin_class_init (VolumePluginClass *klass)
{
	XfcePanelPluginClass *plugin_class;

	plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
	plugin_class->construct = volume_plugin_construct;
	plugin_class->free_data = volume_plugin_free_data;
	plugin_class->size_changed = volume_plugin_size_changed;
	plugin_class->mode_changed = volume_plugin_mode_changed;
}
