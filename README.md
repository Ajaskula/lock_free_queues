# Lock-Free Queue Library

## Introduction
This project provides a set of lock-free queue implementations for concurrent programming. Lock-free queues offer advantages in scenarios where contention on locks can lead to performance bottlenecks.

### SimpleQueue
SimpleQueue is a queue implemented with a singly linked list and two mutexes. It is one of the simpler implementations of a queue. Having separate mutexes for producers and consumers allows better parallelization of operations.

The SimpleQueue structure consists of:
- A singly linked list of nodes, where each node contains:
  - An atomic pointer 'next' to the next node in the list.
  - A value of type 'Value'.
- A 'head' pointer to the first node in the list, along with a mutex to protect access to it.
- A 'tail' pointer to the last node in the list, along with a mutex to protect access to it.

### RingsQueue
RingsQueue combines the simplicity of SimpleQueue with the efficiency of a queue implemented on a circular buffer. This hybrid approach combines the unlimited size of the first with the performance of the second (singly linked lists are relatively slow due to continuous memory allocations).

The RingsQueue structure consists of:
- A singly linked list of nodes, where each node contains:
  - An atomic pointer 'next' to the next node in the list.
  - A circular buffer in the form of an array of size RING_SIZE of values of type 'Value'.
  - Atomic counters 'push_idx' and 'pop_idx' for push and pop operations in this node.
- A 'head' pointer to the first node in the list.
- A 'tail' pointer to the last node in the list (head and tail may point to the same node).
- A 'pop_mtx' mutex to lock the entire pop operation.
- A 'push_mtx' mutex to lock the entire push operation.

## LLQueue

LLQueue is one of the simplest implementations of a lock-free queue.

### Structure
The LLQueue structure consists of:
- A singly linked list of nodes, where each node contains:
  - An atomic pointer 'next' to the next node in the list.
  - A value of type 'Value', which is set to EMPTY_VALUE if the value from the node has already been retrieved.
- An atomic pointer 'head' to the first node in the list.
- An atomic pointer 'tail' to the last node in the list.
- A HazardPointer structure (see below).

## BLQueue

BLQueue is a simple yet highly efficient implementation of a lock-free queue. It combines the idea of a singly linked list with a solution where the queue is a simple array with atomic indices for inserting and retrieving elements (but the number of operations would be limited by the length of the array). By merging the advantages of both, we create a list of arrays; we only need to move to the next list node when the array is filled. However, the array here is not a cyclic buffer; each field in it is filled at most once (the variant with cyclic buffers would be much more challenging).

### Structure
The BLQueue structure consists of:
- A singly linked list of nodes, where each node contains:
  - An atomic pointer 'next' to the next node in the list.
  - A buffer with BUFFER_SIZE atomic values of type Value.
  - Atomic indices:
    - 'push_idx' for the next place in the buffer to be filled by a push operation (increases with each push attempt).
    - 'pop_idx' for the next place in the buffer to be emptied by a pop operation (increases with each pop attempt).
- Atomic pointers 'head' and 'tail' to the first and last nodes in the list, respectively.
- A HazardPointer structure (see below).

  # Hazard Pointer

The Hazard Pointer is a technique for dealing with the problem of safe memory reclamation in data structures shared by multiple threads, as well as the ABA problem. The idea is that each thread can reserve one address for a node (one for each queue instance) that it needs to protect from deletion (or ABA replacement) during push/pop/is_empty operations. When wanting to free a node (free()), the thread instead adds its address to its set of retired addresses and later periodically scans this set, freeing addresses that are not reserved.

### Features
- Provides a mechanism for safe memory reclamation in multi-threaded data structures.
- Helps mitigate the ABA problem by ensuring that pointers are not reused too quickly after being freed.

## Usage
1. Reserve hazard pointers for each thread.
2. Mark nodes as protected using hazard pointers during critical operations.
3. When wanting to free a node, add its address to the retired set instead of immediately freeing it.
4. Periodically scan the retired set and free addresses that are no longer protected by hazard pointers.

## Considerations
- Hazard pointers should be used sparingly to minimize contention and overhead.
- Proper synchronization mechanisms should be employed to ensure thread safety when accessing and modifying hazard pointers and retired sets.
