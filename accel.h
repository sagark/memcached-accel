#ifndef __ACCEL_H__
#define __ACCEL_H__

#define KVSTORE_NOT_FOUND 0xffffff
#define KVSTORE_MAX_KEYS 1024
#define KVSTORE_SRAM_SIZE (512 * 1024)

struct sram_entry {
    struct sram_entry *next;
    struct sram_entry *prev;
    unsigned long addr;
    unsigned long len;
};

struct accel_state {
    struct sram_entry *entries[KVSTORE_MAX_KEYS];
    struct sram_entry *head;
};

void accel_init(struct accel_state *accel);
int accel_set(struct accel_state *accel,
    const char *key, unsigned int keylen, void *value, unsigned long vallen,
    unsigned int weight);

#endif
