#include "app_db.h"


#define PATH "./test.db"

typedef enum {
    DB_OPE_INSERT,
    DB_OPE_REPLACE,
    DB_OPE_SEARCH,
    DB_OPE_DELETE
}db_ope_type_e;


typedef enum {
    DB_NULL,
    DB_INIT_SUCCESS,
    DB_INIT_ERROR
}db_type_e;


typedef struct {
    db_t *db;
    db_type_e type;
}app_db_local_t;

app_db_local_t app_db_local;
#define local app_db_local


/*
    brief:0 if success, ==-2 not found, ==-1 error
*/
void app_db_delete(void *key)
{
    return db_delete(local.db, key);
}

void app_db_replace(void *key, void *value)
{
    return (db_delete(local.db, key) == 0) && (db_insert(local.db, key, value, sizeof(value)) == 0);
}
/*
    brief:0 if success, ==-1 error
*/
void app_db_search(void *key, char *value)
{
    return db_search(local.db, key, value, sizeof(value));
}
/*
    brief:0 if success, ==-2 if key repeat, ==-1 error
*/
int app_db_insert(void *key,void *value)
{
    return db_insert(local.db, key, value, strlen(value))
}
void app_db_close(void)
{
    return db_close(local.db);
}

/*
    brief: 用户接口
*/
void app_db_operate(db_ope_type_e type, void *key, void *value)
{
    if(local.type != DB_INIT_SUCCESS || key == NULL || value == NULL || local.db == NULL) {
        printf("Error: db_init failed or key, value, or local.db is NULL\n");
        return ;
    }
    int res;
    switch (type)
    {
    case DB_OPE_INSERT:
        res = app_db_insert(key, value);
        break;
    case DB_OPE_REPLACE:
        res = app_db_replace(key, value);
        break;
    case DB_OPE_SEARCH:
        res = app_db_search(key, value);
        break;
    case DB_OPE_DELETE:
        res = app_db_delete(key);
        break;
    }
    if(res != 0) {
        printf("Error: db operate type = %d failed!\n", type);
    }
}


void app_db_init(void)
{
    local.type = DB_NULL;
    local.db = NULL;
    unlink(PATH); // 创建数据库
    if(db_create(PATH, DB_INT32KEY, sizeof(int)) != 0 || db_open(local.db, PATH) != 0) {
        app_db_close();
        local.type = DB_INIT_ERROR;
        printf("app db init failed!\n");
        return;
    }
    local.type = DB_INIT_SUCCESS;
}



