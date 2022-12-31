#include "../inc/imports.h"
#include "../inc/vm.h"
#include "../inc/exec.h"
#include "../inc/heap.h"
#include "../inc/panics.h"
#include "../inc/os.h"
#include "../inc/std_lib.h"


static void try_load_stdlib_module(PyroVM* vm, PyroObjStr* name, PyroObjModule* module) {
    if (strcmp(name->bytes, "math") == 0) {
        pyro_load_std_mod_math(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (strcmp(name->bytes, "mt64") == 0) {
        pyro_load_std_mod_mt64(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (strcmp(name->bytes, "prng") == 0) {
        pyro_load_std_mod_prng(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (strcmp(name->bytes, "pyro") == 0) {
        pyro_load_std_mod_pyro(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (strcmp(name->bytes, "sqlite") == 0) {
        pyro_load_std_mod_sqlite(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (strcmp(name->bytes, "path") == 0) {
        pyro_load_std_mod_path(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (strcmp(name->bytes, "log") == 0) {
        pyro_load_std_mod_log(vm, module);
        if (vm->memory_allocation_failed) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (strcmp(name->bytes, "args") == 0) {
        pyro_exec_code_as_module(vm, (char*)std_mod_args_pyro, std_mod_args_pyro_len, "$std::args", module);
        return;
    }

    if (strcmp(name->bytes, "email") == 0) {
        pyro_exec_code_as_module(vm, (char*)std_mod_email_pyro, std_mod_email_pyro_len, "$std::email", module);
        return;
    }

    if (strcmp(name->bytes, "html") == 0) {
        pyro_exec_code_as_module(vm, (char*)std_mod_html_pyro, std_mod_html_pyro_len, "$std::html", module);
        return;
    }

    if (strcmp(name->bytes, "cgi") == 0) {
        pyro_exec_code_as_module(vm, (char*)std_mod_cgi_pyro, std_mod_cgi_pyro_len, "$std::cgi", module);
        return;
    }

    if (strcmp(name->bytes, "json") == 0) {
        pyro_exec_code_as_module(vm, (char*)std_mod_json_pyro, std_mod_json_pyro_len, "$std::json", module);
        return;
    }

    if (strcmp(name->bytes, "pretty") == 0) {
        pyro_exec_code_as_module(vm, (char*)std_mod_pretty_pyro, std_mod_pretty_pyro_len, "$std::pretty", module);
        return;
    }

    pyro_panic(vm, "no module in standard library named '%s'", name->bytes);
}


void try_load_compiled_module(PyroVM* vm, const char* path, PyroValue name, PyroObjModule* module) {
    pyro_load_dyn_lib_as_mod(vm, path, PYRO_AS_STR(name)->bytes, module);
}


void pyro_import_module(PyroVM* vm, uint8_t arg_count, PyroValue* args, PyroObjModule* module) {
    if (arg_count == 2 && strcmp(PYRO_AS_STR(args[0])->bytes, "$std") == 0) {
        try_load_stdlib_module(vm, PYRO_AS_STR(args[1]), module);
        return;
    }

    for (size_t i = 0; i < vm->import_roots->count; i++) {
        PyroObjStr* base = PYRO_AS_STR(vm->import_roots->values[i]);
        if (base->length == 0) {
            pyro_panic(vm, "invalid import root (empty string)");
            return;
        }

        // Support `$roots` entries with or without a trailing slash.
        bool base_has_trailing_slash = false;
        if (base->bytes[base->length - 1] == '/') {
            base_has_trailing_slash = true;
        }

        // Given 'import foo::bar::baz', allocate enough space for:
        // - BASE/foo/bar/baz
        // - BASE/foo/bar/baz.so
        // - BASE/foo/bar/baz.pyro
        // - BASE/foo/bar/baz/self.so
        // - BASE/foo/bar/baz/self.pyro
        size_t path_capacity = base_has_trailing_slash ? base->length : base->length + 1;
        for (uint8_t j = 0; j < arg_count; j++) {
            path_capacity += PYRO_AS_STR(args[j])->length + 1;
        }
        path_capacity += strlen("self.pyro");

        // Add an extra space for the terminating NULL.
        path_capacity += 1;

        // Allocate the path buffer.
        char* path = PYRO_ALLOCATE_ARRAY(vm, char, path_capacity);
        if (!path) {
            pyro_panic(vm, "out of memory");
            return;
        }

        // Start with path = BASE/
        memcpy(path, base->bytes, base->length);
        size_t path_count = base->length;
        if (!base_has_trailing_slash) {
            path[path_count++] = '/';
        }

        // Given 'import foo::bar::baz', assemble path = BASE/foo/bar/baz/
        for (uint8_t j = 0; j < arg_count; j++) {
            PyroObjStr* name = PYRO_AS_STR(args[j]);
            memcpy(path + path_count, name->bytes, name->length);
            path_count += name->length;
            path[path_count++] = '/';
        }

        // 1. Try file: BASE/foo/bar/baz.so
        path_count -= 1;
        memcpy(path + path_count, ".so", strlen(".so"));
        path_count += strlen(".so");
        path[path_count] = '\0';

        if (pyro_is_file(path)) {
            try_load_compiled_module(vm, path, args[arg_count - 1], module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return;
        }

        // 2. Try file: BASE/foo/bar/baz.pyro
        path_count -= strlen(".so");
        memcpy(path + path_count, ".pyro", strlen(".pyro"));
        path_count += strlen(".pyro");
        path[path_count] = '\0';

        if (pyro_is_file(path)) {
            pyro_exec_file_as_module(vm, path, module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return;
        }

        // 3. Try file: BASE/foo/bar/baz/self.so
        path_count -= strlen(".pyro");
        memcpy(path + path_count, "/self.so", strlen("/self.so"));
        path_count += strlen("/self.so");
        path[path_count] = '\0';

        if (pyro_is_file(path)) {
            try_load_compiled_module(vm, path, args[arg_count - 1], module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return;
        }

        // 4. Try file: BASE/foo/bar/baz/self.pyro
        path_count -= strlen("/self.so");
        memcpy(path + path_count, "/self.pyro", strlen("/self.pyro"));
        path_count += strlen("/self.pyro");
        path[path_count] = '\0';

        if (pyro_is_file(path)) {
            pyro_exec_file_as_module(vm, path, module);
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return;
        }

        // 5. Try dir: BASE/foo/bar/baz
        path_count -= strlen("/self.pyro");
        path[path_count] = '\0';

        if (pyro_is_dir(path)) {
            PYRO_FREE_ARRAY(vm, char, path, path_capacity);
            return;
        }

        PYRO_FREE_ARRAY(vm, char, path, path_capacity);
    }

    pyro_panic(
        vm,
        "unable to locate module '%s'",
        PYRO_AS_STR(args[arg_count - 1])->bytes
    );
}
