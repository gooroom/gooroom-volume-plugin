/*
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
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <keybinder.h>
#include <canberra-gtk.h>


#include "volume-plugin.h"
#include "pulseaudio-config.h"
#include "pulseaudio-volume.h"


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
	XfcePanelPlugin      __parent__;

	PulseaudioConfig    *config;
	PulseaudioVolume    *volume;

	GtkWidget           *button;
	GtkWidget           *image;
	GtkWidget           *scale;
	GtkWidget           *popup_window;
	GtkWidget           *popup_vol_icon;

	const gchar         *icon_name;
};


/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (VolumePlugin, volume_plugin)


static void
volume_plugin_volume_key_pressed (const char *keystring, gpointer data)
{
	VolumePlugin *plugin      = VOLUME_PLUGIN (data);
	gdouble       volume      = pulseaudio_volume_get_volume (plugin->volume);
	gdouble       volume_step = pulseaudio_config_get_volume_step (plugin->config) / 100.0;

	if (g_strcmp0 (keystring, VOLUME_PLUGIN_RAISE_VOLUME_KEY) == 0)
		pulseaudio_volume_set_volume (plugin->volume, MIN (volume + volume_step, MAX (volume, 1.0)));
	else if (strcmp (keystring, VOLUME_PLUGIN_LOWER_VOLUME_KEY) == 0)
		pulseaudio_volume_set_volume (plugin->volume, volume - volume_step);
}

static void
volume_plugin_mute_pressed (const char *keystring, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	if (pulseaudio_volume_get_muted (plugin->volume)) {
		pulseaudio_volume_set_muted (plugin->volume, FALSE);
	} else {
		pulseaudio_volume_set_muted (plugin->volume, TRUE);
	}

	pulseaudio_volume_toggle_muted (plugin->volume);
}

static gboolean
volume_plugin_bind_keys (VolumePlugin *plugin)
{
	gboolean success;
	g_return_if_fail (IS_VOLUME_PLUGIN (plugin));

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
volume_button_update (VolumePlugin *plugin, gboolean force_update)
{
	gboolean     connected;
	gboolean     muted;
	const gchar *icon_name;

	muted = pulseaudio_volume_get_muted (plugin->volume);
	connected = pulseaudio_volume_get_connected (plugin->volume);

	if (!connected || muted) {
		icon_name = "audio-volume-muted-panel-symbolic";
	} else {
		gdouble volume = pulseaudio_volume_get_volume (plugin->volume);
		if (volume <= 0.0)
			icon_name = "audio-volume-zero-panel-symbolic";
		else if (volume <= 0.3)
			icon_name = "audio-volume-low-panel-symbolic";
		else if (volume <= 0.7)
			icon_name = "audio-volume-medium-panel-symbolic";
		else
			icon_name = "audio-volume-high-panel-symbolic";
	}

	if (force_update || icon_name != plugin->icon_name) {
		plugin->icon_name = icon_name;
		gtk_image_set_from_icon_name (GTK_IMAGE (plugin->image), icon_name, GTK_ICON_SIZE_MENU);
		gtk_image_set_pixel_size (GTK_IMAGE (plugin->image), 22);
	}
}

static void
on_mute_button_clicked (GtkToggleButton *button, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	if (pulseaudio_volume_get_muted (plugin->volume)) {
		gtk_image_set_from_icon_name (GTK_IMAGE (plugin->popup_vol_icon), "audio-volume-high-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_widget_set_sensitive (plugin->scale, TRUE);
		pulseaudio_volume_set_muted (plugin->volume, FALSE);
	} else {
		gtk_image_set_from_icon_name (GTK_IMAGE (plugin->popup_vol_icon), "audio-volume-muted-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_widget_set_sensitive (plugin->scale, FALSE);
		pulseaudio_volume_set_muted (plugin->volume, TRUE);
	}
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

	new_volume = gtk_range_get_value (GTK_RANGE (range));
	pulseaudio_volume_set_volume (plugin->volume, new_volume / 100.0);
}

static void
on_volume_changed (PulseaudioVolume *volume, gpointer data)
{
	VolumePlugin *plugin = VOLUME_PLUGIN (data);

	g_signal_handlers_block_by_func (G_OBJECT (plugin->scale), on_scale_value_changed, data);

	gtk_range_set_value (GTK_RANGE (plugin->scale), pulseaudio_volume_get_volume (volume) * 100.0);

	if (pulseaudio_volume_get_muted (volume)) {
		if (plugin->popup_vol_icon)
			gtk_image_set_from_icon_name (GTK_IMAGE (plugin->popup_vol_icon), "audio-volume-muted-symbolic", GTK_ICON_SIZE_BUTTON);
	} else {
		if (plugin->popup_vol_icon)
			gtk_image_set_from_icon_name (GTK_IMAGE (plugin->popup_vol_icon), "audio-volume-high-symbolic", GTK_ICON_SIZE_BUTTON);
	}

	g_signal_handlers_unblock_by_func (G_OBJECT (plugin->scale), on_scale_value_changed, data);

	volume_button_update (plugin, FALSE);
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
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_KEY_Escape) {
		on_popup_window_closed (data);
		return TRUE;
	}

	return FALSE;
}

static GtkWidget *
popup_volume_window (VolumePlugin *plugin)
{
	GtkWidget *window;
	GtkWidget *img, *hbox, *button;
	GdkScreen *screen;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW (window), TRUE);
	gtk_window_stick(GTK_WINDOW (window));
	gtk_container_set_border_width (GTK_CONTAINER (window), 9);

	screen = gtk_widget_get_screen (GTK_WIDGET (plugin->button));
	gtk_window_set_screen (GTK_WINDOW (window), screen);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

	img = plugin->popup_vol_icon = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (button), img);

	if (pulseaudio_volume_get_muted (plugin->volume)) {
		gtk_image_set_from_icon_name (GTK_IMAGE (img), "audio-volume-muted-symbolic", GTK_ICON_SIZE_BUTTON);
	} else {
		gtk_image_set_from_icon_name (GTK_IMAGE (img), "audio-volume-high-symbolic", GTK_ICON_SIZE_BUTTON);
	}

	plugin->scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);

	gtk_widget_set_size_request (plugin->scale, 200, -1);
	gtk_range_set_inverted (GTK_RANGE (plugin->scale), FALSE);
	gtk_scale_set_draw_value (GTK_SCALE (plugin->scale), FALSE);

	gtk_box_pack_start (GTK_BOX (hbox), plugin->scale, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (window), hbox);

	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (on_mute_button_clicked), plugin);
	g_signal_connect (G_OBJECT (plugin->scale), "value-changed", G_CALLBACK (on_scale_value_changed), plugin);
	g_signal_connect (G_OBJECT (plugin->scale), "button-release-event", G_CALLBACK (on_scale_button_release_event), plugin);

	g_signal_connect (G_OBJECT (window), "realize", G_CALLBACK (on_popup_window_realized), plugin);
	g_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK (on_popup_key_press_event), plugin);
	g_signal_connect_swapped (G_OBJECT (window), "delete-event", G_CALLBACK (on_popup_window_closed), plugin);
	g_signal_connect_swapped (G_OBJECT (window), "focus-out-event", G_CALLBACK (on_popup_window_closed), plugin);

	gtk_widget_show_all (window);

	if (pulseaudio_volume_get_connected (plugin->volume))
		on_volume_changed (plugin->volume, plugin);
	else
		gtk_widget_set_sensitive (window, FALSE);

	xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (plugin), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), TRUE);

	return window;
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
		return TRUE;
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
	GtkWidget *image;

	xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	plugin->volume         = NULL;
	plugin->button         = NULL;
	plugin->image          = NULL;
	plugin->scale          = NULL;
	plugin->popup_window   = NULL;
	plugin->popup_vol_icon = NULL;

	plugin->config = pulseaudio_config_new (xfce_panel_plugin_get_property_base (XFCE_PANEL_PLUGIN (plugin)));
	plugin->volume = pulseaudio_volume_new (plugin->config);

	/* Initialize libkeybinder */
	keybinder_init ();

	g_signal_connect (G_OBJECT (plugin->config), "notify::enable-keyboard-shortcuts",
			G_CALLBACK (volume_plugin_bind_keys_cb), plugin);
	if (pulseaudio_config_get_enable_keyboard_shortcuts (plugin->config))
		volume_plugin_bind_keys (plugin);
	else
		volume_plugin_unbind_keys (plugin);

	plugin->button = xfce_panel_create_toggle_button ();
	gtk_button_set_relief (GTK_BUTTON (plugin->button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (plugin), plugin->button);
	xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
	gtk_widget_show (plugin->button);

	plugin->image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (plugin->button), plugin->image);
	gtk_widget_show (plugin->image);

	g_signal_connect (G_OBJECT (plugin->button), "button-press-event", G_CALLBACK (on_volume_button_pressed), plugin);
	g_signal_connect (G_OBJECT (plugin->volume), "volume-changed", G_CALLBACK (on_volume_changed), plugin);

	volume_button_update (plugin, TRUE);
}


static void
volume_plugin_construct (XfcePanelPlugin *panel_plugin)
{
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