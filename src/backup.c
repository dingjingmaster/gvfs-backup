//
// Created by dingjing on 1/8/25.
//

#include "backup.h"

typedef enum
{
    PROP_0,
    PROP_FILE_PATH,
    PROP_FILE_URI,
    PROP_N
} BackupFileProperty;

struct _BackupFile
{
    GObject                 parent;

    char*                   filePath;
    char*                   fileURI;
};

static void backup_file_init            (BackupFile* self);
static void backup_file_interface_init  (GFileIface* interface);
static void backup_file_class_init      (BackupFileClass* klass);
static void backup_file_get_property    (GObject* object, guint id, GValue* value, GParamSpec* spec);
static void backup_file_set_property    (GObject* object, guint id, const GValue* value, GParamSpec* spec);

G_DEFINE_TYPE_WITH_CODE (BackupFile, backup_file, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(G_TYPE_FILE, backup_file_interface_init));

static void backup_file_set_path_and_uri(BackupFile* obj, const char* pu);
static GFile* vfs_lookup                (GVfs* vfs, const char* uri, gpointer data);
static GFile* vfs_parse_name            (GVfs* vfs, const char* parseName, gpointer data);

static char*        vfs_get_uri         (GFile* file);
static char*        vfs_get_path        (GFile* file);


static GParamSpec* gsBackupFileProperty[PROP_N] = { NULL };


GFile* backup_file_new_for_uri (const gchar* uri)
{
    g_return_val_if_fail(uri && !strncmp(uri, BACKUP_STR, strlen(BACKUP_STR)) && (strlen(uri) > strlen(BACKUP_STR) + 3), NULL);

    BackupFile* self = BACKUP_FILE(g_object_new(BACKUP_FILE_TYPE, "s-path", uri + strlen(BACKUP_STR) + 3, NULL));

    return (GFile*) self;
}

GFile* backup_file_new_for_path (const gchar* path)
{
    g_return_val_if_fail(path && '/' == path[0], NULL);

    BackupFile* self = BACKUP_FILE(g_object_new(BACKUP_FILE_TYPE, "s-path", path, NULL));

    return (GFile*) self;
}

gboolean backup_file_backup(GFile * self)
{
    g_return_val_if_fail (BACKUP_IS_FILE(self), FALSE);

    gboolean result = FALSE;

    return result;
}

gboolean backup_file_backup_by_abspath(const char* path)
{
    g_return_val_if_fail (path && '/' == path[0], FALSE);

    gboolean result = FALSE;

    GFile* file = backup_file_new_for_path (path);
    if (BACKUP_IS_FILE (file)) {
        result = backup_file_backup(file);
        g_object_unref (file);
    }

    return result;
}

gboolean backup_file_restore(GFile * self)
{
    g_return_val_if_fail (BACKUP_IS_FILE(self), FALSE);

    gboolean result = FALSE;

    return result;
}

gboolean backup_file_restore_by_abspath(const char* path)
{
    g_return_val_if_fail (path && '/' == path[0], FALSE);

    gboolean result = FALSE;

    GFile* file = backup_file_new_for_path (path);
    if (BACKUP_IS_FILE (file)) {
        result = backup_file_restore(file);
        g_object_unref (file);
    }

    return result;
}

void backup_file_register()
{
    static gsize init = 0;

    if (g_once_init_enter (&init)) {
        g_vfs_register_uri_scheme(g_vfs_get_default(), BACKUP_STR, vfs_lookup, NULL, NULL, vfs_parse_name, NULL, NULL);
        g_once_init_leave (&init, 1);
    }
}

static void backup_file_init (BackupFile* self)
{

}

static void backup_file_class_init (BackupFileClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS (klass);

    objClass->get_property = backup_file_get_property;
    objClass->set_property = backup_file_set_property;

    gsBackupFileProperty[PROP_FILE_PATH] = g_param_spec_string ("s-path", "Path", "", NULL, G_PARAM_READWRITE);
    gsBackupFileProperty[PROP_FILE_URI]  = g_param_spec_string ("s-uri", "URI", "", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(objClass, PROP_N, gsBackupFileProperty);

    backup_file_register();
}

static void backup_file_set_property (GObject* object, guint id, const GValue* value, GParamSpec* spec)
{
    g_return_if_fail(BACKUP_IS_FILE(object) && value);

    BackupFile* self = BACKUP_FILE (object);

    switch ((BackupFileProperty) id) {
        case PROP_FILE_PATH:
        case PROP_FILE_URI: {
            backup_file_set_path_and_uri(self, g_value_get_string(value));
            break;
        }
        case PROP_0:
        case PROP_N:
        default: {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
            break;
        }
    }
}

static void backup_file_get_property (GObject* object, guint id, GValue* value, GParamSpec* spec)
{
    g_return_if_fail(BACKUP_IS_FILE(object) && value);

    const BackupFile* self = BACKUP_FILE (object);

    switch ((BackupFileProperty) id) {
        case PROP_FILE_PATH: {
            g_value_set_string(value, self->filePath);
            break;
        }
        case PROP_FILE_URI: {
            g_value_set_string(value, self->fileURI);
            break;
        }
        case PROP_0:
        case PROP_N:
        default: {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
            break;
        }
    }
}

static void backup_file_set_path_and_uri(BackupFile* obj, const char* pu)
{
    g_return_if_fail (BACKUP_IS_FILE(obj) && pu);

    const int puLen = strlen (pu);
    const int schemaLen = strlen(BACKUP_STR);

    STR_FREE(obj->fileURI);
    STR_FREE(obj->filePath);

    // is uri
    if ((puLen - schemaLen >= 3) && (0 == strncmp(pu, BACKUP_STR, schemaLen)) && ('/' == pu[schemaLen + 3])) {
        obj->fileURI = g_strdup (pu);
        obj->filePath = g_strdup (pu + schemaLen + 4);
    }
    else if (puLen > 0 && '/' == pu[0]) {
        obj->filePath = g_strdup (pu);
        obj->fileURI = g_strdup_printf ("%s://%s", BACKUP_STR, pu);
    }
}

// iface
static void backup_file_interface_init (GFileIface* interface)
{
    interface->get_uri              = vfs_get_uri;
    interface->get_path             = vfs_get_path;
}

static GFile* vfs_lookup (GVfs* vfs, const char* uri, gpointer data)
{
    return vfs_parse_name(vfs, uri, data);
}

static GFile* vfs_parse_name (GVfs* vfs, const char* parseName, gpointer data)
{
    return backup_file_new_for_uri(parseName);
    (void) vfs;
    (void) data;
}

static char* vfs_get_uri (GFile* file)
{
    GValue value = G_VALUE_INIT;
    g_value_init (&value, G_TYPE_STRING);

    g_object_get_property (G_OBJECT (file), "s-uri", &value);

    return g_value_dup_string (&value);
}

static char* vfs_get_path (GFile* file)
{
    GValue value = G_VALUE_INIT;
    g_value_init (&value, G_TYPE_STRING);

    g_object_get_property (G_OBJECT (file), "s-path", &value);

    return g_value_dup_string (&value);
}

