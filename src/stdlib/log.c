#include "../includes/pyro.h"


#define PYRO_STD_LOG_LEVEL_DEBUG 10
#define PYRO_STD_LOG_LEVEL_INFO 20
#define PYRO_STD_LOG_LEVEL_WARN 30
#define PYRO_STD_LOG_LEVEL_ERROR 40
#define PYRO_STD_LOG_LEVEL_FATAL 50


static void write_msg(
    PyroVM* vm,
    const char* err_prefix,
    const char* log_level,
    PyroFile* file,
    size_t arg_count,
    PyroValue* args,
    bool use_utc
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

    time_t now_sec = 0;
    int64_t now_nsec = 0;

    #ifdef CLOCK_REALTIME
        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
            pyro_panic(vm, "%s: failed to get time", err_prefix);
            return;
        }
        now_sec = now.tv_sec;
        now_nsec = now.tv_nsec;
    #else
        now_sec = time(NULL);
    #endif

    int64_t now_usec = now_nsec / 1000; // microseconds
    int64_t now_msec = now_usec / 1000; // milliseconds

    char* timestamp_sec = "0000-00-00 00:00:00";
    char timestamp_sec_buffer[128];

    struct tm tm;
    if (use_utc) {
        gmtime_r(&now_sec, &tm);
    } else {
        localtime_r(&now_sec, &tm);
    }

    size_t timestamp_sec_buffer_count = strftime(timestamp_sec_buffer, 128, "%Y-%m-%d %H:%M:%S", &tm);
    if (timestamp_sec_buffer_count > 0) {
        timestamp_sec = timestamp_sec_buffer;
    }

    if (file) {
        if (!file->stream) {
            pyro_panic(vm, "%s: failed to write log message, file has been closed", err_prefix);
            return;
        }
        int result = fprintf(file->stream, "[%5s]  %s.%03d  %s\n", log_level, timestamp_sec, (int)now_msec, message->bytes);
        if (result < 0) {
            pyro_panic(vm, "%s: failed to write log message", err_prefix);
        }
        return;
    }

    int64_t result = pyro_stdout_write_f(vm, "[%5s]  %s.%03d  %s\n", log_level, timestamp_sec, (int)now_msec, message->bytes);
    if (result < 0) {
        pyro_panic(vm, "%s: failed to write log message to the standard output stream", err_prefix);
    }
}


static PyroValue fn_debug(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "debug()", "DEBUG", NULL, arg_count, args, false);
    return pyro_null();
}


static PyroValue fn_info(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "info()", "INFO", NULL, arg_count, args, false);
    return pyro_null();
}


static PyroValue fn_warn(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "warn()", "WARN", NULL, arg_count, args, false);
    return pyro_null();
}


static PyroValue fn_error(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "error()", "ERROR", NULL, arg_count, args, false);
    return pyro_null();
}


static PyroValue fn_fatal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "fatal()", "FATAL", NULL, arg_count, args, false);
    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return pyro_null();
}


static PyroValue logger_debug(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;
    PyroFile* output_file = PYRO_IS_NULL(instance->fields[1]) ? NULL : PYRO_AS_FILE(instance->fields[1]);
    bool use_utc = instance->fields[2].as.boolean;

    if (PYRO_STD_LOG_LEVEL_DEBUG >= logging_level) {
        write_msg(vm, "debug()", "DEBUG", output_file, arg_count, args, use_utc);
    }

    return pyro_null();
}


static PyroValue logger_info(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;
    PyroFile* output_file = PYRO_IS_NULL(instance->fields[1]) ? NULL : PYRO_AS_FILE(instance->fields[1]);
    bool use_utc = instance->fields[2].as.boolean;

    if (PYRO_STD_LOG_LEVEL_INFO >= logging_level) {
        write_msg(vm, "info()", "INFO", output_file, arg_count, args, use_utc);
    }

    return pyro_null();
}


static PyroValue logger_warn(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;
    PyroFile* output_file = PYRO_IS_NULL(instance->fields[1]) ? NULL : PYRO_AS_FILE(instance->fields[1]);
    bool use_utc = instance->fields[2].as.boolean;

    if (PYRO_STD_LOG_LEVEL_WARN >= logging_level) {
        write_msg(vm, "warn()", "WARN", output_file, arg_count, args, use_utc);
    }

    return pyro_null();
}


static PyroValue logger_error(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;
    PyroFile* output_file = PYRO_IS_NULL(instance->fields[1]) ? NULL : PYRO_AS_FILE(instance->fields[1]);
    bool use_utc = instance->fields[2].as.boolean;

    if (PYRO_STD_LOG_LEVEL_ERROR >= logging_level) {
        write_msg(vm, "error()", "ERROR", output_file, arg_count, args, use_utc);
    }

    return pyro_null();
}


static PyroValue logger_fatal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;
    PyroFile* output_file = PYRO_IS_NULL(instance->fields[1]) ? NULL : PYRO_AS_FILE(instance->fields[1]);
    bool use_utc = instance->fields[2].as.boolean;

    if (PYRO_STD_LOG_LEVEL_FATAL >= logging_level) {
        write_msg(vm, "fatal()", "FATAL", output_file, arg_count, args, use_utc);
    }

    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return pyro_null();
}


// Deprecated.
static PyroValue logger_level(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "level(): invalid argument [level], expected an integer");
        return pyro_null();
    }

    instance->fields[0] = args[0];
    return pyro_null();
}


// Deprecated.
static PyroValue logger_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    if (!PYRO_IS_FILE(args[0])) {
        pyro_panic(vm, "file(): invalid argument [file], expected a file");
        return pyro_null();
    }

    instance->fields[1] = args[0];
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

    pyro_define_pub_field(vm, logger_class, "logging_level", pyro_i64(PYRO_STD_LOG_LEVEL_INFO));
    pyro_define_pub_field(vm, logger_class, "output_file", pyro_null());
    pyro_define_pub_field(vm, logger_class, "use_utc", pyro_bool(false));

    pyro_define_pub_method(vm, logger_class, "debug", logger_debug, -1);
    pyro_define_pub_method(vm, logger_class, "info", logger_info, -1);
    pyro_define_pub_method(vm, logger_class, "warn", logger_warn, -1);
    pyro_define_pub_method(vm, logger_class, "error", logger_error, -1);
    pyro_define_pub_method(vm, logger_class, "fatal", logger_fatal, -1);

    // Deprecated.
    pyro_define_pub_method(vm, logger_class, "level", logger_level, 1);
    pyro_define_pub_method(vm, logger_class, "file", logger_file, 1);
}
