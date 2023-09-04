#include "../includes/pyro.h"


static bool try_load_builtin_module(PyroVM* vm, uint8_t arg_count, PyroValue* args, PyroMod* module) {
    if (arg_count != 2 || strcmp(PYRO_AS_STR(args[0])->bytes, "std") != 0) {
        return false;
    }

    PyroStr* name = PYRO_AS_STR(args[1]);

    if (strcmp(name->bytes, "constants") == 0) {
        pyro_load_std_mod_constants(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    if (strcmp(name->bytes, "math") == 0) {
        pyro_load_std_mod_math(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    if (strcmp(name->bytes, "prng") == 0) {
        pyro_load_std_mod_prng(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    if (strcmp(name->bytes, "pyro") == 0) {
        pyro_load_std_mod_pyro(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    if (strcmp(name->bytes, "sqlite") == 0) {
        pyro_load_std_mod_sqlite(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    if (strcmp(name->bytes, "fs") == 0) {
        pyro_load_std_mod_fs(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    if (strcmp(name->bytes, "log") == 0) {
        pyro_load_std_mod_log(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    // Deprecated. Replaced by [std::fs].
    if (strcmp(name->bytes, "path") == 0) {
        pyro_load_std_mod_fs(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return true;
    }

    return false;
}


// Given 'import foo::bar::baz', we want to check for:
// 1. foo/bar/baz.pyro
// 2. foo/bar/baz/self.pyro
static bool try_load_embedded_module(PyroVM* vm, uint8_t arg_count, PyroValue* args, PyroMod* module) {
    size_t path_capacity = 0;
    for (uint8_t i = 0; i < arg_count; i++) {
        path_capacity += PYRO_AS_STR(args[i])->count + 1;
    }
    path_capacity += strlen("self.pyro") + 1;

    char* path = PYRO_ALLOCATE_ARRAY(vm, char, path_capacity);
    if (!path) {
        pyro_panic(vm, "out of memory");
        return true;
    }

    // Assemble the string: foo/bar/baz/
    size_t path_count = 0;
    for (uint8_t i = 0; i < arg_count; i++) {
        PyroStr* name = PYRO_AS_STR(args[i]);
        memcpy(path + path_count, name->bytes, name->count);
        path_count += name->count;
        path[path_count++] = '/';
    }

    const unsigned char* data;
    size_t count;

    // 1. Try: foo/bar/baz.pyro
    memcpy(path + path_count - 1, ".pyro", strlen(".pyro") + 1);
    if (pyro_get_embedded(path, &data, &count)) {
        pyro_exec_code(vm, (const char*)data, count, path, module);
        PYRO_FREE_ARRAY(vm, char, path, path_capacity);
        return true;
    }

    // 2. Try: foo/bar/baz/self.pyro
    memcpy(path + path_count - 1, "/self.pyro", strlen("/self.pyro") + 1);
    if (pyro_get_embedded(path, &data, &count)) {
        pyro_exec_code(vm, (const char*)data, count, path, module);
        PYRO_FREE_ARRAY(vm, char, path, path_capacity);
        return true;
    }

    PYRO_FREE_ARRAY(vm, char, path, path_capacity);
    return false;
}


// Given 'import foo::bar::baz', we want to check for:
// 1. BASE/foo/bar/baz.so
// 2. BASE/foo/bar/baz.pyro
// 3. BASE/foo/bar/baz/self.so
// 4. BASE/foo/bar/baz/self.pyro
// 5. BASE/foo/bar/baz
static bool try_load_filesystem_module(PyroVM* vm, uint8_t arg_count, PyroValue* args, PyroMod* module) {
    for (size_t i = 0; i < vm->import_roots->count; i++) {
        PyroStr* base = PYRO_AS_STR(vm->import_roots->values[i]);
        if (base->count == 0) {
            base = PyroStr_COPY(".");
            if (!base) {
                pyro_panic(vm, "out of memory");
                return true;
            }
        }

        // Support `$roots` entries with or without a trailing slash.
        bool base_has_trailing_slash = false;
        if (base->bytes[base->count - 1] == '/') {
            base_has_trailing_slash = true;
        }

        // Allocate enough space for "BASE/foo/bar/baz/self.pyro".
        size_t path_capacity = base_has_trailing_slash ? base->count : base->count + 1;
        for (uint8_t j = 0; j < arg_count; j++) {
            path_capacity += PYRO_AS_STR(args[j])->count + 1;
        }
        path_capacity += strlen("self.pyro") + 1;

        char* path = PYRO_ALLOCATE_ARRAY(vm, char, path_capacity);
        if (!path) {
            pyro_panic(vm, "out of memory");
            return true;
        }

        // Start with path = BASE/
        memcpy(path, base->bytes, base->count);
        size_t path_count = base->count;
        if (!base_has_trailing_slash) {
            path[path_count++] = '/';
        }

        // Assemble path = BASE/foo/bar/baz/
        for (uint8_t j = 0; j < arg_count; j++) {
            PyroStr* name = PYRO_AS_STR(args[j]);
            memcpy(path + path_count, name->bytes, name->count);
            path_count += name->count;
            path[path_count++] = '/';
        }

        // 1. Try file: BASE/foo/bar/baz.so
        memcpy(path + path_count - 1, ".so", strlen(".so") + 1);
        if (pyro_is_file(path)) {
            PyroStr* module_name = PYRO_AS_STR(args[arg_count - 1]);
            pyro_dlopen_as_module(vm, path, module_name->bytes, module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return true;
        }

        // 2. Try file: BASE/foo/bar/baz.pyro
        memcpy(path + path_count - 1, ".pyro", strlen(".pyro") + 1);
        if (pyro_is_file(path)) {
            pyro_exec_file(vm, path, module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return true;
        }

        // 3. Try file: BASE/foo/bar/baz/self.so
        memcpy(path + path_count - 1, "/self.so", strlen("/self.so") + 1);
        if (pyro_is_file(path)) {
            PyroStr* module_name = PYRO_AS_STR(args[arg_count - 1]);
            pyro_dlopen_as_module(vm, path, module_name->bytes, module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return true;
        }

        // 4. Try file: BASE/foo/bar/baz/self.pyro
        memcpy(path + path_count - 1, "/self.pyro", strlen("/self.pyro") + 1);
        if (pyro_is_file(path)) {
            pyro_exec_file(vm, path, module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return true;
        }

        // 5. Try dir: BASE/foo/bar/baz
        path[path_count - 1] = '\0';
        if (pyro_is_dir(path)) {
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return true;
        }

        PYRO_FREE_ARRAY(vm, char, path, path_capacity);
    }

    return false;
}


void pyro_import_module(PyroVM* vm, uint8_t arg_count, PyroValue* args, PyroMod* module) {
    if (try_load_builtin_module(vm, arg_count, args, module)) {
        return;
    }

    if (try_load_embedded_module(vm, arg_count, args, module)) {
        return;
    }

    if (try_load_filesystem_module(vm, arg_count, args, module)) {
        return;
    }

    if (arg_count > 3) {
        pyro_panic(vm, "unable to locate module '...::%s::%s::%s'",
            PYRO_AS_STR(args[arg_count - 3])->bytes,
            PYRO_AS_STR(args[arg_count - 2])->bytes,
            PYRO_AS_STR(args[arg_count - 1])->bytes
        );
        return;
    }

    if (arg_count == 3) {
        pyro_panic(vm, "unable to locate module '%s::%s::%s'",
            PYRO_AS_STR(args[0])->bytes,
            PYRO_AS_STR(args[1])->bytes,
            PYRO_AS_STR(args[2])->bytes
        );
        return;
    }

    if (arg_count == 2) {
        pyro_panic(vm, "unable to locate module '%s::%s'",
            PYRO_AS_STR(args[0])->bytes,
            PYRO_AS_STR(args[1])->bytes
        );
        return;
    }

    pyro_panic(vm, "unable to locate module '%s'",
        PYRO_AS_STR(args[0])->bytes
    );
}


void pyro_import_module_from_string(PyroVM* vm, const char* path, PyroMod* module) {
    char* path_copy = pyro_strdup(path);
    if (!path_copy) {
        pyro_panic(vm, "out of memory");
        return;
    }

    size_t count = 0;
    char* saveptr;
    char* token = strtok_r(path_copy, ":", &saveptr);

    while (token != NULL) {
        if (strlen(token) == 0) {
            free(path_copy);
            pyro_panic(vm, "invalid import path '%s'", path);
            return;
        }

        PyroStr* token_string = PyroStr_COPY(token);
        if (!token_string) {
            free(path_copy);
            pyro_panic(vm, "out of memory");
            return;
        }

        if (!pyro_push(vm, pyro_obj(token_string))) {
            free(path_copy);
            return;
        }

        count++;
        token = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);

    if (count == 0) {
        pyro_panic(vm, "invalid import path '%s'", path);
        return;
    }

    pyro_import_module(vm, count, vm->stack_top - count, module);
    vm->stack_top -= count;
}