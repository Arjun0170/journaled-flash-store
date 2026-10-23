#ifndef STORE_H
#define STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STORE_SECTOR_BYTES 1024u
#define STORE_HEADER_BYTES 16u
#define STORE_RECORD_BYTES 48u
#define STORE_RECORD_SLOTS 21u
#define STORE_MAX_KEYS 8u
#define STORE_MAX_VALUE 32u

typedef struct {
    bool (*read)(void *context, unsigned sector, uint32_t offset,
                 void *out, size_t length);
    bool (*program)(void *context, unsigned sector, uint32_t offset,
                    const void *data, size_t length);
    bool (*erase)(void *context, unsigned sector);
    void *context;
} store_flash_t;

typedef enum {
    STORE_OK = 0,
    STORE_NOT_FOUND,
    STORE_BAD_ARGUMENT,
    STORE_IO_ERROR,
    STORE_CORRUPT,
    STORE_EPOCH_EXHAUSTED
} store_result_t;

typedef struct {
    store_flash_t flash;
    uint32_t epoch;
    unsigned active_sector;
    bool mounted;
} store_t;

/* Mounts the latest committed sector; formats empty flash on first use. */
store_result_t store_mount(store_t *store, store_flash_t flash);
store_result_t store_get(const store_t *store, uint8_t key,
                         uint8_t out[STORE_MAX_VALUE], size_t *length);
store_result_t store_put(store_t *store, uint8_t key,
                         const uint8_t *value, size_t length);
store_result_t store_delete(store_t *store, uint8_t key);
uint32_t store_crc32(const uint8_t *data, size_t length);

#endif
