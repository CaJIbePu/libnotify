/*
 * @file libnotify/notify.c Notifications library
 *
 * @Copyright (C) 2004-2006 Christian Hammond <chipx86@chipx86.com>
 * @Copyright (C) 2004-2006 Mike Hearn <mike@navi.cx>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <libnotify/notify.h>
#include <libnotify/internal.h>
#include <libnotify/notify-marshal.h>

static gboolean _initted = FALSE;
/* cached data */
static gchar *_app_name = NULL;
static GList *_server_caps = NULL;
static gchar *_server_name = NULL;
static gchar *_server_version = NULL;
static gchar *_server_vendor = NULL;
static gchar *_server_spec_version = NULL;

/* org.freedesktop.Notifications proxy */
static DBusGProxy *_proxy = NULL; 
/* org.freedesktop.DBus proxy */
static DBusGProxy *_proxy_for_dbus = NULL;
static DBusGConnection *_dbus_gconn = NULL;
static GList *_active_notifications = NULL;
static int _spec_version_major = 0;
static int _spec_version_minor = 0;

gboolean
_notify_check_spec_version(int major, int minor)
{
	if (_spec_version_major > major)
		return TRUE;
	if (_spec_version_major < major)
		return FALSE;
	return _spec_version_minor >= minor;
}

static gboolean
notify_update_spec_version (void)
{
	char *spec_version;
	if (!notify_get_server_info(NULL, NULL, NULL, &spec_version))
		return FALSE;

	sscanf (spec_version, "%d.%d", &_spec_version_major, &_spec_version_minor);
	g_free (spec_version);
	return TRUE;
}

static void     notify_get_server_caps_internal (void);
static gboolean notify_get_server_info_internal (void);

static void
reload_cached_data (void)
{
	notify_get_server_caps_internal ();
	notify_get_server_info_internal ();
	notify_update_spec_version ();
}

static void
notify_name_owner_changed (DBusGProxy   *proxy,
			   const char   *name,
			   const char   *prev_owner,
			   const char   *new_owner,
			   gpointer     *user_data)
{
	if ((strcmp (name, NOTIFY_DBUS_NAME) == 0) && 
            (new_owner != NULL || *new_owner != '\0'))
	{
		reload_cached_data ();
	}
}

/**
 * notify_init:
 * @app_name: (type utf8) (direction in): The name of the application 
 *     initializing libnotify.
 *
 * Initialized libnotify. This must be called before any other functions.
 *
 * Returns: %TRUE if successful, or %FALSE on error.
 */
gboolean
notify_init(const char *app_name)
{
	GError *error = NULL;

	g_return_val_if_fail(app_name != NULL, FALSE);
	g_return_val_if_fail(*app_name != '\0', FALSE);

	if (_initted)
		return TRUE;

	_app_name = g_strdup(app_name);

	g_type_init();

	_dbus_gconn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

	if (error != NULL)
	{
		g_message("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	_proxy = dbus_g_proxy_new_for_name(_dbus_gconn,
									   NOTIFY_DBUS_NAME,
									   NOTIFY_DBUS_CORE_OBJECT,
									   NOTIFY_DBUS_CORE_INTERFACE);
	_proxy_for_dbus = dbus_g_proxy_new_for_name (_dbus_gconn, 
							DBUS_SERVICE_DBUS,
							DBUS_PATH_DBUS,
							DBUS_INTERFACE_DBUS);

	dbus_g_object_register_marshaller(notify_marshal_VOID__UINT_UINT,
									  G_TYPE_NONE,
									  G_TYPE_UINT,
									  G_TYPE_UINT, G_TYPE_INVALID);

	dbus_g_object_register_marshaller(notify_marshal_VOID__UINT_STRING,
									  G_TYPE_NONE,
									  G_TYPE_UINT,
									  G_TYPE_STRING, G_TYPE_INVALID);

	/* notification signals */
	dbus_g_proxy_add_signal(_proxy, "NotificationClosed",
							G_TYPE_UINT, G_TYPE_UINT, 
							G_TYPE_INVALID);
	dbus_g_proxy_add_signal(_proxy, "ActionInvoked",
							G_TYPE_UINT, G_TYPE_STRING,
							G_TYPE_INVALID);

	/* dbus signals */
	dbus_g_proxy_add_signal (_proxy_for_dbus, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING,
				 G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (_proxy_for_dbus, "NameOwnerChanged",
				     G_CALLBACK (notify_name_owner_changed),
				     NULL, NULL);

	reload_cached_data ();

	_initted = TRUE;

	return TRUE;
}

/**
 * notify_get_app_name:
 *
 * Gets the application name registered.
 *
 * Returns: The registered application name, passed to notify_init().
 */
const gchar *
notify_get_app_name(void)
{
	return _app_name;
}

/**
 * notify_uninit:
 *
 * Uninitialized libnotify.
 *
 * This should be called when the program no longer needs libnotify for
 * the rest of its lifecycle, typically just before exitting.
 */
void
notify_uninit(void)
{
	GList *l;

	if (!_initted)
		return;

	g_free(_app_name);
	_app_name = NULL;

	g_free(_server_name);
	_server_name = NULL;

	g_free(_server_version);
	_server_version = NULL;

	g_free(_server_vendor);
	_server_vendor = NULL;

	g_free(_server_spec_version);
	_server_spec_version = NULL;

	if (_server_caps != NULL)
	{
		g_list_foreach (_server_caps, (GFunc)g_free, NULL);
		g_list_free (_server_caps);
		_server_caps = NULL;
	}

	for (l = _active_notifications; l != NULL; l = l->next)
	{
		NotifyNotification *n = NOTIFY_NOTIFICATION(l->data);

		if (_notify_notification_get_timeout(n) == 0 ||
			_notify_notification_has_nondefault_actions(n))
		{
			notify_notification_close(n, NULL);
		}
	}

	g_object_unref(_proxy);
	g_object_unref(_proxy_for_dbus);

	dbus_g_connection_unref(_dbus_gconn);

	_initted = FALSE;
}

/**
 * notify_is_initted:
 *
 * Gets whether or not libnotify is initialized.
 *
 * Returns: %TRUE if libnotify is initialized, or %FALSE otherwise.
 */
gboolean
notify_is_initted(void)
{
	return _initted;
}

DBusGConnection *
_notify_get_dbus_g_conn(void)
{
	return _dbus_gconn;
}

DBusGProxy *
_notify_get_g_proxy(void)
{
	return _proxy;
}

static void
notify_get_server_caps_internal (void)
{
	GError *error = NULL;
	char **caps = NULL, **cap;
	GList *result = NULL;
	DBusGProxy *proxy;

	proxy = _notify_get_g_proxy();
	g_return_if_fail(proxy != NULL);

	if (!dbus_g_proxy_call(proxy, "GetCapabilities", &error,
						   G_TYPE_INVALID,
						   G_TYPE_STRV, &caps, G_TYPE_INVALID))
	{
		g_message("GetCapabilities call failed: %s", error->message);
		g_error_free(error);
		return;
	}

	for (cap = caps; *cap != NULL; cap++)
	{
		result = g_list_append(result, g_strdup(*cap));
	}

	g_strfreev(caps);

	_server_caps = result;
}

/**
 * notify_get_server_caps:
 *
 * Queries the server for its capabilities and returns them in a #GList.
 *
 * Returns: (element-type utf8) (transfer full): A #GList of 
 *        server capability strings. The contents of this list should be 
 *        freed with g_free(), and the list freed with g_list_free().
 */
GList *
notify_get_server_caps(void)
{
	GList *l, *result = NULL;

	g_return_val_if_fail (_server_caps != NULL, NULL);

	for (l = _server_caps; l; l = l->next)
	{
		result = g_list_append (result, g_strdup (l->data));
	}

	return result;
}

/**
 * notify_has_server_cap:
 * @capability: The capability to test for.
 * 
 * Convenience function that checks if server has a certain capability.
 *
 * Returns: %TRUE if the current notification server has the capability
 *          otherwise %FALSE.
 */
gboolean
notify_has_server_cap(const char* capability)
{
	GList *caps;
	gboolean has_cap;

	g_return_val_if_fail(capability, FALSE);

	caps = notify_get_server_caps();
	has_cap = g_list_find_custom(caps, (gconstpointer)capability, (GCompareFunc)strcmp) != NULL;

	g_list_foreach(caps, (GFunc)g_free, NULL);
	g_list_free(caps);

	return has_cap;
}

static gboolean
notify_get_server_info_internal (void)
{
	char *name, *vendor, *version, *spec_version;
	GError *error = NULL;
	DBusGProxy *proxy;
	{
		proxy = _notify_get_g_proxy();
		g_return_val_if_fail(proxy != NULL, FALSE);

		if (!dbus_g_proxy_call(proxy, "GetServerInformation", &error,
						   G_TYPE_INVALID,
						   G_TYPE_STRING, &name,
						   G_TYPE_STRING, &vendor,
						   G_TYPE_STRING, &version,
						   G_TYPE_STRING, &spec_version,
						   G_TYPE_INVALID))
		{
			g_message("GetServerInformation call failed: %s", error->message);
			return FALSE;
		}
	}

	g_free (_server_name);
	g_free (_server_vendor);
	g_free (_server_version);
	g_free (_server_spec_version);

	_server_name = name;
	_server_vendor = vendor;
	_server_version = version;
	_server_spec_version = spec_version;

	return TRUE;
}

/**
 * notify_get_server_info:
 * @ret_name: (out) (transfer full): The resulting server name.
 * @ret_vendor: (out) (transfer full): The resulting server vendor.
 * @ret_version: (out) (transfer full): The resulting server version.
 * @ret_spec_version: (out) (transfer full): The resulting version of the 
 *                    specification the server is compliant with.
 *
 * Queries the server for its information, specifically, the name, vendor,
 * server version, and the version of the notifications specification that it
 * is compliant with.
 *
 * Returns: %TRUE if successful, and the variables passed will be set. %FALSE
 *          on failure.
 */
gboolean
notify_get_server_info(char **ret_name, char **ret_vendor,
					   char **ret_version, char **ret_spec_version)
{
	gboolean needs_name, needs_vendor, needs_version, needs_spec_version;
	gboolean retval;

	retval = TRUE;
	needs_name = (ret_name != NULL);
	needs_vendor = (ret_vendor != NULL);
	needs_version = (ret_version != NULL);
	needs_spec_version = (ret_spec_version != NULL);

	if ((needs_name && _server_name == NULL) ||
            (needs_vendor && _server_vendor == NULL) ||
	    (needs_version && _server_vendor == NULL) ||
	    (needs_spec_version && _server_spec_version == NULL))
		retval = notify_get_server_info_internal ();

	if (!retval)
		return FALSE;

	if (ret_name != NULL)
		*ret_name = g_strdup (_server_name);

	if (ret_vendor != NULL)
		*ret_vendor = g_strdup (_server_vendor);

	if (ret_version != NULL)
		*ret_version = g_strdup (_server_version);

	if (ret_spec_version != NULL)
		*ret_spec_version = g_strdup (_server_spec_version);	

	return TRUE;
}

void
_notify_cache_add_notification(NotifyNotification *n)
{
	_active_notifications = g_list_prepend(_active_notifications, n);
}

void
_notify_cache_remove_notification(NotifyNotification *n)
{
	_active_notifications = g_list_remove(_active_notifications, n);
}
