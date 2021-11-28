#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/errors.h"


static Value fn_range(PyroVM* vm, size_t arg_count, Value* args) {
    int64_t start, stop, step;

    if (arg_count == 1) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $range().");
        }
        start = 0;
        stop = args[0].as.i64;
        step = 1;
    } else if (arg_count == 2) {
        if (!IS_I64(args[0]) || !IS_I64(args[1])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $range().");
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = 1;
    } else if (arg_count == 3) {
        if (!IS_I64(args[0]) || !IS_I64(args[1]) || !IS_I64(args[2])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $range().");
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = args[2].as.i64;
    } else {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1, 2, or 3 arguments for $range().");
        return NULL_VAL();
    }

    ObjRange* range = ObjRange_new(start, stop, step, vm);
    if (!range) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(range);
}


static Value fn_is_range(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_RANGE(args[0]));
}


static Value range_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value range_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjRange* range = AS_RANGE(args[-1]);

    if (range->step > 0) {
        if (range->next < range->stop) {
            int64_t next = range->next;
            range->next += range->step;
            return I64_VAL(next);
        }
        return OBJ_VAL(vm->empty_error);
    }

    if (range->step < 0) {
        if (range->next > range->stop) {
            int64_t next = range->next;
            range->next += range->step;
            return I64_VAL(next);
        }
        return OBJ_VAL(vm->empty_error);
    }

    return OBJ_VAL(vm->empty_error);
}


void pyro_load_std_core_range(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$range", fn_range, -1);
    pyro_define_global_fn(vm, "$is_range", fn_is_range, 1);

    // Methods.
    pyro_define_method(vm, vm->range_class, "$iter", range_iter, 0);
    pyro_define_method(vm, vm->range_class, "$next", range_next, 0);
}
