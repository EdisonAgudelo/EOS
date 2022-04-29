
#ifndef _EOS_SCHEDULER_H_
#define _EOS_SCHEDULER_H_

#include "eos.h"

typedef void (*EOSTask)(EOSStackT stack, void *args);

typedef struct EOSStaticTask
{
    EOSStackT stack; //user assigned stack pointer
    uint32_t stack_size; //user assigned stack len
    void *args; //user task args
    EOSTask task; //the execution task
    EOSTaskStateT state;    //task state
    uint32_t unblock_tick;      //if task is blocked... this saves when it will be unblocked
    bool tick_over_flow;
    uint32_t ticks_to_delay;
    uint8_t priority;
    //for list handling
    struct EOSStaticTask *prev;
    struct EOSStaticTask *next;
    
    void *belong_to_list;
}EOSStaticTaskT;

typedef EOSStaticTaskT *EOSTaskT;


#define EOS_MAX_TICK 0xffffffff

#define EOS_DELAY(ticks)                                                \
    do {                                                                \
        *eos_jumper = &&CONCAT(EOS_DELAY_LABEL, __LINE__);              \
        if((ticks) == EOS_MAX_TICK)                                     \
        {                                                               \
            *eos_task_state = kEOSTaskSuspended;                        \
        } else                                                          \
        {                                                               \
            *eos_task_state = kEOSTaskBlocked;                          \
            eos_running_task->ticks_to_delay = ticks;                   \
        }                                                               \
        goto EOS_END_LABEL;                                             \
        CONCAT(EOS_DELAY_LABEL, __LINE__):;                             \
    }while(0)


extern uint32_t eos_tick;
extern EOSTaskT eos_running_task;


void EOSTickIncrement(void);
EOSTaskT EOSCreateStaticTask(EOSTask task, void *args, uint8_t priority, uint32_t stack_size, 
                            EOSStaticStack *stack_buffer, EOSStaticTaskT *task_buffer);
void EOSScheduler(void);

void EOSIdleHook(void);



#endif
