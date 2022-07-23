#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../lib/sqlite/sqlite3.h"


void pyro_load_std_mod_sqlite(PyroVM* vm, ObjModule* module) {
    ObjStr* version = STR(SQLITE_VERSION);
    if (!version) {
        return;
    }
    pyro_define_member(vm, module, "version", MAKE_OBJ(version));
}
