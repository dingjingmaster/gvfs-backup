cmake_minimum_required(VERSION 3.10)
project(gvfs-backup C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig)

pkg_check_modules(GIO REQUIRED gio-2.0)

add_library(gvfs-backup SHARED src/backup.c src/backup.h)
target_link_libraries(gvfs-backup PUBLIC ${GIO_LIBRARIES})
target_include_directories(gvfs-backup PUBLIC ${GIO_INCLUDE_DIRS})

add_executable(file-new example/file-new.c)
target_link_libraries(file-new PUBLIC ${GIO_LIBRARIES} gvfs-backup)
target_compile_options(file-new PUBLIC -Wl,rpath=${CMAKE_BINARY_DIR}/)
target_include_directories(file-new PUBLIC ${GIO_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)
