#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>

#include <stdio.h>
#include <stdlib.h>

#include "HazardPointer.h"
#include "RingsQueue.h"


struct RingsQueueNode;
typedef struct RingsQueueNode RingsQueueNode;

struct RingsQueueNode {
    _Atomic(RingsQueueNode*) next;
    Value buffer[RING_SIZE];
    _Atomic(int) push_idx;
    _Atomic(int) pop_idx;
};

struct RingsQueue {
    RingsQueueNode* head;
    RingsQueueNode* tail;
    pthread_mutex_t pop_mtx;
    pthread_mutex_t push_mtx;
};
RingsQueueNode* create_new_Node(){
    RingsQueueNode* new_node = (RingsQueueNode*)malloc(sizeof(RingsQueueNode));
    for(int i = 0; i < RING_SIZE; i++){
        new_node->buffer[i] = EMPTY_VALUE;
    }
    atomic_init(&new_node->next, NULL);
    atomic_init(&new_node->pop_idx, 0);
    atomic_init(&new_node->push_idx, 0);

    return new_node;
}
RingsQueue* RingsQueue_new(void)
{
    RingsQueue* queue = (RingsQueue*)malloc(sizeof(RingsQueue));
    RingsQueueNode* node = create_new_Node();

    queue->head = node;
    queue->tail = node;

    pthread_mutex_init(&queue -> pop_mtx, NULL);
    pthread_mutex_init(&queue -> push_mtx, NULL);
    return queue;
}

void RingsQueue_delete(RingsQueue* queue)
{   
    while(queue->head != NULL){
        RingsQueueNode* next_to_free = atomic_load(&queue->head->next);
        free(queue->head);
        queue->head = next_to_free;
    }
    pthread_mutex_destroy(&queue->pop_mtx);
    pthread_mutex_destroy(&queue->push_mtx);
    free(queue);
}

void RingsQueue_push(RingsQueue* queue, Value item)
{
    pthread_mutex_lock(&queue->push_mtx);

    int rem_push_idx = atomic_load(&queue->tail->push_idx);
    int copy_push_idx = rem_push_idx;
    rem_push_idx++;
    rem_push_idx %= RING_SIZE;

    // check if the buffer is full
    if(atomic_load(&queue->tail->pop_idx) == rem_push_idx){

        RingsQueueNode* new_node = create_new_Node();
        atomic_store(&queue->tail->next, new_node);
        queue->tail = new_node;
        queue->tail->buffer[0] = item;
        atomic_store(&queue->tail->push_idx, 1);
        
    // add to the buffer
    }else{
        queue->tail->buffer[copy_push_idx] = item;
        atomic_store(&queue->tail->push_idx, rem_push_idx);
    }
    pthread_mutex_unlock(&queue->push_mtx);
}

Value RingsQueue_pop(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);
    Value ret_value = EMPTY_VALUE;
    // there are more nodes in the queue
    if(atomic_load(&queue->head->next) != NULL){
        
        // try to pop from the head
        int pop_idx = atomic_load(&queue->head->pop_idx);
        if(pop_idx == atomic_load(&queue->head->push_idx)){
            RingsQueueNode* rem_head = queue->head;
            queue->head = atomic_load(&queue->head->next);
            atomic_store(&rem_head->next, NULL);
        pthread_mutex_unlock(&queue->pop_mtx);
        free(rem_head);
        return ret_value;
        // buffer is empty
        }else{
            ret_value = queue->head->buffer[pop_idx];
            pop_idx++;
            pop_idx %= RING_SIZE;
            atomic_store(&queue->head->pop_idx, pop_idx);
            pthread_mutex_unlock(&queue->pop_mtx);
            return ret_value;
        }
    // there is only one node in the queue
    }else{
        // try to pop from the head
        ret_value = EMPTY_VALUE;
        int rem_pop_idx = atomic_load(&queue->head->pop_idx);
        if(atomic_load(&queue->head->push_idx) == rem_pop_idx){
            pthread_mutex_unlock(&queue->pop_mtx);
            return ret_value;
        // buffer is not empty
        }else{
            ret_value = queue->head->buffer[rem_pop_idx];
            rem_pop_idx++;
            rem_pop_idx %= RING_SIZE;
            atomic_store(&queue->head->pop_idx, rem_pop_idx);
            pthread_mutex_unlock(&queue->pop_mtx);
            return ret_value;
        }
        
    }

    pthread_mutex_unlock(&queue->pop_mtx);   
    return ret_value;
}

bool RingsQueue_is_empty(RingsQueue* queue)
{   
    pthread_mutex_lock(&queue->pop_mtx);
    int rem_pop_idx = atomic_load(&queue->head->pop_idx);
    if(atomic_load(&queue->head->next) == NULL){
        
        if(atomic_load(&queue->head->push_idx) == rem_pop_idx){
            pthread_mutex_unlock(&queue->pop_mtx);
            return true;
        }
    }
    pthread_mutex_unlock(&queue->pop_mtx);
    return false;
}
