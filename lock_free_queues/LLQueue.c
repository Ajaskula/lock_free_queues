#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "HazardPointer.h"
#include "LLQueue.h"

struct LLNode;
typedef struct LLNode LLNode;
typedef _Atomic(LLNode*) AtomicLLNodePtr;

struct LLNode {
    AtomicLLNodePtr next;
    _Atomic(Value) item;
};

LLNode* LLNode_new(Value item)
{
    LLNode* node = (LLNode*)malloc(sizeof(LLNode));
    atomic_store(&node->item, item);
    atomic_store(&node->next, NULL);
    return node;
}

struct LLQueue {
    AtomicLLNodePtr head;
    AtomicLLNodePtr tail;
    HazardPointer hp;
};

LLQueue* LLQueue_new(void)
{
    LLQueue* queue = (LLQueue*)malloc(sizeof(LLQueue));
    LLNode* new_node = LLNode_new(EMPTY_VALUE);
    atomic_store(&queue->head, new_node);
    atomic_store(&queue->tail, new_node);
    HazardPointer_initialize(&queue->hp);
    return queue;
}

void LLQueue_delete(LLQueue* queue)
{   

    AtomicLLNodePtr to_free;
    while ((to_free = atomic_load(&queue->head)) != NULL) {
        AtomicLLNodePtr next = atomic_load(&to_free->next);
        atomic_store(&queue->head, next);
        free(to_free);
    }
    HazardPointer_finalize(&queue->hp);
    free(queue);
}

void LLQueue_push(LLQueue* queue, Value item)
{   
    LLNode* new_node = LLNode_new(item);
    while (true) {
        LLNode* tail = (LLNode*)HazardPointer_protect(&queue->hp, (const _Atomic(void*)*)&queue->tail);
        // protect tail
        if (tail == (LLNode*)atomic_load(&queue->tail)) {
            LLNode* next = NULL;
            // there is no next node
            if (atomic_compare_exchange_strong(&tail->next, &next, new_node)) {

                // try to change tail
                if(atomic_compare_exchange_strong(&queue->tail, &tail, new_node)){
                    HazardPointer_clear(&queue->hp);
                    return;
                }else{
                    HazardPointer_clear(&queue->hp);
                    return;
                }
            // try to change tail
            } else {           
                if(atomic_compare_exchange_strong(&queue->tail, &tail, atomic_load(&tail->next))){
                    HazardPointer_clear(&queue->hp);
                    continue;
                }else{
                    continue;
                }
    
            }
        }else{
            HazardPointer_clear(&queue->hp);
            continue;
        }
    }
}


Value LLQueue_pop(LLQueue* queue)
{
    while (true) {
        LLNode* head = (LLNode*)HazardPointer_protect(&queue->hp, (_Atomic(void*)*)&queue->head);
        // protect head
        if (atomic_load(&queue->head) == head) {
            Value ret_value = atomic_exchange(&head->item, EMPTY_VALUE);
            LLNode* next_node = atomic_load(&head->next);
            // return value is not empty
            if (ret_value != EMPTY_VALUE) {
                    if (next_node == NULL) {
                        HazardPointer_clear(&queue->hp);
                        return ret_value;
                    } else {
                        // try to change head
                        if (atomic_compare_exchange_strong(&queue->head, &head, next_node)) {
                            HazardPointer_clear(&queue->hp);
                            HazardPointer_retire(&queue->hp, head);
                            return ret_value;
                        } else {
                            HazardPointer_clear(&queue->hp);
                            return ret_value;
                        }
                    }
            // return value is empty
            } else {
                if (next_node == NULL) {
                    HazardPointer_clear(&queue->hp);
                    return EMPTY_VALUE;
                // try to change head
                } else {
                    if (atomic_compare_exchange_strong(&queue->head, &head, next_node)) {
                        HazardPointer_clear(&queue->hp);
                        HazardPointer_retire(&queue->hp, head);
                        continue;
                    } else {
                        HazardPointer_clear(&queue->hp);
                        continue;
                    }
                }
            }
        }
    }
}


bool LLQueue_is_empty(LLQueue* queue)
{   
    LLNode* head = HazardPointer_protect(&queue->hp, (_Atomic(void*)*)&queue->head);
    Value value = atomic_load(&head->item);
    LLNode* next = atomic_load(&head->next);

    // protect head
    while(atomic_load(&queue->head) != head){
        head = HazardPointer_protect(&queue->hp, (_Atomic(void*)*)&queue->head);
        value = atomic_load(&head->item);
        next = atomic_load(&head->next);
    }
            if(value == EMPTY_VALUE){
                if(next == NULL){
                    HazardPointer_clear(&queue->hp);
                    return true;
                }
            }
    HazardPointer_clear(&queue->hp);
    return false;
}
