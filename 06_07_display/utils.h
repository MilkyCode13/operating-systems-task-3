#ifndef TASK2_UTILS_H
#define TASK2_UTILS_H

#include "params.h"
#include <stddef.h>

#define GARDEN_IN_CLIENT 1
#define GARDEN_OUT_CLIENT 2
#define GARDENER_CLIENT 3

struct flowers {
    int flower[FLOWER_COUNT];
};

struct message {
    size_t flower_num;
};

#endif//TASK2_UTILS_H
