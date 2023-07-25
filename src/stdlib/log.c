#include "../includes/pyro.h"


#define PYRO_STD_LOG_LEVEL_DEBUG 10
#define PYRO_STD_LOG_LEVEL_INFO 20
#define PYRO_STD_LOG_LEVEL_WARN 30
#define PYRO_STD_LOG_LEVEL_ERROR 40
#define PYRO_STD_LOG_LEVEL_FATAL 50


static void write_msg(
    PyroVM* vm,
    const char* err_prefix,
    const char* timestamp_format,
    const char* log_level,
    PyroFile* file,
    size_t arg_count,
    PyroValue* args
) {
    if (arg_count == 0) {
        pyro_panic(vm, "%s: expected 1 or more arguments, found 0", err_prefix);
        return;
    }

    PyroStr* message;
    if (arg_count == 1) {
        message = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return;
        }
    } else {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "%s: invalid argument [format_string], expected a string", err_prefix);
            return;
        }
        message = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], err_prefix);
        if (vm->halt_flag) {
            return;
        }
    }

    char timestamp_buffer[128];
    timestamp_buffer[0] = '\0';

    if (strlen(timestamp_format) > 0) {
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);
        size_t dt_count = strftime(timestamp_buffer, 128, timestamp_format, tm);
        if (dt_count == 0) {
            pyro_panic(vm, "%s: timestamp string is too long", err_prefix);
            return;
        }
    }

    if (file) {
        if (file->stream) {
            int result = fprintf(file->stream, "[%5s]  [%s]  %s\n", log_level, timestamp_buffer, message->bytes);
            if (result < 0) {
                pyro_panic(vm, "%s: failed to write log message to file", err_prefix);
            }
        } else {
            pyro_panic(vm, "%s: failed to write to log file, file has been closed", err_prefix);
        }
    } else {
        int64_t result = pyro_stderr_write_f(vm, "[%5s]  [%s]  %s\n", log_level, timestamp_buffer, message->bytes);
        if (result < 0) {
            pyro_panic(vm, "%s: failed to write log message to the standard error stream", err_prefix);
        }
    }
}


static PyroValue fn_debug(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "debug()", "%Y-%m-%d %H:%M:%S", "DEBUG", NULL, arg_count, args);
    return pyro_null();
}


static PyroValue fn_info(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "info()", "%Y-%m-%d %H:%M:%S", "INFO", NULL, arg_count, args);
    return pyro_null();
}


static PyroValue fn_warn(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "warn()", "%Y-%m-%d %H:%M:%S", "WARN", NULL, arg_count, args);
    return pyro_null();
}


static PyroValue fn_error(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "error()", "%Y-%m-%d %H:%M:%S", "ERROR", NULL, arg_count, args);
    return pyro_null();
}


static PyroValue fn_fatal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "fatal()", "%Y-%m-%d %H:%M:%S", "FATAL", NULL, arg_count, args);
    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return pyro_null();
}


static PyroValue logger_debug(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    PyroStr* timestamp_format = PYRO_AS_STR(instance->fields[1]);
    PyroFile* file = PYRO_IS_NULL(instance->fields[2]) ? NULL : PYRO_AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_DEBUG >= log_level) {
        write_msg(vm, "debug()", timestamp_format->bytes, "DEBUG", file, arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_info(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    PyroStr* timestamp_format = PYRO_AS_STR(instance->fields[1]);
    PyroFile* file = PYRO_IS_NULL(instance->fields[2]) ? NULL : PYRO_AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_INFO >= log_level) {
        write_msg(vm, "info()", timestamp_format->bytes, "INFO", file, arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_warn(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    PyroStr* timestamp_format = PYRO_AS_STR(instance->fields[1]);
    PyroFile* file = PYRO_IS_NULL(instance->fields[2]) ? NULL : PYRO_AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_WARN >= log_level) {
        write_msg(vm, "warn()", timestamp_format->bytes, "WARN", file, arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_error(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    PyroStr* timestamp_format = PYRO_AS_STR(instance->fields[1]);
    PyroFile* file = PYRO_IS_NULL(instance->fields[2]) ? NULL : PYRO_AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_ERROR >= log_level) {
        write_msg(vm, "error()", timestamp_format->bytes, "ERROR", file, arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_fatal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    PyroStr* timestamp_format = PYRO_AS_STR(instance->fields[1]);
    PyroFile* file = PYRO_IS_NULL(instance->fields[2]) ? NULL : PYRO_AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_FATAL >= log_level) {
        write_msg(vm, "fatal()", timestamp_format->bytes, "FATAL", file, arg_count, args);
    }

    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return pyro_null();
}


static PyroValue logger_level(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "level(): invalid argument [level], expected an integer");
        return pyro_null();
    }

    instance->fields[0] = args[0];
    return pyro_null();
}


static PyroValue logger_timestamp(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "timestamp(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    instance->fields[1] = args[0];
    return pyro_null();
}


static PyroValue logger_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    if (!PYRO_IS_FILE(args[0])) {
        pyro_panic(vm, "file(): invalid argument [file], expected a file");
        return pyro_null();
    }

    instance->fields[2] = args[0];
    return pyro_null();
}


void pyro_load_std_mod_log(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "debug", fn_debug, -1);
    pyro_define_pub_member_fn(vm, module, "info", fn_info, -1);
    pyro_define_pub_member_fn(vm, module, "warn", fn_warn, -1);
    pyro_define_pub_member_fn(vm, module, "error", fn_error, -1);
    pyro_define_pub_member_fn(vm, module, "fatal", fn_fatal, -1);

    pyro_define_pub_member(vm, module, "DEBUG", pyro_i64(PYRO_STD_LOG_LEVEL_DEBUG));
    pyro_define_pub_member(vm, module, "INFO", pyro_i64(PYRO_STD_LOG_LEVEL_INFO));
    pyro_define_pub_member(vm, module, "WARN", pyro_i64(PYRO_STD_LOG_LEVEL_WARN));
    pyro_define_pub_member(vm, module, "ERROR", pyro_i64(PYRO_STD_LOG_LEVEL_ERROR));
    pyro_define_pub_member(vm, module, "FATAL", pyro_i64(PYRO_STD_LOG_LEVEL_FATAL));

    PyroClass* logger_class = PyroClass_new(vm);
    if (!logger_class) {
        return;
    }
    logger_class->name = PyroStr_COPY("Logger");
    pyro_define_pub_member(vm, module, "Logger", pyro_obj(logger_class));

    pyro_define_pub_field(vm, logger_class, "level", pyro_i64(PYRO_STD_LOG_LEVEL_INFO));
    pyro_define_pub_field(vm, logger_class, "timestamp", pyro_obj(PyroStr_COPY("%Y-%m-%d %H:%M:%S")));
    pyro_define_pub_field(vm, logger_class, "file", pyro_null());

    pyro_define_pub_method(vm, logger_class, "level", logger_level, 1);
    pyro_define_pub_method(vm, logger_class, "timestamp", logger_timestamp, 1);
    pyro_define_pub_method(vm, logger_class, "file", logger_file, 1);
    pyro_define_pub_method(vm, logger_class, "debug", logger_debug, -1);
    pyro_define_pub_method(vm, logger_class, "info", logger_info, -1);
    pyro_define_pub_method(vm, logger_class, "warn", logger_warn, -1);
    pyro_define_pub_method(vm, logger_class, "error", logger_error, -1);
    pyro_define_pub_method(vm, logger_class, "fatal", logger_fatal, -1);
}
