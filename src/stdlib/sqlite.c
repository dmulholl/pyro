#include "../includes/pyro.h"
#include "../../lib/sqlite/sqlite3.h"


void pyro_load_std_mod_sqlite(PyroVM* vm, PyroMod* module) {
    /* PyroStr* version = PyroStr_COPY(SQLITE_VERSION); */
    /* if (!version) { */
    /*     return; */
    /* } */
    /* pyro_define_pub_member(vm, module, "version", pyro_obj(version)); */
}
