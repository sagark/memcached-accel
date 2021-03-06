#include "accel-inst.h"
#include "accel.h"
#include <string.h>
#include <syslog.h>

void accel_init(struct accel_state *accel) {
    memset(accel, 0, sizeof(*accel));
}

static int can_resize(struct sram_entry *entry, unsigned long len) {
    int available_size;

    if (entry->len >= len)
        return 1;

    if (entry->next == NULL)
        available_size = KVSTORE_SRAM_SIZE - entry->addr;
    else
        available_size = entry->next->addr - entry->addr;

    return available_size > len;
}

static long accel_add_entry(struct accel_state *accel,
        unsigned long hash, unsigned long len) {
    struct sram_entry *entry;
    struct sram_entry *search;
    unsigned long search_end;
    unsigned long space;

    entry = accel->entries[hash];
    if (entry == NULL) {
        entry = malloc(sizeof(struct sram_entry));
        if (entry == NULL)
            return -1;
        entry->len = len;
        accel->entries[hash] = entry;
    } else if (can_resize(entry, len)) {
        entry->len = len;
        return entry->addr;
    } else {
        if (entry->prev != NULL)
            entry->prev->next = entry->next;
        if (entry->next != NULL)
            entry->next->prev = entry->prev;
    }

    search = accel->head;

    if (search == NULL) {
        accel->head = entry;
        entry->prev = NULL;
        entry->next = NULL;
        entry->addr = 0;
        return 0;
    }

    while (search != NULL) {
        search_end = search->addr + search->len;
        if (search->next == NULL)
            space = KVSTORE_SRAM_SIZE - search_end;
        else
            space = search->next->addr - search_end;
        if (space > len) {
            entry->addr = search_end;
            entry->prev = search;
            entry->next = search->next;
            search->next = entry;
            if (entry->next != NULL)
                entry->next->prev = entry;
            return entry->addr;
        }
        search = search->next;
    }
    return -1;
}

int accel_set(struct accel_state *accel,
        const char *key, unsigned int keylen,
        void *value, unsigned long vallen,
        unsigned int weight) {
    unsigned long hash;
    long accel_addr;

    fence();
    hash = reserve_key((void*) key, weight, keylen);
    if (hash == KVSTORE_NOT_FOUND)
        return -1;

    accel_addr = accel_add_entry(accel, hash, vallen);
    if (accel_addr < 0)
        return -1;

    fence();
    assoc_addr(hash, accel_addr);
    fence();
    assoc_len(hash, vallen);
    fence();
    write_val(hash, value);

    syslog(LOG_INFO, "Hash %lu, Addr %ld, Len: %lu\n", hash, accel_addr, vallen);

    return 0;
}
