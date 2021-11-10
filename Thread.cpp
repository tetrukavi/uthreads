//
// Created by tetrukavi on 02/05/2021.
//


#include "Thread.h"
#include <setjmp.h>
#include <signal.h>



#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif





/*
 * This is the constructor of the thread object
 */
Thread::Thread(int id, void (*f)(void)) {
    tid = id;

    address_t sp, pc;
    sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;
    sigsetjmp(env[0], 1);
    (env[0]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[0]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[0]->__saved_mask);
}

/*
 * This function returns the status of the thread - running or ready
 */
int Thread::get_state() const {
    return my_state;
}

/*
 * This function returns the id of the thread
 */
int Thread::get_tid() const {
    return tid;
}
/*
 * This function returns true if the thread is blocked, false otherwise
 */
bool Thread::get_blocked_by_thread() const {
    return blocked_by_thread;
}

/*
 * This function gets the total number of quantums this thread ran
 */
int Thread::get_quantum_running_time() const {
    return quantum_running_time;
}



/*
 * This function sets the status of the thread - running or ready
 */
void Thread::set_state(int state) {
    my_state = state;
}

/*
 * This function sets if the thread is blocked or not
 */
void Thread::set_blocked_by_thread(bool check_if_blocked) {
    blocked_by_thread = check_if_blocked;
}

/*
 * This function sets the total number of quantums of this thread
 */
void Thread::set_quantum_running_time(int its_quantum) {
    quantum_running_time = its_quantum;
}

bool Thread::get_blocked_by_mutex() const {
    return blocked_by_mutex;
}

void Thread::set_blocked_by_mutex(bool mutex_status) {
    blocked_by_mutex = mutex_status;

}





