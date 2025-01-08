//
// Created by dingjing on 1/8/25.
//

#ifndef gvfs_backup_BACKUP_H
#define gvfs_backup_BACKUP_H
#define G_DISABLE_DEPRECATED
#include <gio/gio.h>

G_BEGIN_DECLS

#define STR_FREE(f)                                     G_STMT_START { if (f) { g_free (f); f = NULL; } } G_STMT_END

#define BACKUP_FILE_TYPE                                (backup_file_get_type())
#define BACKUP_IS_FILE_CLASS(k)                         (G_TYPE_CHECK_CLASS_TYPE((k), BACKUP_FILE_TYPE))
#define BACKUP_IS_FILE(k)                               (G_TYPE_CHECK_INSTANCE_TYPE((k), BACKUP_FILE_TYPE))
#define BACKUP_FILE_CLASS(k)                            (G_TYPE_CHECK_CLASS_CAST((k), BACKUP_FILE_TYPE, BackupFileClass))
#define BACKUP_FILE(k)                                  (G_TYPE_CHECK_INSTANCE_CAST((k), BACKUP_FILE_TYPE, BackupFile))
#define BACKUP_FILE_GET_CLASS(k)                        (G_TYPE_INSTANCE_GET_CLASS((k), BACKUP_FILE_TYPE, BackupFileClass))

G_DECLARE_FINAL_TYPE(BackupFile, backup_file, BackupFile, BACKUP_FILE_TYPE, GObject)

GType                   backup_file_get_type            (void) G_GNUC_CONST;
GFile*                  backup_file_new_for_path        (const gchar* path);
char*                   backup_file_get_path            (GFile* self);

G_END_DECLS

#endif // gvfs_backup_BACKUP_H
