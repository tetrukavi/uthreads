//
// Created by tetrukavi on 02/05/2021.
//

#include "Thread.h"
#include "uthreads.h"
#include <map>
#include <iostream>
#include <set>
#include <deque>
#include <csetjmp>
#include <csignal>
#include <bits/stdc++.h>
#include <sys/time.h>


/// macros ///
#define FAILURE -1;
#define SUCCESS 0;

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define RUNNING 1
#define READY 2

#define BLOCKED true
#define UNBLOCKED false

#define READY_MAP 1
#define BLOCKED_MAP 2


/// declarations ///
using std::map;
using std::set;
using std::deque;
using std::pair;


/// fields ///
map<int, Thread*> blocked_threads; // tid, thread
map<int, Thread*> ready_threads_map; // tid, thread
deque<Thread*> ordered_deque_threads; // all the created threads -> first in first out
deque<Thread*> mutex_deque_threads; // all the mutex blocked threads -> first in first out
pair<bool, int> mutex_pair; // pair that symbolized the mutex: first = locked\unlocked, second = thread id



int total_quantum;
int available_tid;
set<int> min_available_tids;

Thread* running_thread_ptr; // pointer to the running thread
int running_dest = 1; // which contact switch to do


/// timer ///
struct sigaction sa = {0};
struct itimerval timer;


/// signals ///
sigset_t signal_set;


/// functions ///



/*
 * Description: This function blocks the SIGVTALRM signal
 */
void block_signals(){
    if (sigemptyset(&signal_set)){
        std::cerr << "system error: sigemptyset error\n";
        exit(EXIT_FAILURE);
    }

    if (sigaddset(&signal_set,SIGVTALRM)){
        std::cerr << "system error: sigaddset - SIGVTALRM error\n";
        exit(EXIT_FAILURE);
    }

    if (sigprocmask(SIG_BLOCK, &signal_set, nullptr) < 0){
        std::cerr << "system error: sigprocmask - SIG_BLOCK error\n";
        exit(EXIT_FAILURE);
    }
}

/*
 * Description: This function unblocks the SIGVTALRM signal
 */
void unblock_signals(){
    if (sigprocmask(SIG_UNBLOCK, &signal_set, nullptr) < 0){
        std::cerr << "system error: sigprocmask - SIG_UNBLOCK error\n";
        exit(EXIT_FAILURE);
    }
}

/*
 * Description: This function resets the timer. Each thread get quantum_usecs microseconds.
 */
void reset_timer(int quantum_usecs) {
    // Configure the timer to expire after 1 quantum_usecs sec for finishing the running of the main thread
    timer.it_value.tv_sec = 0;        // first time interval, seconds part
    timer.it_value.tv_usec = quantum_usecs;        // first time interval, microseconds part

    // configure the timer to expire every quantum_usecs sec after that.
    timer.it_interval.tv_sec = 0;    // following time intervals, seconds part
    timer.it_interval.tv_usec = quantum_usecs;    // following time intervals, microseconds part

    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
        std::cerr << "system error: setitimer error\n";
        unblock_signals();
        exit(EXIT_FAILURE);
    }
}

/*
 * Description: This function erases the pointer to the thread with the
 * given tid in the given map.
 */
void erase_from_map(int tid, int which_map) {
    map<int, Thread*>::iterator it;

    if (which_map == READY_MAP)
    {
        it = ready_threads_map.find(tid);
        ready_threads_map.erase(it);
    }
    else {
        it = blocked_threads.find(tid);
        blocked_threads.erase(it);
    }
}

/*
 * Description: if running_dest == 1 -> ready, if running_dest == 2 -> blocked
 * and if running_dest == 3 -> terminate
 */
void contact_switch(int sig)
{
    if (ready_threads_map.empty()) {

        //insert the main thread to the ready map and deque
        if (running_thread_ptr->get_tid() == 0)
        {
            ready_threads_map.insert({running_thread_ptr->get_tid(), running_thread_ptr});
            ordered_deque_threads.push_back(running_thread_ptr);
        }

        total_quantum++;
        running_thread_ptr->set_quantum_running_time(running_thread_ptr->get_quantum_running_time() + 1);
        unblock_signals();
        siglongjmp(running_thread_ptr->env[0],1);
    }

    if (running_dest == 3) {
        running_dest = 1;
        total_quantum++;
        running_thread_ptr->set_quantum_running_time(running_thread_ptr->get_quantum_running_time() + 1);
        // Changing the env of the running thread
        siglongjmp(running_thread_ptr->env[0], 1); // jump to the new thread sp & pc
    }

    if (running_dest == 2){
        // The running thread that we want to block
        Thread *to_block_thread = running_thread_ptr;
        // The first thread in the ready deque -> make it the running thread
        Thread *thread_to_run = ordered_deque_threads.front();
        ordered_deque_threads.pop_front();
        erase_from_map(thread_to_run->get_tid(), READY_MAP);
        // Making the first thread in the ready deque, the running thread
        thread_to_run->set_state(RUNNING);
        running_thread_ptr = thread_to_run;
        // blocked the prev running thread
        blocked_threads.insert({to_block_thread->get_tid(), to_block_thread});
        to_block_thread->set_blocked_by_thread(BLOCKED);
        running_dest = 1;

        // save the prev env & set the env of the new running thread
        int ret_val = sigsetjmp(to_block_thread->env[0],1);
        if (ret_val == 1) {
            unblock_signals();
            return;
        }
        total_quantum++;
        running_thread_ptr->set_quantum_running_time(running_thread_ptr->get_quantum_running_time() + 1);
        unblock_signals();
        siglongjmp(running_thread_ptr->env[0],1);
    }

    if (running_dest == 1) {
        // The running thread that we want to make ready
        Thread *to_ready_thread = running_thread_ptr;

        Thread *thread_to_run;
        if ((ordered_deque_threads.front() ==  running_thread_ptr) && (ready_threads_map.size() > 1)){
            thread_to_run = ordered_deque_threads.front();
            ordered_deque_threads.pop_front();
            erase_from_map(thread_to_run->get_tid(), READY_MAP);
        }

        // The first thread in the ready deque -> make it the running thread
        thread_to_run = ordered_deque_threads.front();
        ordered_deque_threads.pop_front();
        erase_from_map(thread_to_run->get_tid(), READY_MAP);
        // Making the first thread in the ready deque, the running thread
        thread_to_run->set_state(RUNNING);
        running_thread_ptr = thread_to_run;

        // ready the prev running thread
        if (sig != 120) {
            ready_threads_map.insert({to_ready_thread->get_tid(), to_ready_thread});
            ordered_deque_threads.push_back(to_ready_thread);
            to_ready_thread->set_state(READY);
        }
        running_dest = 1;
        // save the prev env & set the env of the new running thread
        int ret_val = sigsetjmp(to_ready_thread->env[0],1);
        if (ret_val == 1) {
            unblock_signals();
            return;
        }
        total_quantum++;
        running_thread_ptr->set_quantum_running_time(running_thread_ptr->get_quantum_running_time() + 1);
        unblock_signals();
        siglongjmp(running_thread_ptr->env[0],1);
    }
}

/*
 * Description: This function resets the running_dest flag to 1 ->
 * for contact switch to ready position.
 */
void reset_clock(int sig){
    running_dest = 1;
    contact_switch(SIGVTALRM);
}

/*
 * Description: This function initializes the Thread library.
 * You may assume that this function is called before any other Thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
    block_signals();
    if (quantum_usecs <= 0){
        unblock_signals();
        std::cerr << "thread library error: quantum_usecs is non-positive\n";
        return FAILURE
    }
    auto *main_thread = new Thread(0, nullptr);
    main_thread->set_state(RUNNING);
    main_thread->set_blocked_by_thread(UNBLOCKED);
    main_thread->set_quantum_running_time(1);
    running_thread_ptr = main_thread;

    available_tid = 1;
    total_quantum++;

    // Install contact_switch as the signal handler for SIGVTALRM.
    sa.sa_handler = &reset_clock;
    // After quantum seconds, we will change the running thread
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        unblock_signals();
        std::cerr << "system error: sigaction error\n";
        exit(EXIT_FAILURE);
    }

    reset_timer(quantum_usecs);
    sigsetjmp(running_thread_ptr->env[0],1);
    mutex_pair.first = false;
    mutex_pair.second = -1;
    unblock_signals();
    return SUCCESS
}


/*
 * Description: This function creates a new Thread, whose entry point is the
 * function f with the signature void f(void). The Thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each Thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created Thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void)){
    block_signals();
    if ((available_tid + 1 > MAX_THREAD_NUM) && min_available_tids.empty()){
        unblock_signals();
        std::cerr << "thread library error: error in uthread_spawn function\n";
        return FAILURE
    }

    // when there is id that is in the available id-s, we want to create new thread with the minimal id
    if (!min_available_tids.empty()) {
        int min_elem_set = *min_available_tids.begin();
        auto *new_thread = new Thread(min_elem_set, f);
        ready_threads_map.insert({min_elem_set, new_thread});
        new_thread->set_state(READY);
        new_thread->set_blocked_by_thread(UNBLOCKED);
        ordered_deque_threads.push_back(new_thread);
        min_available_tids.erase(min_elem_set); // delete from the available set of threads
        unblock_signals();
        return min_elem_set;
    }
    auto *new_thread = new Thread(available_tid, f);
    ready_threads_map.insert({available_tid, new_thread});
    new_thread->set_state(READY);
    new_thread->set_blocked_by_thread(UNBLOCKED);
    ordered_deque_threads.push_back(new_thread);
    available_tid++;
    unblock_signals();
    return available_tid - 1;
}


/*
 * Description: This function terminates the Thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this Thread should be released. If no Thread with ID tid
 * exists it is considered an error. Terminating the main Thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the Thread was successfully
 * terminated and -1 otherwise. If a Thread terminates itself or the main
 * Thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    block_signals();

    if ((min_available_tids.find(tid) != min_available_tids.end()) || (available_tid <= tid) || (tid < 0)){
        unblock_signals();
        std::cerr << "thread library error: no Thread with ID tid exists - terminate\n";
        return FAILURE
    }

    // main thread
    if (tid == 0){
        bool running_in_ready = false;
        while (!blocked_threads.empty()){
            auto it = blocked_threads.begin();
            Thread* to_delete = it->second;
            erase_from_map(to_delete->get_tid(), BLOCKED_MAP);
            if (!to_delete->get_blocked_by_mutex())
            {
                delete to_delete;
            }
        }
        blocked_threads.clear();

        while (!ready_threads_map.empty()){
            auto it = ready_threads_map.begin();
            Thread* to_delete = it->second;
            erase_from_map(it->second->get_tid(), READY_MAP);
            if (to_delete->get_tid() == running_thread_ptr->get_tid())
            {
                running_in_ready = true;
            }
            delete to_delete;
        }
        ready_threads_map.clear();
        ordered_deque_threads.clear();
        min_available_tids.clear();

        for (Thread* thread: mutex_deque_threads){
            delete thread;
        }
        mutex_deque_threads.clear();

        if (!running_in_ready)
        {
            delete running_thread_ptr;
        }
        unblock_signals();
        exit(EXIT_SUCCESS);
    }

    // running thread
    if (running_thread_ptr->get_tid() == tid) {
        if (mutex_pair.second == running_thread_ptr->get_tid()){
            uthread_mutex_unlock();
        }
        running_dest = 3;
        // The running thread that we want to terminate
        Thread *wanted_thread_to_delete = running_thread_ptr;
        // The first thread in the ready deque -> make it the running thread
        Thread *swap_thread = ordered_deque_threads.front();
        ordered_deque_threads.pop_front();
        erase_from_map(swap_thread->get_tid(), READY_MAP);
        // The terminated running thread -> take its id -> become available
        min_available_tids.insert(wanted_thread_to_delete->get_tid());
        // Making the first thread in the ready deque, the running thread
        swap_thread->set_state(RUNNING);
        running_thread_ptr = swap_thread;
        // Free the prev running thread
        min_available_tids.insert(wanted_thread_to_delete->get_tid());
        delete wanted_thread_to_delete;
        // reset the timer for the new thread
        if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
            unblock_signals();
            std::cerr << "thread library error: setitimer error\n";
        }
        contact_switch(SIGVTALRM);
    }

    // ready thread
    if (ready_threads_map.find(tid) != ready_threads_map.end()) {
        Thread *to_delete = ready_threads_map[tid];
        ordered_deque_threads.erase(std::remove(ordered_deque_threads.begin(), ordered_deque_threads.end(),
                                                ready_threads_map[tid]), ordered_deque_threads.end());
        erase_from_map(to_delete->get_tid(), READY_MAP);// delete the thread from the ready map
        min_available_tids.insert(to_delete->get_tid());
        if (to_delete->get_tid() == mutex_pair.second){
            mutex_pair.first = false;
            mutex_pair.second = -1;
            if (!mutex_deque_threads.empty()) {
                Thread *blocked_to_ready = mutex_deque_threads.front();
                mutex_deque_threads.pop_front();
                blocked_to_ready->set_blocked_by_mutex(false);

                if (!blocked_to_ready->get_blocked_by_thread()) {
                    ready_threads_map.insert({blocked_to_ready->get_tid(), blocked_to_ready});
                    blocked_to_ready->set_state(READY);
                    ordered_deque_threads.push_back(blocked_to_ready);
                }
            }
        }
        delete to_delete;
    }

    // blocked thread
    if (blocked_threads.find(tid) != blocked_threads.end()){
        Thread *to_delete = blocked_threads[tid];
        if (to_delete->get_tid() == mutex_pair.second){
            mutex_pair.first = false;
            mutex_pair.second = -1;
            if (!mutex_deque_threads.empty()) {
                Thread *blocked_to_ready = mutex_deque_threads.front();
                mutex_deque_threads.pop_front();
                blocked_to_ready->set_blocked_by_mutex(false);

                if (!blocked_to_ready->get_blocked_by_thread()) {
                    ready_threads_map.insert({blocked_to_ready->get_tid(), blocked_to_ready});
                    blocked_to_ready->set_state(READY);
                    ordered_deque_threads.push_back(blocked_to_ready);
                }
            }
        }
        min_available_tids.insert(blocked_threads[tid]->get_tid());
        erase_from_map(blocked_threads[tid]->get_tid(), BLOCKED_MAP);// delete the thread from the blocked map
        delete to_delete;
    }
    unblock_signals();
    return SUCCESS
}

/*
 * Description: This function blocks the Thread with ID tid. The Thread may
 * be resumed later using uthread_resume. If no Thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main Thread (tid == 0). If a Thread blocks itself, a scheduling decision
 * should be made. Blocking a Thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
    block_signals();
    if ((min_available_tids.find(tid) != min_available_tids.end()) || (available_tid <= tid) || (tid < 0)){
        unblock_signals();
        std::cerr << "thread library error: no Thread with ID tid exists - block\n";
        return FAILURE
    }
    if (tid == 0){
        unblock_signals();
        std::cerr << "thread library error: error for block the main thread\n";
        return FAILURE
    }

    // blocking the running thread - blocking itself
    if (running_thread_ptr->get_tid() == tid){
        running_dest = 2;

        if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
            unblock_signals();
            std::cerr << "thread library error: setitimer error\n";
        }
        contact_switch(SIGVTALRM);
    }

    // blocking a thread in the ready thread
    if (ready_threads_map.find(tid) != ready_threads_map.end()) {
        Thread *to_block_from_ready = ready_threads_map[tid];
        ordered_deque_threads.erase(std::remove(ordered_deque_threads.begin(), ordered_deque_threads.end(),
                                                ready_threads_map[tid]), ordered_deque_threads.end());
        erase_from_map(to_block_from_ready->get_tid(), READY_MAP); // delete the thread from the ready map
        // blocked the thread from the ready map
        to_block_from_ready->set_blocked_by_thread(BLOCKED);
        blocked_threads.insert({to_block_from_ready->get_tid(), to_block_from_ready});
    }
    else {
        // blocking a thread that in the mutex deque
        for (auto in_deque_mutex : mutex_deque_threads){
            if (in_deque_mutex->get_tid() == tid){
                in_deque_mutex->set_blocked_by_thread(BLOCKED);
                blocked_threads.insert({in_deque_mutex->get_tid(), in_deque_mutex});
            }
        }
    }
    unblock_signals();
    return SUCCESS
}

/*
 * Description: This function resumes a blocked Thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a Thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no Thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
    block_signals();
    if ((min_available_tids.find(tid) != min_available_tids.end()) || (available_tid <= tid) || (tid < 0)){
        unblock_signals();
        std::cerr << "thread library error: error in uthread_terminate function\n";
        return FAILURE
    }
    // From blocked to ready
    if (blocked_threads.find(tid) != blocked_threads.end()){
        Thread* to_ready = blocked_threads[tid];
        erase_from_map(tid, BLOCKED_MAP);
        to_ready->set_blocked_by_thread(UNBLOCKED);

        if (!to_ready->get_blocked_by_mutex()){
            ready_threads_map.insert({tid, to_ready});
            to_ready->set_state(READY);
            ordered_deque_threads.push_back(to_ready);
        }
    }
    unblock_signals();
    return SUCCESS
}


/*
 * Description: This function tries to acquire a mutex.
 * If the mutex is unlocked, it locks it and returns.
 * If the mutex is already locked by different Thread, the Thread moves to BLOCK state.
 * In the future when this Thread will be back to RUNNING state,
 * it will try again to acquire the mutex.
 * If the mutex is already locked by this Thread, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_lock(){
    block_signals();

    // The mutex is available
    if (!mutex_pair.first){
        mutex_pair.first = true;
        mutex_pair.second = running_thread_ptr->get_tid();
        unblock_signals();
        return SUCCESS
    }

    if (mutex_pair.first){
        // If the mutex is already locked by this Thread, it is considered an error
        if (mutex_pair.second == running_thread_ptr->get_tid())
        {
            std::cerr << "thread library error: error in uthread_mutex_lock - "
                         "thread tries to lock itself\n";
            unblock_signals();
            return -1;
        }
        else
        {
            while (mutex_pair.first) {
                mutex_deque_threads.push_back(running_thread_ptr);
                running_thread_ptr->set_blocked_by_mutex(true);
                if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
                    unblock_signals();
                    std::cerr << "thread library error: setitimer error\n";
                }
                running_dest = 1;
                contact_switch(120);
            }
            mutex_pair.first = true;
            mutex_pair.second = running_thread_ptr->get_tid();
            running_thread_ptr->set_blocked_by_mutex(false);
            unblock_signals();
            return SUCCESS
        }
    }
    unblock_signals();
    return SUCCESS
}


/*
 * Description: This function releases a mutex.
 * If there are blocked threads waiting for this mutex,
 * one of them (no matter which one) moves to READY state.
 * If the mutex is already unlocked, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_unlock(){
    block_signals();

    // If the mutex is already unlocked, it is considered an error.
    if (!mutex_pair.first){
        std::cerr << "thread library error: error in uthread_mutex_unlock - "
                     "unlocking thread that is already unlocked\n";
        unblock_signals();
        return FAILURE
    }

    if (mutex_pair.first) {

        if (running_thread_ptr->get_tid() != mutex_pair.second)
        {
            std::cerr << "thread library error: error in uthread_mutex_unlock - "
                         "the mutex has been unlocked by another thread\n";
            unblock_signals();
            return FAILURE

        }
        else
        {
            mutex_pair.first = false;
            mutex_pair.second = -1;
            if (!mutex_deque_threads.empty()) {
                Thread *blocked_to_ready = mutex_deque_threads.front();
                mutex_deque_threads.pop_front();
                blocked_to_ready->set_blocked_by_mutex(false);

                if (!blocked_to_ready->get_blocked_by_thread()) {
                    ready_threads_map.insert({blocked_to_ready->get_tid(), blocked_to_ready});
                    blocked_to_ready->set_state(READY);
                    ordered_deque_threads.push_back(blocked_to_ready);
                }
            }
        }
    }
    unblock_signals();
    return SUCCESS
}


/*
 * Description: This function returns the Thread ID of the calling Thread.
 * Return value: The ID of the calling Thread.
*/
int uthread_get_tid(){
    return running_thread_ptr->get_tid();
}


/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums(){
    return total_quantum;
}


/*
 * Description: This function returns the number of quantums the Thread with
 * ID tid was in RUNNING state. On the first time a Thread runs, the function
 * should return 1. Every additional quantum that the Thread starts should
 * increase this value by 1 (so if the Thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * Thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the Thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid){
    block_signals();

    if ((min_available_tids.find(tid) != min_available_tids.end()) || (available_tid <= tid) || (tid < 0)){
        unblock_signals();
        std::cerr << "thread library error: error in uthread_terminate function\n";
        return FAILURE
    }
    // running thread
    if (running_thread_ptr->get_tid() == tid){
        unblock_signals();
        return running_thread_ptr->get_quantum_running_time();
    }
    // ready thread
    else if (ready_threads_map.find(tid) != ready_threads_map.end()) {
        unblock_signals();
        return ready_threads_map[tid]->get_quantum_running_time();
    }
    // blocked thread ==> blocked_threads.find(tid) != blocked_threads.end()
    else {
        unblock_signals();
        return blocked_threads[tid]->get_quantum_running_time();
    }
}

