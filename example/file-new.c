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
        GFile* file2 = g_file_new_for_uri(BACKUP_STR ":///");
        const gboolean result = g_file_copy(file1, file2, 0, NULL, NULL, NULL, NULL);
        printf("[Backup] result: %s\n", result ? "true" : "false");
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


    return 0;
}
