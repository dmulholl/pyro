#include "../../inc/pyro.h"


static PyroValue fn_str(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* string = pyro_stringify_value(vm, args[0]);
    if (vm->halt_flag) {
        return pyro_null();
    }
    return pyro_obj(string);
}


static PyroValue fn_is_str(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_STR(args[0]));
}


static PyroValue str_is_empty(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    return pyro_bool(str->count == 0);
}


static PyroValue str_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    PyroIter* iter = PyroIter_new((PyroObject*)str, PYRO_ITER_STR, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue str_byte_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    return pyro_i64(str->count);
}


static PyroValue str_byte(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (PYRO_IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < str->count) {
            return pyro_i64((uint8_t)str->bytes[index]);
        }
        pyro_panic(vm, "byte(): invalid argument [index], integer is out of range");
        return pyro_null();
    }
    pyro_panic(vm, "byte(): invalid argument [index], expected an integer");
    return pyro_null();
}


static PyroValue str_bytes(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    PyroIter* iter = PyroIter_new((PyroObject*)str, PYRO_ITER_STR_BYTES, vm);
    if (!iter) {
        pyro_panic(vm, "bytes(): out of memory");
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue str_lines(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    PyroIter* iter = PyroIter_new((PyroObject*)str, PYRO_ITER_STR_LINES, vm);
    if (!iter) {
        pyro_panic(vm, "lines(): out of memory");
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue str_is_ascii(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < str->count; i++) {
        if (str->bytes[i] & 0x80) {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue str_is_ascii_ws(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < str->count; i++) {
        switch (str->bytes[i]) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\v':
            case '\f':
                break;
            default:
                return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue str_is_utf8_ws(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_bool(false);
    }

    size_t byte_index = 0;
    Utf8CodePoint cp;

    while (byte_index < str->count) {
        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->count - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            byte_index += cp.length;
            if (!pyro_is_unicode_whitespace(cp.value)) {
                return pyro_bool(false);
            }
        } else {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue str_chars(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    PyroIter* iter = PyroIter_new((PyroObject*)str, PYRO_ITER_STR_CHARS, vm);
    if (!iter) {
        pyro_panic(vm, "chars(): out of memory");
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue str_char(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "char(): invalid argument [index], expected an integer");
        return pyro_null();
    }

    int64_t target_index = args[0].as.i64;
    if (target_index < 0 || str->count == 0) {
        pyro_panic(vm, "char(): invalid argument [index], out of range");
        return pyro_null();
    }

    size_t byte_index = 0;
    size_t char_count = 0;
    Utf8CodePoint cp;

    while (char_count < (size_t)target_index + 1) {
        if (byte_index == str->count) {
            pyro_panic(vm, "char(): invalid argument [index], out of range");
            return pyro_null();
        }

        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->count - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            char_count++;
            byte_index += cp.length;
        } else {
            pyro_panic(vm, "$char(): string contains invalid utf-8 at byte index %zu", byte_index);
            return pyro_null();
        }
    }

    return pyro_char(cp.value);
}


static PyroValue str_char_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    size_t byte_index = 0;
    size_t char_count = 0;
    Utf8CodePoint cp;

    while (byte_index < str->count) {
        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->count - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            char_count++;
            byte_index += cp.length;
        } else {
            pyro_panic(vm, "char_count(): string contains invalid utf-8 at byte index %zu", byte_index);
            return pyro_null();
        }
    }

    return pyro_i64(char_count);
}


static PyroValue str_is_utf8(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_bool(false);
    }

    size_t byte_index = 0;
    Utf8CodePoint cp;

    while (byte_index < str->count) {
        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->count - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            byte_index += cp.length;
        } else {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue str_to_ascii_upper(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_obj(str);
    }

    char* array = PYRO_ALLOCATE_ARRAY(vm, char, str->capacity);
    if (!array) {
        pyro_panic(vm, "to_ascii_upper(): out of memory");
        return pyro_null();
    }

    memcpy(array, str->bytes, str->capacity);
    array[str->count] = '\0';

    for (size_t i = 0; i < str->count; i++) {
        if (array[i] >= 'a' && array[i] <= 'z') {
            array[i] -= 32;
        }
    }

    PyroStr* new_string = PyroStr_take(array, str->count, str->capacity, vm);
    if (!new_string) {
        PYRO_FREE_ARRAY(vm, char, array, str->capacity);
        pyro_panic(vm, "to_ascii_uper(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_string);
}


static PyroValue str_to_ascii_lower(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_obj(str);
    }

    char* array = PYRO_ALLOCATE_ARRAY(vm, char, str->capacity);
    if (!array) {
        pyro_panic(vm, "to_ascii_lower(): out of memory");
        return pyro_null();
    }

    memcpy(array, str->bytes, str->capacity);
    array[str->count] = '\0';

    for (size_t i = 0; i < str->count; i++) {
        if (array[i] >= 'A' && array[i] <= 'Z') {
            array[i] += 32;
        }
    }

    PyroStr* new_string = PyroStr_take(array, str->count, str->capacity, vm);
    if (!new_string) {
        PYRO_FREE_ARRAY(vm, char, array, str->capacity);
        pyro_panic(vm, "to_ascii_lower(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_string);
}


static PyroValue str_starts_with(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "starts_with(): invalid argument [prefix], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (str->count < target->count) {
        return pyro_bool(false);
    }

    if (memcmp(str->bytes, target->bytes, target->count) == 0) {
        return pyro_bool(true);
    }

    return pyro_bool(false);
}


static PyroValue str_ends_with(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "ends_with(): invalid argument [suffix], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (str->count < target->count) {
        return pyro_bool(false);
    }

    if (memcmp(&str->bytes[str->count - target->count], target->bytes, target->count) == 0) {
        return pyro_bool(true);
    }

    return pyro_bool(false);
}


static PyroValue str_strip_prefix(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_prefix(): invalid argument [prefix], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (str->count < target->count) {
        return pyro_obj(str);
    }

    if (memcmp(str->bytes, target->bytes, target->count) == 0) {
        PyroStr* new_str = PyroStr_copy(&str->bytes[target->count], str->count - target->count, false, vm);
        if (!new_str) {
            pyro_panic(vm, "strip_prefix(): out of memory");
            return pyro_null();
        }
        return pyro_obj(new_str);
    }

    return pyro_obj(str);
}


static PyroValue str_strip_suffix(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_suffix(): invalid argument [suffix], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (str->count < target->count) {
        return pyro_obj(str);
    }

    if (memcmp(&str->bytes[str->count - target->count], target->bytes, target->count) == 0) {
        PyroStr* new_str = PyroStr_copy(str->bytes, str->count - target->count, false, vm);
        if (!new_str) {
            pyro_panic(vm, "strip_suffix(): out of memory");
            return pyro_null();
        }
        return pyro_obj(new_str);
    }

    return pyro_obj(str);
}


static PyroValue str_strip_prefix_bytes(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_prefix_bytes(), invalid argument [bytes], expected a string");
        return pyro_null();
    }

    PyroStr* prefix = PYRO_AS_STR(args[0]);

    if (prefix->count == 0 || str->count == 0) {
        return pyro_obj(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->count;

    while (start < end) {
        char c = *start;
        if (memchr(prefix->bytes, c, prefix->count) == NULL) {
            break;
        }
        start++;
    }

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_prefix_bytes(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip_suffix_bytes(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_suffix_bytes(): invalid argument [bytes], expected a string");
        return pyro_null();
    }

    PyroStr* suffix = PYRO_AS_STR(args[0]);

    if (suffix->count == 0 || str->count == 0) {
        return pyro_obj(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->count;

    while (start < end) {
        char c = *(end - 1);
        if (memchr(suffix->bytes, c, suffix->count) == NULL) {
            break;
        }
        end--;
    }

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_suffix_bytes(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip_bytes(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_bytes(): invalid argument [bytes], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (target->count == 0 || str->count == 0) {
        return pyro_obj(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->count;

    while (start < end) {
        char c = *start;
        if (memchr(target->bytes, c, target->count) == NULL) {
            break;
        }
        start++;
    }

    while (start < end) {
        char c = *(end - 1);
        if (memchr(target->bytes, c, target->count) == NULL) {
            break;
        }
        end--;
    }

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_bytes(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip_ascii_ws(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_obj(str);
    }

    const char* whitespace = " \t\r\n\v\f";

    char* start = str->bytes;
    char* end = str->bytes + str->count;

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

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_ascii_ws(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip_utf8_ws(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_obj(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->count;

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

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_utf8_ws(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip_chars(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_chars(): invalid argument [chars], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->count)) {
        pyro_panic(vm, "strip_chars(): invalid argument [chars], not valid UTF-8");
        return pyro_null();
    }

    if (target->count == 0 || str->count == 0) {
        return pyro_obj(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->count;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->count, cp.value)) {
            break;
        }
        start += cp.length;
    }

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->count, cp.value)) {
            break;
        }
        end -= cp.length;
    }

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_chars(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip_suffix_chars(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_suffix_chars(): invalid argument [chars], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->count)) {
        pyro_panic(vm, "strip_suffix_chars(): invalid argument [chars], not valid UTF-8");
        return pyro_null();
    }

    if (target->count == 0 || str->count == 0) {
        return pyro_obj(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->count;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->count, cp.value)) {
            break;
        }
        end -= cp.length;
    }

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_suffix_chars(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip_prefix_chars(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "strip_prefix_chars(): invalid argument [prefix], expected a string");
        return pyro_null();
    }

    PyroStr* target = PYRO_AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->count)) {
        pyro_panic(vm, "strip_prefix_chars(): invalid argument [prefix], not valid UTF-8");
        return pyro_null();
    }

    if (target->count == 0 || str->count == 0) {
        return pyro_obj(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->count;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->count, cp.value)) {
            break;
        }
        start += cp.length;
    }

    PyroStr* new_str = PyroStr_copy(start, end - start, false, vm);
    if (!new_str) {
        pyro_panic(vm, "strip_prefix_chars(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_strip(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        return str_strip_ascii_ws(vm, arg_count, args);
    }

    if (arg_count == 1) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "strip(): invalid argument [bytes], expected a string");
            return pyro_null();
        }
        return str_strip_bytes(vm, arg_count, args);
    }

    pyro_panic(vm, "strip(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue str_match(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "match(): invalid argument [target], expected a string");
        return pyro_null();
    }
    PyroStr* target = PYRO_AS_STR(args[0]);

    if (!PYRO_IS_I64(args[1]) || args[1].as.i64 < 0) {
        pyro_panic(vm, "match(): invalid argument [index], expected a positive integer");
        return pyro_null();
    }
    size_t index = (size_t)args[1].as.i64;

    if (index + target->count > str->count) {
        return pyro_bool(false);
    }

    if (memcmp(&str->bytes[index], target->bytes, target->count) == 0) {
        return pyro_bool(true);
    }

    return pyro_bool(false);
}


static PyroValue str_replace(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "replace(): invalid argument [old], expected a string");
        return pyro_null();
    }
    PyroStr* old = PYRO_AS_STR(args[0]);

    if (!PYRO_IS_STR(args[1])) {
        pyro_panic(vm, "replace(): invalid argument [new], expected a string");
        return pyro_null();
    }
    PyroStr* new = PYRO_AS_STR(args[1]);

    if (old->count == 0 || old->count > str->count) {
        return pyro_obj(str);
    }

    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "replace(): out of memory");
        return pyro_null();
    }

    size_t index = 0;
    size_t last_possible_match_index = str->count - old->count;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], old->bytes, old->count) == 0) {
            if (!PyroBuf_append_bytes(buf, new->count, (uint8_t*)new->bytes, vm)) {
                pyro_panic(vm, "replace(): out of memory");
                return pyro_null();
            }
            index += old->count;
        } else {
            if (!PyroBuf_append_byte(buf, str->bytes[index], vm)) {
                pyro_panic(vm, "replace(): out of memory");
                return pyro_null();
            }
            index++;
        }
    }

    if (index < str->count) {
        if (!PyroBuf_append_bytes(buf, str->count - index, (uint8_t*)&str->bytes[index], vm)) {
            pyro_panic(vm, "replace(): out of memory");
            return pyro_null();
        }
    }

    PyroStr* new_str = PyroBuf_to_str(buf, vm);
    if (!new_str) {
        pyro_panic(vm, "replace(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_index_of(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (arg_count == 0 || arg_count > 2) {
        pyro_panic(vm, "index_of(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_null();
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "index_of(): invalid argument [target], expected a string");
        return pyro_null();
    }
    PyroStr* target = PYRO_AS_STR(args[0]);

    size_t index = 0;
    if (arg_count == 2) {
        if (!PYRO_IS_I64(args[1])) {
            pyro_panic(vm, "index_of(): invalid argument [start_index], expected an integer");
            return pyro_null();
        }
        if (args[1].as.i64 < 0 || (size_t)args[1].as.i64 > str->count) {
            pyro_panic(vm, "index_of(): invalid argument [start_index], integer is out of range");
            return pyro_null();
        }
        index = (size_t)args[1].as.i64;
    }

    if (index + target->count > str->count) {
        return pyro_obj(vm->error);
    }

    size_t last_possible_match_index = str->count - target->count;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], target->bytes, target->count) == 0) {
            return pyro_i64((int64_t)index);
        }
        index++;
    }

    return pyro_obj(vm->error);
}


static PyroValue str_contains(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    char* target;
    size_t target_length;
    uint8_t codepoint_buffer[4];

    if (PYRO_IS_STR(args[0])) {
        target = PYRO_AS_STR(args[0])->bytes;
        target_length = PYRO_AS_STR(args[0])->count;
    } else if (PYRO_IS_CHAR(args[0])) {
        target = (char*)codepoint_buffer;
        target_length = pyro_write_utf8_codepoint(args[0].as.u32, codepoint_buffer);
    } else {
        pyro_panic(vm, "contains(): invalid argument [target], expected a string or char");
        return pyro_null();
    }

    if (str->count < target_length) {
        return pyro_bool(false);
    }

    size_t index = 0;
    size_t last_possible_match_index = str->count - target_length;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], target, target_length) == 0) {
            return pyro_bool(true);
        }
        index++;
    }

    return pyro_bool(false);
}


static PyroValue str_split_on_ascii_ws(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    PyroVec* vec = PyroVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "split_on_ascii_ws(): out of memory");
        return pyro_null();
    }

    const char* whitespace = " \t\r\n\v\f";

    char* start = str->bytes;
    char* end = str->bytes + str->count;

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
            PyroStr* new_string = PyroStr_copy(start, current - start, false, vm);
            if (!new_string) {
                pyro_panic(vm, "split_on_ascii_ws(): out of memory");
                return pyro_null();
            }
            if (!PyroVec_append(vec, pyro_obj(new_string), vm)) {
                pyro_panic(vm, "split_on_ascii_ws(): out of memory");
                return pyro_null();
            }
            while (memchr(whitespace, *current, 6) != NULL) {
                current++;
            }
            start = current;
        } else {
            current++;
        }
    }

    PyroStr* new_string = PyroStr_copy(start, current - start, false, vm);
    if (!new_string) {
        pyro_panic(vm, "split_on_ascii_ws(): out of memory");
        return pyro_null();
    }

    if (!PyroVec_append(vec, pyro_obj(new_string), vm)) {
        pyro_panic(vm, "split_on_ascii_ws(): out of memory");
        return pyro_null();
    }

    return pyro_obj(vec);
}


static PyroValue str_split(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        return str_split_on_ascii_ws(vm, arg_count, args);
    }

    if (arg_count > 1) {
        pyro_panic(vm, "split(): expected 0 or 1 arguments, found %zu", arg_count);
        return pyro_null();
    }

    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "split(): invalid argument [sep], expected a string");
        return pyro_null();
    }
    PyroStr* sep = PYRO_AS_STR(args[0]);

    PyroVec* vec = PyroVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "split(): out of memory");
        return pyro_null();
    }

    if (str->count < sep->count || sep->count == 0) {
        if (!PyroVec_append(vec, pyro_obj(str), vm)) {
            pyro_panic(vm, "split(): out of memory");
            return pyro_null();
        }
        return pyro_obj(vec);
    }

    size_t start = 0;
    size_t current = 0;
    size_t last_possible_match_index = str->count - sep->count;

    while (current <= last_possible_match_index) {
        if (memcmp(&str->bytes[current], sep->bytes, sep->count) == 0) {
            PyroStr* new_string = PyroStr_copy(&str->bytes[start], current - start, false, vm);
            if (!new_string) {
                pyro_panic(vm, "split(): out of memory");
                return pyro_null();
            }
            if (!PyroVec_append(vec, pyro_obj(new_string), vm)) {
                pyro_panic(vm, "split(): out of memory");
                return pyro_null();
            }
            current += sep->count;
            start = current;
        } else {
            current++;
        }
    }

    PyroStr* new_string = PyroStr_copy(&str->bytes[start], str->count - start, false, vm);
    if (!new_string) {
        pyro_panic(vm, "split(): out of memory");
        return pyro_null();
    }

    if (!PyroVec_append(vec, pyro_obj(new_string), vm)) {
        pyro_panic(vm, "split(): out of memory");
        return pyro_null();
    }

    return pyro_obj(vec);
}


static PyroValue str_to_hex(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_obj(str);
    }

    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "to_hex(): out of memory");
        return pyro_null();
    }

    for (size_t i = 0; i < str->count; i++) {
        if (!PyroBuf_append_hex_escaped_byte(buf, str->bytes[i], vm)) {
            pyro_panic(vm, "to_hex(): out of memory");
            return pyro_null();
        }
    }

    PyroStr* output_string = PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "to_hex(): out of memory");
        return pyro_null();
    }

    return pyro_obj(output_string);
}


static PyroValue str_slice(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    if (!(arg_count == 1 || arg_count == 2)) {
        pyro_panic(vm, "slice(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "slice(): invalid argument [start_index], expected an integer");
        return pyro_null();
    }

    size_t start_index;
    if (args[0].as.i64 >= 0 && (size_t)args[0].as.i64 <= str->count) {
        start_index = (size_t)args[0].as.i64;
    } else if (args[0].as.i64 < 0 && (size_t)(args[0].as.i64 * -1) <= str->count) {
        start_index = (size_t)((int64_t)str->count + args[0].as.i64);
    } else {
        pyro_panic(vm, "slice(): invalid argument [start_index], out of range");
        return pyro_null();
    }

    size_t length = str->count - start_index;
    if (arg_count == 2) {
        if (!PYRO_IS_I64(args[1])) {
            pyro_panic(vm, "slice(): invalid argument [length], expected an integer");
            return pyro_null();
        }
        if (args[1].as.i64 < 0) {
            pyro_panic(vm, "slice(): invalid argument [length], expected a positive integer");
            return pyro_null();
        }
        if (start_index + (size_t)args[1].as.i64 > str->count) {
            pyro_panic(vm, "slice(): invalid argument [length], out of range");
            return pyro_null();
        }
        length = (size_t)args[1].as.i64;
    }

    if (length == 0) {
        return pyro_obj(vm->empty_string);
    }

    PyroStr* new_str = PyroStr_copy(&str->bytes[start_index], length, false, vm);
    if (!new_str) {
        pyro_panic(vm, "slice(): out of memory");
        return pyro_null();
    }

    return pyro_obj(new_str);
}


static PyroValue str_join(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);

    // Does the argument have an :$iter() method?
    PyroValue iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
    if (PYRO_IS_NULL(iter_method)) {
        pyro_panic(vm, "join(): invalid argument [items], expected an iterable object");
        return pyro_null();
    }

    // Call the :$iter() method to get an iterator.
    pyro_push(vm, args[0]); // receiver for the :$iter() method call
    PyroValue iterator = pyro_call_method(vm, iter_method, 0);
    if (vm->halt_flag) {
        return pyro_null();
    }
    pyro_push(vm, iterator); // protect from GC

    // Get the iterator's :$next() method.
    PyroValue next_method = pyro_get_method(vm, iterator, vm->str_dollar_next);
    if (PYRO_IS_NULL(next_method)) {
        pyro_panic(vm, "join(): invalid argument [items], iterator has no :$next() method");
        return pyro_null();
    }
    pyro_push(vm, next_method); // protect from GC

    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "join(): out of memory");
        return pyro_null();
    }
    pyro_push(vm, pyro_obj(buf)); // protect from GC

    bool is_first_item = true;

    while (true) {
        pyro_push(vm, iterator); // receiver for the :$next() method call
        PyroValue next_value = pyro_call_method(vm, next_method, 0);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (PYRO_IS_ERR(next_value)) {
            break;
        }

        if (!is_first_item) {
            if (!PyroBuf_append_bytes(buf, str->count, (uint8_t*)str->bytes, vm)) {
                pyro_panic(vm, "join(): out of memory");
                return pyro_null();
            }
        }

        pyro_push(vm, next_value);
        PyroStr* value_string = pyro_stringify_value(vm, next_value);
        if (vm->halt_flag) {
            return pyro_null();
        }
        pyro_pop(vm); // next_value

        pyro_push(vm, pyro_obj(value_string));
        if (!PyroBuf_append_bytes(buf, value_string->count, (uint8_t*)value_string->bytes, vm)) {
            pyro_panic(vm, "join(): out of memory");
            return pyro_null();
        }
        pyro_pop(vm); // value_string

        is_first_item = false;
    }

    PyroStr* output_string = PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "join(): out of memory");
        return pyro_null();
    }

    pyro_pop(vm); // buf
    pyro_pop(vm); // next_method
    pyro_pop(vm); // iterator

    return pyro_obj(output_string);
}


static PyroValue str_is_ascii_decimal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < str->count; i++) {
        char c = str->bytes[i];
        if (c < '0' || c > '9') {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue str_is_ascii_octal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < str->count; i++) {
        char c = str->bytes[i];
        if (c < '0' || c > '7') {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue str_is_ascii_hex(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* str = PYRO_AS_STR(args[-1]);
    if (str->count == 0) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < str->count; i++) {
        char c = str->bytes[i];
        if (!pyro_is_hex_digit(c)) {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


void pyro_load_std_builtins_str(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$str", fn_str, 1);
    pyro_define_superglobal_fn(vm, "$is_str", fn_is_str, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_str, "$contains", str_contains, 1);
    pyro_define_pri_method(vm, vm->class_str, "$iter", str_iter, 0);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_str, "is_utf8", str_is_utf8, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_ascii", str_is_ascii, 0);
    pyro_define_pub_method(vm, vm->class_str, "iter", str_iter, 0);
    pyro_define_pub_method(vm, vm->class_str, "byte", str_byte, 1);
    pyro_define_pub_method(vm, vm->class_str, "bytes", str_bytes, 0);
    pyro_define_pub_method(vm, vm->class_str, "byte_count", str_byte_count, 0);
    pyro_define_pub_method(vm, vm->class_str, "count", str_byte_count, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_empty", str_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_str, "char", str_char, 1);
    pyro_define_pub_method(vm, vm->class_str, "chars", str_chars, 0);
    pyro_define_pub_method(vm, vm->class_str, "char_count", str_char_count, 0);
    pyro_define_pub_method(vm, vm->class_str, "to_ascii_upper", str_to_ascii_upper, 0);
    pyro_define_pub_method(vm, vm->class_str, "to_upper", str_to_ascii_upper, 0);
    pyro_define_pub_method(vm, vm->class_str, "to_ascii_lower", str_to_ascii_lower, 0);
    pyro_define_pub_method(vm, vm->class_str, "to_lower", str_to_ascii_lower, 0);
    pyro_define_pub_method(vm, vm->class_str, "starts_with", str_starts_with, 1);
    pyro_define_pub_method(vm, vm->class_str, "ends_with", str_ends_with, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_prefix", str_strip_prefix, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_suffix", str_strip_suffix, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_prefix_bytes", str_strip_prefix_bytes, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_suffix_bytes", str_strip_suffix_bytes, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_bytes", str_strip_bytes, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_chars", str_strip_chars, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_prefix_chars", str_strip_prefix_chars, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip_suffix_chars", str_strip_suffix_chars, 1);
    pyro_define_pub_method(vm, vm->class_str, "strip", str_strip, -1);
    pyro_define_pub_method(vm, vm->class_str, "strip_ascii_ws", str_strip_ascii_ws, 0);
    pyro_define_pub_method(vm, vm->class_str, "strip_utf8_ws", str_strip_utf8_ws, 0);
    pyro_define_pub_method(vm, vm->class_str, "match", str_match, 2);
    pyro_define_pub_method(vm, vm->class_str, "replace", str_replace, 2);
    pyro_define_pub_method(vm, vm->class_str, "index_of", str_index_of, -1);
    pyro_define_pub_method(vm, vm->class_str, "contains", str_contains, 1);
    pyro_define_pub_method(vm, vm->class_str, "split", str_split, -1);
    pyro_define_pub_method(vm, vm->class_str, "split_on_ascii_ws", str_split_on_ascii_ws, 0);
    pyro_define_pub_method(vm, vm->class_str, "to_hex", str_to_hex, 0);
    pyro_define_pub_method(vm, vm->class_str, "slice", str_slice, -1);
    pyro_define_pub_method(vm, vm->class_str, "join", str_join, 1);
    pyro_define_pub_method(vm, vm->class_str, "lines", str_lines, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_utf8_ws", str_is_utf8_ws, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_ascii_ws", str_is_ascii_ws, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_ws", str_is_ascii_ws, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_ascii_decimal", str_is_ascii_decimal, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_ascii_octal", str_is_ascii_octal, 0);
    pyro_define_pub_method(vm, vm->class_str, "is_ascii_hex", str_is_ascii_hex, 0);
}
