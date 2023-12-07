#pragma once
#include <stdbool.h>
#include <inttypes.h>

#ifdef XMS_UMB
  #define UMB_START_ADDRESS 0xC0000ul
  #define UMB_BLOCKS 2
  #define RESERVED_XMS_KB (UMB_BLOCKS * 32 + 64)
#else
  #define RESERVED_XMS_KB 0
#endif

#define XMS_ERROR_CODE   0x0000
#define XMS_SUCCESS_CODE 0x0001

#ifdef XMS_UMB
void init_umb();
bool umb_in_use(uint32_t addr32);
uint16_t umb_allocate(uint16_t* psz, uint16_t* err);
uint16_t umb_deallocate(uint16_t* seg, uint16_t* err);
#endif
