//
// Created by tetrukavi on 02/05/2021.
//

#ifndef OS_EX2_THREAD_H
#define OS_EX2_THREAD_H

#include <setjmp.h>
#include <utility>
#include "uthreads.h"

typedef unsigned long address_t;


/*
 * This class represents a thread object.
 */
class Thread {

private:

    int my_state; // 1 - running, 2 - ready
    bool blocked_by_thread = false; // default not blocked
    bool blocked_by_mutex = false;
    int quantum_running_time = 0; // total number of quantums of this thread
    int tid;


public:


    Thread(int id, void (*f)(void));

    sigjmp_buf env[1];
    char stack[STACK_SIZE]; // pointer of the stack - allocate in the heap

    int get_state() const;
    bool get_blocked_by_thread() const;
    bool get_blocked_by_mutex() const;
    void set_blocked_by_mutex(bool mutex_status) ;
    int get_quantum_running_time() const;
    int get_tid() const;
    void set_state(int state);
    void set_blocked_by_thread(bool check_if_blocked);
    void set_quantum_running_time(int quantum_usecs);

};



#endif //OS_EX2_THREAD_H
