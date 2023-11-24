#include "a20.h"

static bool is_a20_enabled = true;

bool get_a20_enabled() {
    return is_a20_enabled;
}

uint16_t portin(uint16_t portnum);
void portout(uint16_t portnum, uint16_t value);

uint8_t set_a20(uint8_t cond) {
    uint8_t val = portin(PORT_A20);
    is_a20_enabled = (val & A20_ENABLE_BIT) != 0;
    if (is_a20_enabled != !!cond)
        portout(PORT_A20, val ^ A20_ENABLE_BIT);
    return is_a20_enabled;
}


// Maximum number of map entries in the e820 map
#define BUILD_MAX_E820 32
int e820_count = 0;
struct e820entry e820_list[BUILD_MAX_E820];

// Remove an entry from the e820_list.
static void remove_e820(int i) {
    e820_count--;
    memmove(&e820_list[i], &e820_list[i+1], sizeof(e820_list[0]) * (e820_count - i));
}

// Insert an entry in the e820_list at the given position.
static void insert_e820(int i, uint64_t start, uint64_t size, uint32_t type) {
    if (e820_count >= BUILD_MAX_E820) {
        warn_noalloc();
        return;
    }
    memmove(&e820_list[i+1], &e820_list[i], sizeof(e820_list[0]) * (e820_count - i));
    e820_count++;
    struct e820entry *e = &e820_list[i];
    e->start = start;
    e->size = size;
    e->type = type;
}

static const char* e820_type_name(uint32_t type) {
    switch (type) {
    case E820_RAM:      return "RAM";
    case E820_RESERVED: return "RESERVED";
    case E820_ACPI:     return "ACPI";
    case E820_NVS:      return "NVS";
    case E820_UNUSABLE: return "UNUSABLE";
    default:            return "UNKNOWN";
    }
}

// Show the current e820_list.
static void dump_map(void) {
    dprintf(1, "e820 map has %d items:\n", e820_count);
    for (int i = 0; i < e820_count; i++) {
        struct e820entry *e = &e820_list[i];
        uint64_t e_end = e->start + e->size;
        dprintf(1, "  %d: %016llx - %016llx = %d %s\n", i, e->start, e_end, e->type, e820_type_name(e->type));
    }
}

#define E820_HOLE ((uint32_t)-1) // Used internally to remove entries

// Add a new entry to the list.  This scans for overlaps and keeps the
// list sorted.
void e820_add(uint64_t start, uint64_t size, uint32_t type) {
    dprintf(8, "Add to e820 map: %08llx %08llx %d\n", start, size, type);
    if (! size)
        // Huh?  Nothing to do.
        return;
    // Find position of new item (splitting existing item if needed).
    uint64_t end = start + size;
    int i;
    for (i = 0; i < e820_count; i++) {
        struct e820entry *e = &e820_list[i];
        uint64_t e_end = e->start + e->size;
        if (start > e_end)
            continue;
        // Found position - check if an existing item needs to be split.
        if (start > e->start) {
            if (type == e->type) {
                // Same type - merge them.
                size += start - e->start;
                start = e->start;
            } else {
                // Split existing item.
                e->size = start - e->start;
                i++;
                if (e_end > end)
                    insert_e820(i, end, e_end - end, e->type);
            }
        }
        break;
    }
    // Remove/adjust existing items that are overlapping.
    while (i < e820_count) {
        struct e820entry *e = &e820_list[i];
        if (end < e->start)
            // No overlap - done.
            break;
        uint64_t e_end = e->start + e->size;
        if (end >= e_end) {
            // Existing item completely overlapped - remove it.
            remove_e820(i);
            continue;
        }
        // Not completely overlapped - adjust its start.
        e->start = end;
        e->size = e_end - end;
        if (type == e->type) {
            // Same type - merge them.
            size += e->size;
            remove_e820(i);
        }
        break;
    }
    // Insert new item.
    if (type != E820_HOLE)
        insert_e820(i, start, size, type);
    //dump_map();
}

// Remove any definitions in a memory range (make a memory hole).
void e820_remove(uint64_t start, uint64_t size) {
    e820_add(start, size, E820_HOLE);
}

// Report on final memory locations.
void e820_prepboot(void) {
    dump_map();
}
