/**
 * @file factory.c
 * @brief Node type registration and create by name.
 */
#include "../../include/media_core/factory.h"
#include "../../include/media_core/node.h"
#include <string.h>
#include <stdlib.h>

#define MAX_TYPES 24

typedef struct reg {
    char name[64];
    node_create_fn create_fn;
    node_destroy_fn destroy_fn;
} reg_t;

static reg_t g_regs[MAX_TYPES];
static int g_num_regs;

int factory_register_node_type(const char *name, node_create_fn create_fn,
                               node_destroy_fn destroy_fn) {
    if (!name || !create_fn || !destroy_fn || g_num_regs >= MAX_TYPES)
        return -1;
    size_t n = strlen(name);
    if (n >= sizeof(g_regs[0].name)) return -1;
    for (int i = 0; i < g_num_regs; i++)
        if (strcmp(g_regs[i].name, name) == 0) {
            g_regs[i].create_fn = create_fn;
            g_regs[i].destroy_fn = destroy_fn;
            return 0;
        }
    memcpy(g_regs[g_num_regs].name, name, n + 1);
    g_regs[g_num_regs].create_fn = create_fn;
    g_regs[g_num_regs].destroy_fn = destroy_fn;
    g_num_regs++;
    return 0;
}

media_node_t* factory_create_node(const char *type_name, const char *instance_id,
                                  const node_config_t *config) {
    if (!type_name || !instance_id) return NULL;
    for (int i = 0; i < g_num_regs; i++) {
        if (strcmp(g_regs[i].name, type_name) != 0) continue;
        return g_regs[i].create_fn(instance_id, config);
    }
    return NULL;
}

void factory_unregister_node_type(const char *name) {
    for (int i = 0; i < g_num_regs; i++) {
        if (strcmp(g_regs[i].name, name) != 0) continue;
        memmove(&g_regs[i], &g_regs[i + 1], (g_num_regs - 1 - i) * sizeof(reg_t));
        g_num_regs--;
        return;
    }
}

void factory_destroy_node(media_node_t *node) {
    if (!node) return;
    for (int i = 0; i < g_num_regs; i++) {
        if (node->desc && strcmp(g_regs[i].name, node->desc->name) == 0) {
            g_regs[i].destroy_fn(node);
            return;
        }
    }
}
