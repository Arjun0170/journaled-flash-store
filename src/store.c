#include "store.h"

#include <string.h>

#define SECTOR_MAGIC 0x53454354u /* SECT */
#define SECTOR_COMMIT 0xA51C0DE1u
#define RECORD_MAGIC 0x52454344u /* RECD */
#define RECORD_COMMIT 0xC0DEC0DEu

static void put32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4u; ++i) p[i] = (uint8_t)(value >> (8u * i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint32_t store_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

static store_result_t read_sector_header(const store_flash_t *flash,
                                         unsigned sector, bool *valid,
                                         uint32_t *epoch) {
    uint8_t header[STORE_HEADER_BYTES];
    if (!flash->read(flash->context, sector, 0u, header, sizeof(header)))
        return STORE_IO_ERROR;
    *valid = get32(header) == SECTOR_MAGIC &&
             get32(header + 8) == store_crc32(header, 8u) &&
             get32(header + 12) == SECTOR_COMMIT;
    if (*valid) *epoch = get32(header + 4);
    return STORE_OK;
}

static store_result_t sector_blank(const store_flash_t *flash, unsigned sector,
                                   bool *blank) {
    *blank = true;
    for (uint32_t offset = 0; offset < STORE_SECTOR_BYTES; offset += 64u) {
        uint8_t bytes[64];
        if (!flash->read(flash->context, sector, offset, bytes, sizeof(bytes)))
            return STORE_IO_ERROR;
        for (unsigned i = 0; i < sizeof(bytes); ++i)
            if (bytes[i] != 0xFFu) *blank = false;
    }
    return STORE_OK;
}

static bool write_header_body(const store_flash_t *flash, unsigned sector,
                              uint32_t epoch) {
    uint8_t header[12];
    put32(header, SECTOR_MAGIC);
    put32(header + 4, epoch);
    put32(header + 8, store_crc32(header, 8u));
    return flash->program(flash->context, sector, 0u, header, sizeof(header));
}
static bool commit_sector(const store_flash_t *flash, unsigned sector) {
    uint8_t marker[4];
    put32(marker, SECTOR_COMMIT);
    return flash->program(flash->context, sector, 12u, marker, sizeof(marker));
}

store_result_t store_mount(store_t *store, store_flash_t flash) {
    if (store == 0 || flash.read == 0 || flash.program == 0 || flash.erase == 0)
        return STORE_BAD_ARGUMENT;
    memset(store, 0, sizeof(*store));
    store->flash = flash;
    uint32_t epoch0 = 0, epoch1 = 0;
    bool valid0 = false, valid1 = false;
    if (read_sector_header(&flash, 0u, &valid0, &epoch0) != STORE_OK ||
        read_sector_header(&flash, 1u, &valid1, &epoch1) != STORE_OK)
        return STORE_IO_ERROR;
    if (valid0 || valid1) {
        store->active_sector = valid1 && (!valid0 || epoch1 > epoch0) ? 1u : 0u;
        store->epoch = store->active_sector ? epoch1 : epoch0;
        store->mounted = true;
        return STORE_OK;
    }
    bool blank0, blank1;
    if (sector_blank(&flash, 0u, &blank0) != STORE_OK ||
        sector_blank(&flash, 1u, &blank1) != STORE_OK)
        return STORE_IO_ERROR;
    if (!blank0 || !blank1) return STORE_CORRUPT;
    if (!flash.erase(flash.context, 0u) ||
        !write_header_body(&flash, 0u, 0u) ||
        !commit_sector(&flash, 0u)) return STORE_IO_ERROR;
    store->active_sector = 0u;
    store->mounted = true;
    return STORE_OK;
}

static uint32_t record_offset(unsigned slot) {
    return STORE_HEADER_BYTES + slot * STORE_RECORD_BYTES;
}
static bool blank_record(const uint8_t record[STORE_RECORD_BYTES]) {
    for (unsigned i = 0; i < STORE_RECORD_BYTES; ++i)
        if (record[i] != 0xFFu) return false;
    return true;
}
static bool valid_record(const uint8_t record[STORE_RECORD_BYTES]) {
    return get32(record) == RECORD_MAGIC &&
           record[4] < STORE_MAX_KEYS && record[5] <= STORE_MAX_VALUE &&
           get32(record + 40) == store_crc32(record, 40u) &&
           get32(record + 44) == RECORD_COMMIT;
}

store_result_t store_get(const store_t *store, uint8_t key,
                         uint8_t out[STORE_MAX_VALUE], size_t *length) {
    if (store == 0 || !store->mounted || key >= STORE_MAX_KEYS ||
        out == 0 || length == 0) return STORE_BAD_ARGUMENT;
    bool found = false;
    size_t current_length = 0u;
    for (unsigned slot = 0; slot < STORE_RECORD_SLOTS; ++slot) {
        uint8_t record[STORE_RECORD_BYTES];
        if (!store->flash.read(store->flash.context, store->active_sector,
                               record_offset(slot), record, sizeof(record)))
            return STORE_IO_ERROR;
        if (!valid_record(record) || record[4] != key) continue;
        found = record[5] != 0u;
        current_length = record[5];
        if (found) memcpy(out, record + 8, current_length);
    }
    if (!found) return STORE_NOT_FOUND;
    *length = current_length;
    return STORE_OK;
}

static store_result_t first_blank(const store_t *store, unsigned *slot_out) {
    for (unsigned slot = 0; slot < STORE_RECORD_SLOTS; ++slot) {
        uint8_t record[STORE_RECORD_BYTES];
        if (!store->flash.read(store->flash.context, store->active_sector,
                               record_offset(slot), record, sizeof(record)))
            return STORE_IO_ERROR;
        if (blank_record(record)) {
            *slot_out = slot;
            return STORE_OK;
        }
    }
    *slot_out = STORE_RECORD_SLOTS;
    return STORE_OK;
}

static store_result_t append_record(const store_flash_t *flash, unsigned sector,
                                    unsigned slot, uint8_t key,
                                    const uint8_t *value, size_t length) {
    uint8_t record[STORE_RECORD_BYTES];
    memset(record, 0xFF, sizeof(record));
    put32(record, RECORD_MAGIC);
    record[4] = key;
    record[5] = (uint8_t)length;
    if (length != 0u) memcpy(record + 8, value, length);
    put32(record + 40, store_crc32(record, 40u));
    put32(record + 44, RECORD_COMMIT);
    uint32_t offset = record_offset(slot);
    if (!flash->program(flash->context, sector, offset, record, 44u) ||
        !flash->program(flash->context, sector, offset + 44u,
                        record + 44, 4u)) return STORE_IO_ERROR;
    return STORE_OK;
}

static store_result_t compact(store_t *store) {
    if (store->epoch == UINT32_MAX) return STORE_EPOCH_EXHAUSTED;
    uint8_t values[STORE_MAX_KEYS][STORE_MAX_VALUE];
    size_t lengths[STORE_MAX_KEYS] = {0};
    for (uint8_t key = 0; key < STORE_MAX_KEYS; ++key) {
        store_result_t result = store_get(store, key, values[key], &lengths[key]);
        if (result != STORE_OK && result != STORE_NOT_FOUND) return result;
        if (result == STORE_NOT_FOUND) lengths[key] = 0u;
    }
    unsigned target = 1u - store->active_sector;
    if (!store->flash.erase(store->flash.context, target) ||
        !write_header_body(&store->flash, target, store->epoch + 1u))
        return STORE_IO_ERROR;
    unsigned slot = 0;
    for (uint8_t key = 0; key < STORE_MAX_KEYS; ++key) {
        if (lengths[key] == 0u) continue;
        store_result_t result = append_record(&store->flash, target, slot++,
                                               key, values[key], lengths[key]);
        if (result != STORE_OK) return result;
    }
    if (!commit_sector(&store->flash, target)) return STORE_IO_ERROR;
    store->active_sector = target;
    store->epoch++;
    return STORE_OK;
}

static store_result_t write_value(store_t *store, uint8_t key,
                                  const uint8_t *value, size_t length) {
    unsigned slot;
    store_result_t result = first_blank(store, &slot);
    if (result != STORE_OK) return result;
    if (slot == STORE_RECORD_SLOTS) {
        result = compact(store);
        if (result != STORE_OK) return result;
        result = first_blank(store, &slot);
        if (result != STORE_OK) return result;
    }
    return append_record(&store->flash, store->active_sector, slot,
                         key, value, length);
}

store_result_t store_put(store_t *store, uint8_t key,
                         const uint8_t *value, size_t length) {
    if (store == 0 || !store->mounted || key >= STORE_MAX_KEYS ||
        value == 0 || length == 0u || length > STORE_MAX_VALUE)
        return STORE_BAD_ARGUMENT;
    uint8_t previous[STORE_MAX_VALUE];
    size_t previous_length = 0;
    store_result_t result = store_get(store, key, previous, &previous_length);
    if (result == STORE_OK && previous_length == length &&
        memcmp(previous, value, length) == 0) return STORE_OK;
    if (result != STORE_OK && result != STORE_NOT_FOUND) return result;
    return write_value(store, key, value, length);
}

store_result_t store_delete(store_t *store, uint8_t key) {
    if (store == 0 || !store->mounted || key >= STORE_MAX_KEYS)
        return STORE_BAD_ARGUMENT;
    uint8_t previous[STORE_MAX_VALUE];
    size_t length;
    store_result_t result = store_get(store, key, previous, &length);
    if (result != STORE_OK) return result;
    return write_value(store, key, 0, 0u);
}
