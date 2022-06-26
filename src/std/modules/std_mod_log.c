#include "../std_lib.h"

#include "../../vm/values.h"
#include "../../vm/vm.h"
#include "../../vm/setup.h"
#include "../../vm/panics.h"
#include "../../vm/stringify.h"
#include "../../vm/io.h"


#define PYRO_STD_LOG_LEVEL_DEBUG 2
#define PYRO_STD_LOG_LEVEL_INFO 4
#define PYRO_STD_LOG_LEVEL_WARN 8
#define PYRO_STD_LOG_LEVEL_ERROR 16
#define PYRO_STD_LOG_LEVEL_FATAL 32


static void pyro_log(
    PyroVM* vm,
    const char* fn_name,
    const char* timestamp_format,
    const char* level,
    ObjFile* file,
    size_t arg_count,
    Value* args
) {
    if (arg_count == 0) {
        pyro_panic(vm, "%s(): expected 1 or more arguments, found 0", fn_name);
        return;
    }

    ObjStr* message;
    if (arg_count == 1) {
        message = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return;
        }
    } else {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "%s(): invalid argument [format_string], expected a string", fn_name);
            return;
        }
        Value formatted = pyro_fn_fmt(vm, arg_count, args);
        if (vm->halt_flag) {
            return;
        }
        message = AS_STR(formatted);
    }

    char timestamp_buffer[128];
    timestamp_buffer[0] = '\0';

    if (strlen(timestamp_format) > 0) {
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);
        size_t dt_count = strftime(timestamp_buffer, 128, timestamp_format, tm);
        if (dt_count == 0) {
            pyro_panic(vm, "%s(): formatted timestamp string is too long", fn_name);
            return;
        }
    }

    if (file) {
        if (file->stream) {
            int result = fprintf(file->stream, "%s  %5s  %s\n", timestamp_buffer, level, message->bytes);
            if (result < 0) {
                pyro_panic(vm, "%s(): failed to write log message to file", fn_name);
            }
        } else {
            pyro_panic(vm, "%s(): unable to write to log file, file has been closed", fn_name);
        }
    } else {
        int64_t result = pyro_stderr_write_f(vm, "%s  %5s  %s\n", timestamp_buffer, level, message->bytes);
        if (result < 0) {
            pyro_panic(vm, "%s(): failed to write log message to standard error stream", fn_name);
        }
    }
}


static Value fn_debug(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_log(vm, "debug", "[%Y-%m-%d %H:%M:%S]", "DEBUG", NULL, arg_count, args);
    return MAKE_NULL();
}


static Value fn_info(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_log(vm, "info", "[%Y-%m-%d %H:%M:%S]", "INFO", NULL, arg_count, args);
    return MAKE_NULL();
}


static Value fn_warn(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_log(vm, "warn", "[%Y-%m-%d %H:%M:%S]", "WARN", NULL, arg_count, args);
    return MAKE_NULL();
}


static Value fn_error(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_log(vm, "error", "[%Y-%m-%d %H:%M:%S]", "ERROR", NULL, arg_count, args);
    return MAKE_NULL();
}


static Value fn_fatal(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_log(vm, "fatal", "[%Y-%m-%d %H:%M:%S]", "FATAL", NULL, arg_count, args);
    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return MAKE_NULL();
}


static Value logger_debug(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    ObjStr* timestamp_format = AS_STR(instance->fields[1]);
    ObjFile* file = IS_NULL(instance->fields[2]) ? NULL : AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_DEBUG >= log_level) {
        pyro_log(vm, "debug", timestamp_format->bytes, "DEBUG", file, arg_count, args);
    }

    return MAKE_NULL();
}


static Value logger_info(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    ObjStr* timestamp_format = AS_STR(instance->fields[1]);
    ObjFile* file = IS_NULL(instance->fields[2]) ? NULL : AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_INFO >= log_level) {
        pyro_log(vm, "info", timestamp_format->bytes, "INFO", file, arg_count, args);
    }

    return MAKE_NULL();
}


static Value logger_warn(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    ObjStr* timestamp_format = AS_STR(instance->fields[1]);
    ObjFile* file = IS_NULL(instance->fields[2]) ? NULL : AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_WARN >= log_level) {
        pyro_log(vm, "warn", timestamp_format->bytes, "WARN", file, arg_count, args);
    }

    return MAKE_NULL();
}


static Value logger_error(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    ObjStr* timestamp_format = AS_STR(instance->fields[1]);
    ObjFile* file = IS_NULL(instance->fields[2]) ? NULL : AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_ERROR >= log_level) {
        pyro_log(vm, "error", timestamp_format->bytes, "ERROR", file, arg_count, args);
    }

    return MAKE_NULL();
}


static Value logger_fatal(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    int64_t log_level = instance->fields[0].as.i64;
    ObjStr* timestamp_format = AS_STR(instance->fields[1]);
    ObjFile* file = IS_NULL(instance->fields[2]) ? NULL : AS_FILE(instance->fields[2]);

    if (PYRO_STD_LOG_LEVEL_FATAL >= log_level) {
        pyro_log(vm, "fatal", timestamp_format->bytes, "FATAL", file, arg_count, args);
    }

    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return MAKE_NULL();
}


static Value logger_level(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "level(): invalid argument [level], expected an integer");
        return MAKE_NULL();
    }

    instance->fields[0] = args[0];
    return MAKE_NULL();
}


static Value logger_timestamp(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "timestamp(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    instance->fields[1] = args[0];
    return MAKE_NULL();
}


static Value logger_file(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    if (!IS_FILE(args[0])) {
        pyro_panic(vm, "file(): invalid argument [file], expected a file");
        return MAKE_NULL();
    }

    instance->fields[2] = args[0];
    return MAKE_NULL();
}


void pyro_load_std_mod_log(PyroVM* vm, ObjModule* module) {
    pyro_define_member_fn(vm, module, "debug", fn_debug, -1);
    pyro_define_member_fn(vm, module, "info", fn_info, -1);
    pyro_define_member_fn(vm, module, "warn", fn_warn, -1);
    pyro_define_member_fn(vm, module, "error", fn_error, -1);
    pyro_define_member_fn(vm, module, "fatal", fn_fatal, -1);

    pyro_define_member(vm, module, "DEBUG", MAKE_I64(PYRO_STD_LOG_LEVEL_DEBUG));
    pyro_define_member(vm, module, "INFO", MAKE_I64(PYRO_STD_LOG_LEVEL_INFO));
    pyro_define_member(vm, module, "WARN", MAKE_I64(PYRO_STD_LOG_LEVEL_WARN));
    pyro_define_member(vm, module, "ERROR", MAKE_I64(PYRO_STD_LOG_LEVEL_ERROR));
    pyro_define_member(vm, module, "FATAL", MAKE_I64(PYRO_STD_LOG_LEVEL_FATAL));

    ObjClass* logger_class = ObjClass_new(vm);
    if (!logger_class) {
        return;
    }
    logger_class->name = STR("Logger");
    pyro_define_member(vm, module, "Logger", MAKE_OBJ(logger_class));

    pyro_define_field(vm, logger_class, "level", MAKE_I64(PYRO_STD_LOG_LEVEL_INFO));
    pyro_define_field(vm, logger_class, "timestamp", MAKE_OBJ(STR("[%Y-%m-%d %H:%M:%S]")));
    pyro_define_field(vm, logger_class, "file", MAKE_NULL());

    pyro_define_method(vm, logger_class, "level", logger_level, 1);
    pyro_define_method(vm, logger_class, "timestamp", logger_timestamp, 1);
    pyro_define_method(vm, logger_class, "file", logger_file, 1);
    pyro_define_method(vm, logger_class, "debug", logger_debug, -1);
    pyro_define_method(vm, logger_class, "info", logger_info, -1);
    pyro_define_method(vm, logger_class, "warn", logger_warn, -1);
    pyro_define_method(vm, logger_class, "error", logger_error, -1);
    pyro_define_method(vm, logger_class, "fatal", logger_fatal, -1);
}
