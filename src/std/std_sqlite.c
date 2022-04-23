#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/setup.h"
#include "../vm/panics.h"
#include "../lib/sqlite/sqlite3.h"


void pyro_load_std_mod_sqlite(PyroVM* vm, ObjModule* module) {
    ObjStr* version = STR(SQLITE_VERSION);
    if (!version) {
        return;
    }
    pyro_define_member(vm, module, "version", MAKE_OBJ(version));
}
