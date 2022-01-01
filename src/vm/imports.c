#include "imports.h"
#include "vm.h"
#include "exec.h"
#include "heap.h"
#include "panics.h"

#include "../lib/os/os.h"


void pyro_import_module(PyroVM* vm, uint8_t arg_count, Value* args, ObjModule* module) {
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
