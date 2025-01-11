//
// Created by dingjing on 1/8/25.
//
#include "backup.h"

#include <time.h>
#include <stdio.h>
#include <sys/stat.h>

#define BREAK_IF_FAIL(x)        if (!(x)) { break; }
#define BREAK_NULL(x)           if ((x) == NULL) { break; }
#define NOT_NULL_RUN(x,f,...)   G_STMT_START if (x) { f(x, ##__VA_ARGS__); x = NULL; } G_STMT_END
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

struct _BackupFileEnum
{
    GFileEnumerator         parent;
    GList*                  files;
    GList*                  iter;
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

static void backup_file_init                    (BackupFile* self);
static void backup_file_interface_init          (GFileIface* interface);
static void backup_file_class_init              (BackupFileClass* klass);
static void backup_file_get_property            (GObject* object, guint id, GValue* value, GParamSpec* spec);
static void backup_file_set_property            (GObject* object, guint id, const GValue* value, GParamSpec* spec);

static void backup_file_enum_init               (BackupFileEnum* self);
static void backup_file_enum_class_init         (BackupFileEnumClass* klass);
static GFileEnumerator* vfs_file_enum_children  (GFile* file, const char* attribute, GFileQueryInfoFlags flags, GCancellable* cancel, GError** error);


G_DEFINE_TYPE (BackupFileEnum, backup_file_enum, G_TYPE_FILE_ENUMERATOR);
G_DEFINE_TYPE_WITH_CODE (BackupFile, backup_file, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(G_TYPE_FILE, backup_file_interface_init));


static void backup_file_set_path_and_uri            (BackupFile* obj, const char* pu);
static GFile* vfs_lookup                            (GVfs* vfs, const char* uri, gpointer data);
static GFile* vfs_parse_name                        (GVfs* vfs, const char* parseName, gpointer data);

static char*        vfs_get_uri                     (GFile* file);
static char*        vfs_get_path                    (GFile* file);
static char*        vfs_get_uri_schema              (GFile* file);
static char*        vfs_get_basename                (GFile* file);
static gboolean     vfs_is_native                   (GFile* file);
static GFile*       vfs_dup                         (GFile* file);
static GFile*       vfs_get_parent                  (GFile* file);
static guint        vfs_hash                        (GFile* file);
static gboolean     vfs_is_equal                    (GFile* file, GFile* file2);
static gboolean     vfs_has_schema                  (GFile* file, const char* uriSchema);
static GFile*       vfs_resolve_relative_path       (GFile* file, const char* relativePath);
static GFile*       vfs_get_child_for_display_name  (GFile* file, const char* displayName, GError** error);
static GFileInfo*   vfs_file_query_fs_info          (GFile* file, const char* attr, GCancellable* cancel, GError** error);
static GFileInfo*   vfs_file_query_info             (GFile* file, const char* attr, GFileQueryInfoFlags flags, GCancellable* cancel, GError** error);
static gboolean     vfs_file_backup_restore         (GFile* src, GFile* dest, GFileCopyFlags flags, GCancellable* cancel, GFileProgressCallback progress, gpointer uData, GError** error);

static GFileInfo*   vfs_file_enum_next_file         (GFileEnumerator *enumerator, GCancellable *cancellable, GError **error);
static gboolean     vfs_file_enum_close             (GFileEnumerator *enumerator, GCancellable *cancellable, GError **error);

static GList*       get_all_mount_points            ();
static char*        read_line                       (FILE* fr);
static char*        get_mount_point_by_uri          (GFile* file);
static void         file_path_format                (char* filePath);
static void         file_name_to_lower              (char* fileName);
static char*        get_file_content_md5            (const char* path);
static char*        get_file_path_md5               (const char* path);
static gboolean     make_backup_dirs_if_needed      (const char* mountPoint);
static gint         mount_point_compare             (gconstpointer a, gconstpointer b);
static gboolean     do_backup                       (const char* path, const char* mountPoint);
static gboolean     do_restore                      (const char* path, const char* mountPoint);
static gboolean     vfs_backup                      (GFile* file1, BackupFile* file2, GError** error);
static gboolean     vfs_restore                     (BackupFile* file1, GFile* file2, GError** error);
static char*        file_get_restore_path           (const char* srcFilePath, const char* extName, guint64 timestamp);

static void         backup_meta_free                (BackupMetaFile* info);
static gboolean     backup_meta_parse_file_path     (BackupMetaFile* info/*in*/, const char* filePath);
static gboolean     backup_meta_save                (const BackupMetaFile* info, const char* filePathMD5, const char* mountPoint);
static gboolean     backup_meta_parse               (BackupMetaFile* info/*in*/, const char* filePath, const char* filePathMD5, const char* mountPoint);


static GParamSpec* gsBackupFileProperty[PROP_N] = { NULL };
static const char* gsFileExt[] = {
    ".tar.gz",
    ".tar.xz",
    ".docx",
    ".xlsx",
    ".java",
    ".pptx",
    ".bz2",
    ".cpp",
    ".dxf",
    ".doc",
    ".hpp",
    ".odp",
    ".odt",
    ".ods",
    ".pdf",
    ".ppt",
    ".rar",
    ".txt",
    ".tmp",
    ".wps",
    ".wps",
    ".xls",
    ".zip",
    ".bz",
    ".cc",
    ".py",
    ".xz",
    ".c",
    ".h",
    NULL,
};


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
    g_return_val_if_fail (G_IS_FILE(self) && !BACKUP_IS_FILE(self), FALSE);

    GFile* bf = backup_file_new_for_path("/");
    const gboolean ret = g_file_copy(self, bf, 0, NULL, NULL, NULL, NULL);
    NOT_NULL_RUN(bf, g_object_unref);

    return ret;
}

gboolean backup_file_backup_by_abspath(const char* path)
{
    g_return_val_if_fail (path && '/' == path[0], FALSE);

    gboolean result = FALSE;

    GFile* file = g_file_new_for_path (path);
    GFile* bf = backup_file_new_for_path("/");
    result = g_file_copy(file, bf, 0, NULL, NULL, NULL, NULL);

    NOT_NULL_RUN(bf, g_object_unref);
    NOT_NULL_RUN(file, g_object_unref);

    return result;
}

gboolean backup_file_restore(GFile* self)
{
    g_return_val_if_fail (BACKUP_IS_FILE(self), FALSE);

    char* path = g_file_get_path(self);
    GFile* file = g_file_new_for_path (path);
    const gboolean result = g_file_copy(self, file, 0, NULL, NULL, NULL, NULL);
    STR_FREE(path);
    NOT_NULL_RUN(file, g_object_unref);

    return result;
}

gboolean backup_file_restore_by_abspath(const char* path)
{
    g_return_val_if_fail (path && '/' == path[0], FALSE);

    GFile* file = backup_file_new_for_path (path);
    GFile* bf = g_file_new_for_path(path);
    const gboolean result = g_file_copy(file, bf, 0, NULL, NULL, NULL, NULL);
    NOT_NULL_RUN(bf, g_object_unref);
    NOT_NULL_RUN(file, g_object_unref);

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

static void backup_file_enum_init (BackupFileEnum* self)
{
    g_return_if_fail(BACKUP_IS_FILE_ENUM(self));

    char* path = NULL;
    self->files = NULL;
    GFile* file = NULL;
    GFile* iterFile= NULL;
    GFileEnumerator* fe = NULL;
    // GFileInfo* iterFileInfo = NULL;
    GList* mp = get_all_mount_points();
    for (GList* itr = mp; itr; itr = itr->next) {
        NOT_NULL_RUN(path, g_free);
        path = g_strdup_printf("%s/.%s/meta", (char*) itr->data, BACKUP_STR);
        if (path) {
            NOT_NULL_RUN(file, g_object_unref);
            file = g_file_new_for_path (path);
            if (G_IS_FILE(file)) {
                if (g_file_query_exists(file, NULL)) {
                    NOT_NULL_RUN(fe, g_object_unref);
                    fe = g_file_enumerate_children(file, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
                    if (G_IS_FILE_ENUMERATOR(fe)) {
                        do {
                            g_file_enumerator_iterate(fe, NULL, &iterFile, NULL, NULL);
                            if (NULL == iterFile) {
                                break;
                            }
                            char* dir = g_file_get_path(iterFile);
                            BackupMetaFile bf;
                            memset(&bf, 0, sizeof(BackupMetaFile));
                            if (backup_meta_parse_file_path(&bf, dir)) {
                                if (bf.srcFilePath) {
                                    self->files = g_list_append (self->files, g_strdup(bf.srcFilePath));
                                    printf("==>%s\n", bf.srcFilePath);
                                }
                            }
                            STR_FREE(dir);
                            backup_meta_free(&bf);
                        } while (TRUE);
                    }
                }
            }
        }
    }
    NOT_NULL_RUN(path, g_free);
    NOT_NULL_RUN(fe, g_object_unref);
    NOT_NULL_RUN(file, g_object_unref);
    NOT_NULL_RUN(iterFile, g_object_unref);
    NOT_NULL_RUN(mp, g_list_free_full, g_free);

    self->iter = self->files;
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

static void backup_file_enum_class_init (BackupFileEnumClass* klass)
{
    g_return_if_fail (BACKUP_IS_FILE_ENUM_CLASS(klass));

    GObjectClass* objClass = G_OBJECT_CLASS (klass);
    GFileEnumeratorClass* enumerator = G_FILE_ENUMERATOR_CLASS (objClass);

    enumerator->next_file       = vfs_file_enum_next_file;
    enumerator->close_fn        = vfs_file_enum_close;
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

    const int puLen = (int) strlen (pu);
    const int schemaLen = (int) strlen(BACKUP_STR);

    STR_FREE(obj->fileURI);
    STR_FREE(obj->filePath);

    // format
    char* dupPath = NULL;
    if ((puLen - schemaLen >= 3) && (0 == strncmp(pu, BACKUP_STR, schemaLen)) && ('/' == pu[schemaLen + 3])) {
        dupPath = g_strdup(pu + schemaLen + 4);
    }
    else if (puLen > 0 && '/' == pu[0]) {
        dupPath = g_strdup(pu);
    }

    if (dupPath) {
        file_path_format(dupPath);
        obj->filePath = g_strdup (dupPath);
        obj->fileURI = g_strdup_printf ("%s://%s", BACKUP_STR, dupPath);
    }

    STR_FREE (dupPath);
}

static void backup_file_interface_init (GFileIface* interface)
{
    interface->dup                          = vfs_dup;
    interface->hash                         = vfs_hash;
    interface->get_uri                      = vfs_get_uri;
    interface->get_path                     = vfs_get_path;
    interface->equal                        = vfs_is_equal;
    interface->is_native                    = vfs_is_native;
    interface->get_parent                   = vfs_get_parent;
    interface->has_uri_scheme               = vfs_has_schema;
    interface->get_basename                 = vfs_get_basename;
    interface->get_uri_scheme               = vfs_get_uri_schema;
    interface->query_info                   = vfs_file_query_info;
    interface->enumerate_children           = vfs_file_enum_children;
    interface->query_filesystem_info        = vfs_file_query_fs_info;
    interface->move                         = vfs_file_backup_restore;
    interface->copy                         = vfs_file_backup_restore;
    interface->resolve_relative_path        = vfs_resolve_relative_path;
    interface->get_child_for_display_name   = vfs_get_child_for_display_name;
    interface->supports_thread_contexts     = FALSE;
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

static char* vfs_get_basename (GFile* file)
{
    g_return_val_if_fail(BACKUP_IS_FILE(file), NULL);

    char** strArr = NULL;
    char* basename = NULL;
    char* path = vfs_get_path(file);
    if (path) {
        strArr = g_strsplit(path, "/", -1);
        if (g_strv_length(strArr) > 1) {
            for (int i = 0; strArr[i]; i++) {
                if (strArr[i] && (strArr[i + 1] == NULL)) {
                    basename = g_strdup(strArr[i]);
                }
            }
        }
    }

    STR_FREE(path);
    NOT_NULL_RUN(strArr, g_strfreev);

    return basename;
}

static char* vfs_get_uri (GFile* file)
{
    g_return_val_if_fail(BACKUP_IS_FILE(file), NULL);

    GValue value = G_VALUE_INIT;
    g_value_init (&value, G_TYPE_STRING);

    g_object_get_property (G_OBJECT (file), "s-uri", &value);

    return g_value_dup_string (&value);
}

static char* vfs_get_path (GFile* file)
{
    g_return_val_if_fail(BACKUP_IS_FILE(file), NULL);

    GValue value = G_VALUE_INIT;
    g_value_init (&value, G_TYPE_STRING);

    g_object_get_property (G_OBJECT (file), "s-path", &value);

    char* path = g_value_dup_string (&value);

    return path;
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

static gboolean vfs_is_equal (GFile* file, GFile* file2)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file) && BACKUP_IS_FILE(file2), FALSE);

    const BackupFile* s1 = BACKUP_FILE (file);
    const BackupFile* s2 = BACKUP_FILE (file2);

    return (0 == g_strcmp0 (s1->fileURI, s2->fileURI));
}

static GFile* vfs_resolve_relative_path (GFile* file, const char* relativePath)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file), NULL);

    char* pp1 = NULL;
    char** strArr = NULL;
    GFile* resFile = NULL;
    char* path = g_file_get_path (file);

    if (0 == g_strcmp0(path, "") || 0 == g_strcmp0(path, "/")) {
        strArr = g_strsplit(relativePath, "{]", -1);
        if (strArr) {
            pp1 = g_strjoinv("/", strArr);
            if (pp1) {
                resFile = backup_file_new_for_path(pp1);
            }
        }
    }

    STR_FREE(pp1);
    STR_FREE(path);
    NOT_NULL_RUN(strArr, g_strfreev);

    return resFile;
}

static GFile* vfs_dup (GFile* file)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file), NULL);

    GFile* f = backup_file_new_for_uri(BACKUP_FILE(file)->fileURI);

    return f;
}

static GFile* vfs_get_parent (GFile* file)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file), NULL);

    GFile* f = backup_file_new_for_path("/");
    return f;
    (void) file;
}

static GFile* vfs_get_child_for_display_name (GFile* file, const char* displayName, GError** error)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file), NULL);

    return NULL;
    (void) file;
}

static guint vfs_hash (GFile* file)
{
    guint hash = 0;

    g_return_val_if_fail (BACKUP_IS_FILE(file), hash);

    char* uri = vfs_get_uri (file);
    if (uri) {
        hash = g_str_hash(uri);
    }

    STR_FREE (uri);

    return hash;
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
    g_return_val_if_fail (G_IS_FILE(file1) && !BACKUP_IS_FILE(file1), FALSE);
    if (!error) { NOT_NULL_RUN(*error, g_error_free); }

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

    if (!ret && error) {
        // printf("set error: %d\n", __LINE__);
        *error = g_error_new (g_quark_from_static_string(BACKUP_STR), G_IO_ERROR_EXISTS, "%s", g_strdup(""));
    }

    return ret;
    (void) file2;
    (void) error;
}

static gboolean vfs_restore (BackupFile* file1, GFile* file2, GError** error)
{
    g_return_val_if_fail (BACKUP_IS_FILE(file1), FALSE);
    if (!error) { NOT_NULL_RUN(*error, g_error_free); }

    char* path = NULL;                  // free
    gboolean ret = FALSE;
    char* mountPoint = NULL;            // free

    do {
        path = g_file_get_path(G_FILE(file1));
        BREAK_NULL(path);

        mountPoint = get_mount_point_by_uri(G_FILE(file1));
        BREAK_NULL(mountPoint);

        if (!do_restore(path, mountPoint)) { break; }
        ret = TRUE;
    } while (0);

    STR_FREE(path);
    STR_FREE(mountPoint);

    if (!ret && error) {
        *error = g_error_new (g_quark_from_static_string(BACKUP_STR), G_IO_ERROR_FAILED, "%s", g_strdup(""));
    }

    return ret;

    (void) file2;
    (void) error;
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
        int size = (int) fread(buf, 1, sizeof(buf) - 1, fr);
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
    g_return_val_if_fail (G_IS_FILE(file), NULL);

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
    NOT_NULL_RUN(list, g_list_free_full, g_free);

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

static gboolean do_restore (const char* path, const char* mountPoint)
{
    g_return_val_if_fail (path && mountPoint, FALSE);

    GError* error = NULL;               // free
    gboolean ret = FALSE;
    char* fileName = NULL;              // free
    GFile* srcFileF = NULL;             // free
    GFile* dstFileF = NULL;             // free
    char* fileExtStr = NULL;            // free
    char* srcFileStr = NULL;            // free
    char** extStrArr = NULL;            // free
    char* filePathMD5 = NULL;           // free
    char* restoreFileStr = NULL;        // free
    BackupMetaFile backupMetaFile;      // free

    do {
        filePathMD5 = get_file_path_md5 (path);
        BREAK_NULL(filePathMD5);

        if (!backup_meta_parse(&backupMetaFile, path, filePathMD5, mountPoint)) { break; }
        BREAK_NULL(backupMetaFile.srcFilePath);

        dstFileF = g_file_new_for_path (backupMetaFile.srcFilePath);
        BREAK_NULL(dstFileF);

        fileName = g_file_get_basename(dstFileF);
        BREAK_NULL(fileName);

        file_name_to_lower(fileName);
        BREAK_IF_FAIL(strlen(fileName) > 0);
        for (int i = 0; gsFileExt[i]; ++i) {
            if (g_str_has_suffix(fileName, gsFileExt[i])) {
                fileExtStr = g_strdup(gsFileExt[i]);
                break;
            }
        }

        if (!fileExtStr) {
            extStrArr = g_strsplit(fileName, ".", -1);
            if (g_strv_length(extStrArr) > 1) {
                for (int i = 0; extStrArr[i]; ++i) {
                    if (extStrArr[i] && !extStrArr[i + 1]) {
                        fileExtStr = g_strdup_printf(".%s", extStrArr[i]);
                        break;
                    }
                }
            }
        }

        if (backupMetaFile.backupFileCtxMD53) {
            restoreFileStr = file_get_restore_path (backupMetaFile.srcFilePath, fileExtStr, backupMetaFile.backupFileTimestamp3);
            g_object_unref(dstFileF);

            srcFileStr = g_strdup_printf("%s/.%s/backup/%s-3", mountPoint, BACKUP_STR, filePathMD5);
            BREAK_NULL(srcFileStr);

            srcFileF = g_file_new_for_path (srcFileStr);
            BREAK_NULL(srcFileF);

            dstFileF = g_file_new_for_path (restoreFileStr);
            BREAK_NULL(dstFileF);

            g_file_copy(srcFileF, dstFileF, G_FILE_COPY_BACKUP | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            ret = (0 == access(restoreFileStr, F_OK));
        }
        else if (backupMetaFile.backupFileCtxMD52) {
            restoreFileStr = file_get_restore_path (backupMetaFile.srcFilePath, fileExtStr, backupMetaFile.backupFileTimestamp2);
            g_object_unref(dstFileF);

            srcFileStr = g_strdup_printf("%s/.%s/backup/%s-2", mountPoint, BACKUP_STR, filePathMD5);
            BREAK_NULL(srcFileStr);

            srcFileF = g_file_new_for_path (srcFileStr);
            BREAK_NULL(srcFileF);

            dstFileF = g_file_new_for_path (restoreFileStr);
            BREAK_NULL(dstFileF);

            g_file_copy(srcFileF, dstFileF, G_FILE_COPY_BACKUP | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            ret = (0 == access(restoreFileStr, F_OK));
        }
        else if (backupMetaFile.backupFileCtxMD51) {
            restoreFileStr = file_get_restore_path (backupMetaFile.srcFilePath, fileExtStr, backupMetaFile.backupFileTimestamp1);
            g_object_unref(dstFileF);

            srcFileStr = g_strdup_printf("%s/.%s/backup/%s-1", mountPoint, BACKUP_STR, filePathMD5);
            BREAK_NULL(srcFileStr);

            srcFileF = g_file_new_for_path (srcFileStr);
            BREAK_NULL(srcFileF);

            dstFileF = g_file_new_for_path (restoreFileStr);
            BREAK_NULL(dstFileF);

            g_file_copy(srcFileF, dstFileF, G_FILE_COPY_BACKUP | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            ret = (0 == access(restoreFileStr, F_OK));
        }
        else {
            // printf("Not found backup file\n");
            break;
        }
    } while (0);

    STR_FREE(fileName);
    STR_FREE(fileExtStr);
    STR_FREE(srcFileStr);
    STR_FREE(filePathMD5);
    STR_FREE(restoreFileStr);
    NOT_NULL_RUN(error, g_error_free);
    NOT_NULL_RUN(extStrArr, g_strfreev);
    NOT_NULL_RUN(dstFileF, g_object_unref);
    NOT_NULL_RUN(srcFileF, g_object_unref);

    backup_meta_free(&backupMetaFile);

    return ret;
}

static gboolean do_backup (const char* path, const char* mountPoint)
{
    g_return_val_if_fail (path && mountPoint, FALSE);
    if (0 != access(path, F_OK)) { return FALSE; }

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
            backupMetaFile.backupFileCtxMD53 = NULL;
            backupMetaFile.backupFileTimestamp3 = 0;
            ret = g_file_copy(backupFileF, backupFileF3, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            if (ret) {
                backupMetaFile.backupFileCtxMD53 = g_strdup(fileContentMD5);
                backupMetaFile.backupFileTimestamp3 = time(NULL);
            }
        }
        else if (backupMetaFile.backupFileCtxMD52) {
            if (0 == g_strcmp0(backupMetaFile.backupFileCtxMD52, fileContentMD5)) { ret = TRUE; break; }
            backupMetaFile.backupFileCtxMD53 = NULL;
            backupMetaFile.backupFileTimestamp3 = 0;
            ret = g_file_copy(backupFileF, backupFileF3, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            if (ret) {
                backupMetaFile.backupFileCtxMD53 = g_strdup(fileContentMD5);
                backupMetaFile.backupFileTimestamp3 = time(NULL);
            }
        }
        else if (backupMetaFile.backupFileCtxMD51) {
            if (0 == g_strcmp0(backupMetaFile.backupFileCtxMD51, fileContentMD5)) { ret = TRUE; break; }
            backupMetaFile.backupFileCtxMD52 = NULL;
            backupMetaFile.backupFileTimestamp2 = 0;
            ret = g_file_copy(backupFileF, backupFileF2, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &error);
            if (ret) {
                backupMetaFile.backupFileCtxMD52 = g_strdup(fileContentMD5);
                backupMetaFile.backupFileTimestamp2 = time(NULL);
            }
        }
        else {
            backupMetaFile.backupFileCtxMD51 = NULL;
            backupMetaFile.backupFileTimestamp1 = 0;
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

static gboolean backup_meta_parse_file_path (BackupMetaFile* info/*in*/, const char* filePath)
{
    g_return_val_if_fail (info && filePath && '/' == filePath[0], FALSE);

    memset(info, 0, sizeof(BackupMetaFile));

    FILE* metaFr = NULL;
    gboolean ret = FALSE;
    char** strArr = NULL;               // free
    char* metaFileCtx = NULL;           // free

    do {
        if (0 != access(filePath, F_OK)) {
            ret = TRUE;
            break;
        }

        struct stat statBuf;
        guint64 backupFileSize = 0;
        if (!stat(filePath, &statBuf)) {
            backupFileSize = statBuf.st_size + 1;
        }
        if (backupFileSize <= 0) { break; }

        metaFileCtx = g_malloc0(backupFileSize);
        BREAK_NULL(metaFileCtx);

        metaFr = fopen(filePath, "r");
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

                    if (strArr[3] && strlen(strArr[3]) > 0) { info->backupFileCtxMD51    = g_strdup(strArr[3]); }
                    if (strArr[4] && strlen(strArr[4]) > 0) { info->backupFileTimestamp1 = strtoll(strArr[4], NULL, 10); }
                    if (strArr[5] && strlen(strArr[5]) > 0) { info->backupFileCtxMD52    = g_strdup(strArr[5]); }
                    if (strArr[6] && strlen(strArr[6]) > 0) { info->backupFileTimestamp2 = strtoll(strArr[6], NULL, 10); }
                    if (strArr[7] && strlen(strArr[7]) > 0) { info->backupFileCtxMD53    = g_strdup(strArr[7]); }
                    if (strArr[8] && strlen(strArr[8]) > 0) { info->backupFileTimestamp3 = strtoll(strArr[8], NULL, 10); }
                }
            }
        }
        ret = TRUE;
    } while (FALSE);

    STR_FREE(metaFileCtx);
    NOT_NULL_RUN(strArr, g_strfreev);
    if (metaFr) { fclose(metaFr); metaFr = NULL; }

    return ret;
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
        if (backupFileSize <= 0) { break; }

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

                    if (strArr[3] && strlen(strArr[3]) > 0) { info->backupFileCtxMD51    = g_strdup(strArr[3]); }
                    if (strArr[4] && strlen(strArr[4]) > 0) { info->backupFileTimestamp1 = strtoll(strArr[4], NULL, 10); }
                    if (strArr[5] && strlen(strArr[5]) > 0) { info->backupFileCtxMD52    = g_strdup(strArr[5]); }
                    if (strArr[6] && strlen(strArr[6]) > 0) { info->backupFileTimestamp2 = strtoll(strArr[6], NULL, 10); }
                    if (strArr[7] && strlen(strArr[7]) > 0) { info->backupFileCtxMD53    = g_strdup(strArr[7]); }
                    if (strArr[8] && strlen(strArr[8]) > 0) { info->backupFileTimestamp3 = strtoll(strArr[8], NULL, 10); }
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

        fwrite(metaFileCtx, 1, strlen(metaFileCtx), metaFr);
        fclose(metaFr);
        metaFr = NULL;
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

static void file_path_format (char* filePath)
{
    g_return_if_fail(filePath);

    int i = 0;
    const int fLen = (int) strlen (filePath);
    for (i = 0; filePath[i]; ++i) {
        while (filePath[i] && '/' == filePath[i] && '/' == filePath[i + 1]) {
            for (int j = i; filePath[j] || j < fLen; filePath[j] = filePath[j + 1], ++j);
        }
    }

    if ((i - 1 >= 0) && filePath[i - 1] == '/') {
        filePath[i - 1] = '\0';
    }
}

static void file_name_to_lower (char* fileName)
{
    g_return_if_fail (fileName);

    const int step = 'a' - 'A';
    const int len = strlen (fileName);

    for (int i = 0; i < len; i++) {
        if (fileName[i] >= 'A' && fileName[i] <= 'Z') {
            fileName[i] = fileName[i] + step;
        }
    }
}

static char* file_get_restore_path (const char* srcFilePath, const char* extName, guint64 timestamp)
{
    g_return_val_if_fail(srcFilePath && timestamp > 0, FALSE);

    char* ret = NULL;
    char timeBuf[32] = {0};
    struct tm* curTime = localtime (&timestamp);
    int nameLen = strlen (srcFilePath) + sizeof (timeBuf) + 2;

    if (extName) {
        nameLen += strlen (extName);
    }

    ret = g_malloc0 (nameLen);

    if (curTime) {
        snprintf(timeBuf, sizeof (timeBuf) - 1, "%04d%02d%02d%02d%02d%02d",
            1900 + curTime->tm_year, curTime->tm_mon + 1, curTime->tm_mday, curTime->tm_hour, curTime->tm_min, curTime->tm_sec);
    }

    memcpy(ret, srcFilePath, strlen (srcFilePath));
    if (extName && strlen(srcFilePath) > strlen(extName)) {
        const int timeStart = strlen(srcFilePath) - strlen (extName);
        memcpy(ret + timeStart, "-", 1);
        memcpy(ret + timeStart + 1, timeBuf, strlen(timeBuf));
        memcpy(ret + timeStart + 1 + strlen(timeBuf), extName, strlen(extName));
    }
    else {
        memcpy(ret + strlen(srcFilePath), "-", 1);
        memcpy(ret + strlen(srcFilePath) + 1, timeBuf, strlen(timeBuf));
    }

    return ret;
}

static GFileInfo* vfs_file_enum_next_file (GFileEnumerator *enumerator, GCancellable *cancellable, GError **error)
{
    g_return_val_if_fail(BACKUP_IS_FILE_ENUM(enumerator), NULL);

    BackupFileEnum* eb = BACKUP_FILE_ENUM(enumerator);

    if (cancellable && g_cancellable_is_cancelled(cancellable)) {
        if (error) {
            *error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_CANCELLED, "cancelled");
        }
        return NULL;
    }

    char* uri = NULL;
    GFile* ff = NULL;
    GFileInfo* info = NULL;
    if (eb->iter && eb->iter->data) {
        const char* f = eb->files->data;
        if (f && '/' == f[0]) {
            uri = g_strdup_printf ("%s://%s", BACKUP_STR, (char*) eb->iter->data);
            if (uri) {
                ff = g_file_new_for_uri (uri);
                info = g_file_query_info(ff, "standard::*", G_FILE_QUERY_INFO_NONE, cancellable, error);
            }
        }
    }

    if (eb->iter) {
        eb->iter = eb->iter->next;
    }
    else {
        NOT_NULL_RUN(info, g_object_unref);
    }

    STR_FREE(uri);
    NOT_NULL_RUN(ff, g_object_unref);

    return info;
}

GFileEnumerator* vfs_file_enum_children (GFile* file, const char* attribute, GFileQueryInfoFlags flags, GCancellable* cancel, GError** error)
{
    g_return_val_if_fail(BACKUP_IS_FILE(file), NULL);

    GFileEnumerator* e = G_FILE_ENUMERATOR(g_object_new(BACKUP_FILE_ENUM_TYPE, "container", file, NULL));

    return e;

    (void) flags;
    (void) error;
    (void) cancel;
    (void) attribute;
}

static gboolean vfs_file_enum_close (GFileEnumerator *enumerator, GCancellable *cancellable, GError **error)
{
    g_return_val_if_fail(BACKUP_IS_FILE_ENUM(enumerator), FALSE);

    BackupFileEnum* e = BACKUP_FILE_ENUM(enumerator);

    e->iter = NULL;
    NOT_NULL_RUN(e->files, g_list_free_full, g_free);

    return TRUE;

    (void) error;
    (void) cancellable;
}

static GFileInfo* vfs_file_query_fs_info (GFile* file, const char* attr, GCancellable* cancel, GError** error)
{
    g_return_val_if_fail(BACKUP_IS_FILE(file), NULL);

    GFile* f = NULL;
    GFileInfo* info = NULL;
    f = g_file_new_for_path ("/");

    if (G_IS_FILE(f)) {
        info = g_file_query_info (f, attr, G_FILE_QUERY_INFO_NONE, cancel, error);
    }


    return info;
}

static GFileInfo* vfs_file_query_info (GFile* file, const char* attr, GFileQueryInfoFlags flags, GCancellable* cancel, GError** error)
{
    g_return_val_if_fail(BACKUP_IS_FILE(file), NULL);

    char** strArr = NULL;
    char* baseName = NULL;
    GFileInfo* info = NULL;
    char* uri = g_file_get_uri(file);
    // printf("[URL]: %s\n", uri);
    char* path = g_file_get_path(file);
    if (path) {
        // printf("[PATH]: %s, uri: %s\n", path, uri);
        info = g_file_info_new();

        strArr = g_strsplit(path, "/", -1);
        baseName = g_strjoinv("{]", strArr);

        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL, TRUE);
        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN, FALSE);
        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP, FALSE);
        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK, FALSE);
        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_VOLATILE, FALSE);

        g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_TYPE_REGULAR);

        g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI, uri);

        g_file_info_set_attribute_byte_string(info, G_FILE_ATTRIBUTE_STANDARD_NAME, baseName);
        g_file_info_set_attribute_byte_string(info, G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME, baseName);
        g_file_info_set_attribute_byte_string(info, G_FILE_ATTRIBUTE_STANDARD_COPY_NAME, baseName);
        g_file_info_set_attribute_byte_string(info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, baseName);

        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, TRUE);
        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH, FALSE);
        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, FALSE);
        g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, FALSE);
    }

    STR_FREE(uri);
    STR_FREE(path);
    STR_FREE(baseName);
    NOT_NULL_RUN(strArr, g_strfreev);

    return info;

    (void) attr;
    (void) flags;
    (void) error;
    (void) cancel;
}

static gboolean vfs_has_schema (GFile* file, const char* uriSchema)
{
    g_return_val_if_fail(BACKUP_IS_FILE(file) && uriSchema, FALSE);

    return g_str_has_prefix(uriSchema, BACKUP_STR);
}
