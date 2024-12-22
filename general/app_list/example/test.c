#include "rtservice.h"

#include <stdio.h>
#include <stdint.h>

#include <pthread.h>
#include <semaphore.h>
#define LIST_NODE_MAX 10

typedef struct {
    rt_list_t list;
    int value;
    int state;
}test_node_t;

typedef struct {
    rt_list_t save_list;
    test_node_t node[LIST_NODE_MAX];
    sem_t sem;
    pthread   thread;
}test_local_t;

test_local_t test_local;
#define local test_local



/*
    * @brief 示例中每个节点存储一个int类型的数据
*/

void test_list_insert(void *data, uint16_t size)
{
    // 获取未使用的节点
    uint8_t index;
    for(index = 0; index < LIST_NODE_MAX; index++) { 
        if(local.node[index].state == 0) {
            break;
        }
    }
    if(index == LIST_NODE_MAX - 1) {
        printf("list full\n");
        return ;
    }
    // 数据存储到节点中
    memcpy(&local.node[index].list, data, size);
    // 把节点插入到链表中
    rt_list_insert_before(&local.save_list, &local.node[index].list);
    // 标记节点状态
    local.node[index].state = 1;
    // 释放信号量
    sem_post(&local.sem);
}


static void test_thread_entry(void * paras)
{
    while(1) {
        sem_wait(&local.sem);
        if(rt_list_len(&local.save_list) {
            test_node_t *node;
            rt_list_for_each_entry(node, &local.save_list, test_node_t, list) {
                printf("node value:%d\n", node->value);
                // 节点状态置0
                node->state = 0;
            }
            // 把状态为0的节点从链表中移除
            test_node_t *next_node, *rm_node;
            rt_list_for_each_entry_safe(rm_node, next_node, &local.save_list, list) {
                if(node->state == 0) {
                    rt_list_remove(&rm_node->list);
                }
            }
            
        }
    }
}
void test_thread(void *parameter)
{
    if(pthread_create(&local.thread, NULL, (void *)&test_thread_entry, NULL) != 0)
	{
		perror("test pthread_create");
	}
}

void test_init(void)
{
    sem_init(&local.sem, 0, 0);
    rt_list_init(&local.list);
}