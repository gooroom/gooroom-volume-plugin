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

#ifndef __VOLUME_PLUGIN_H__
#define __VOLUME_PLUGIN_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

G_BEGIN_DECLS
typedef struct _VolumePluginClass VolumePluginClass;
typedef struct _VolumePlugin      VolumePlugin;

#define TYPE_VOLUME_PLUGIN            (volume_plugin_get_type ())
#define VOLUME_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_VOLUME_PLUGIN, VolumePlugin))
#define VOLUME_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_VOLUME_PLUGIN, VolumePluginClass))
#define IS_VOLUME_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_VOLUME_PLUGIN))
#define IS_VOLUME_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_VOLUME_PLUGIN))
#define VOLUME_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_VOLUME_PLUGIN, VolumePluginClass))

GType volume_plugin_get_type      (void) G_GNUC_CONST;

void  volume_plugin_register_type (XfcePanelTypeModule *type_module);

G_END_DECLS

#endif /* !__VOLUME_PLUGIN_H__ */
