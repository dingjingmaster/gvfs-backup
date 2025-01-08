//
// Created by dingjing on 1/8/25.
//
#include <stdio.h>

#include "backup.h"

int main (int argc, char* argv[])
{
    {
        GFile* file1 = backup_file_new_for_path("/tmp/file1");
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

    return 0;
}