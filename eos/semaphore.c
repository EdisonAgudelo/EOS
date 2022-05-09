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

#include "semaphore.h"

extern EOSListT ready_list[];

//can't be called from ISR
static void EOSSemaphoreAddBlockedTask(EOSSemaphoreT semaphore, EOSTaskT task)
{
    EOSTaskT index_task;

   
    //look for a task that has less priority than actual task
    for(index_task = EOS_GET_HEAD_FROM_LIST(semaphore->waiting_tasks); 
        index_task != NULL; index_task = EOS_GET_NEXT_FROM_ITEM(index_task, sync))
    {
        if(index_task->priority < task->priority)
            break;
    }
    
    if(index_task == NULL)
    {
        EOS_ADD_TO_LIST(semaphore->waiting_tasks, task, sync);
    } else {
        EOS_INSERT_PREV_TO_ITEM_IN_LIST(semaphore->waiting_tasks, task, index_task, sync);
    }
    
    if(semaphore->type == kEOSSemphrMutex)
    {
        //get holding task
        index_task = EOS_GET_INDEX_FROM_LIST(semaphore->waiting_tasks);
        EOS_ASSERT(index_task != NULL);

        //inversion priority?
        if(index_task != task && index_task->priority < task->priority)
        {
            //task inherint priority

            //check if task belong to any list
            if(index_task->scheduler.parent_list != NULL){
                //check if task belong to ready list
                if(EOS_ITEM_BELONG_TO_LIST(ready_list[index_task->priority], index_task, scheduler)){
                    //remove from index if task was "pre-scheduled"
                    if(EOS_GET_INDEX_FROM_LIST(ready_list[index_task->priority]) == index_task)
                    {
                        EOS_SET_LIST_INDEX(ready_list[index_task->priority], EOS_GET_NEXT_FROM_ITEM(index_task, scheduler));
                    }
                    //level up priority
                    EOS_REMOVE_FROM_LIST(ready_list[index_task->priority], index_task, scheduler);
                    EOS_ADD_TO_LIST(ready_list[task->priority], index_task, scheduler);
                }
            }
            index_task->priority = task->priority;

        }
    }
    
}


//shouldn't be called by user directly
void *EOSInternalSemaphoreTake(EOSSemaphoreT semaphore, EOSJumperT *jumper, EOSTaskStateT *state, bool *success,
                            void *wait, void *semaphore_end, void *task_end)
{                                
    portEOS_DISABLE_ISR(); 
    //if asking task is alereading holding semaphore, then just return success                                                                
    if(semaphore->free_keys || 
        EOS_GET_INDEX_FROM_LIST(semaphore->waiting_tasks) == eos_running_task){                                                               
        
        if(semaphore->free_keys)
            semaphore->free_keys--;      
        
        if(semaphore->type != kEOSSemphrMutex)
            EOS_SET_LIST_INDEX(semaphore->waiting_tasks, NULL);
        else
        //the list remove is auto done by EOSSemaphoreGiveISR
            EOS_SET_LIST_INDEX(semaphore->waiting_tasks, eos_running_task);                      
        portEOS_ENABLE_ISR();
        *success = true;
        return semaphore_end;                                     
    }
    if(eos_running_task->ticks_to_delay == 0){   
        //this list remove is auto done by scheduler if any              
        *success = false;                              
        portEOS_ENABLE_ISR();                                                               
        return semaphore_end;                                     
    }
    EOSSemaphoreAddBlockedTask(semaphore, eos_running_task);                                
    *jumper =  wait;
    eos_running_task->block_source = kEOSBlockSrcSemaphore;                                 
    *state = ((eos_running_task->ticks_to_delay) == EOS_INFINITE_TICKS) ?                                                             
                kEOSTaskSuspended : kEOSTaskBlocked;                            
    portEOS_ENABLE_ISR();    

    return task_end;                                                                                                                                                    
}








bool EOSSemaphoreGiveISR(EOSSemaphoreT semaphore)
{
    bool high_priority = false;
    EOSTaskT holding_task;
    
    portEOS_DISABLE_ISR();

    holding_task = EOS_GET_INDEX_FROM_LIST(semaphore->waiting_tasks);
    
    //disinherit priority
    if(holding_task != NULL)
    {
        if(holding_task->priority != holding_task->original_priority)
        {
            //check if task belong to any list
            if(holding_task->scheduler.parent_list != NULL){
                //check if task belong to ready list
                if(EOS_ITEM_BELONG_TO_LIST(ready_list[holding_task->priority], holding_task, scheduler)){

                    //check list index to avoid closed loops beetween priorities
                    if(EOS_GET_INDEX_FROM_LIST(ready_list[holding_task->priority]) == holding_task)
                    {
                        EOS_SET_LIST_INDEX(ready_list[holding_task->priority], EOS_GET_NEXT_FROM_ITEM(holding_task, scheduler));
                    }
                    //level down priority
                    EOS_REMOVE_FROM_LIST(ready_list[holding_task->priority], holding_task, scheduler);
                    EOS_ADD_TO_LIST(ready_list[holding_task->original_priority], holding_task, scheduler);

                }
            }
            holding_task->priority = holding_task->original_priority;
        }
    }

    do{
        holding_task = EOS_GET_HEAD_FROM_LIST(semaphore->waiting_tasks);
        EOS_SET_LIST_INDEX(semaphore->waiting_tasks, holding_task);
    
        if(holding_task)
        {
            //reomove from waiting_tasks
            EOS_REMOVE_FROM_LIST(semaphore->waiting_tasks, holding_task, sync);
            
            if(holding_task->block_source == kEOSBlockSrcSemaphore){
                
                high_priority = holding_task->priority > eos_running_task->priority;
                holding_task->block_source = kEOSBlockSrcNone;
                
                //check if scheduler alredy deque task from ready lists
                if(holding_task->scheduler.parent_list != NULL && !EOS_ITEM_BELONG_TO_LIST(ready_list[holding_task->priority], holding_task, scheduler))
                {
                    //reomove from either, block or suspended
                    EOS_REMOVE_FROM_LIST(*((EOSListT *)holding_task->scheduler.parent_list), holding_task, scheduler);
                    EOS_ADD_TO_LIST(ready_list[holding_task->priority], holding_task, scheduler);
                }
                
                //we can deque one task per semaphore give
                break;
                
            }  else {
                //ignore error
            }
        } else {
            //free keys is given only if semaphore list is empty... in that way 
            //after a freekey = 0 all tasks are equeued and prioritezed 
            semaphore->free_keys += semaphore->free_keys < semaphore->max_free_keys ? 1 : 0;
            
        }
    }while(holding_task);
    
    portEOS_ENABLE_ISR();
    return high_priority;
}

EOSSemaphoreT EOSCreateStaticSemphrGen(EOSStaticSemaphoreT *semaphore_buffer, uint32_t init_count, uint32_t max_keys, EOSSemaphoreTypeT type)
{
    semaphore_buffer->free_keys = init_count; 
    semaphore_buffer->max_free_keys = max_keys; 
    semaphore_buffer->waiting_tasks.head = NULL;
    semaphore_buffer->waiting_tasks.tail = NULL;
    semaphore_buffer->waiting_tasks.index = NULL;
    semaphore_buffer->type = type;
    return semaphore_buffer;
}