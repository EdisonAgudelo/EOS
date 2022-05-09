
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

#ifndef _EOS_MAILBOX_H_
#define _EOS_MAILBOX_H_

#include <stdbool.h>

#include "config.h"
#include "scheduler.h"


//receive a pointer to store message value and max ticks to wait
//return true if Mail was received
#define EOS_MAIL_WAIT(message, ticks_to_wait)                                               \
        ({                                                                                  \
            bool success;                                                                   \
            eos_running_task->ticks_to_delay = (ticks_to_wait);                             \
            CONCAT(EOS_MAIL_WAIT_LABEL, __LINE__):                                          \
            goto *EOSInternalMailWait(message, eos_jumper, eos_task_state, &success,        \
                                    &&CONCAT(EOS_MAIL_WAIT_LABEL, __LINE__),                \
                                    &&CONCAT(EOS_MAIL_END_LABEL, __LINE__),                 \
                                    &&EOS_END_LABEL);                                       \
            CONCAT(EOS_MAIL_END_LABEL, __LINE__):                                           \
            success;                                                                        \
        })

//send a mail to a task auto yields if mailed task was waiting for it and have higer priority
#define EOS_MAIL_SEND(task, mail)   \
    if(EOSMailSendISR(task, mail))  \
        EOS_YIELD()

//clear any message in box
#define EOS_MAIL_CLEAR()                        \
    do{                                         \
            portEOS_DISABLE_ISR();              \
            eos_running_task->mail_count = 0;   \
            portEOS_ENABLE_ISR();               \
    } while(0)

#define EOS_MAIL_PENDING()          \
        eos_running_task->mail_count

//shouldn't be called by user directly
void *EOSInternalMailWait(uint32_t *message, EOSJumperT *jumper, EOSTaskStateT *state, bool *success,
                            void *wait, void *mail_end, void *task_end);


//can be called outside EOS scope
bool EOSMailSendISR(EOSTaskT task, uint32_t mail);

#endif

/*

#include <stdio.h>


#include "eos.h"
#include "scheduler.h"
#include "mailbox.h"



void thread(EOSStackT stack, void *args);
void thread2(EOSStackT stack, void *args);

void EOSIdleHook(void){
    printf("idle\n");
}

EOSTaskT g_task_to_notify;

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
    g_task_to_notify = EOSCreateStaticTask(thread, &id2, 1, "t2",sizeof(stack2), stack2, &task2_buffer);
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
        
        EOS_DELAY(8);
        
        (*counter)++;
        printf("thread2 send noty_val %u\n", *counter);
        
        EOS_MAIL_SEND(g_task_to_notify, *counter);
               
    }

    EOS_END ();
}



void thread (EOSStackT stack, void *args)
{
    //local variables
    int i;
    //in this case, this varible isn't required to be in stack as it is for
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
        
        if(EOS_MAIL_WAIT(&notify_val, 10))
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
            EOS_DELAY(4);
        }  
    }

    EOS_END ();
    EOS_STACK_PUSH_COPY(i,stack);
}

*/