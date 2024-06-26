#include "../includes/pyro.h"


static PyroValue fn_exists(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "exists(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_exists(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_is_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "is_file(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_is_file(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_is_dir(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "is_dir(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_is_dir(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_is_symlink(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "is_symlink(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_is_symlink(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_dirname(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "dirname(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);
    size_t dirname_length = pyro_dirname(path->bytes);

    PyroStr* result = PyroStr_copy(path->bytes, dirname_length, false, vm);
    if (!result) {
        pyro_panic(vm, "dirname(): out of memory");
        return pyro_null();
    }

    return pyro_obj(result);
}


static PyroValue fn_basename(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "basename(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);
    const char* basename_ptr = pyro_basename(path->bytes);

    PyroStr* result = PyroStr_COPY(basename_ptr);
    if (!result) {
        pyro_panic(vm, "basename(): out of memory");
        return pyro_null();
    }

    return pyro_obj(result);
}


static PyroValue fn_join(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "join(): expected 1 or more arguments, found 0");
        return pyro_null();
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (!PYRO_IS_STR(args[i])) {
            pyro_panic(vm, "join(): invalid argument (%zu), expected a string", i + 1);
            return pyro_null();
        }
    }

    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "join(): out of memory");
        return pyro_null();
    }

    for (size_t i = 0; i < arg_count; i++) {
        PyroStr* arg = PYRO_AS_STR(args[i]);
        if (arg->count == 0) {
            continue;
        }

        if (buf->count > 0 && buf->bytes[buf->count - 1] != '/') {
            if (!PyroBuf_append_byte(buf, '/', vm)) {
                pyro_panic(vm, "join(): out of memory");
                return pyro_null();
            }
        }

        if (arg->bytes[0] == '/') {
            buf->count = 0;
        }

        if (!PyroBuf_append_bytes(buf, arg->count, (uint8_t*)arg->bytes, vm)) {
            pyro_panic(vm, "join(): out of memory");
            return pyro_null();
        }
    }

    PyroStr* result = PyroBuf_to_str(buf, vm);
    if (!result) {
        pyro_panic(vm, "join(): out of memory");
        return pyro_null();
    }

    return pyro_obj(result);
}


static PyroValue fn_getcwd(PyroVM* vm, size_t arg_count, PyroValue* args) {
    char* cwd = pyro_getcwd();
    if (!cwd) {
        pyro_panic(vm, "getcwd(): out of memory");
        return pyro_null();
    }

    PyroStr* string = PyroStr_copy(cwd, strlen(cwd), false, vm);
    free(cwd);

    if (!string) {
        pyro_panic(vm, "getcwd(): out of memory");
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue fn_listdir(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "listdir(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroVec* vec = pyro_listdir(vm, PYRO_AS_STR(args[0])->bytes);
    if (vm->halt_flag) {
        return pyro_null();
    }

    return pyro_obj(vec);
}


static PyroValue fn_realpath(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "realpath(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);

    char* real_path = pyro_realpath(path->bytes);
    if (!real_path) {
        pyro_panic(vm, "realpath(): failed to resolve path '%s'", path->bytes);
        return pyro_null();
    }

    PyroStr* result = PyroStr_COPY(real_path);
    if (!result) {
        free(real_path);
        pyro_panic(vm, "realpath(): out of memory");
        return pyro_null();
    }

    free(real_path);
    return pyro_obj(result);
}


static PyroValue fn_chdir(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "chdir(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);
    if (!pyro_chdir(path->bytes)) {
        pyro_panic(vm, "chdir(): failed to change the current working directory to '%s'", path->bytes);
        return pyro_null();
    }

    return pyro_null();
}


static PyroValue fn_normpath(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "normpath(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);
    const char* src = path->bytes;
    size_t len = path->count;

    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "normpath(): out of memory");
        return pyro_null();
    }

    size_t index = 0;

    // Special handling for paths beginning with exactly two slashes.
    if (path->count == 2 && memcmp(src, "//", 2) == 0) {
        if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)"//", vm)) {
            pyro_panic(vm, "normpath(): out of memory");
            return pyro_null();
        }
        index += 2;
    } else if (path->count > 2 && memcmp(src, "//", 2) == 0 && src[2] != '/') {
        if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)"//", vm)) {
            pyro_panic(vm, "normpath(): out of memory");
            return pyro_null();
        }
        index += 2;
    }

    while (index < len) {
        // Check if the next token is: "./"
        if (len - index >= 2 && memcmp(&src[index], "./", 2) == 0) {
            index += 2;
            while (index < len && src[index] == '/') {
                index++;
            }
            continue;
        }

        // Check if the next token is: "../"
        if (len - index >= 3 && memcmp(&src[index], "../", 3) == 0) {
            if (buf->count == 0) {
                if (!PyroBuf_append_bytes(buf, 3, (uint8_t*)"../", vm)) {
                    pyro_panic(vm, "normpath(): out of memory");
                    return pyro_null();
                }
                index += 3;
                continue;
            }

            if (buf->count == 1 && buf->bytes[0] == '/') {
                index += 3;
                continue;
            }

            if (buf->count == 2 && buf->bytes[0] == '/' && buf->bytes[1] == '/') {
                index += 3;
                continue;
            }

            if (buf->count == 3 && memcmp(buf->bytes, "../", 3) == 0) {
                if (!PyroBuf_append_bytes(buf, 3, (uint8_t*)"../", vm)) {
                    pyro_panic(vm, "normpath(): out of memory");
                    return pyro_null();
                }
                index += 3;
                continue;
            }

            if (buf->count > 3 && memcmp(&buf->bytes[buf->count - 4], "/..", 3) == 0) {
                if (!PyroBuf_append_bytes(buf, 3, (uint8_t*)"../", vm)) {
                    pyro_panic(vm, "normpath(): out of memory");
                    return pyro_null();
                }
                index += 3;
                continue;
            }

            if (buf->bytes[buf->count - 1] == '/') {
                buf->count--;
            }

            while (buf->count > 0 && buf->bytes[buf->count - 1] != '/') {
                buf->count--;
            }

            index += 3;
            while (index < len && src[index] == '/') {
                index++;
            }
            continue;
        }

        // Consume any characters that are not: "/"
        while (index < len && src[index] != '/') {
            if (!PyroBuf_append_byte(buf, src[index], vm)) {
                pyro_panic(vm, "normpath(): out of memory");
                return pyro_null();
            }
            index++;
        }

        // Consume any number of "/" but write only one.
        while (index < len && src[index] == '/') {
            if (buf->count == 0 || buf->bytes[buf->count - 1] != '/') {
                if (!PyroBuf_append_byte(buf, '/', vm)) {
                    pyro_panic(vm, "normpath(): out of memory");
                    return pyro_null();
                }
            }
            index++;
        }
    }

    // Handle a trailing: /..
    if (buf->count >= 3 && memcmp(&buf->bytes[buf->count - 3], "/..", 3) == 0) {
        if (buf->count == 3) {
            buf->count -= 2;
        } else if (buf->count == 4 && buf->bytes[0] == '/') {
            buf->count -= 2;
        } else if (buf->count == 5 && memcmp(buf->bytes, "../..", 5) == 0) {
            // Keep the trailing "..".
        } else if (buf->count >= 6 && memcmp(&buf->bytes[buf->count - 6], "/..", 3) == 0) {
            // Keep the trailing "..".
        } else {
            buf->count -= 3;
            while (buf->count > 0 && buf->bytes[buf->count - 1] != '/') {
                buf->count--;
            }
        }
    }

    // Handle a trailing: /.
    if (buf->count >= 2 && buf->bytes[buf->count - 2] == '/' && buf->bytes[buf->count - 1] == '.') {
        buf->count--;
    }

    // Handle a trailing: /
    if (buf->count >= 1 && buf->bytes[buf->count - 1] == '/') {
        buf->count--;
        if (buf->count == 0 || (buf->count == 1 && buf->bytes[0] == '/')) {
            buf->count++;
        }
    }

    // Convert "" -> ".".
    if (buf->count == 0) {
        if (!PyroBuf_append_byte(buf, '.', vm)) {
            pyro_panic(vm, "normpath(): out of memory");
            return pyro_null();
        }
    }

    PyroStr* result = PyroBuf_to_str(buf, vm);
    if (!result) {
        pyro_panic(vm, "normpath(): out of memory");
        return pyro_null();
    }

    return pyro_obj(result);
}


static PyroValue fn_abspath(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "abspath(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroValue cwd = fn_getcwd(vm, 0, NULL);
    if (vm->halt_flag) {
        return pyro_null();
    }

    PyroValue args_array_for_fn_join[] = {cwd, args[0]};
    PyroValue joined_path = fn_join(vm, 2, args_array_for_fn_join);
    if (vm->halt_flag) {
        return pyro_null();
    }

    PyroValue normalized_path = fn_normpath(vm, 1, &joined_path);
    if (vm->halt_flag) {
        return pyro_null();
    }

    return normalized_path;
}


void pyro_load_stdlib_module_fs(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "exists", fn_exists, 1);
    pyro_define_pub_member_fn(vm, module, "is_file", fn_is_file, 1);
    pyro_define_pub_member_fn(vm, module, "is_dir", fn_is_dir, 1);
    pyro_define_pub_member_fn(vm, module, "is_symlink", fn_is_symlink, 1);
    pyro_define_pub_member_fn(vm, module, "dirname", fn_dirname, 1);
    pyro_define_pub_member_fn(vm, module, "basename", fn_basename, 1);
    pyro_define_pub_member_fn(vm, module, "join", fn_join, -1);
    pyro_define_pub_member_fn(vm, module, "listdir", fn_listdir, 1);
    pyro_define_pub_member_fn(vm, module, "realpath", fn_realpath, 1);
    pyro_define_pub_member_fn(vm, module, "chdir", fn_chdir, 1);
    pyro_define_pub_member_fn(vm, module, "getcwd", fn_getcwd, 0);
    pyro_define_pub_member_fn(vm, module, "normpath", fn_normpath, 1);
    pyro_define_pub_member_fn(vm, module, "abspath", fn_abspath, 1);
}
