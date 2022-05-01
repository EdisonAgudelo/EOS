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
void EOSAddTaskToSemaphore(EOSSemaphoreT semaphore, EOSTaskT task)
{
    EOSTaskT index_task;
    
    portEOS_DISABLE_ISR();

    //look for a task that has less priority than actual task
    for(index_task = EOS_GET_HEAD_FROM_LIST(semaphore->asking_tasks); 
        index_task != NULL; index_task = EOS_GET_NEXT_FROM_ITEM(index_task, sync))
    {
        if(index_task->priority < task->priority)
            break;
    }
    
    if(index_task == NULL)
    {
        EOS_ADD_TO_LIST(semaphore->asking_tasks, task, sync);
    } else {
        EOS_INSERT_PREV_TO_ITEM_IN_LIST(semaphore->asking_tasks, task, index_task, sync);
    }
    
    if(semaphore->type == kEOSSemphrMutex)
    {
        //get holding task
        index_task = EOS_GET_INDEX_FROM_LIST(semaphore->asking_tasks);
        EOS_ASSERT(index_task != NULL);

        //inversion priority?
        if(index_task != task && index_task->priority < task->priority)
        {
            //task inherint priority

            //if first time inhirentace
            if(semaphore->original_priority == 0)
                semaphore->original_priority = index_task->priority;

            //check if task belong to any list
            if(index_task->scheduler.parent_list != NULL){
                //check if task belong to ready list
                if(EOS_ITEM_BELONG_TO_LIST(ready_list[index_task->priority], index_task, scheduler)){
                    //level up priority
                    EOS_REMOVE_FROM_LIST(*((EOSListT *)index_task->scheduler.parent_list), index_task, scheduler);
                    EOS_ADD_TO_LIST(ready_list[task->priority], index_task, scheduler);
                }
            }
            index_task->priority = task->priority;

        }
    }
    
    portEOS_ENABLE_ISR();
    
}


bool EOSSemaphoreGiveISR(EOSSemaphoreT semaphore)
{
    bool high_priority = false;
    EOSTaskT holding_task;
    
    portEOS_DISABLE_ISR();

    holding_task = EOS_GET_INDEX_FROM_LIST(semaphore->asking_tasks);
    
    //deinhirent priority
    if(holding_task != NULL && semaphore->original_priority != 0)
    {
        //check if task belong to any list
        if(holding_task->scheduler.parent_list != NULL){
            //check if task belong to ready list
            if(EOS_ITEM_BELONG_TO_LIST(ready_list[holding_task->priority], holding_task, scheduler)){
                //level down priority
                EOS_REMOVE_FROM_LIST(*((EOSListT *)holding_task->scheduler.parent_list), holding_task, scheduler);
                EOS_ADD_TO_LIST(ready_list[semaphore->original_priority], holding_task, scheduler);
            }
        }
        holding_task->priority = semaphore->original_priority;
        semaphore->original_priority = 0;
    }

    do{
        holding_task = EOS_GET_HEAD_FROM_LIST(semaphore->asking_tasks);
        EOS_SET_LIST_INDEX(semaphore->asking_tasks, holding_task);
    
        if(holding_task)
        {
            //reomove from asking_tasks
            EOS_REMOVE_FROM_LIST(semaphore->asking_tasks, holding_task, sync);
            
            if(holding_task->block_source == kEOSBlockSrcSemaphore){
                
                high_priority = holding_task->priority > eos_running_task->priority;
                holding_task->block_source = kEOSBlockSrcNone;
                
                //check if scheduler alredy deque task from ready lists
                if(!EOS_ITEM_BELONG_TO_LIST(ready_list[holding_task->priority], holding_task, scheduler) && holding_task->scheduler.parent_list != NULL)
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
    semaphore_buffer->asking_tasks.head = NULL;
    semaphore_buffer->asking_tasks.tail = NULL;
    semaphore_buffer->asking_tasks.index = NULL;
    semaphore_buffer->type = type;
    semaphore_buffer->original_priority = 0;
    return semaphore_buffer;
}