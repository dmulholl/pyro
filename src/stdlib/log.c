#include "../includes/pyro.h"


#define PYRO_STD_LOG_LEVEL_DEBUG 10
#define PYRO_STD_LOG_LEVEL_INFO 20
#define PYRO_STD_LOG_LEVEL_WARN 30
#define PYRO_STD_LOG_LEVEL_ERROR 40
#define PYRO_STD_LOG_LEVEL_FATAL 50


static void write_msg(
    PyroVM* vm,
    const char* err_prefix,
    const char* log_level_name,
    PyroValue file,
    PyroValue use_utc,
    PyroValue show_milliseconds,
    size_t arg_count,
    PyroValue* args
) {
    if (!PYRO_IS_FILE(file)) {
        pyro_panic(vm, "%s: invalid type for output_file field: expected a file, found %s", err_prefix, pyro_get_type_name(vm, file)->bytes);
        return;
    }

    if (!PYRO_AS_FILE(file)->stream) {
        pyro_panic(vm, "%s: invalid value for output_file field, file has been closed", err_prefix);
        return;
    }

    if (!PYRO_IS_BOOL(use_utc)) {
        pyro_panic(vm, "%s: invalid type for use_utc field: expected a boolean, found %s", err_prefix, pyro_get_type_name(vm, use_utc)->bytes);
        return;
    }

    if (!PYRO_IS_BOOL(show_milliseconds)) {
        pyro_panic(vm, "%s: invalid type for show_milliseconds field: expected a boolean, found %s", err_prefix, pyro_get_type_name(vm, show_milliseconds)->bytes);
        return;
    }

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

    time_t timestamp_now_seconds = 0;
    int64_t timestamp_now_nanoseconds = 0;

    #ifdef CLOCK_REALTIME
        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
            pyro_panic(vm, "%s: failed to get time", err_prefix);
            return;
        }
        timestamp_now_seconds = now.tv_sec;
        timestamp_now_nanoseconds = now.tv_nsec;
    #else
        timestamp_now_seconds = time(NULL);
    #endif

    int64_t microseconds = timestamp_now_nanoseconds / 1000;
    int64_t milliseconds = microseconds / 1000;

    struct tm tm;
    if (use_utc.as.boolean) {
        gmtime_r(&timestamp_now_seconds, &tm);
    } else {
        localtime_r(&timestamp_now_seconds, &tm);
    }

    char* timestamp_str = "0000-00-00 00:00:00";
    char timestamp_str_buffer[128];

    size_t timestamp_str_buffer_count = strftime(timestamp_str_buffer, 128, "%Y-%m-%d %H:%M:%S", &tm);
    if (timestamp_str_buffer_count > 0) {
        timestamp_str = timestamp_str_buffer;
    }

    if (show_milliseconds.as.boolean) {
        int result = fprintf(
            PYRO_AS_FILE(file)->stream,
            "[%5s]  %s.%03d%s  %s\n",
            log_level_name,
            timestamp_str,
            (int)milliseconds,
            use_utc.as.boolean ? "Z" : "",
            message->bytes
        );

        if (result < 0) {
            pyro_panic(vm, "%s: failed to write log message", err_prefix);
        }

        return;
    }

    int result = fprintf(
        PYRO_AS_FILE(file)->stream,
        "[%5s]  %sd%s  %s\n",
        log_level_name,
        timestamp_str,
        use_utc.as.boolean ? "Z" : "",
        message->bytes
    );

    if (result < 0) {
        pyro_panic(vm, "%s: failed to write log message", err_prefix);
    }
}


static PyroValue fn_debug(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "debug()", "DEBUG", pyro_obj(vm->stdout_file), pyro_bool(false), pyro_bool(false), arg_count, args);
    return pyro_null();
}


static PyroValue fn_info(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "info()", "INFO", pyro_obj(vm->stdout_file), pyro_bool(false), pyro_bool(false), arg_count, args);
    return pyro_null();
}


static PyroValue fn_warn(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "warn()", "WARN", pyro_obj(vm->stdout_file), pyro_bool(false), pyro_bool(false), arg_count, args);
    return pyro_null();
}


static PyroValue fn_error(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "error()", "ERROR", pyro_obj(vm->stdout_file), pyro_bool(false), pyro_bool(false), arg_count, args);
    return pyro_null();
}


static PyroValue fn_fatal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    write_msg(vm, "fatal()", "FATAL", pyro_obj(vm->stdout_file), pyro_bool(false), pyro_bool(false), arg_count, args);
    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return pyro_null();
}


static PyroValue logger_debug(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;

    if (PYRO_STD_LOG_LEVEL_DEBUG >= logging_level) {
        write_msg(vm, "debug()", "DEBUG", instance->fields[1], instance->fields[2], instance->fields[3], arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_info(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;

    if (PYRO_STD_LOG_LEVEL_INFO >= logging_level) {
        write_msg(vm, "info()", "INFO", instance->fields[1], instance->fields[2], instance->fields[3], arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_warn(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;

    if (PYRO_STD_LOG_LEVEL_WARN >= logging_level) {
        write_msg(vm, "warn()", "WARN", instance->fields[1], instance->fields[2], instance->fields[3], arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_error(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;

    if (PYRO_STD_LOG_LEVEL_ERROR >= logging_level) {
        write_msg(vm, "error()", "ERROR", instance->fields[1], instance->fields[2], instance->fields[3], arg_count, args);
    }

    return pyro_null();
}


static PyroValue logger_fatal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    int64_t logging_level = instance->fields[0].as.i64;

    if (PYRO_STD_LOG_LEVEL_FATAL >= logging_level) {
        write_msg(vm, "fatal()", "FATAL", instance->fields[1], instance->fields[2], instance->fields[3], arg_count, args);
    }

    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
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
    pyro_define_pub_field(vm, logger_class, "output_file", pyro_obj(vm->stdout_file));
    pyro_define_pub_field(vm, logger_class, "use_utc", pyro_bool(false));
    pyro_define_pub_field(vm, logger_class, "show_milliseconds", pyro_bool(false));
    pyro_define_pub_field(vm, logger_class, "show_microseconds", pyro_bool(false));

    pyro_define_pub_method(vm, logger_class, "debug", logger_debug, -1);
    pyro_define_pub_method(vm, logger_class, "info", logger_info, -1);
    pyro_define_pub_method(vm, logger_class, "warn", logger_warn, -1);
    pyro_define_pub_method(vm, logger_class, "error", logger_error, -1);
    pyro_define_pub_method(vm, logger_class, "fatal", logger_fatal, -1);
}
