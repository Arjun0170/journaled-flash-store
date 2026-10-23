#include "flash_model.h"

#include <stdio.h>

static void show(store_t *store, const char *label) {
    uint8_t value[STORE_MAX_VALUE];
    size_t length = 0;
    store_result_t result = store_get(store, 0u, value, &length);
    printf("%s: sector=%u epoch=%lu key0=%s", label, store->active_sector,
           (unsigned long)store->epoch,
           result == STORE_OK ? (const char *)value : "<missing>");
    if (result == STORE_OK) printf(" (%zu bytes)", length);
    putchar('\n');
}

int main(void) {
    flash_model_t model;
    flash_model_init(&model);
    store_t store;
    if (store_mount(&store, flash_model_interface(&model)) != STORE_OK) return 1;
    const uint8_t old[] = "level=3";
    const uint8_t newer[] = "level=4";
    if (store_put(&store, 0u, old, sizeof(old)) != STORE_OK) return 1;
    show(&store, "first save");
    model.program_budget = 20; /* cut power partway through a new record */
    if (store_put(&store, 0u, newer, sizeof(newer)) != STORE_IO_ERROR) return 1;
    model.program_budget = -1;
    if (store_mount(&store, flash_model_interface(&model)) != STORE_OK) return 1;
    show(&store, "after reset");
    if (store_put(&store, 0u, newer, sizeof(newer)) != STORE_OK) return 1;
    show(&store, "after retry");
    for (unsigned i = 0; i < 30u; ++i) {
        uint8_t score[] = {(uint8_t)i};
        if (store_put(&store, 1u, score, sizeof(score)) != STORE_OK) return 1;
    }
    printf("after rotation: sector=%u epoch=%lu erase_counts=%lu,%lu\n",
           store.active_sector, (unsigned long)store.epoch,
           (unsigned long)model.erase_count[0],
           (unsigned long)model.erase_count[1]);
    return 0;
}
