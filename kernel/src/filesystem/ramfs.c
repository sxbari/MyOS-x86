
#include "ramfs.h"
#include "memory/heap.h"

typedef struct {
    bool used;
    char name[RAMFS_NAME_SIZE];
    char *data;
} ramfs_file_t;

static ramfs_file_t files[RAMFS_MAX_FILES];

static bool equals(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

static size_t text_length(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

static void text_copy(char *dst, const char *src, size_t capacity)
{
    size_t i = 0;
    while (i + 1 < capacity && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static ramfs_file_t *find_file(const char *name)
{
    for (size_t i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used && equals(files[i].name, name))
            return &files[i];

    return NULL;
}

void ramfs_init(void)
{
    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        files[i].used = false;
        files[i].data = NULL;
        files[i].name[0] = '\0';
    }
}

bool ramfs_create(const char *name)
{
    size_t len = text_length(name);

    if (len == 0 || len >= RAMFS_NAME_SIZE || find_file(name))
        return false;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used)
            continue;

        char *data = kmalloc(RAMFS_FILE_SIZE);
        if (!data)
            return false;

        data[0] = '\0';
        text_copy(files[i].name, name, RAMFS_NAME_SIZE);
        files[i].data = data;
        files[i].used = true;
        return true;
    }

    return false;
}

bool ramfs_write(const char *name, const char *content)
{
    ramfs_file_t *file = find_file(name);

    if (!file || text_length(content) >= RAMFS_FILE_SIZE)
        return false;

    text_copy(file->data, content, RAMFS_FILE_SIZE);
    return true;
}

const char *ramfs_read(const char *name)
{
    ramfs_file_t *file = find_file(name);
    return file ? file->data : NULL;
}

bool ramfs_delete(const char *name)
{
    ramfs_file_t *file = find_file(name);

    if (!file)
        return false;

    kfree(file->data);
    file->data = NULL;
    file->name[0] = '\0';
    file->used = false;
    return true;
}

const char *ramfs_name(size_t index)
{
    if (index >= RAMFS_MAX_FILES || !files[index].used)
        return NULL;

    return files[index].name;
}
