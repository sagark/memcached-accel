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

    hash = reserve_key((void*) key, weight, keylen);
    if (hash == KVSTORE_NOT_FOUND) {
	syslog(LOG_WARNING, "could not reserve hash for key %s\n", key);
        return -1;
    }

    syslog(LOG_INFO, "Key %s has hash %lu\n", key, hash);

    accel_addr = accel_add_entry(accel, hash, vallen);
    if (accel_addr < 0) {
        syslog(LOG_WARNING, "could not add entry for hash %lu\n", hash);
        return -1;
    }

    syslog(LOG_INFO, "Placing value of length %lu at address %lu\n",
            vallen, accel_addr);

    assoc_addr(hash, accel_addr);
    assoc_len(hash, vallen);
    write_val(hash, value);

    return 0;
}
