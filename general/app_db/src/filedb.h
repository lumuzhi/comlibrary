// 在 filedb.h 中定义 db_t
#ifndef __FILEDB_H__
#define _FILEDB_H__

#include <stddef.h>            // for size_t
#include <stdlib.h>            // for malloc(), free()
#include <string.h>            // for memcpy(), memmove(), strcpy(), strlen()
#include <fcntl.h>             // for open(), O_CREAT, O_RDWR, O_TRUNC
#include <sys/stat.h>          // for struct stat, fstat()
#include <unistd.h>            // for pread(), pwrite(), access(), ftruncate(), close()
#include <stdint.h>            // for int32_t
#include <errno.h>             // for E2BIG



/**
 * @brief 创建数据库时，指定的key类型
 */
#define DB_STRINGKEY 0 /** 4 <= max_key_size <= 128, include '\0', 包含'\0'在内 */
#define DB_BYTESKEY  1 /** 4 <= max_key_size <= 128 */
#define DB_INT32KEY  2 /** max_key_size = sizeof(int32_t) */
#define DB_INT64KEY  3 /** max_key_size = sizeof(int64_t) */

/**
 * @brief 文件数据库的头，即是句柄
 */
typedef struct db_s{
    int fd;                             /** 文件句柄 */
    int key_type;                       /** key类型，必须在创建文件数据库时指定 */
    size_t key_size;                    /** key的最大长度 */
    size_t key_align;                   /** 对齐，值 = db_align(sizeof(btree_key) + key_size, DB_ALIGNMENT) */
    size_t M;                           /** Btree 节点child的最大值 */
    size_t key_total;                   /** 已存储的key总数 */
    size_t key_use_block;               /** 数据块为btree_key类型的总数 */
    size_t value_use_block;             /** 数据块为btree_value类型的总数 */ 
    off_t free;                         /** 空闲链表的头 */
    off_t current;                      /** 当前作为btree_value的数据块，未用完分配空间 */
    int (*key_cmp)(void*,void*,size_t); /** key比较方式 */
}db_t;



int db_insert(db_t *db, void *key, void *value, size_t value_size);
int db_search(db_t *db, void *key, void *value, size_t value_size);
int db_create(const char *path, int key_type, size_t key_size);
int db_open(db_t **db, const char *path);
int db_delete(db_t *db, void *key);
int db_close(db_t *db);

#endif // FILEDB_H