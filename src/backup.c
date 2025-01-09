//
// Created by dingjing on 1/8/25.
//
#include "backup.h"

#include <stdio.h>
#include <sys/stat.h>

#define NOT_NULL_RUN(x,f,...)   G_STMT_START if (x) { f(x, ##__VA_ARGS__); } G_STMT_END
#define BREAK_NULL(x)           G_STMT_START if ((x) == NULL) { break; } G_STMT_END
#define G_OBJ_FREE(x)           G_STMT_START if (G_IS_OBJECT(x)) {g_object_unref (G_OBJECT(x)); x = NULL;} G_STMT_END

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

typedef struct _BackupMetaFile
{
    int                     version;
    char*                   srcFilePath;
    char*                   srcFilePathMD5;

    char*                   backupFileCtxMD51;
    char*                   backupFileCtxMD52;
    char*                   backupFileCtxMD53;

    guint64                 backupFileTimestamp1;
    guint64                 backupFileTimestamp2;
    guint64                 backupFileTimestamp3;
} BackupMetaFile;

static void backup_file_init            (BackupFile* self);
static void backup_file_interface_init  (GFileIface* interface);
static void backup_file_class_init      (BackupFileClass* klass);
static void backup_file_get_property    (GObject* object, guint id, GValue* value, GParamSpec* spec);
static void backup_file_set_property    (GObject* object, guint id, const GValue* value, GParamSpec* spec);

G_DEFINE_TYPE_WITH_CODE (BackupFile, backup_file, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(G_TYPE_FILE, backup_file_interface_init));

static void backup_file_set_path_and_uri(BackupFile* obj, const char* pu);
static GFile* vfs_lookup                (GVfs* vfs, const char* uri, gpointer data);
static GFile* vfs_parse_name            (GVfs* vfs, const char* parseName, gpointer data);

static char*        vfs_get_uri                 (GFile* file);
static char*        vfs_get_path                (GFile* file);
static char*        vfs_get_uri_schema          (GFile* file);
static gboolean     vfs_is_native               (GFile* file);
static GFile*       vfs_dup                     (GFile* file);
static gboolean     vfs_is_equal                (GFile* file1, GFile* file2);
static gboolean     vfs_file_backup_restore     (GFile* src, GFile* dest, GFileCopyFlags flags, GCancellable* cancel, GFileProgressCallback progress, gpointer uData, GError** error);

static GList*       get_all_mount_points        ();
static char*        read_line                   (FILE* fr);
static char*        get_mount_point_by_uri      (GFile* file);
static char*        get_file_content_md5        (const char* path);
static char*        get_file_path_md5           (const char* path);
static gboolean     make_backup_dirs_if_needed  (const char* mountPoint);
static gint         mount_point_compare         (gconstpointer a, gconstpointer b);
static gboolean     do_backup                   (const char* path, const char* mountPoint);
static gboolean     vfs_backup                  (GFile* file1, BackupFile* file2, GError** error);
static gboolean     vfs_restore                 (BackupFile* file1, GFile* file2, GError** error);

static void         backup_meta_free            (BackupMetaFile* info);
static gboolean     backup_meta_save            (const BackupMetaFile* info, const char* filePathMD5, const char* mountPoint);
static gboolean     backup_meta_parse           (BackupMetaFile* info/*in*/, const char* filePath, const char* filePathMD5, const char* mountPoint);


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

gboolean backup_file_backup(GFile* self)
{
    g_return_val_if_fail (BACKUP_IS_FILE(self), FALSE);

    return vfs_backup(self, NULL, NULL);
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

gboolean backup_file_restore(GFile* self)
{
    g_return_val_if_fail (BACKUP_IS_FILE(self), FALSE);

    return vfs_restore(BACKUP_FILE(self), NULL, NULL);
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

static void backup_file_interface_init (GFileIface* interface)
{
    interface->dup                  = vfs_dup;
    interface->get_uri              = vfs_get_uri;
    interface->get_path             = vfs_get_path;
    interface->equal                = vfs_is_equal;
    interface->is_native            = vfs_is_native;
    interface->get_uri_scheme       = vfs_get_uri_schema;
    interface->move                 = vfs_file_backup_restore;
    interface->copy                 = vfs_file_backup_restore;
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

static char* vfs_get_uri_schema (GFile* file)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file), NULL);

    return g_strdup(BACKUP_STR);
}

static gboolean vfs_is_native (GFile* file)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file), FALSE);

    return TRUE;
}

static gboolean vfs_is_equal (GFile* file1, GFile* file2)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file1) && BACKUP_IS_FILE(file2), FALSE);

    const BackupFile* s1 = BACKUP_FILE (file1);
    const BackupFile* s2 = BACKUP_FILE (file2);

    return (0 == g_strcmp0 (s1->fileURI, s2->fileURI));
}

static GFile* vfs_dup (GFile* file)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file), NULL);

    return backup_file_new_for_uri(BACKUP_FILE(file)->fileURI);
}

static gboolean vfs_file_backup_restore (GFile* src, GFile* dest, GFileCopyFlags flags, GCancellable* cancel, GFileProgressCallback progress, gpointer uData, GError** error)
{
    g_return_val_if_fail ((BACKUP_IS_FILE(src) && !BACKUP_IS_FILE(dest) && G_IS_FILE(dest)) || (G_IS_FILE(src) && !BACKUP_IS_FILE(src) && BACKUP_IS_FILE(dest)), TRUE);

    gboolean ret = FALSE;

    if (!BACKUP_IS_FILE(src)) {
        ret = vfs_backup(src, BACKUP_FILE(dest), error);
    }
    else {
        ret = vfs_restore(BACKUP_FILE(src), dest, error);
    }

    return ret;

    (void) flags;
    (void) uData;
    (void) error;
    (void) cancel;
    (void) progress;
}

static gboolean vfs_backup (GFile* file1, BackupFile* file2, GError** error)
{
    g_return_val_if_fail (G_IS_FILE(file1) && !BACKUP_IS_FILE(file1) && g_file_query_exists(file1, NULL) && (G_FILE_TYPE_REGULAR == g_file_query_file_type(file1, G_FILE_QUERY_INFO_NONE, NULL)), FALSE);

    char* path = NULL;                  // free
    gboolean ret = FALSE;
    char* mountPoint = NULL;            // free

    do {
        path = g_file_get_path(file1);
        BREAK_NULL(path);

        mountPoint = get_mount_point_by_uri(file1);
        BREAK_NULL(mountPoint);

        if (!make_backup_dirs_if_needed (mountPoint)) { break; };
        if (!do_backup(path, mountPoint)) { break; }

        ret = TRUE;
    } while (0);

    STR_FREE(path);
    STR_FREE(mountPoint);

    return ret;
}

static gboolean vfs_restore (BackupFile* file1, GFile* file2, GError** error)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file1), FALSE);

    char* path = g_file_get_path(G_FILE(file1));
    g_return_val_if_fail(path, FALSE);


}

static GList* get_all_mount_points ()
{
    FILE* fr = NULL;
    GList* list = NULL;
    const char* mpFile = NULL;

#define MTAB "/etc/mtab"

    if (0 == access (MTAB, R_OK)) {
        mpFile = MTAB;
    }

    g_return_val_if_fail (mpFile, NULL);

    char* bufLine = NULL;
    fr = fopen (mpFile, "r");

    while (NULL != (bufLine = read_line(fr))) {
        if ('/' != bufLine[0]) {
            STR_FREE (bufLine);
            continue;
        }
        for (int i = 0; bufLine[i] != '\0'; i++) {
            if (bufLine[i] == '\t') {
                bufLine[i] = ' ';
            }
        }
        char** strArr = g_strsplit(bufLine, " ", -1);
        if (strArr && strArr[1] && g_strcmp0(strArr[1], " ") && strlen(strArr[1]) > 0) {
            list = g_list_append (list, g_strdup (strArr[1]));
        }
        if (strArr) { g_strfreev (strArr); }
        STR_FREE (bufLine);
    }

    fclose(fr);

    return g_list_sort(list, mount_point_compare);
}

static inline char* calloc_and_memcpy(const char* oldStr, int64_t oldLen, const char* addStr, int64_t addLen)
{
    int64_t newLineLen = oldLen + addLen;
    char* tmp = (char*) malloc (newLineLen + 1);
    if (!tmp) {
        return NULL;
    }

    memset(tmp, 0, newLineLen + 1);
    if (oldStr) {
        memcpy(tmp, oldStr, oldLen);
    }
    memcpy(tmp + oldLen, addStr, addLen);

    return tmp;
}

static char* read_line (FILE* fr)
{
    g_return_val_if_fail (fr, NULL);

    char* res = NULL;
    char buf[64] = {0};
    int64_t lineLen = 0;
    int64_t cur = ftell(fr);

    while (TRUE) {
        memset(buf, 0, sizeof(buf));
        int size = fread(buf, 1, sizeof(buf) - 1, fr);
        if (size <= 0) {
            break;
        }

        int i = 0;
        gboolean find = FALSE;
        for (i = 0; i < size; ++i) {
            if (buf[i] == '\n') {
                char* tmp = calloc_and_memcpy(res, lineLen, buf, i);
                if (res) { free (res); res = NULL; }
                if (!tmp) { find = TRUE; break; }           // impossible
                res = tmp;
                lineLen += i;
                find = TRUE;
                break;
            }
        }

        if (find) { fseek(fr, cur + lineLen + 1, SEEK_SET); break; }

        char* tmp = calloc_and_memcpy(res, lineLen, buf, size);
        if (res) { free(res); res = NULL; }
        if (!tmp) { find = TRUE; break; } // impossible
        res = tmp;
        lineLen += size;
    }

    return res;
}

static gint mount_point_compare (gconstpointer a, gconstpointer b)
{
    return (gint) (strlen(b) - strlen(a));
}

static char* get_mount_point_by_uri (GFile* file)
{
    g_return_val_if_fail (G_IS_FILE(file) && !BACKUP_IS_FILE(file), NULL);

    char* mountPoint = NULL;
    char* path = g_file_get_path(file);

    g_return_val_if_fail (path, NULL);

    GList* list = get_all_mount_points();
    for (GList* iter = list; iter; iter = iter->next) {
        if (g_str_has_prefix (path, iter->data)) {
            mountPoint = g_strdup(iter->data);
            break;
        }
    }

    STR_FREE(path);
    g_list_free_full(list, g_free);

    return mountPoint;
}

static gboolean make_backup_dirs_if_needed (const char* mountPoint)
{
    g_return_val_if_fail (mountPoint, FALSE);

    gboolean ret = FALSE;
    char* metaDir = NULL;
    char* backupDir = NULL;

    do {
        metaDir = g_strdup_printf("%s/.%s/meta", mountPoint, BACKUP_STR);
        BREAK_NULL(metaDir);

        backupDir = g_strdup_printf("%s/.%s/backup", mountPoint, BACKUP_STR);
        BREAK_NULL(backupDir);

        if (g_mkdir_with_parents(metaDir, 0755) < 0) { break; }
        if (g_mkdir_with_parents(backupDir, 0755) < 0) { break; }

        ret = TRUE;
    } while (FALSE);

    STR_FREE(metaDir);
    STR_FREE(backupDir);

    return ret;
}

static gboolean do_backup (const char* path, const char* mountPoint)
{
    g_return_val_if_fail (path && mountPoint && (0 == access(path, F_OK)), FALSE);

    GError* error = NULL;               // free
    gboolean ret = FALSE;
    char* metaFile = NULL;              // free
    char* filePathMD5 = NULL;           // free
    char* backupFile1 = NULL;           // free
    char* backupFile2 = NULL;           // free
    char* backupFile3 = NULL;           // free
    GFile* backupFileF = NULL;          // free
    GFile* backupFileF1 = NULL;         // free
    GFile* backupFileF2 = NULL;         // free
    GFile* backupFileF3 = NULL;         // free
    char* fileContentMD5 = NULL;        // free
    BackupMetaFile backupMetaFile;      // free

    do {
        backupFileF = g_file_new_for_path(path);
        BREAK_NULL(backupFileF);

        filePathMD5 = get_file_path_md5 (path);
        BREAK_NULL(filePathMD5);

        fileContentMD5 = get_file_content_md5 (path);
        BREAK_NULL(fileContentMD5);

        if (!backup_meta_parse(&backupMetaFile, path, filePathMD5, mountPoint)) { break; }

        if (0 == backupMetaFile.version) {
            backupMetaFile.version = 1;
        }

        if (NULL == backupMetaFile.srcFilePath) {
            backupMetaFile.srcFilePath = g_strdup (path);
        }

        if (NULL == backupMetaFile.srcFilePathMD5) {
            backupMetaFile.srcFilePathMD5 = g_strdup (filePathMD5);
        }

        backupFile1 = g_strdup_printf("%s/.%s/backup/%s-1", mountPoint, BACKUP_STR, filePathMD5);
        BREAK_NULL(backupFile1);
        backupFileF1 = g_file_new_for_path (backupFile1);
        BREAK_NULL(backupFileF1);

        backupFile2 = g_strdup_printf("%s/.%s/backup/%s-2", mountPoint, BACKUP_STR, filePathMD5);
        BREAK_NULL(backupFile2);
        backupFileF2 = g_file_new_for_path (backupFile2);
        BREAK_NULL(backupFileF2);

        backupFile3 = g_strdup_printf("%s/.%s/backup/%s-3", mountPoint, BACKUP_STR, filePathMD5);
        BREAK_NULL(backupFile3);
        backupFileF3 = g_file_new_for_path (backupFile3);
        BREAK_NULL(backupFileF3);

        if (backupMetaFile.backupFileCtxMD53) {
            if (0 == g_strcmp0(backupMetaFile.backupFileCtxMD53, fileContentMD5)) { ret = TRUE; break; }
            STR_FREE(backupMetaFile.backupFileCtxMD51);
            backupMetaFile.backupFileCtxMD51 = backupMetaFile.backupFileCtxMD52;
            backupMetaFile.backupFileTimestamp1 = backupMetaFile.backupFileTimestamp2;
            remove(backupFile1);
            rename(backupFile2, backupFile1);
            backupMetaFile.backupFileCtxMD52 = backupMetaFile.backupFileCtxMD53;
            backupMetaFile.backupFileTimestamp2 = backupMetaFile.backupFileTimestamp3;
            rename(backupFile3, backupFile2);
            ret = g_file_copy(backupFileF, backupFileF3, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            if (ret) {
                backupMetaFile.backupFileCtxMD53 = g_strdup(fileContentMD5);
                backupMetaFile.backupFileTimestamp3 = time(NULL);
            }
        }
        else if (backupMetaFile.backupFileCtxMD52) {
            if (0 == g_strcmp0(backupMetaFile.backupFileCtxMD52, fileContentMD5)) { ret = TRUE; break; }
            ret = g_file_copy(backupFileF, backupFileF3, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            if (ret) {
                backupMetaFile.backupFileCtxMD53 = g_strdup(fileContentMD5);
                backupMetaFile.backupFileTimestamp3 = time(NULL);
            }
        }
        else if (backupMetaFile.backupFileCtxMD51) {
            if (0 == g_strcmp0(backupMetaFile.backupFileCtxMD51, fileContentMD5)) { ret = TRUE; break; }
            ret = g_file_copy(backupFileF, backupFileF2, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            if (ret) {
                backupMetaFile.backupFileCtxMD52 = g_strdup(fileContentMD5);
                backupMetaFile.backupFileTimestamp2 = time(NULL);
            }
        }
        else {
            ret = g_file_copy(backupFileF, backupFileF1, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            if (ret) {
                backupMetaFile.backupFileCtxMD51 = g_strdup(fileContentMD5);
                backupMetaFile.backupFileTimestamp1 = time(NULL);
            }
        }

        if (ret) {
            backup_meta_save(&backupMetaFile, filePathMD5, mountPoint);
        }
    } while (FALSE);

#if 0
    if (error) {
        printf("[BACKUP] error: %d %s\n", error->code, error->message);
    }
#endif

    STR_FREE(metaFile);
    STR_FREE(filePathMD5);
    STR_FREE(backupFile1);
    STR_FREE(backupFile2);
    STR_FREE(backupFile3);
    STR_FREE(fileContentMD5);
    NOT_NULL_RUN(error, g_error_free);
    NOT_NULL_RUN(backupFileF, g_object_unref);
    NOT_NULL_RUN(backupFileF1, g_object_unref);
    NOT_NULL_RUN(backupFileF2, g_object_unref);
    NOT_NULL_RUN(backupFileF3, g_object_unref);
    backup_meta_free(&backupMetaFile);

    return ret;
}

static char* get_file_content_md5 (const char* path)
{
    g_return_val_if_fail (path && '/' == path[0], NULL);

    int bufLen = 0;
    FILE* fr = NULL;
    char* res = NULL;
    char buf[1024] = {0};
    GChecksum* cs = NULL;

    do {
        fr = fopen(path, "r");
        BREAK_NULL(fr);

        cs = g_checksum_new(G_CHECKSUM_MD5);
        BREAK_NULL(cs);

        while ((bufLen = (int) fread (buf, 1, sizeof(buf), fr)) > 0) {
            g_checksum_update(cs, buf, bufLen);
        }
        res = g_strdup(g_checksum_get_string(cs));

    } while (0);

    NOT_NULL_RUN(fr, fclose);
    NOT_NULL_RUN(cs, g_checksum_free);

    return res;
}

static char* get_file_path_md5 (const char* path)
{
    g_return_val_if_fail (path, NULL);

    char* res = NULL;
    GChecksum* cs = NULL;

    do {
        cs = g_checksum_new(G_CHECKSUM_MD5);
        BREAK_NULL(cs);
        g_checksum_update(cs, (const guchar*) path, (gssize) strlen(path));
        res = g_strdup(g_checksum_get_string(cs));

    } while (0);

    NOT_NULL_RUN(cs, g_checksum_free);

    return res;
}

static gboolean backup_meta_parse (BackupMetaFile* info, const char* filePath, const char* filePathMD5, const char* mountPoint)
{
    g_return_val_if_fail (info && filePath && filePathMD5 && mountPoint, FALSE);

    memset(info, 0, sizeof(BackupMetaFile));

    FILE* metaFr = NULL;
    gboolean ret = FALSE;
    char** strArr = NULL;               // free
    char* metaFile = NULL;              // free
    char* metaFileCtx = NULL;           // free

    do {
        metaFile = g_strdup_printf("%s/.%s/meta/%s", mountPoint, BACKUP_STR, filePathMD5);
        BREAK_NULL(metaFile);

        if (0 != access(metaFile, F_OK)) {
            ret = TRUE;
            break;
        }

        struct stat statBuf;
        guint64 backupFileSize = 0;
        if (!stat(metaFile, &statBuf)) {
            backupFileSize = statBuf.st_size + 1;
        }

        metaFileCtx = g_malloc0(backupFileSize);
        BREAK_NULL(metaFileCtx);

        metaFr = fopen(metaFile, "r");
        BREAK_NULL(metaFr);

        if (fread(metaFileCtx, 1, backupFileSize, metaFr) < 0) { break; }

        fclose(metaFr);
        metaFr = NULL;

        strArr = g_strsplit(metaFileCtx, "|", -1);
        if (strArr) {
            const char* version = strArr[0];
            if (version) {
                const int verInt = (int) strtol(version, NULL, 10);
                info->version = verInt;
                if (1 == verInt) {
                    if (g_strv_length(strArr) != 9) {
                        break;
                    }
                    if (strArr[1]) { info->srcFilePath          = g_strdup(strArr[1]); }
                    if (strArr[2]) { info->srcFilePathMD5       = g_strdup(strArr[2]); }

                    if (strArr[3]) { info->backupFileCtxMD51    = g_strdup(strArr[3]); }
                    if (strArr[4]) { info->backupFileTimestamp1 = strtoll(strArr[4], NULL, 10); }
                    if (strArr[5]) { info->backupFileCtxMD52    = g_strdup(strArr[5]); }
                    if (strArr[6]) { info->backupFileTimestamp2 = strtoll(strArr[6], NULL, 10); }
                    if (strArr[7]) { info->backupFileCtxMD53    = g_strdup(strArr[7]); }
                    if (strArr[8]) { info->backupFileTimestamp3 = strtoll(strArr[8], NULL, 10); }
                }
            }
        }
        ret = TRUE;
    } while (FALSE);

    STR_FREE(metaFile);
    STR_FREE(metaFileCtx);
    NOT_NULL_RUN(strArr, g_strfreev);
    if (metaFr) { fclose(metaFr); metaFr = NULL; }

    return ret;
}

static gboolean backup_meta_save (const BackupMetaFile* info, const char* filePathMD5, const char* mountPoint)
{
    g_return_val_if_fail (info && filePathMD5 && mountPoint, FALSE);

    FILE* metaFr = NULL;
    gboolean ret = FALSE;
    char* metaFile = NULL;              // free
    char* metaFileCtx = NULL;           // free

    do {
        metaFile = g_strdup_printf("%s/.%s/meta/%s", mountPoint, BACKUP_STR, filePathMD5);
        BREAK_NULL(metaFile);

        metaFr = fopen(metaFile, "w+");
        BREAK_NULL(metaFr);

        metaFileCtx = g_strdup_printf("1|%s|%s|%s|%lu|%s|%lu|%s|%lu",
                    info->srcFilePath, filePathMD5,
                    info->backupFileCtxMD51 ? info->backupFileCtxMD51 : "", info->backupFileTimestamp1,
                    info->backupFileCtxMD52 ? info->backupFileCtxMD52 : "", info->backupFileTimestamp2,
                    info->backupFileCtxMD53 ? info->backupFileCtxMD53 : "", info->backupFileTimestamp3);
        BREAK_NULL(metaFileCtx);

        fwrite(metaFile, 1, strlen(metaFileCtx), metaFr);
        fclose(metaFr);
        ret = TRUE;
    } while (0);

    STR_FREE(metaFile);
    STR_FREE(metaFileCtx);
    if (metaFr) { fclose(metaFr); metaFr = NULL; }

    return ret;
}

static void backup_meta_free (BackupMetaFile* info)
{
    g_return_if_fail (info);

    STR_FREE(info->srcFilePath);
    STR_FREE(info->srcFilePathMD5);
    STR_FREE(info->backupFileCtxMD51);
    STR_FREE(info->backupFileCtxMD52);
    STR_FREE(info->backupFileCtxMD53);
}
