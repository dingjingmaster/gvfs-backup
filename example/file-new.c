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
        // GList* ls = get_all_mount_points();
        // for (const GList* id = ls; id; id = id->next) {
        //     printf("%s\n", (char*) id->data);
        // }
        // g_list_free_full(ls, g_free);
    }


    return 0;
}