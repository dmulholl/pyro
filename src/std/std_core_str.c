#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/setup.h"
#include "../vm/stringify.h"
#include "../vm/panics.h"
#include "../vm/exec.h"


static Value fn_str(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* string = pyro_stringify_value(vm, args[0]);
    if (vm->halt_flag) {
        return NULL_VAL();
    }
    return OBJ_VAL(string);
}


static Value fn_is_str(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_STR(args[0]));
}


static Value str_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    return BOOL_VAL(str->length == 0);
}


static Value str_byte_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    return I64_VAL(str->length);
}


// Returns the byte value at index [i] as an integer in the range [0,256).
static Value str_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < str->length) {
            return I64_VAL((uint8_t)str->bytes[index]);
        }
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, ERR_VALUE_ERROR, "Invalid index type.");
    return NULL_VAL();
}


static Value str_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)str, ITER_STR_BYTES, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iter);
}


static Value str_is_ascii(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    for (size_t i = 0; i < str->length; i++) {
        if (str->bytes[i] & 0x80) {
            return BOOL_VAL(false);
        }
    }

    return BOOL_VAL(true);
}


static Value str_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)str, ITER_STR_CHARS, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iter);
}


static Value str_char(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type.");
        return NULL_VAL();
    }

    int64_t target_index = args[0].as.i64;
    if (target_index < 0 || str->length == 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }

    size_t byte_index = 0;
    size_t char_count = 0;
    Utf8CodePoint cp;

    while (char_count < (size_t)target_index + 1) {
        if (byte_index == str->length) {
            pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
            return NULL_VAL();
        }

        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->length - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            char_count++;
            byte_index += cp.length;
        } else {
            pyro_panic(vm, ERR_VALUE_ERROR, "String contains invalid utf-8 at byte index %zu.", byte_index);
            return NULL_VAL();
        }
    }

    return CHAR_VAL(cp.value);
}


static Value str_char_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    size_t byte_index = 0;
    size_t char_count = 0;
    Utf8CodePoint cp;

    while (byte_index < str->length) {
        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->length - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            char_count++;
            byte_index += cp.length;
        } else {
            pyro_panic(vm, ERR_VALUE_ERROR, "String contains invalid utf-8 at byte index %zu.", byte_index);
            return NULL_VAL();
        }
    }

    return I64_VAL(char_count);
}


static Value str_is_utf8(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    size_t byte_index = 0;
    Utf8CodePoint cp;

    while (byte_index < str->length) {
        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->length - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            byte_index += cp.length;
        } else {
            return BOOL_VAL(false);
        }
    }

    return BOOL_VAL(true);
}


static Value str_to_ascii_upper(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    char* array = ALLOCATE_ARRAY(vm, char, str->length + 1);
    if (!array) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    memcpy(array, str->bytes, str->length + 1);

    for (size_t i = 0; i < str->length; i++) {
        if (array[i] >= 'a' && array[i] <= 'z') {
            array[i] -= 32;
        }
    }

    ObjStr* new_string = ObjStr_take(array, str->length, vm);
    if (!new_string) {
        FREE_ARRAY(vm, char, array, str->length + 1);
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_string);
}


static Value str_to_ascii_lower(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    char* array = ALLOCATE_ARRAY(vm, char, str->length + 1);
    if (!array) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    memcpy(array, str->bytes, str->length + 1);

    for (size_t i = 0; i < str->length; i++) {
        if (array[i] >= 'A' && array[i] <= 'Z') {
            array[i] += 32;
        }
    }

    ObjStr* new_string = ObjStr_take(array, str->length, vm);
    if (!new_string) {
        FREE_ARRAY(vm, char, array, str->length + 1);
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_string);
}


static Value str_starts_with(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :starts_with().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return BOOL_VAL(false);
    }

    if (memcmp(str->bytes, target->bytes, target->length) == 0) {
        return BOOL_VAL(true);
    }

    return BOOL_VAL(false);
}


static Value str_ends_with(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :ends_with().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return BOOL_VAL(false);
    }

    if (memcmp(&str->bytes[str->length - target->length], target->bytes, target->length) == 0) {
        return BOOL_VAL(true);
    }

    return BOOL_VAL(false);
}


static Value str_strip_prefix(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_prefix().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return OBJ_VAL(str);
    }

    if (memcmp(str->bytes, target->bytes, target->length) == 0) {
        ObjStr* new_str = ObjStr_copy_raw(&str->bytes[target->length], str->length - target->length, vm);
        if (!new_str) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(new_str);
    }

    return OBJ_VAL(str);
}


static Value str_strip_suffix(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_suffix().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return OBJ_VAL(str);
    }

    if (memcmp(&str->bytes[str->length - target->length], target->bytes, target->length) == 0) {
        ObjStr* new_str = ObjStr_copy_raw(str->bytes, str->length - target->length, vm);
        if (!new_str) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(new_str);
    }

    return OBJ_VAL(str);
}


static Value str_strip_prefix_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_prefix_bytes().");
        return NULL_VAL();
    }

    ObjStr* prefix = AS_STR(args[0]);

    if (prefix->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(prefix->bytes, c, prefix->length) == NULL) {
            break;
        }
        start++;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_suffix_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_suffix_bytes().");
        return NULL_VAL();
    }

    ObjStr* suffix = AS_STR(args[0]);

    if (suffix->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *(end - 1);
        if (memchr(suffix->bytes, c, suffix->length) == NULL) {
            break;
        }
        end--;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_bytes(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(target->bytes, c, target->length) == NULL) {
            break;
        }
        start++;
    }

    while (start < end) {
        char c = *(end - 1);
        if (memchr(target->bytes, c, target->length) == NULL) {
            break;
        }
        end--;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_ascii_ws(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    const char* whitespace = " \t\r\n\v\f";

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        start++;
    }

    while (start < end) {
        char c = *(end - 1);
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        end--;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_utf8_ws(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_is_unicode_whitespace(cp.value)) {
            break;
        }
        start += cp.length;
    }

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_is_unicode_whitespace(cp.value)) {
            break;
        }
        end -= cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_chars(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->length)) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Argument to :strip_chars() is not valid UTF-8.");
        return NULL_VAL();
    }

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        start += cp.length;
    }

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        end -= cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_suffix_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_suffix_chars(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->length)) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Argument to :strip_suffix_chars() is not valid UTF-8.");
        return NULL_VAL();
    }

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        end -= cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_prefix_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip_prefix_chars(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->length)) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Argument to :strip_prefix_chars() is not valid UTF-8.");
        return NULL_VAL();
    }

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        start += cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        return str_strip_ascii_ws(vm, arg_count, args);
    } else if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :strip(), expected a string.");
            return NULL_VAL();
        }
        return str_strip_bytes(vm, arg_count, args);
    } else {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments for :strip(), found %d.", arg_count);
        return NULL_VAL();
    }
}


static Value str_match(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :match().");
        return NULL_VAL();
    }
    ObjStr* target = AS_STR(args[0]);

    if (!IS_I64(args[1]) || args[1].as.i64 < 0) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :match().");
        return NULL_VAL();
    }
    size_t index = (size_t)args[1].as.i64;

    if (index + target->length > str->length) {
        return BOOL_VAL(false);
    }

    if (memcmp(&str->bytes[index], target->bytes, target->length) == 0) {
        return BOOL_VAL(true);
    }

    return BOOL_VAL(false);
}


static Value str_replace(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :replace().");
        return NULL_VAL();
    }
    ObjStr* old = AS_STR(args[0]);

    if (!IS_STR(args[1])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :replace().");
        return NULL_VAL();
    }
    ObjStr* new = AS_STR(args[1]);

    if (old->length == 0 || old->length > str->length) {
        return OBJ_VAL(str);
    }

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(buf)); // keep the buffer safe from the GC

    size_t index = 0;
    size_t last_possible_match_index = str->length - old->length;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], old->bytes, old->length) == 0) {
            if (!ObjBuf_append_bytes(buf, new->length, (uint8_t*)new->bytes, vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            index += old->length;
        } else {
            if (!ObjBuf_append_byte(buf, str->bytes[index], vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            index++;
        }
    }

    if (index < str->length) {
        if (!ObjBuf_append_bytes(buf, str->length - index, (uint8_t*)&str->bytes[index], vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
    }

    ObjStr* new_str = ObjBuf_to_str(buf, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    pyro_pop(vm); // pop the buffer
    return OBJ_VAL(new_str);
}


static Value str_index_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :index_of().");
        return NULL_VAL();
    }
    ObjStr* target = AS_STR(args[0]);

    if (!IS_I64(args[1]) || args[1].as.i64 < 0) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :index_of().");
        return NULL_VAL();
    }
    size_t index = (size_t)args[1].as.i64;

    if (index + target->length > str->length) {
        return OBJ_VAL(vm->empty_error);
    }

    size_t last_possible_match_index = str->length - target->length;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], target->bytes, target->length) == 0) {
            return I64_VAL((int64_t)index);
        }
        index++;
    }

    return OBJ_VAL(vm->empty_error);
}


static Value str_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :contains().");
        return NULL_VAL();
    }
    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return BOOL_VAL(false);
    }

    size_t index = 0;
    size_t last_possible_match_index = str->length - target->length;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], target->bytes, target->length) == 0) {
            return BOOL_VAL(true);
        }
        index++;
    }

    return BOOL_VAL(false);
}


static Value str_split(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :split().");
        return NULL_VAL();
    }
    ObjStr* sep = AS_STR(args[0]);

    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(vec));

    if (str->length < sep->length || sep->length == 0) {
        if (!ObjVec_append(vec, OBJ_VAL(str), vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        pyro_pop(vm);
        return OBJ_VAL(vec);
    }

    size_t start = 0;
    size_t current = 0;
    size_t last_possible_match_index = str->length - sep->length;

    while (current <= last_possible_match_index) {
        if (memcmp(&str->bytes[current], sep->bytes, sep->length) == 0) {
            ObjStr* new_string = ObjStr_copy_raw(&str->bytes[start], current - start, vm);
            if (!new_string) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            pyro_push(vm, OBJ_VAL(new_string));
            if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            pyro_pop(vm);
            current += sep->length;
            start = current;
        } else {
            current++;
        }
    }

    ObjStr* new_string = ObjStr_copy_raw(&str->bytes[start], str->length - start, vm);
    if (!new_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(new_string));
    if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_pop(vm);

    pyro_pop(vm); // pop the vector
    return OBJ_VAL(vec);
}


static Value str_split_on_ascii_ws(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(vec));

    const char* whitespace = " \t\r\n\v\f";

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        start++;
    }

    while (start < end) {
        char c = *(end - 1);
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        end--;
    }

    char* current = start;

    while (current < end) {
        if (memchr(whitespace, *current, 6) != NULL) {
            ObjStr* new_string = ObjStr_copy_raw(start, current - start, vm);
            if (!new_string) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            pyro_push(vm, OBJ_VAL(new_string));
            if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            pyro_pop(vm);
            while (memchr(whitespace, *current, 6) != NULL) {
                current++;
            }
            start = current;
        } else {
            current++;
        }
    }

    ObjStr* new_string = ObjStr_copy_raw(start, current - start, vm);
    if (!new_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(new_string));
    if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_pop(vm);

    pyro_pop(vm); // pop the vector
    return OBJ_VAL(vec);
}


static Value str_to_hex(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(buf));

    for (size_t i = 0; i < str->length; i++) {
        if (!ObjBuf_append_hex_escaped_byte(buf, str->bytes[i], vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
    }

    ObjStr* output_string = ObjBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    pyro_pop(vm);
    return OBJ_VAL(output_string);
}


static Value str_slice(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!(arg_count == 1 || arg_count == 2)) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or 2 arguments for :slice(), found %i.", arg_count);
        return NULL_VAL();
    }

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument type, start_index must be an integer.");
        return NULL_VAL();
    }

    size_t start_index;
    if (args[0].as.i64 >= 0 && (size_t)args[0].as.i64 <= str->length) {
        start_index = (size_t)args[0].as.i64;
    } else if (args[0].as.i64 < 0 && (size_t)(args[0].as.i64 * -1) <= str->length) {
        start_index = (size_t)((int64_t)str->length + args[0].as.i64);
    } else {
        pyro_panic(vm, ERR_VALUE_ERROR, "Invalid argument value, start_index is out of range.");
        return NULL_VAL();
    }

    size_t length = str->length - start_index;
    if (arg_count == 2) {
        if (!IS_I64(args[1])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument type, length must be an integer.");
            return NULL_VAL();
        }
        if (args[1].as.i64 < 0) {
            pyro_panic(vm, ERR_VALUE_ERROR, "Invalid argument value, length cannot be negative.");
            return NULL_VAL();
        }
        if (start_index + (size_t)args[1].as.i64 > str->length) {
            pyro_panic(vm, ERR_VALUE_ERROR, "Invalid argument value, length is out of range.");
            return NULL_VAL();
        }
        length = (size_t)args[1].as.i64;
    }

    if (length == 0) {
        return OBJ_VAL(vm->empty_string);
    }

    ObjStr* new_str = ObjStr_copy_raw(&str->bytes[start_index], length, vm);
    if (!new_str) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_join(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    // Does the argument have an :$iter() method?
    Value iter_method = pyro_get_method(vm, args[0], vm->str_iter);
    if (IS_NULL(iter_method)) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Argument to :join() is not iterable.");
        return NULL_VAL();
    }

    // Call the :$iter() method to get an iterator.
    pyro_push(vm, args[0]); // receiver for the :$iter() method call
    Value iterator = pyro_call_method(vm, iter_method, 0);
    if (vm->halt_flag) {
        return NULL_VAL();
    }
    pyro_push(vm, iterator); // protect from GC

    // Get the iterator's :$next() method.
    Value next_method = pyro_get_method(vm, iterator, vm->str_next);
    if (IS_NULL(next_method)) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid iterator -- no :$next() method.");
        return NULL_VAL();
    }
    pyro_push(vm, next_method); // protect from GC

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(buf)); // protect from GC

    bool is_first_item = true;

    while (true) {
        pyro_push(vm, iterator); // receiver for the :$next() method call
        Value next_value = pyro_call_method(vm, next_method, 0);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (IS_ERR(next_value)) {
            break;
        }

        if (!is_first_item) {
            if (!ObjBuf_append_bytes(buf, str->length, (uint8_t*)str->bytes, vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
        }

        pyro_push(vm, next_value);
        ObjStr* value_string = pyro_stringify_value(vm, next_value);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        pyro_pop(vm); // next_value

        pyro_push(vm, OBJ_VAL(value_string));
        if (!ObjBuf_append_bytes(buf, value_string->length, (uint8_t*)value_string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        pyro_pop(vm); // value_string

        is_first_item = false;
    }

    ObjStr* output_string = ObjBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    pyro_pop(vm); // buf
    pyro_pop(vm); // next_method
    pyro_pop(vm); // iterator

    return OBJ_VAL(output_string);
}


void pyro_load_std_core_str(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$str", fn_str, 1);
    pyro_define_global_fn(vm, "$is_str", fn_is_str, 1);

    // Methdods.
    pyro_define_method(vm, vm->str_class, "is_utf8", str_is_utf8, 0);
    pyro_define_method(vm, vm->str_class, "is_ascii", str_is_ascii, 0);
    pyro_define_method(vm, vm->str_class, "byte", str_byte, 1);
    pyro_define_method(vm, vm->str_class, "bytes", str_bytes, 0);
    pyro_define_method(vm, vm->str_class, "byte_count", str_byte_count, 0);
    pyro_define_method(vm, vm->str_class, "count", str_byte_count, 0);
    pyro_define_method(vm, vm->str_class, "is_empty", str_is_empty, 0);
    pyro_define_method(vm, vm->str_class, "char", str_char, 1);
    pyro_define_method(vm, vm->str_class, "chars", str_chars, 0);
    pyro_define_method(vm, vm->str_class, "char_count", str_char_count, 0);
    pyro_define_method(vm, vm->str_class, "to_ascii_upper", str_to_ascii_upper, 0);
    pyro_define_method(vm, vm->str_class, "to_ascii_lower", str_to_ascii_lower, 0);
    pyro_define_method(vm, vm->str_class, "starts_with", str_starts_with, 1);
    pyro_define_method(vm, vm->str_class, "ends_with", str_ends_with, 1);
    pyro_define_method(vm, vm->str_class, "strip_prefix", str_strip_prefix, 1);
    pyro_define_method(vm, vm->str_class, "strip_suffix", str_strip_suffix, 1);
    pyro_define_method(vm, vm->str_class, "strip_prefix_bytes", str_strip_prefix_bytes, 1);
    pyro_define_method(vm, vm->str_class, "strip_suffix_bytes", str_strip_suffix_bytes, 1);
    pyro_define_method(vm, vm->str_class, "strip_bytes", str_strip_bytes, 1);
    pyro_define_method(vm, vm->str_class, "strip_chars", str_strip_chars, 1);
    pyro_define_method(vm, vm->str_class, "strip_prefix_chars", str_strip_prefix_chars, 1);
    pyro_define_method(vm, vm->str_class, "strip_suffix_chars", str_strip_suffix_chars, 1);
    pyro_define_method(vm, vm->str_class, "strip", str_strip, -1);
    pyro_define_method(vm, vm->str_class, "strip_ascii_ws", str_strip_ascii_ws, 0);
    pyro_define_method(vm, vm->str_class, "strip_utf8_ws", str_strip_utf8_ws, 0);
    pyro_define_method(vm, vm->str_class, "match", str_match, 2);
    pyro_define_method(vm, vm->str_class, "replace", str_replace, 2);
    pyro_define_method(vm, vm->str_class, "index_of", str_index_of, 2);
    pyro_define_method(vm, vm->str_class, "contains", str_contains, 1);
    pyro_define_method(vm, vm->str_class, "split", str_split, 1);
    pyro_define_method(vm, vm->str_class, "split_on_ascii_ws", str_split_on_ascii_ws, 0);
    pyro_define_method(vm, vm->str_class, "to_hex", str_to_hex, 0);
    pyro_define_method(vm, vm->str_class, "slice", str_slice, -1);
    pyro_define_method(vm, vm->str_class, "join", str_join, 1);
}
