#include "os.h"
#include "heap.h"
#include "vm.h"

// These headers are POSIX only.
#include <dirent.h>
#include <unistd.h>


bool pyro_file_exists(const char* path) {
    return access(path, F_OK) == 0;
}


bool pyro_dir_exists(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}


bool pyro_run_shell_cmd(PyroVM* vm, const char* cmd, CmdResult* out) {
    FILE* file = popen(cmd, "r");
    if (!file) {
        pyro_panic(vm, "Failed to run comand.");
        return false;
    }

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 >= capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, "Out of memory.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pclose(file);
                return false;
            }
            capacity = new_capacity;
            array = new_array;
        }

        size_t max_bytes = capacity - count - 1;
        size_t bytes_read = fread(&array[count], sizeof(uint8_t), max_bytes, file);
        count += bytes_read;

        if (bytes_read < max_bytes) {
            if (ferror(file)) {
                pyro_panic(vm, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pclose(file);
                return false;
            }
            break; // EOF
        }
    }

    int exit_code = pclose(file);

    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }
    array[count] = '\0';

    ObjStr* output = ObjStr_take((char*)array, count, vm);
    if (!output) {
        pyro_panic(vm, "Out of memory.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return false;
    }

    *out = (CmdResult) {
        .output = output,
        .exit_code = exit_code
    };

    return true;
}

