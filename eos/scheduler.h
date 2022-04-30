/*
MIT License

Copyright (c) 2022 Edison Agudelo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _EOS_SCHEDULER_H_
#define _EOS_SCHEDULER_H_


#include "eos.h"



//this is the format we expect from threads or tasks
typedef void (*EOSTaskFunction)(EOSStackT stack, void *args);


typedef enum {
    kEOSBlockSrcNone,  //used to notify ISR events (//may be faster than scheduler)
    kEOSBlockSrcDelay,
    kEOSBlockSrcMail
} EOSBlockSourceT;

typedef struct EOSStaticTask
{
    //task basic informatino
    EOSStackT stack; //user assigned stack pointer
    uint32_t stack_size; //user assigned stack len
    void *args; //user task custom arguments
    EOSTaskFunction task; //the execution task function
    uint8_t priority; //assigned task priority
    char name[EOS_TASK_MAX_NAME_LEN]; //for printing stats
    

    //dalay/block variables
    uint32_t unblock_tick;      //if task is blocked... this saves when it will be unblocked
    bool tick_over_flow; //signals if it have to wait eos_tick overrun
    uint32_t ticks_to_delay; //how many ticks is gonna wait to unblock
    
    
    uint32_t mail_value;
    uint32_t mail_count;  //how many signals received
    EOSBlockSourceT block_source; //if a task state is different from yield, but there is no source... block is discarted
    
    //list handling
    struct EOSStaticTask *prev;
    struct EOSStaticTask *next;
    
    void *parent_list; //owner list
}EOSStaticTaskT;

//task handler
typedef EOSStaticTaskT *EOSTaskT;

// If task is unblocked for a undefined amount of ticks
#define EOS_INFINITE_TICKS 0xffffffff

//block or dalay a task execution for n ticks.
#define EOS_DELAY(ticks)                                                \
    do {                                                                \
        *eos_jumper = &&CONCAT(EOS_DELAY_LABEL, __LINE__);              \
        eos_running_task->block_source = kEOSBlockSrcDelay;             \
        if((ticks) == EOS_INFINITE_TICKS)                               \
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

//actual tick value
extern uint32_t eos_tick;
//actual execution task
extern EOSTaskT eos_running_task;

//should be called from a tick generator
void EOSTickIncrement(void);

//api for statical task creation
EOSTaskT EOSCreateStaticTask(EOSTaskFunction task, void *args, uint8_t priority, char *name, uint32_t stack_size, 
                            EOSStaticStack *stack_buffer, EOSStaticTaskT *task_buffer);

//starts schedules activities
void EOSScheduler(void);

//hook called when task is executing 
void EOSIdleHook(void);


/*
//simple example

#include <stdio.h>


#include "eos.h"
#include "scheduler.h"


void thread(EOSStackT stack, void *args);
void thread2(EOSStackT stack, void *args);

void EOSIdleHook(void){
    printf("idle\n");
}

int main ()
{
    int i = 0;
    EOSStaticStack stack1[50];
    EOSStaticStack stack2[50];
    EOSStaticStack stack3[50];
    EOSStaticTaskT task1_buffer;
    EOSStaticTaskT task2_buffer;
    EOSStaticTaskT task3_buffer;
    
    int id1 = 3;
    int id2 = 2;

    EOSCreateStaticTask(thread, &id1, 1, "t1", sizeof(stack1), stack1, &task1_buffer);
    EOSCreateStaticTask(thread, &id2, 1, "t2", sizeof(stack2), stack2, &task2_buffer);
    EOSCreateStaticTask(thread2, NULL, 2, "t3", sizeof(stack3), stack3, &task3_buffer);
    
    
    EOSScheduler();
    
    //should never reach here
    return 0;
}



void thread2 (EOSStackT stack, void *args)
{
    
    //should be called at top of fucntion
    EOS_INIT(stack);
    
    //here program starts
    EOS_BEGIN();
  
    while (1){
        
        printf("thread2\n");
        EOS_DELAY(4);
               
    }

    EOS_END ();
}



void thread (EOSStackT stack, void *args)
{
    //local variables
    int i;
    
    //should be called at top of fucntion
    EOS_INIT(stack);
    
    //for local copy... but volatile between yields ... so it is required a push at the end of task
    EOS_STACK_POP_COPY(i, stack);
    
    //here program starts
    EOS_BEGIN();
    
    i=0;
  
    while (1){
        
        if(i++ < *(int *)args){
            printf("thread:%i yield\n", *(int *)args);
            EOS_YIELD();
        }
        else{
            i=0;
            printf("thread:%i delay\n", *(int *)args);
            EOS_DELAY(4);
        }  
    }

    EOS_END ();
    EOS_STACK_PUSH_COPY(i,stack);
}

*/


#endif
