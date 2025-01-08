//
// Created by dingjing on 1/8/25.
//

#include "backup.h"

typedef enum
{
    PROP_0,
    PROP_FILE_PATH,
    PROP_N
} BackupFileProperty;

struct _BackupFile
{
    GObject                 parent;

    char*                   filePath;
};

// struct _BackupFileClass
// {
//     GObjectClass            parent;
// };

static void backup_file_init            (BackupFile* self);
static void backup_file_interface_init  (GFileIface* interface);
static void backup_file_class_init      (BackupFileClass* klass);
static void backup_file_get_property    (GObject* object, guint id, GValue* value, GParamSpec* spec);
static void backup_file_set_property    (GObject* object, guint id, const GValue* value, GParamSpec* spec);

G_DEFINE_TYPE_WITH_CODE (BackupFile, backup_file, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(G_TYPE_FILE, backup_file_interface_init));

static GParamSpec* gsBackupFileProperty[PROP_N] = { NULL };


GFile* backup_file_new_for_path (const gchar* path)
{
    BackupFile* self = BACKUP_FILE(g_object_new(BACKUP_FILE_TYPE, "path", path, NULL));

    return (GFile*) self;
}

char* backup_file_get_path(GFile* self)
{
    g_return_val_if_fail (BACKUP_IS_FILE(self), NULL);

    const BackupFile* obj = BACKUP_FILE(self);

    return g_strdup (obj->filePath);
}

static void backup_file_init (BackupFile* self)
{

}

static void backup_file_class_init (BackupFileClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS (klass);

    objClass->get_property = backup_file_get_property;
    objClass->set_property = backup_file_set_property;

    gsBackupFileProperty[PROP_FILE_PATH] = g_param_spec_string ("path", "Path", "", NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(objClass, PROP_N, gsBackupFileProperty);
}

static void backup_file_set_property (GObject* object, guint id, const GValue* value, GParamSpec* spec)
{
    BackupFile* self = BACKUP_FILE (object);

    switch ((BackupFileProperty) id) {
        case PROP_FILE_PATH: {
            STR_FREE(self->filePath);
            self->filePath = g_value_dup_string (value);
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
    BackupFile* self = BACKUP_FILE (object);

    switch ((BackupFileProperty) id) {
        case PROP_FILE_PATH: {
            g_value_set_string(value, self->filePath);
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

// iface
static void backup_file_interface_init (GFileIface* interface)
{

}