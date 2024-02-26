#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "HazardPointer.h"

thread_local int _thread_id = -1;
int _num_threads = -1;

void HazardPointer_register(int thread_id, int num_threads)
{
    if(thread_id < 0 || thread_id >= MAX_THREADS){
        fprintf(stderr, "Invalid thread_id at HazardPointer_register\n");
        exit(1);
    }
    if(num_threads <= 0 || num_threads > MAX_THREADS){
        fprintf(stderr, "Invalid num_threads at HazardPointer_register\n");
        exit(1);
    }
    if(thread_id == 0){
    _num_threads = num_threads;
    }
    _thread_id = thread_id;
}

void HazardPointer_initialize(HazardPointer* hp)
{   
    
    for(int i = 0; i < MAX_THREADS; i++){
        atomic_init(&hp->pointer[i], NULL);
    }
    for(int i = 0; i < MAX_THREADS; i++){
        hp->retired[i] = (void**)malloc((RETIRED_THRESHOLD + 2) * sizeof(void*));
        if(hp->retired[i] == NULL){
            fprintf(stderr, "Memory allocation failed at HazardPointer_initialize\n");
            exit(1);
        }
        for(int j = 0; j < RETIRED_THRESHOLD + 1; j++){
            hp->retired[i][j] = NULL;
        }
    }
    
    for(int i = 0; i < MAX_THREADS; i++){
        int* cnt = (int*)malloc(sizeof(int));
        if(cnt == NULL){
            fprintf(stderr, "Memory allocation failed at HazardPointer_initialize\n");
            exit(1);
        }
        hp->retired[i][RETIRED_THRESHOLD+1] = (void*)cnt;
        *(int*)hp->retired[i][RETIRED_THRESHOLD+1] = 0;
    }
}

void HazardPointer_finalize(HazardPointer* hp)
{   

    for(int i = 0; i < MAX_THREADS; i++){
        atomic_store(&hp->pointer[i], NULL);
    }

    for(int i = 0; i < MAX_THREADS; i++){
        for(int j = 0; j < RETIRED_THRESHOLD + 2; j++){
            if(hp->retired[i][j] != NULL){
                free(hp->retired[i][j]);
                hp->retired[i][j] = NULL;
            }
        }
    }

    for(int i = 0; i < MAX_THREADS; i++){
        free(hp->retired[i]);
    }

}

void* HazardPointer_protect(HazardPointer* hp, const _Atomic(void*)* atom)
{   

    // try to protect the pointer
    _Atomic(void*) ptr_to_store;
    void* tmp = atomic_load(atom);
    atomic_store(&ptr_to_store, tmp);

    atomic_store(&hp->pointer[_thread_id], ptr_to_store);

    // check if the pointer has changed
    if(atomic_load(atom) == ptr_to_store){
        return atomic_load(&hp->pointer[_thread_id]);
    }else{
        atomic_store(&hp->pointer[_thread_id], NULL);
        return NULL;
    }
}


void HazardPointer_clear(HazardPointer* hp)
{
    atomic_store(&hp->pointer[_thread_id], NULL);
}

void HazardPointer_retire(HazardPointer* hp, void* ptr)
{   
    int* cnt = (int*)hp->retired[_thread_id][RETIRED_THRESHOLD+1];
    // add the pointer to the retired list
    for(int i = 0; i < RETIRED_THRESHOLD+1; i++){
        if(hp->retired[_thread_id][i] == NULL){
            hp->retired[_thread_id][i] = ptr;
            (*cnt)++;
            break;
        }
    }
    // check the number of retired pointers
    // try to free the retired pointers
    if(  cnt != NULL && (*cnt) > RETIRED_THRESHOLD){

        for(int i = 0; i < RETIRED_THRESHOLD+1; i++){
            void* retired_addres = (void*)hp->retired[_thread_id][i];
            if(retired_addres == NULL){
                continue;
            }
            for(int j = 0; j < _num_threads; j++){

                if(atomic_load(&hp->pointer[j]) == retired_addres){
                    break;
                }
                if(j == _num_threads - 1){
                    hp->retired[_thread_id][i] = NULL;
                    free(retired_addres);
                    (*cnt)--;

                }
            }
        }

    }
}

