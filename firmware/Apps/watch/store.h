/* store.h
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
void grpr_store_init();

/* Returns index if action completed or -1 on error */
int grpr_store_add(UINT8 *addr);
int grpr_store_get(UINT8 *addr);
int grpr_store_get_first();
int grpr_store_get_next(int idx);

/* first arg is the index returned by one of the previous functions */
int grpr_store_timestamp(int idx);
int grpr_store_is_expired(int idx);
INT8 grpr_store_get_rssi(int idx);
int grpr_store_set_rssi(int idx, INT8 rssi);
INT8 grpr_store_get_avatar(int idx);
int grpr_store_set_avatar(int idx, INT8 avatar);

int grpr_store_get_total_expired();
int grpr_store_get_total_count();

/* print store for debugging */
int grpr_store_dump();
