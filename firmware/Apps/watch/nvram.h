/* nvram.h
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
enum muchi_nvm_ids {
    MUCHI_NVM_CONFIG_IS_VALID = 0x10,
    MUCHI_NVM_AVATAR_CODE,
    MUCHI_NVM_STORE_COUNT = 0x3D,
    MUCHI_NVM_STORE_BASE = 0x3E,    // 50 entries, 2 nodes per entry
    MUCHI_NVM_STORE_LAST = 0x6F,
    __MUCHI_NVM_INVALID_AND_BEYOND = 0x70,
};
