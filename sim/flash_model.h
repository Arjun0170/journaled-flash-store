#ifndef FLASH_MODEL_H
#define FLASH_MODEL_H

#include "store.h"

typedef struct {
    uint8_t bytes[2][STORE_SECTOR_BYTES];
    uint32_t erase_count[2];
    int32_t program_budget; /* -1 unlimited; 0 fails before next byte */
    int32_t erase_budget;   /* -1 unlimited; 0 fails before next byte */
} flash_model_t;

void flash_model_init(flash_model_t *model);
store_flash_t flash_model_interface(flash_model_t *model);

#endif
