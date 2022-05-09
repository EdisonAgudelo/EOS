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

#ifndef _EOS_SEMAPHORE_H_
#define _EOS_SEMAPHORE_H_

#include <stdint.h>
#include <stdbool.h>

#include "eos.h"
#include "list.h"
#include "scheduler.h"

//return true if take was success
#define EOS_SEMAPHORE_TAKE(semaphore, ticks_to_wait)                                            \
        ({                                                                                      \
            bool success;                                                                \
            eos_running_task->ticks_to_delay = (ticks_to_wait);                                 \
            CONCAT(EOS_SEMAPHORE_WAIT_LABEL, __LINE__):                                         \
            goto *EOSInternalSemaphoreTake(semaphore, eos_jumper, eos_task_state, &success,     \
                                        &&CONCAT(EOS_SEMAPHORE_WAIT_LABEL, __LINE__),           \
                                        &&CONCAT(EOS_SEMAPHORE_END_LABEL, __LINE__),            \
                                        &&EOS_END_LABEL);                                       \
            CONCAT(EOS_SEMAPHORE_END_LABEL, __LINE__):                                          \
            success;                                                                            \
        })

#define EOS_SEMAPHORE_GIVE(semaphore)       \
        if(EOSSemaphoreGiveISR(semaphore))  \
            EOS_YIELD()


typedef enum {
    kEOSSemphrBinary,
    kEOSSemphrConter,
    kEOSSemphrMutex
} EOSSemaphoreTypeT;

typedef struct {
    uint32_t free_keys;  //if >0 take pass
    uint32_t max_free_keys;
    EOSSemaphoreTypeT type; 
    
    EOSListT waiting_tasks;  //a list of blocked task waiting for this resource. Index of this list is the holding task
    
} EOSStaticSemaphoreT;

typedef EOSStaticSemaphoreT * EOSSemaphoreT;


#define EOSCreateStaticBinarySemaphore(semaphore_buffer) EOSCreateStaticSemphrGen(semaphore_buffer, 0, 1, kEOSSemphrBinary)
#define EOSCreateStaticCounterSemaphore(semaphore_buffer, init, max) EOSCreateStaticSemphrGen(semaphore_buffer, init, max, kEOSSemphrConter)
#define EOSCreateStaticMutexSemaphore(semaphore_buffer) EOSCreateStaticSemphrGen(semaphore_buffer, 1, 1, kEOSSemphrMutex)

EOSSemaphoreT EOSCreateStaticSemphrGen(EOSStaticSemaphoreT *semaphore_buffer, uint32_t init_count, uint32_t max_count, EOSSemaphoreTypeT type);

//can be called from ISR context
bool EOSSemaphoreGiveISR(EOSSemaphoreT semaphore);


//shouldn't be called by user directly
void *EOSInternalSemaphoreTake(EOSSemaphoreT semaphore, EOSJumperT *jumper, EOSTaskStateT *state,
                        bool *success, void *wait, void *semaphore_end, void *task_end);



#endif

/*
#include <stdio.h>


#include "eos.h"
#include "scheduler.h"
#include "mailbox.h"
#include "semaphore.h"



void thread(EOSStackT stack, void *args);
void thread2(EOSStackT stack, void *args);

void EOSIdleHook(void){
    printf("idle\n");
}

EOSTaskT g_task1;
EOSTaskT g_task2;
EOSSemaphoreT g_semaphore;

int main ()
{
    int i = 0;
    EOSStaticStack stack1[50];
    EOSStaticStack stack2[50];
    EOSStaticStack stack3[50];
    EOSStaticTaskT task1_buffer;
    EOSStaticTaskT task2_buffer;
    EOSStaticTaskT task3_buffer;
    EOSStaticSemaphoreT semaphore_buffer;
    
    int id1 = 3;
    int id2 = 2;

    g_semaphore = EOSCreateStaticMutexSemaphore(&semaphore_buffer);
    
    g_task1 = EOSCreateStaticTask(thread, &id1, 1, "t1", sizeof(stack1), stack1, &task1_buffer);
    g_task2 = EOSCreateStaticTask(thread, &id2, 1, "t2",sizeof(stack2), stack2, &task2_buffer);
    EOSCreateStaticTask(thread2, NULL, 2, "t3", sizeof(stack3), stack3, &task3_buffer);
    
    
    EOSScheduler();
    
    //should never reach here
    return 0;
}



void thread2 (EOSStackT stack, void *args)
{
    uint32_t *counter;
    
    //should be called at top of fucntion
    EOS_INIT(stack);

    EOS_STACK_POP(counter, stack);
    
    //here program starts
    EOS_BEGIN();
    
    *counter = 0;
  
    while (1){
        
        printf("thread2\n");
        
        EOS_DELAY(2);

        EOS_SEMAPHORE_TAKE(g_semaphore, EOS_INFINITE_TICKS);
        
        (*counter)++;
        printf("thread2 send noty_val %u\n", *counter);
        
        EOS_MAIL_SEND(g_task1, *counter);
        EOS_MAIL_SEND(g_task2, *counter);

        EOS_SEMAPHORE_GIVE(g_semaphore);
    }

    EOS_END ();
}



void thread (EOSStackT stack, void *args)
{
    //local variables
    int i;
    //in this case, this varible isn't required to be in stack as it is for really
    //temporal usage an there is no yield or switching apis between values assigment
    //and usage.
    uint32_t notify_val;
    
    //should be called at top of fucntion
    EOS_INIT(stack);
    
   
    //for local copy... but volatile between yields ... so it is required a push at the end of task
    EOS_STACK_POP_COPY(i, stack);
    
    //here program starts
    EOS_BEGIN();
    
    i=0;
  
    while (1){
        
        EOS_SEMAPHORE_TAKE(g_semaphore, EOS_INFINITE_TICKS);
        while(1){
            if(EOS_MAIL_WAIT(&notify_val, 3))
            {
                printf("thread:%i notify val %u\n", *(int *)args, notify_val);   
            } else {
                printf("thread:%i mail timeout\n", *(int *)args);      
            }
            
            if(i++ < *(int *)args){
                printf("thread:%i yield\n", *(int *)args);
                EOS_YIELD();
            }
            else{
                i=0;
                printf("thread:%i delay\n", *(int *)args);
                EOS_SEMAPHORE_GIVE(g_semaphore);
                EOS_DELAY(6);
                break;
            }  
        }
    }

    EOS_END ();
    EOS_STACK_PUSH_COPY(i,stack);
}
*/