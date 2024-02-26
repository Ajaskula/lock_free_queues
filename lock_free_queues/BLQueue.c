#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "BLQueue.h"
#include "HazardPointer.h"

struct BLNode;
typedef struct BLNode BLNode;
typedef _Atomic(BLNode*) AtomicBLNodePtr;

struct BLNode {
    AtomicBLNodePtr next;
    _Atomic(Value) buffor[BUFFER_SIZE];
    _Atomic(int*) push_idx;
    _Atomic(int*) pop_idx;
};

struct BLNode* BLnode_new(){

    struct BLNode* new_node = (struct BLNode*)malloc(sizeof(BLNode));
    for(int i = 0; i < BUFFER_SIZE; i++){
        atomic_store(&new_node->buffor[i], EMPTY_VALUE);
    }
    atomic_store(&new_node->push_idx, 0);
    atomic_store(&new_node->pop_idx, 0);
    atomic_store(&new_node->next, NULL);
    return new_node;
}

struct BLQueue {
    AtomicBLNodePtr head;
    AtomicBLNodePtr tail;
    HazardPointer hp;
};

BLQueue* BLQueue_new(void)
{
    BLQueue* queue = (BLQueue*)malloc(sizeof(BLQueue));
    BLNode* new_node = BLnode_new();
    atomic_store(&queue->head, new_node);
    atomic_store(&queue->tail, new_node);
    HazardPointer_initialize(&queue->hp);
    return queue;
}

void BLQueue_delete(BLQueue* queue)
{   
    BLNode* to_free = atomic_load(&queue->head);
    while(atomic_load(&queue->head) != atomic_load(&queue->tail)){
        to_free = atomic_load(&queue->head);
        queue->head = atomic_load(&queue->head->next);
        free(to_free);
    }
    free(queue->head);
    HazardPointer_finalize(&queue->hp);
    free(queue);
}


void BLQueue_push(BLQueue* queue, Value item)
{
    while(true){
        // try to protect tail
        BLNode* tail = HazardPointer_protect(&queue->hp, (_Atomic(void*)*)&queue->tail);
        
        if(tail == atomic_load(&queue->tail)){

            int push_idx = (uint64_t)atomic_fetch_add(&tail->push_idx, 1);
            // check if push_idx is in buffer
            if(push_idx < BUFFER_SIZE){
                Value empty_val = EMPTY_VALUE;
                // try to push value to buffer
                if(atomic_compare_exchange_strong(&tail->buffor[push_idx], &empty_val, item)){
                    HazardPointer_clear(&queue->hp);
                    return;
                }else{
                    HazardPointer_clear(&queue->hp);
                    continue;
                }
            // push_idx is out of buffer
            }else{
                // check if there is next node
                if(atomic_load(&tail->next) == NULL){
                    BLNode* new_node = BLnode_new();
                    atomic_store(&new_node->buffor[0], item);
                    atomic_fetch_add(&new_node->push_idx, 1);
                    BLNode* next = NULL;
                    // try to add new node to queue
                    if(atomic_compare_exchange_strong(&tail->next, &next, new_node)){
                        atomic_compare_exchange_strong(&queue->tail, &tail, atomic_load(&tail->next));
                        HazardPointer_clear(&queue->hp);
                        return;

                    }else{ 
                        free(new_node);
                        HazardPointer_clear(&queue->hp);
                        continue;
                    }

                // there is next node
                }else{
                    if(atomic_compare_exchange_strong(&queue->tail, &tail, atomic_load(&tail->next))){
                        continue;
                    }
                }
            }
        }
    }
}

Value BLQueue_pop(BLQueue* queue)
{   

    while(true){

        Value ret_value;
        // protect head node
        BLNode* head = (BLNode*)HazardPointer_protect(&queue->hp, (_Atomic(void*)*)&queue->head);
        if(head != atomic_load(&queue->head)){
            continue;
        }
            int pop_idx =  (uint64_t)atomic_fetch_add(&head->pop_idx, 1);
            // check if pop_idx is in buffer
            if(pop_idx < BUFFER_SIZE){
                // try to pop value from buffer
                ret_value = atomic_exchange(&head->buffor[pop_idx], TAKEN_VALUE);
                if(ret_value == EMPTY_VALUE){
                    continue;
                }else{ 
                    HazardPointer_clear(&queue->hp);
                    return ret_value;
                }
            // pop_idx is out of buffer
            }else{
                
                // check if there is next node
                if(atomic_load(&head->next) != NULL){
                    // try to move to next node
                    if(atomic_compare_exchange_strong(&queue->head, &head, atomic_load(&head->next))){
                        HazardPointer_clear(&queue->hp);
                        HazardPointer_retire(&queue->hp, head);
                        continue;
                    }else{
                        HazardPointer_clear(&queue->hp);
                        continue;
                    }
                // there is no next node
                }else{
                    HazardPointer_clear(&queue->hp);
                    return EMPTY_VALUE;
                }

            }
    }
}


bool BLQueue_is_empty(BLQueue* queue)
{   
    
    BLNode* head = NULL;
    // protect head
    while(head != atomic_load(&queue->head)){
        head = HazardPointer_protect(&queue->hp, (_Atomic(void*)*)&queue->head);
    }
    // check if node is empty
    if((uint64_t)atomic_load(&head->pop_idx) >= BUFFER_SIZE){
        // check if there is next node
        if(atomic_load(&head->next) == NULL){
                HazardPointer_clear(&queue->hp);
                return true;
        }
    }
    HazardPointer_clear(&queue->hp);
    return false;
}
