#include "flash_model.h"

#include <string.h>

static bool bounds(unsigned sector, uint32_t offset, size_t length) {
    return sector < 2u && offset <= STORE_SECTOR_BYTES &&
           length <= STORE_SECTOR_BYTES - offset;
}
static bool model_read(void *context, unsigned sector, uint32_t offset,
                       void *out, size_t length) {
    flash_model_t *model = context;
    if (out == 0 || !bounds(sector, offset, length)) return false;
    memcpy(out, model->bytes[sector] + offset, length);
    return true;
}
static bool model_program(void *context, unsigned sector, uint32_t offset,
                          const void *data, size_t length) {
    flash_model_t *model = context;
    const uint8_t *source = data;
    if (data == 0 || !bounds(sector, offset, length)) return false;
    for (size_t i = 0; i < length; ++i) {
        if (model->program_budget == 0) return false;
        if ((model->bytes[sector][offset + i] & source[i]) != source[i])
            return false;
        model->bytes[sector][offset + i] &= source[i];
        if (model->program_budget > 0) model->program_budget--;
    }
    return true;
}
static bool model_erase(void *context, unsigned sector) {
    flash_model_t *model = context;
    if (sector >= 2u) return false;
    for (unsigned i = 0; i < STORE_SECTOR_BYTES; ++i) {
        if (model->erase_budget == 0) return false;
        model->bytes[sector][i] = 0xFFu;
        if (model->erase_budget > 0) model->erase_budget--;
    }
    model->erase_count[sector]++;
    return true;
}
void flash_model_init(flash_model_t *model) {
    memset(model, 0, sizeof(*model));
    memset(model->bytes, 0xFF, sizeof(model->bytes));
    model->program_budget = -1;
    model->erase_budget = -1;
}
store_flash_t flash_model_interface(flash_model_t *model) {
    store_flash_t interface = {model_read, model_program, model_erase, model};
    return interface;
}
