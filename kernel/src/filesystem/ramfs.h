
#ifndef MYOS_RAMFS_H
#define MYOS_RAMFS_H

#include <stdbool.h>
#include <stddef.h>

#define RAMFS_MAX_FILES 16
#define RAMFS_NAME_SIZE 32
#define RAMFS_FILE_SIZE 256

void ramfs_init(void);
bool ramfs_create(const char *name);
bool ramfs_write(const char *name, const char *content);
const char *ramfs_read(const char *name);
bool ramfs_delete(const char *name);
const char *ramfs_name(size_t index);

#endif
