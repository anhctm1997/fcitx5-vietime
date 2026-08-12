#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct VietimeEngineHandle VietimeEngineHandle;
typedef struct { uint32_t action_type, delete_before; uint8_t *text_ptr; size_t text_len, text_capacity; } VietimeAction;
enum {
    VIETIME_PASS=0,
    VIETIME_CONSUME=1,
    VIETIME_COMMIT=2,
    VIETIME_REPLACE=3,
    VIETIME_REPLACE_AND_PASS=4,
    VIETIME_COMMIT_AND_PASS=5
};
VietimeEngineHandle *vietime_engine_new(void);
VietimeEngineHandle *vietime_engine_new_deferred(void);
void vietime_engine_free(VietimeEngineHandle *);
void vietime_engine_reset(VietimeEngineHandle *);
VietimeAction vietime_process_key(VietimeEngineHandle *, uint32_t unicode, uint32_t special_key, uint32_t modifiers);
void vietime_action_free(VietimeAction);
#ifdef __cplusplus
}
#endif
