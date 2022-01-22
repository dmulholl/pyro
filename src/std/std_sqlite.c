#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/setup.h"
#include "../vm/panics.h"
#include "../lib/sqlite/sqlite3.h"


void pyro_load_std_sqlite(PyroVM* vm) {
    ObjModule* mod_sqlite = pyro_define_module_2(vm, "$std", "sqlite");
    if (!mod_sqlite) {
        return;
    }

    ObjStr* version = STR(SQLITE_VERSION);
    if (!version) {
        return;
    }

    pyro_define_member(vm, mod_sqlite, "version", MAKE_OBJ(version));

    /* pyro_define_member_fn(vm, mod_prng, "test_mt64", fn_test_mt64, 0); */
}

