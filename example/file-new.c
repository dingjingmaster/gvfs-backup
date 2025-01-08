//
// Created by dingjing on 1/8/25.
//
#include <stdio.h>

#include "backup.h"

int main (int argc, char* argv[])
{
    GFile* file = backup_file_new_for_path("/tmp/file");

    char* path = backup_file_get_path(file);
    if (path) {
        printf("%s\n", path);
    }

    return 0;
}