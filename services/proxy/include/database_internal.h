#ifndef UVA_PROXY_DATABASE_INTERNAL_H
#define UVA_PROXY_DATABASE_INTERNAL_H

#include "sqlite3.h"
#include <pthread.h>

/* Global database handle and mutex, defined in database.c */
extern sqlite3       *g_db;
extern pthread_mutex_t g_db_lock;

void db_lock(void);
void db_unlock(void);

#endif /* UVA_PROXY_DATABASE_INTERNAL_H */
