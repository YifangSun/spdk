#include "stdint.h"



#define VECTOR_GENERATE_STRUCT(name, type)      \							
struct                                          \
name##_VECTOR {                                 \
    size_t size;                                \
    size_t capacity;                            \  
    type * elements[];                          \
}

#define VECTOR_GENERATE_SIZEOF(name, type)      \
const size_t name##_SIZEOF = sizeof( type );    \

#define VECTOR_GENERATE_PUSH(name, type, attr)
attr void
name##_VECTOR_PUSH() {

}

struct kv_op {
    size_t key_size;
    char*  key;
    size_t value_size; // value size equals 0 means deleting
    char*  value;
}

struct
op_VECTOR {
    uint32_t size;
    uint32_t capacity;
    // kv_op * elements[];    
    kv_op ** elements;     
}

void
op_VEC_EXPAND(struct op_VECTOR* vec) {
    if (vec->size < vec->capacity)
        return;

    kv_op ** buf = realloc()
    vec->elements = 
}

void 
op_VEC_PUSH_BACK (struct op_VECTOR* vector, size_t key_size, char* key, size_t value_size, char* value) {




}
