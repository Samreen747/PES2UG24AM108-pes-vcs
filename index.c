#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Load index from .pes/index
 */
int index_load(Index *idx) {
    FILE *f = fopen(".pes/index", "r");

    idx->count = 0;

    if (!f) {
        // No index file → empty index
        return 0;
    }

    while (idx->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &idx->entries[idx->count];

        char hash_hex[HASH_HEX_SIZE + 1];

        int ret = fscanf(f, "%o %40s %lu %lu %255[^\n]\n",
                         &e->mode,
                         hash_hex,
                         &e->mtime,
                         &e->size,
                         e->path);

        if (ret != 5) break;

        // convert hex → binary hash
        if (hex_to_hash(hash_hex, &e->hash) != 0) {
            fclose(f);
            return -1;
        }

        idx->count++;
    }

    fclose(f);
    return 0;
}

/*
 * Save index to .pes/index (atomic write)
 */
int index_save(const Index *idx) {
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;

    for (int i = 0; i < idx->count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&idx->entries[i].hash, hex);

        fprintf(f, "%o %s %lu %lu %s\n",
                idx->entries[i].mode,
                hex,
                idx->entries[i].mtime,
                idx->entries[i].size,
                idx->entries[i].path);
    }

    fclose(f);

    // atomic rename
    rename(".pes/index.tmp", ".pes/index");

    return 0;
}

/*
 * Add file to index
 */
int index_add(Index *idx, const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    void *buffer = malloc(st.st_size);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    fread(buffer, 1, st.st_size, f);
    fclose(f);

    // write blob object
    ObjectID oid;
    if (object_write(OBJ_BLOB, buffer, st.st_size, &oid) != 0) {
        free(buffer);
        return -1;
    }

    free(buffer);

    // check if entry already exists
    for (int i = 0; i < idx->count; i++) {
        if (strcmp(idx->entries[i].path, path) == 0) {
            idx->entries[i].hash = oid;
            idx->entries[i].mode = get_file_mode(path);
            idx->entries[i].mtime = st.st_mtime;
            idx->entries[i].size = st.st_size;
            return 0;
        }
    }

    // add new entry
    if (idx->count >= MAX_INDEX_ENTRIES) {
        return -1;
    }

    IndexEntry *e = &idx->entries[idx->count];

    strcpy(e->path, path);
    e->hash = oid;
    e->mode = get_file_mode(path);
    e->mtime = st.st_mtime;
    e->size = st.st_size;

    idx->count++;

    return 0;
}
