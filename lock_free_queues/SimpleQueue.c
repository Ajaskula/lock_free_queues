#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>

#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

struct SimpleQueueNode {
    _Atomic(SimpleQueueNode*) next;
    Value item;
};

SimpleQueueNode* SimpleQueueNode_new(Value item)
{
    SimpleQueueNode* node = (SimpleQueueNode*)malloc(sizeof(SimpleQueueNode));
    atomic_init(&node->next, NULL);
    node -> item = item;
    return node;
}

struct SimpleQueue {
    SimpleQueueNode* head; 
    SimpleQueueNode* tail; 
    pthread_mutex_t head_mtx; 
    pthread_mutex_t tail_mtx; 
};

SimpleQueue* SimpleQueue_new(void)
{

    SimpleQueue* queue = (SimpleQueue*)malloc(sizeof(SimpleQueue));
    SimpleQueueNode* node = SimpleQueueNode_new(EMPTY_VALUE);
    queue -> head = node;
    queue -> tail = node;
    pthread_mutex_init(&queue -> head_mtx, NULL);
    pthread_mutex_init(&queue -> tail_mtx, NULL);

    return queue;
}

void SimpleQueue_delete(SimpleQueue* queue)
{
    SimpleQueueNode* current = queue->head;
    while (current != NULL) {
        SimpleQueueNode* next_to_free = atomic_load(&current->next); 
        free(current); 
        current = next_to_free; 
    }

    pthread_mutex_destroy(&queue->head_mtx);
    pthread_mutex_destroy(&queue->tail_mtx);

    free(queue);
}

void SimpleQueue_push(SimpleQueue* queue, Value item)
{   
    pthread_mutex_lock(&queue -> tail_mtx);
    SimpleQueueNode* new_node = SimpleQueueNode_new(item);
        atomic_store(&queue->tail->next, new_node);
        queue->tail = new_node;

    pthread_mutex_unlock(&queue -> tail_mtx);
}
Value SimpleQueue_pop(SimpleQueue* queue)
{
    pthread_mutex_lock(&queue->head_mtx);
    SimpleQueueNode* real_head = atomic_load(&queue->head->next);
    // queue is empty
    if(real_head == NULL){

        pthread_mutex_unlock(&queue->head_mtx);
        return EMPTY_VALUE;

    // pop the head
    }else{
        SimpleQueueNode* dummy_node = queue->head;
        Value ret_val = real_head->item;
        real_head->item = EMPTY_VALUE;
        queue->head = real_head;
        pthread_mutex_unlock(&queue->head_mtx);
        free(dummy_node);
        return ret_val;
    }
    
}

bool SimpleQueue_is_empty(SimpleQueue* queue)
{   
    pthread_mutex_lock(&queue->head_mtx);
    bool ret_value = (atomic_load(&queue->head->next) == NULL);
    pthread_mutex_unlock(&queue->head_mtx);
    return ret_value;

}
