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

#include "queue.h"

#include <string.h>

extern EOSListT ready_list[];

//return true if high priotiy
//isr disable should be called firts
static bool EOSQueueRetrieve(EOSQueueT queue, void *item, bool *success)
{
    EOSTaskT waiting_sender;

    if(queue->item_count == 0){
        if(success) *success = false;
        return false;
    }

    if(success) *success = true;
    
    queue->item_count--;

    memcpy(item, queue->tail, queue->item_size);

    queue->tail += queue->item_size;

    //if tails reachs top
    if(queue->tail >= (queue->buffer + (queue->max_items * queue->item_size)))
        queue->tail = queue->buffer;

    //unblock one sender if any
    do{
        waiting_sender = EOS_GET_HEAD_FROM_LIST(queue->waiting_tasks);
    
        if(waiting_sender)
        {
            //reomove from waiting_tasks
            EOS_REMOVE_FROM_LIST(queue->waiting_tasks, waiting_sender, sync);
            
            if(waiting_sender->block_source == kEOSBlockSrcQueue){
                
                waiting_sender->block_source = kEOSBlockSrcNone;
                
                //check if scheduler already dequeued task from ready lists
                if(waiting_sender->scheduler.parent_list != NULL && !EOS_ITEM_BELONG_TO_LIST(ready_list[waiting_sender->priority], waiting_sender, scheduler))
                {
                    //reomove from either, block or suspended
                    EOS_REMOVE_FROM_LIST(*((EOSListT *)waiting_sender->scheduler.parent_list), waiting_sender, scheduler);
                    EOS_ADD_TO_LIST(ready_list[waiting_sender->priority], waiting_sender, scheduler);
                }
                
                //we can dequeue one task per quee read
                return waiting_sender->priority > eos_running_task->priority;
                
            }  else {
                //ignore error
            }
        } 

    }while(waiting_sender);

    return false;
}


void *EOSInternalQueueReceive(EOSQueueT queue, void *item, EOSJumperT *jumper, EOSTaskStateT *state,
                        bool *success, void *wait, void *yield, void *queue_end, void *task_end)
{      
    bool should_yield;

    portEOS_DISABLE_ISR();                                                                                  
    if(EOS_GET_INDEX_FROM_LIST(queue->waiting_tasks) == NULL  ||                                            
        EOS_GET_INDEX_FROM_LIST(queue->waiting_tasks) == eos_running_task){                                
        should_yield = EOSQueueRetrieve(queue, item, success);                                                    
        if(*success){                                                                                       
            EOS_SET_LIST_INDEX(queue->waiting_tasks, NULL)                                                  
            portEOS_ENABLE_ISR();                                                                           
            if(should_yield){                                                                                      
                *state = kEOSTaskYield;
                *jumper = yield;
                return task_end;                                                              
            }    
            return queue_end;
        }                                                                                                     
    }                                                                                                       
    if(eos_running_task->ticks_to_delay == 0 ||  EOS_GET_INDEX_FROM_LIST(queue->waiting_tasks) != NULL)     
    {
        if(EOS_GET_INDEX_FROM_LIST(queue->waiting_tasks) == eos_running_task)                               
            EOS_SET_LIST_INDEX(queue->waiting_tasks, NULL)                                                  
        portEOS_ENABLE_ISR();                                                                               
        *success = false;                                                                                    
        return queue_end;                                                         
    }                                                                                                       
    EOS_SET_LIST_INDEX(queue->waiting_tasks, eos_running_task);                                             
    *jumper = wait;                                                 
    eos_running_task->block_source = kEOSBlockSrcQueue;                                                     
    *state = ((eos_running_task->ticks_to_delay) == EOS_INFINITE_TICKS) ?                                             
                            kEOSTaskSuspended : kEOSTaskBlocked;                                            
    portEOS_ENABLE_ISR();                                                                                   
    return task_end;                                                                                          
}




static void EOSQueueAddBlockedSender(EOSQueueT queue, EOSTaskT sender)
{
    EOSTaskT index_task;
    

    for(index_task = EOS_GET_HEAD_FROM_LIST(queue->waiting_tasks);
        index_task != NULL; index_task = EOS_GET_NEXT_FROM_ITEM(index_task, sync)){
            //Find a less prioritaized task than actual
        if(index_task->priority < sender->priority){
            break;
        }
    }

    if(index_task == NULL)
    {
        EOS_ADD_TO_LIST(queue->waiting_tasks, sender, sync);
    } else {
        EOS_INSERT_PREV_TO_ITEM_IN_LIST(queue->waiting_tasks, sender, index_task, sync);
    }

}

bool EOSQueueSendISR(EOSQueueT queue, void *item, EOSQueueFlagsT flags, bool *success)
{
    EOSTaskT queue_receiver;
    bool high_priority = false;
    uint8_t **work_point;

    portEOS_DISABLE_ISR(); 

    if(queue->item_count >= queue->max_items && 0 == (flags & kEOSQueueOverWrite)){
        if(success) 
            *success = false;
        portEOS_ENABLE_ISR(); 
        return false;
    }    
    
    //queue receive reads from tail so if write to front I trite to the tail
    work_point = (flags & kEOSQueueWriteFront) != 0 ?  &queue->tail :  &queue->head;

    if(success) 
        *success = true;


    if((flags & kEOSQueueWriteFront) != 0){
        
        *work_point -=  queue->item_size;
        
        //if head underrun
        if(*work_point < queue->buffer)
            *work_point = queue->buffer + ((queue->max_items * queue->item_size) - queue->item_size);
    }

    memcpy(*work_point, item, queue->item_size);

    if((flags & kEOSQueueWriteFront) == 0){
        
        *work_point +=  queue->item_size;
        
        //if head reachs top
        if(*work_point >= (queue->buffer + (queue->max_items * queue->item_size)))
            *work_point = queue->buffer;
    }

    //keep it equal (push limits one to another)
    if(queue->item_count >= queue->max_items){
        queue->head = *work_point;
        queue->tail = *work_point;
        }
    else
        queue->item_count++;

    //unblock related task
    queue_receiver =  EOS_GET_INDEX_FROM_LIST(queue->waiting_tasks);
    if(queue_receiver)
    {
        if(queue_receiver->block_source == kEOSBlockSrcQueue)
        {
            queue_receiver->block_source = kEOSBlockSrcNone;
            //check if scheduler already dequeued task from ready lists
            if(queue_receiver->scheduler.parent_list != NULL && !EOS_ITEM_BELONG_TO_LIST(ready_list[queue_receiver->priority], queue_receiver, scheduler))
            {
                //reomove from either, block or suspended
                EOS_REMOVE_FROM_LIST(*((EOSListT *)queue_receiver->scheduler.parent_list), queue_receiver, scheduler);
                EOS_ADD_TO_LIST(ready_list[queue_receiver->priority], queue_receiver, scheduler);
            }
            high_priority = queue_receiver->priority > eos_running_task->priority;
        }
    }
    portEOS_ENABLE_ISR();   
    return high_priority;
}



void *EOSInternalQueueSend(EOSQueueT queue, void *item, EOSQueueFlagsT flags, EOSJumperT *jumper, EOSTaskStateT *state,
                        bool *success, void *wait, void *yield, void *queue_end, void *task_end)
{
    bool should_yield;                                                                                                                                 
                                                           
    should_yield = EOSQueueSendISR(queue, item, flags, success);                                                  
    if(*success){                                                                                            
        if(should_yield){                                                                                          
            *state = kEOSTaskYield;
            *jumper = yield;
            return task_end;                                                                                     
        }
        return queue_end;                                                   
    }                                                                                                      
    if(eos_running_task->ticks_to_delay == 0){                                                              
        *success = false;                                                                                   
        return queue_end;                                                         
    }                 
    portEOS_DISABLE_ISR();                                                                                   
    EOSQueueAddBlockedSender(queue, eos_running_task);                                                      
    *jumper = wait;                                                 
    eos_running_task->block_source = kEOSBlockSrcQueue;                                                     
    *state = ((eos_running_task->ticks_to_delay) == EOS_INFINITE_TICKS) ?                                             
                            kEOSTaskSuspended : kEOSTaskBlocked;                                              
    portEOS_ENABLE_ISR();   
    return task_end;
}

EOSQueueT EOSCreateStaticQueue(uint8_t *buffer, size_t item_size, uint32_t max_items, EOSStaticQueueT *queue_buffer)
{
    queue_buffer->buffer = buffer;
    queue_buffer->head = buffer;
    queue_buffer->tail = buffer;
    queue_buffer->item_count = 0;
    queue_buffer->item_size = item_size;
    queue_buffer->max_items = max_items;
    queue_buffer->waiting_tasks.head = NULL;
    queue_buffer->waiting_tasks.index = NULL;
    queue_buffer->waiting_tasks.tail = NULL;

    return queue_buffer;
}