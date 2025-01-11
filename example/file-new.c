//
// Created by dingjing on 1/8/25.
//
#include <stdio.h>

#include "backup.h"

int main (int argc, char* argv[])
{
    (void) argc;
    (void) argv;

    {
        GFile* file1 = backup_file_new_for_path("/tmp/file1");
        printf("is file: %s, is backup file: %s\n", G_IS_FILE(file1) ? "true" : "false", BACKUP_IS_FILE(file1) ? "true" : "false");
        char* path1 = g_file_get_path(file1);
        if (path1) {
            printf("path: %s\n", path1);
            g_free(path1);
        }
        else {
            printf("null\n");
        }
        char* uri1 = g_file_get_uri(file1);
        if (uri1) {
            printf("uri: %s\n", uri1);
            g_free(uri1);
        }
        g_object_unref(file1);
    }

    {
        GFile* file1 = g_file_new_for_uri("andsec-backup:///tmp/file2");
        char* path1 = g_file_get_path(file1);
        if (path1) {
            printf("path: %s\n", path1);
            g_free(path1);
        }
        else {
            printf("null\n");
        }
        char* uri1 = g_file_get_uri(file1);
        if (uri1) {
            printf("uri: %s\n", uri1);
            g_free(uri1);
        }
        g_object_unref(file1);
    }

    {
        GFile* file1 = g_file_new_for_uri("file:///tmp/file3");
        printf("[file] is file: %s, is backup file: %s\n", G_IS_FILE(file1) ? "true" : "false", BACKUP_IS_FILE(file1) ? "true" : "false");
        char* path1 = g_file_get_path(file1);
        if (path1) {
            printf("path: %s\n", path1);
            g_free(path1);
        }
        else {
            printf("null\n");
        }
        char* uri1 = g_file_get_uri(file1);
        if (uri1) {
            printf("uri: %s\n", uri1);
            g_free(uri1);
        }
        g_object_unref(file1);
    }

    {
        GFile* file1 = g_file_new_for_path("/tmp/file4");
        char* path1 = g_file_get_path(file1);
        if (path1) {
            printf("path: %s\n", path1);
            g_free(path1);
        }
        else {
            printf("null\n");
        }
        char* uri1 = g_file_get_uri(file1);
        if (uri1) {
            printf("uri: %s\n", uri1);
            g_free(uri1);
        }
        g_object_unref(file1);
    }

    {
        // backup
        GFile* file1 = g_file_new_for_path("/tmp/file5");
        GFile* file2 = backup_file_new_for_path("/");
        const gboolean result = g_file_copy(file1, file2, 0, NULL, NULL, NULL, NULL);
        printf("[Backup] file5 result: %s\n", result ? "true" : "false");
        g_object_unref(file1);
        g_object_unref(file2);
    }

    {
        GFile* file1 = g_file_new_for_uri("andsec-backup://////////////tmp///////////////file6////////////");
        char* path1 = g_file_get_path(file1);
        if (path1) {
            printf("path: %s\n", path1);
            g_free(path1);
        }
        else {
            printf("null\n");
        }
        char* uri1 = g_file_get_uri(file1);
        if (uri1) {
            printf("uri: %s\n", uri1);
            g_free(uri1);
        }
        g_object_unref(file1);
    }

    {
        // backup 2
        GFile* file1 = g_file_new_for_path("/tmp/file6");
        backup_file_backup(file1);
        g_object_unref(file1);
    }

    {
        // backup 3
        backup_file_backup_by_abspath("/tmp/file7");
    }

    {
        // restore
        gboolean res = backup_file_restore_by_abspath("/tmp/file7");
        printf("[Restore] res: %s\n", res ? "true" : "false");
    }

    {
        // backup && restore
        gboolean res = FALSE;
        res = backup_file_backup_by_abspath("/tmp/file8.txt");
        printf("[Backup] res: %s\n", res ? "true" : "false");
        res = backup_file_restore_by_abspath("/tmp/file8.txt");
        printf("[Restore] res: %s\n", res ? "true" : "false");
    }

    {
        // enumerator
        GError* error = NULL;
        GFile* f = NULL;
        GFileInfo* fi = NULL;
        GFileEnumerator* fs = NULL;
        GFile* file1 = backup_file_new_for_path("/");
        if (G_IS_FILE(file1)) {
            fs = g_file_enumerate_children (file1, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
            if (G_IS_FILE_ENUMERATOR(fs)) {
                do {
                    if (NULL != f) { g_object_unref (f); f = NULL; }
                    if (NULL != fi) { g_object_unref (fi); fi = NULL; }
                    // g_file_enumerator_iterate (fs, NULL, &f, NULL, NULL);
                    fi = g_file_enumerator_next_file(fs, NULL, NULL);
                    if (NULL != error) {
                        printf ("error:%s\n", error->message ? error->message : "NULL");
                        g_error_free(error);
                        error = NULL;
                        continue;
                    }

                    if (NULL == fi) {
                        printf ("结束迭代!文件总数\n");
                        break;
                    }

                    char* uriT = g_file_info_get_attribute_as_string(fi, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
                    if (uriT) {
                        f = g_file_new_for_uri (uriT);
                        g_free(uriT);
                        uriT = NULL;
                    }

                    printf ("============================= 开始 ===============================\n");
                    printf ("<path>:%s\n", g_file_get_path(f));
                    printf ("<uri> :%s\n", g_file_get_uri(f));
                    printf ("============================= 结束 ===============================\n");
                } while (TRUE);
            }
        }

        if (NULL != f)      { g_object_unref (f); f = NULL; }
        if (NULL != fi)     { g_object_unref (fi); fi = NULL; }
        if (NULL != fs)     { g_object_unref (fs); fs = NULL; }
        if (NULL != error)  { g_error_free (error); error = NULL; }
        if (NULL != file1)  { g_object_unref (file1); file1 = NULL; }
    }

    return 0;
}
