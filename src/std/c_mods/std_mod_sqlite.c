#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../lib/sqlite/sqlite3.h"


void pyro_load_std_mod_sqlite(PyroVM* vm, ObjModule* module) {
    ObjStr* version = ObjStr_new(SQLITE_VERSION, vm);
    if (!version) {
        return;
    }
    pyro_define_pub_member(vm, module, "version", pyro_make_obj(version));
}
