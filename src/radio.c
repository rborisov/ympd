#include "radio.h"

char* cfg_get_db_path()
{
    char *db_path = NULL;

    if (!(&rcm.cfg)) {
        fprintf(stderr, "%s: rcm.cfg is NULL\n", __func__);
        return NULL;
    }

    if (!config_lookup_string(&rcm.cfg, "application.db_path", &db_path))
    {
        fprintf(stderr, "%s: No 'application.db_path' setting in configuration file.\n", __func__);
        //TODO: fill the default db_path to config
    }

    return db_path;
}
