#include "imports.h"
#include "vm.h"
#include "exec.h"
#include "heap.h"
#include "panics.h"
#include "os.h"
#include "../std/std_lib.h"


static void try_load_stdlib_module(PyroVM* vm, ObjStr* name, ObjModule* module) {
    bool found_module = false;

    if (strcmp(name->bytes, "math") == 0) {
        pyro_load_std_mod_math(vm, module);
        found_module = true;
    } else if (strcmp(name->bytes, "mt64") == 0) {
        pyro_load_std_mod_mt64(vm, module);
        found_module = true;
    } else if (strcmp(name->bytes, "prng") == 0) {
        pyro_load_std_mod_prng(vm, module);
        found_module = true;
    } else if (strcmp(name->bytes, "pyro") == 0) {
        pyro_load_std_mod_pyro(vm, module);
        found_module = true;
    } else if (strcmp(name->bytes, "errors") == 0) {
        pyro_load_std_mod_errors(vm, module);
        found_module = true;
    } else if (strcmp(name->bytes, "sqlite") == 0) {
        pyro_load_std_mod_sqlite(vm, module);
        found_module = true;
    } else if (strcmp(name->bytes, "path") == 0) {
        pyro_load_std_mod_path(vm, module);
        found_module = true;
    } else if (strcmp(name->bytes, "args") == 0) {
        pyro_exec_code_as_module(vm, (char*)lib_args_pyro, lib_args_pyro_len, "lib_args.pyro", module);
        if (vm->halt_flag) {
            return;
        }
        found_module = true;
    }

    if (!found_module) {
        pyro_panic(vm, ERR_MODULE_NOT_FOUND, "Invalid standard library module '%s'.", name->bytes);
    } else if (vm->memory_allocation_failed) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }
}


void pyro_import_module(PyroVM* vm, uint8_t arg_count, Value* args, ObjModule* module) {
    if (arg_count == 2 && strcmp(AS_STR(args[0])->bytes, "$std") == 0) {
        try_load_stdlib_module(vm, AS_STR(args[1]), module);
        return;
    }

    for (size_t i = 0; i < vm->import_roots->count; i++) {
        ObjStr* base = AS_STR(vm->import_roots->values[i]);

        if (base->length == 0) {
            pyro_panic(vm, ERR_VALUE_ERROR, "Invalid import root (empty string).");
            return;
        }

        bool base_has_trailing_slash = false;
        if (base->bytes[base->length - 1] == '/') {
            base_has_trailing_slash = true;
        }

        size_t path_length = base_has_trailing_slash ? base->length : base->length + 1;
        for (uint8_t j = 0; j < arg_count; j++) {
            path_length += AS_STR(args[j])->length + 1;
        }
        path_length += 4 + 5; // add space for a [.pyro] or [/self.pyro] suffix

        char* path = ALLOCATE_ARRAY(vm, char, path_length + 1);
        if (!path) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return;
        }

        // We start with path = BASE/
        memcpy(path, base->bytes, base->length);
        size_t path_count = base->length;
        if (!base_has_trailing_slash) {
            path[path_count++] = '/';
        }

        // Given 'import foo::bar::baz', assemble path = BASE/foo/bar/baz/
        for (uint8_t j = 0; j < arg_count; j++) {
            ObjStr* name = AS_STR(args[j]);
            memcpy(path + path_count, name->bytes, name->length);
            path_count += name->length;
            path[path_count++] = '/';
        }

        // 1. Try file: BASE/foo/bar/baz.pyro
        memcpy(path + path_count - 1, ".pyro", 5);
        path_count += 4;
        path[path_count] = '\0';

        if (pyro_is_file(path)) {
            pyro_exec_file_as_module(vm, path, module);
            FREE_ARRAY(vm, char, path, path_length + 1);
            return;
        }

        // 2. Try file: BASE/foo/bar/baz/self.pyro
        memcpy(path + path_count - 5, "/self.pyro", 10);
        path_count += 5;
        path[path_count] = '\0';

        if (pyro_is_file(path)) {
            pyro_exec_file_as_module(vm, path, module);
            FREE_ARRAY(vm, char, path, path_length + 1);
            return;
        }

        // 3. Try dir: BASE/foo/bar/baz
        path_count -= 10;
        path[path_count] = '\0';

        if (pyro_is_dir(path)) {
            FREE_ARRAY(vm, char, path, path_length + 1);
            return;
        }

        FREE_ARRAY(vm, char, path, path_length + 1);
    }

    pyro_panic(
        vm, ERR_MODULE_NOT_FOUND,
        "Unable to locate module '%s'.", AS_STR(args[arg_count - 1])->bytes
    );
}
