#include "ds-theme-watcher.h"

struct _DsThemeWatcher {
    GObject parent;

    GtkSettings *settings;
    guint notify_timeout;

    guint timer_id;
    char *gtk_theme_name;
    char *icon_theme_name;
    char *sound_theme_name;
};

G_DEFINE_TYPE(DsThemeWatcher, ds_theme_watcher, G_TYPE_OBJECT);

enum {
    PROP_SETTINGS = 1,
    PROP_NOTIFY_TIMEOUT,
    PROP_LAST,
};

enum {
    THEME_CHANGED,
    LAST_SIGNAL,
};

static guint watcher_signals[LAST_SIGNAL] = { 0 };

static gboolean
ds_theme_watcher_check(DsThemeWatcher *self)
{
    g_autofree char *gtk_theme_name = NULL;
    g_autofree char *icon_theme_name = NULL;
    g_autofree char *sound_theme_name = NULL;

    self->timer_id = 0;

    g_object_get(self->settings,
                 "gtk-theme-name", &gtk_theme_name,
                 "gtk-icon-theme-name", &icon_theme_name,
                 "gtk-sound-theme-name", &sound_theme_name,
                 NULL);

    /* If nothing has changed, we're done */
    if (!g_strcmp0(gtk_theme_name, self->gtk_theme_name) &&
        !g_strcmp0(icon_theme_name, self->icon_theme_name) &&
        !g_strcmp0(sound_theme_name, self->sound_theme_name)) {
        return G_SOURCE_REMOVE;
    }

    g_clear_pointer(&self->gtk_theme_name, g_free);
    self->gtk_theme_name = g_steal_pointer(&gtk_theme_name);
    g_clear_pointer(&self->icon_theme_name, g_free);
    self->icon_theme_name = g_steal_pointer(&icon_theme_name);
    g_clear_pointer(&self->sound_theme_name, g_free);
    self->sound_theme_name = g_steal_pointer(&sound_theme_name);

    g_signal_emit(self, watcher_signals[THEME_CHANGED], 0,
                  self->gtk_theme_name, self->icon_theme_name, self->sound_theme_name);

    return G_SOURCE_REMOVE;
}

static void
ds_theme_watcher_queue_check(DsThemeWatcher *self)
{
    g_clear_handle_id(&self->timer_id, g_source_remove);
    self->timer_id = g_timeout_add_seconds(
        self->notify_timeout, G_SOURCE_FUNC(ds_theme_watcher_check), self);
}

static void
ds_theme_watcher_notify_cb(GtkSettings *settings, GParamSpec *pspec, DsThemeWatcher *self)
{
    ds_theme_watcher_queue_check(self);
}

static void
ds_theme_watcher_set_gtksettings(DsThemeWatcher *self, GtkSettings *settings)
{
    if (self->settings) {
        g_signal_handlers_disconnect_by_data(self->settings, self);
        g_clear_handle_id(&self->timer_id, g_source_remove);
    }
    g_clear_object(&self->settings);
    g_clear_pointer(&self->gtk_theme_name, g_free);
    g_clear_pointer(&self->icon_theme_name, g_free);
    g_clear_pointer(&self->sound_theme_name, g_free);

    if (settings == NULL) {
        return;
    }

    self->settings = g_object_ref(settings);
    g_signal_connect(settings, "notify::gtk-theme-name",
                     G_CALLBACK(ds_theme_watcher_notify_cb), self);
    g_signal_connect(settings, "notify::gtk-icon-theme-name",
                     G_CALLBACK(ds_theme_watcher_notify_cb), self);
    g_signal_connect(settings, "notify::gtk-sound-theme-name",
                     G_CALLBACK(ds_theme_watcher_notify_cb), self);
    ds_theme_watcher_queue_check(self);
}

static void
ds_theme_watcher_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    DsThemeWatcher *self = DS_THEME_WATCHER(object);

    switch (prop_id) {
    case PROP_NOTIFY_TIMEOUT:
        g_value_set_uint(value, self->notify_timeout);
        break;
    case PROP_SETTINGS:
        g_value_set_object(value, self->settings);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ds_theme_watcher_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    DsThemeWatcher *self = DS_THEME_WATCHER(object);

    switch (prop_id) {
    case PROP_NOTIFY_TIMEOUT:
        self->notify_timeout = g_value_get_uint(value);
        break;
    case PROP_SETTINGS:
        ds_theme_watcher_set_gtksettings(self, g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
ds_theme_watcher_finalize(GObject *object)
{
    DsThemeWatcher *self = DS_THEME_WATCHER(object);

    ds_theme_watcher_set_gtksettings(self, NULL);

    G_OBJECT_CLASS(ds_theme_watcher_parent_class)->finalize(object);
}

static void
ds_theme_watcher_class_init(DsThemeWatcherClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = ds_theme_watcher_finalize;
    gobject_class->get_property = ds_theme_watcher_get_property;
    gobject_class->set_property = ds_theme_watcher_set_property;

    g_object_class_install_property(
        gobject_class, PROP_NOTIFY_TIMEOUT,
        g_param_spec_uint("notify-timeout", "notify timeout", "time to wait before ",
                          0, G_MAXUINT, 1, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
        gobject_class, PROP_SETTINGS,
        g_param_spec_object("settings", "settings", "GtkSettings instance to watch",
                            GTK_TYPE_SETTINGS, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    watcher_signals[THEME_CHANGED] = g_signal_new(
        "theme-changed", G_TYPE_FROM_CLASS (gobject_class),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
        G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

static void
ds_theme_watcher_init(DsThemeWatcher *self)
{
}

DsThemeWatcher *
ds_theme_watcher_new(GtkSettings *settings)
{
    return g_object_new(DS_TYPE_THEME_WATCHER, "settings", settings, NULL);
}
