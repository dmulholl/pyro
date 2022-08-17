#include "../../inc/std_lib.h"
#include "../../inc/vm.h"
#include "../../inc/heap.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/gc.h"


static Value fn_memory(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_I64((int64_t)vm->bytes_allocated);
}


static Value fn_gc(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_collect_garbage(vm);
    return MAKE_NULL();
}


static Value fn_stdout(PyroVM* vm, size_t arg_count, Value* args) {
    if (vm->stdout_stream) {
        return MAKE_OBJ(vm->stdout_stream);
    }
    return MAKE_NULL();
}


static Value fn_stderr(PyroVM* vm, size_t arg_count, Value* args) {
    if (vm->stderr_stream) {
        return MAKE_OBJ(vm->stderr_stream);
    }
    return MAKE_NULL();
}


static Value fn_stdin(PyroVM* vm, size_t arg_count, Value* args) {
    if (vm->stdin_stream) {
        return MAKE_OBJ(vm->stdin_stream);
    }
    return MAKE_NULL();
}


static Value fn_set_stdout(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_FILE(args[0]) && !IS_BUF(args[0])) {
        pyro_panic(vm, "set_stdout(): invalid argument [arg], expected a file");
        return MAKE_NULL();
    }
    vm->stdout_stream = AS_OBJ(args[0]);
    return MAKE_NULL();
}


static Value fn_set_stderr(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_FILE(args[0]) && !IS_BUF(args[0])) {
        pyro_panic(vm, "set_stderr(): invalid argument [arg], expected a file");
        return MAKE_NULL();
    }
    vm->stderr_stream = AS_OBJ(args[0]);
    return MAKE_NULL();
}


static Value fn_set_stdin(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_FILE(args[0])) {
        pyro_panic(vm, "set_stdin(): invalid argument [arg], expected a file");
        return MAKE_NULL();
    }
    vm->stdin_stream = AS_OBJ(args[0]);
    return MAKE_NULL();
}


static Value fn_sizeof(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_OBJ(args[0])) {
        switch (AS_OBJ(args[0])->type) {
            case OBJ_VEC:
            case OBJ_VEC_AS_STACK: {
                ObjVec* vec = AS_VEC(args[0]);
                return MAKE_I64(sizeof(ObjVec) + sizeof(Value) * vec->capacity);
            }
            case OBJ_TUP: {
                ObjTup* tup = AS_TUP(args[0]);
                return MAKE_I64(sizeof(ObjTup) + sizeof(Value) * tup->count);
            }
            case OBJ_MAP:
            case OBJ_MAP_AS_SET: {
                ObjMap* map = AS_MAP(args[0]);
                return MAKE_I64(
                    sizeof(ObjMap) +
                    sizeof(MapEntry) * map->entry_array_capacity +
                    sizeof(int64_t) * map->index_array_capacity
                );
            }
            case OBJ_QUEUE: {
                ObjQueue* queue = AS_QUEUE(args[0]);
                return MAKE_I64(sizeof(ObjQueue) + sizeof(QueueItem) * queue->count);
            }
            case OBJ_BUF: {
                ObjBuf* buf = AS_BUF(args[0]);
                return MAKE_I64(sizeof(ObjBuf) + sizeof(uint8_t) * buf->capacity);
            }
            default: {
                return MAKE_I64(-1);
            }
        }
    } else {
        return MAKE_I64(sizeof(Value));
    }
}


void pyro_load_std_mod_pyro(PyroVM* vm, ObjModule* module) {
    ObjTup* version_tuple = ObjTup_new(5, vm);
    if (!version_tuple) {
        return;
    }
    version_tuple->values[0] = MAKE_I64(PYRO_VERSION_MAJOR);
    version_tuple->values[1] = MAKE_I64(PYRO_VERSION_MINOR);
    version_tuple->values[2] = MAKE_I64(PYRO_VERSION_PATCH);
    version_tuple->values[3] = MAKE_OBJ(STR(PYRO_VERSION_LABEL));
    version_tuple->values[4] = MAKE_OBJ(STR(PYRO_VERSION_BUILD));
    pyro_define_pub_member(vm, module, "version_tuple", MAKE_OBJ(version_tuple));

    char* version_c_string = pyro_get_version_string();
    if (version_c_string) {
        ObjStr* version_pyro_string = STR(version_c_string);
        if (!version_pyro_string) {
            return;
        }
        pyro_define_pub_member(vm, module, "version_string", MAKE_OBJ(version_pyro_string));
    }

    pyro_define_pub_member_fn(vm, module, "memory", fn_memory, 0);
    pyro_define_pub_member_fn(vm, module, "gc", fn_memory, 0);
    pyro_define_pub_member_fn(vm, module, "stdin", fn_stdin, 0);
    pyro_define_pub_member_fn(vm, module, "stdout", fn_stdout, 0);
    pyro_define_pub_member_fn(vm, module, "stderr", fn_stderr, 0);
    pyro_define_pub_member_fn(vm, module, "set_stdin", fn_set_stdin, 1);
    pyro_define_pub_member_fn(vm, module, "set_stdout", fn_set_stdout, 1);
    pyro_define_pub_member_fn(vm, module, "set_stderr", fn_set_stderr, 1);
    pyro_define_pub_member_fn(vm, module, "sizeof", fn_sizeof, 1);
}
