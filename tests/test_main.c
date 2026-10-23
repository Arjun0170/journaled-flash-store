#include "flash_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t read_byte(store_t *store, uint8_t key) {
    uint8_t value[STORE_MAX_VALUE] = {0};
    size_t length = 0;
    assert(store_get(store, key, value, &length) == STORE_OK && length == 1u);
    return value[0];
}

int main(void) {
    assert(store_crc32((const uint8_t *)"123456789", 9u) == 0xCBF43926u);
    flash_model_t model;
    flash_model_init(&model);
    store_t store;
    assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
    uint8_t out[STORE_MAX_VALUE];
    size_t length;
    assert(store_get(&store, 0u, out, &length) == STORE_NOT_FOUND);
    uint8_t one = 1u;
    assert(store_put(&store, 0u, &one, 1u) == STORE_OK);
    assert(read_byte(&store, 0u) == 1u);
    assert(store_put(&store, 0u, &one, 1u) == STORE_OK); /* unchanged: no write */
    assert(store_delete(&store, 0u) == STORE_OK);
    assert(store_get(&store, 0u, out, &length) == STORE_NOT_FOUND);
    assert(store_delete(&store, 0u) == STORE_NOT_FOUND);
    assert(store_put(&store, 8u, &one, 1u) == STORE_BAD_ARGUMENT);
    assert(store_put(&store, 0u, &one, 0u) == STORE_BAD_ARGUMENT);
    flash_model_t damaged = model;
    damaged.bytes[0][0] ^= 1u; /* no valid header, nonblank flash */
    assert(store_mount(&store, flash_model_interface(&damaged)) == STORE_CORRUPT);

    /* Interrupted ordinary record: previous committed value is preserved. */
    flash_model_t baseline;
    flash_model_init(&baseline);
    assert(store_mount(&store, flash_model_interface(&baseline)) == STORE_OK);
    uint8_t old = 7u, newer = 9u;
    assert(store_put(&store, 0u, &old, 1u) == STORE_OK);
    for (int budget = 0; budget < STORE_RECORD_BYTES; ++budget) {
        model = baseline;
        model.program_budget = budget;
        assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
        assert(store_put(&store, 0u, &newer, 1u) == STORE_IO_ERROR);
        model.program_budget = -1;
        assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
        assert(read_byte(&store, 0u) == old);
    }

    /* Fill 21 record slots; next put compacts two live keys into other sector. */
    flash_model_init(&baseline);
    assert(store_mount(&store, flash_model_interface(&baseline)) == STORE_OK);
    uint8_t stable = 42u;
    assert(store_put(&store, 1u, &stable, 1u) == STORE_OK);
    for (unsigned i = 0; i < 20u; ++i) {
        uint8_t value = (uint8_t)i;
        assert(store_put(&store, 0u, &value, 1u) == STORE_OK);
    }
    assert(read_byte(&store, 0u) == 19u);
    /* 12 header bytes + 2*48 copied records + 4 commit + 48 new record. */
    for (int budget = 0; budget < 160; ++budget) {
        model = baseline;
        model.program_budget = budget;
        assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
        uint8_t value = 99u;
        assert(store_put(&store, 0u, &value, 1u) == STORE_IO_ERROR);
        model.program_budget = -1;
        assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
        assert(read_byte(&store, 0u) == 19u);
        assert(read_byte(&store, 1u) == stable);
    }
    for (unsigned b = 0; b < 4u; ++b) {
        int budgets[] = {0, 1, 512, 1023};
        model = baseline;
        model.erase_budget = budgets[b];
        assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
        uint8_t value = 99u;
        assert(store_put(&store, 0u, &value, 1u) == STORE_IO_ERROR);
        model.erase_budget = -1;
        assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
        assert(read_byte(&store, 0u) == 19u);
    }
    model = baseline;
    assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
    uint8_t value = 99u;
    assert(store_put(&store, 0u, &value, 1u) == STORE_OK);
    assert(store.epoch == 1u && store.active_sector == 1u);
    assert(read_byte(&store, 0u) == 99u && read_byte(&store, 1u) == stable);
    /* Corrupt latest record; earlier copied value still validates. */
    model.bytes[1][STORE_HEADER_BYTES + 2u * STORE_RECORD_BYTES + 8u] ^= 1u;
    assert(read_byte(&store, 0u) == 19u);

    /* Several rotations demonstrate bounded, reusable storage. */
    flash_model_init(&model);
    assert(store_mount(&store, flash_model_interface(&model)) == STORE_OK);
    for (unsigned i = 0; i < 100u; ++i) {
        uint8_t byte = (uint8_t)i;
        assert(store_put(&store, 0u, &byte, 1u) == STORE_OK);
    }
    assert(read_byte(&store, 0u) == 99u && store.epoch >= 4u);
    puts("flash store tests passed (208 write interruption points)");
    return 0;
}
