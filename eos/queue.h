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

#ifndef _EOS_QUEUE_H_
#define _EOS_QUEUE_H_


#include "list.h"
#include "scheduler.h"

typedef struct 
{
    uint8_t *buffer;  //saves the user assigned memory for storing que messages
    size_t item_size; //the size of each element in the queue
    uint32_t max_items; //size of the queue in terms of items
    uint32_t item_count; //the amount of saved items
    uint8_t *head; //circular queue
    uint8_t *tail;
    //if task attemps to send in a full queue it saved here if task can wait.
    //Index saves the "receiver" task if any
    //it not possible to have multiples receivers...
    EOSListT waiting_tasks; 
} EOSStaticQueueT;

typedef EOSStaticQueueT * EOSQueueT;

//return true if and element was retrieved
#define EOS_QUEUE_RECEIVE(queue, item, ticks_to_wait)                                           \
        ({                                                                                      \
            bool success;                                                                       \
            eos_running_task->ticks_to_delay = (ticks_to_wait);                                 \
            CONCAT(EOS_QUEUE_WAIT_LABEL, __LINE__):                                             \
            goto *EOSInternalQueueReceive(queue, item, eos_jumper, eos_task_state, &success,    \
                                            &&CONCAT(EOS_QUEUE_WAIT_LABEL, __LINE__),           \
                                            &&CONCAT(EOS_QUEUE_YIELD_LABEL, __LINE__),          \
                                            &&CONCAT(EOS_QUEUE_END_LABEL, __LINE__),            \
                                            &&EOS_END_LABEL);                                   \
            CONCAT(EOS_QUEUE_YIELD_LABEL, __LINE__):                                            \
            success = true;                                                                     \
            CONCAT(EOS_QUEUE_END_LABEL, __LINE__):                                              \
            success;                                                                            \
        })
                                    

#define EOS_QUEUE_SEND(queue, item, flags, ticks_to_wait)                                       \
        ({                                                                                      \
            bool success;                                                                       \
            eos_running_task->ticks_to_delay = (ticks_to_wait);                                 \
            CONCAT(EOS_QUEUE_WAIT_LABEL, __LINE__):                                             \
            goto *EOSInternalQueueSend(queue, item, flags, eos_jumper, eos_task_state, &success,\
                                            &&CONCAT(EOS_QUEUE_WAIT_LABEL, __LINE__),           \
                                            &&CONCAT(EOS_QUEUE_YIELD_LABEL, __LINE__),          \
                                            &&CONCAT(EOS_QUEUE_END_LABEL, __LINE__),            \
                                            &&EOS_END_LABEL);                                   \
            CONCAT(EOS_QUEUE_YIELD_LABEL, __LINE__):                                            \
            success = true;                                                                     \
            CONCAT(EOS_QUEUE_END_LABEL, __LINE__):                                              \
            success;                                                                            \
        })  

typedef enum{
    kEOSQueueWriteBack = 0,
    kEOSQueueWriteFront =  0b1,
    kEOSQueueOverWrite = 0b10
}EOSQueueFlagsT; 

//should not be called directly by user
void *EOSInternalQueueReceive(EOSQueueT queue, void *item, EOSJumperT *jumper, EOSTaskStateT *state,
                        bool *success, void *wait, void *yield, void *queue_end, void *task_end);

void *EOSInternalQueueSend(EOSQueueT queue, void *item, EOSQueueFlagsT flags, EOSJumperT *jumper, EOSTaskStateT *state,
                        bool *success, void *wait, void *yield, void *queue_end, void *task_end);



//return true if task was woken. 
bool EOSQueueSendISR(EOSQueueT queue, void *item, EOSQueueFlagsT flags, bool *success);


EOSQueueT EOSCreateStaticQueue(uint8_t *buffer, size_t item_size, uint32_t max_items, EOSStaticQueueT *queue_buffer);



#endif

/*


#include <stdio.h>


#include "eos.h"
#include "scheduler.h"
#include "queue.h"



void thread(EOSStackT stack, void *args);
void thread2(EOSStackT stack, void *args);
void thread3(EOSStackT stack, void *args);

void EOSIdleHook(void){
    printf("idle\n");
}

EOSQueueT g_queue;

#define QUEUE_LEN 5

struct queue_item{
    uint32_t id;
    uint32_t value;
};

int main ()
{
    int i = 0;
    EOSStaticStack stack1[50];
    EOSStaticStack stack2[50];
    EOSStaticStack stack3[50];
    EOSStaticStack stack4[50];
    EOSStaticTaskT task1_buffer;
    EOSStaticTaskT task2_buffer;
    EOSStaticTaskT task3_buffer;
    EOSStaticTaskT task4_buffer;
    EOSStaticQueueT queue_buffer;

    uint8_t buffer[QUEUE_LEN * sizeof(struct queue_item)];
    
    int id1 = 3;
    int id2 = 2;

    g_queue = EOSCreateStaticQueue(buffer, sizeof(struct queue_item), QUEUE_LEN, &queue_buffer);
    
    EOSCreateStaticTask(thread, &id1, 2, "t1", sizeof(stack1), stack1, &task1_buffer);
    EOSCreateStaticTask(thread, &id2, 2, "t2",sizeof(stack2), stack2, &task2_buffer);
    EOSCreateStaticTask(thread2, NULL, 1, "t3", sizeof(stack3), stack3, &task3_buffer);
    EOSCreateStaticTask(thread3, NULL, 3, "t4", sizeof(stack4), stack4, &task4_buffer);
    
    EOSScheduler();
    
    //should never reach here
    return 0;
}



void thread2 (EOSStackT stack, void *args)
{
    struct queue_item *item;
    
    //should be called at top of fucntion
    EOS_INIT(stack);

    EOS_STACK_POP(item, stack);
    
    //here program starts
    EOS_BEGIN();
  
    printf("thread2: start\n");
    while (1){      

        EOS_QUEUE_RECEIVE(g_queue, item, EOS_INFINITE_TICKS);
        
        printf("thread2: id%u noty val %u\n", item->id, item->value);
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
    struct queue_item *item;
    
    //should be called at top of fucntion
    EOS_INIT(stack);
    
   
    //for local copy... but volatile between yields ... so it is required a push at the end of task
    EOS_STACK_POP(item, stack);
    
    //here program starts
    EOS_BEGIN();
    
    item->id = *(int *)args;
    item->value = 0;
  
    while (1){

        printf("thread:%u: try send queue noty val %u\n", item->id, item->value);
        if(!EOS_QUEUE_SEND(g_queue, item, kEOSQueueWriteBack, 0)){
            printf("thread:%u: send queue fail\n", item->id);  
            EOS_DELAY(3);
        } else {
            printf("thread:%u: send queue ok\n", item->id, item->value);
            EOS_YIELD();
        }
        item->value++;

        //EOS_DELAY(3);
    }

    EOS_END ();
}



void thread3 (EOSStackT stack, void *args)
{
    //local variables
    int i;
    //in this case, this varible isn't required to be in stack as it is for really
    //temporal usage an there is no yield or switching apis between values assigment
    //and usage.
    struct queue_item *item;
    
    //should be called at top of fucntion
    EOS_INIT(stack);
    
    EOS_STACK_POP(item, stack);
    EOS_STACK_POP_COPY(i, stack);
    
    //here program starts
    EOS_BEGIN();
    
    item->id = 0;
    item->value = 0;
    
    i = 0;
    while (1){

        printf("thread:%u: try send queue noty val %u\n", item->id, item->value);
        if(!EOS_QUEUE_SEND(g_queue, item, kEOSQueueWriteFront | kEOSQueueOverWrite, 0)){
            printf("thread:%u: send queue fail\n", item->id);  
            EOS_DELAY(8);
        } else {
            printf("thread:%u: send queue ok\n", item->id);  
            if(++i>=2){
                EOS_DELAY(8);
                i=0;
            }
        }
        item->value++;
    }

    EOS_END ();
    EOS_STACK_PUSH_COPY(i, stack);
}

*/