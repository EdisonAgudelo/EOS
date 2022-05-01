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

#include "config.h"
#include "scheduler.h" 
#include "list.h"

#include <string.h>




//holds the actual tick of system
uint32_t eos_tick = 0;
//holds the actual executing task
EOSTaskT eos_running_task = NULL;

//0 is also a valid priority
//holds all tasks that are ready to execute. 
EOSListT ready_list[EOS_MAX_TASK_PRIORITY + 1] = {{NULL, NULL, NULL}};
//holds all tasks that have a bloking event with a finite timeout
//list head holds the task with less expire time and list tail holds the tasks
//with the great expire time.
EOSListT blocked_list = {NULL, NULL, NULL};
//holds all tasks that have been blocked but have an infinite timeout or was
//directly suspended
EOSListT suspended_list = {NULL, NULL, NULL};


//increments actual tick value and unblock those tasks that have reach maximum block time
void EOSTickIncrement(void)
{
    EOSTaskT delayed_task;
    
    eos_tick++;
    
    portEOS_DISABLE_ISR();
    
    do{
        delayed_task = EOS_GET_HEAD_FROM_LIST(blocked_list);
        
        if(delayed_task == NULL)
            break; //no blocked tasks
            
        //check if its time to unblock
        
        //if overflow we have to wait until that overflow "is cleared"
        //overflow happen when unblock_tick is less that actual tick count
        if(delayed_task->tick_over_flow)
        {
            if(delayed_task->unblock_tick >= eos_tick)
                delayed_task->tick_over_flow = false;
            else
                break;
            
        } else {
            if(delayed_task->unblock_tick <= eos_tick)
            {
                EOS_REMOVE_FROM_LIST(blocked_list, delayed_task, scheduler);
                EOS_ADD_TO_LIST(ready_list[delayed_task->priority], delayed_task, scheduler);
                delayed_task->ticks_to_delay = 0; //signals timeout
                delayed_task->block_source = kEOSBlockSrcNone;
                
                //sync lists
                if(delayed_task->sync.parent_list){
                    //reomove from semaphores or any other sync list
                    EOS_REMOVE_FROM_LIST(*((EOSListT *)delayed_task->sync.parent_list), delayed_task, sync);
                }
            } else 
            {
                break; //we just finish to unblock task waiting for the actual tick
            }
        }
        
    }while(1);
        
    portEOS_ENABLE_ISR();
    
    
}

//minimum executing unit
//this keeps scheduler always doing something if no task are ready... 
//otherwhise, it will crash
static void EOSIdleTask(EOSStackT stack, void *args)
{
    EOS_INIT(stack);
    
    EOS_BEGIN(); 
    while(1)
    {
        EOSIdleHook();
        EOS_YIELD();
    }
    EOS_END ();
}


//found the next task to execute.
//it prioritized task and allows yielding "with the index pointer"
static EOSTaskT EOSGetNextTaskToRun(void)
{
    EOSTaskT task_to_run;
    uint8_t priority = EOS_MAX_TASK_PRIORITY;
    

    portEOS_DISABLE_ISR();
    do {
        task_to_run = EOS_GET_HEAD_FROM_LIST(ready_list[priority]);
        if(task_to_run != NULL){
            
            portEOS_ENABLE_ISR();
            
            return ((EOS_GET_INDEX_FROM_LIST(ready_list[priority]) == NULL) ? task_to_run : ready_list[priority].index);
        }
    } while(priority--);
    
     portEOS_ENABLE_ISR();
    
    //never should reach here as we always have idle task as the last task to execute
    EOS_ASSERT(0);
    
    return NULL;
}

//the Scheduler itself. It is the only one allowed to remove task from de the ready list and add to others list
void EOSScheduler(void)
{
    EOSTaskT index_task = NULL;
  
    
    EOSStaticStack idle_stack[EOS_WATER_MARK_STACK_ROOM + EOS_MIN_STACK];
    EOSStaticTaskT idle_task_buffer;
    
    EOSCreateStaticTask(EOSIdleTask, NULL, 0, "Idle",sizeof(idle_stack), idle_stack, &idle_task_buffer);

    while(eos_tick < 40)
    {
        eos_running_task = EOSGetNextTaskToRun();
        
        eos_running_task->task(eos_running_task->stack, eos_running_task->args);
        EOS_ASSERT(!EOSCheckOverFlow(eos_running_task->stack, eos_running_task->stack_size));
        
        
        portEOS_DISABLE_ISR();
        
        //before any removal... save the next task to execute
        
        //get the nex canditade to run in the same priority level
        //we save this as the actual "first" element of the list... useful for true yielding
        //even wen there is multiple preemption
        EOS_SET_LIST_INDEX(ready_list[eos_running_task->priority], EOS_GET_NEXT_FROM_ITEM(eos_running_task, scheduler));
        
        //then we can update task state moving through list owner
        switch(*((EOSTaskStateT *)eos_running_task->stack))
        {
            case kEOSTaskEnded://the task/function ended all its routines
                EOS_REMOVE_FROM_LIST(ready_list[eos_running_task->priority], eos_running_task, scheduler);
                break;
            
            //just go ahead
            case kEOSTaskYield:

                break;
            case kEOSTaskBlocked:
            
                if(eos_running_task->block_source == kEOSBlockSrcNone)
                    break;
            
                EOS_REMOVE_FROM_LIST(ready_list[eos_running_task->priority], eos_running_task, scheduler);
                
                eos_running_task->unblock_tick = eos_tick + eos_running_task->ticks_to_delay;
                eos_running_task->tick_over_flow = eos_running_task->unblock_tick < eos_tick;
                    
                
                //lock for a task that have more delay than eos_running_task
                for(index_task = EOS_GET_HEAD_FROM_LIST(blocked_list); index_task != NULL; index_task = EOS_GET_NEXT_FROM_ITEM(index_task, scheduler))
                {
                    if(EOS_DELAY_REMAIN(index_task->unblock_tick)>EOS_DELAY_REMAIN(eos_running_task->unblock_tick))
                        break;
                }
                if(index_task == NULL)
                {
                    EOS_ADD_TO_LIST(blocked_list, eos_running_task, scheduler);
                } else {
                    EOS_INSERT_PREV_TO_ITEM_IN_LIST(blocked_list, eos_running_task, index_task, scheduler);
                }
                
                break;
            case kEOSTaskSuspended:
            
                if(eos_running_task->block_source == kEOSBlockSrcNone)
                    break;
                    
                EOS_REMOVE_FROM_LIST(ready_list[eos_running_task->priority], eos_running_task, scheduler);
                EOS_ADD_TO_LIST(suspended_list, eos_running_task, scheduler);
                break;
            
            default:
                EOS_ASSERT(false);
        }
        portEOS_ENABLE_ISR();
        
        //virtual tick for test purposes
        EOSTickIncrement();
    }
}


//init a task and add it to the ready list according to priority
EOSTaskT EOSCreateStaticTask(EOSTaskFunction task, void *args, uint8_t priority, char *name, uint32_t stack_size, EOSStaticStack *stack_buffer, EOSStaticTaskT *task_buffer)
{
    portEOS_DISABLE_ISR();
    
    task_buffer->stack = stack_buffer;
    task_buffer->stack_size = stack_size;
    task_buffer->args = args;
    task_buffer->task = task;
    
    task_buffer->unblock_tick = 0;
    task_buffer->priority = priority <= EOS_MAX_TASK_PRIORITY ? priority : EOS_MAX_TASK_PRIORITY;
    
    strncpy(task_buffer->name, name, EOS_TASK_MAX_NAME_LEN);
    task_buffer->name[EOS_TASK_MAX_NAME_LEN - 1] = '\0'; 
    
    task_buffer->mail_value = 0;
    task_buffer->mail_count = 0;
    task_buffer->block_source = kEOSBlockSrcNone;

    EOS_INIT_STACK(stack_buffer, stack_size);
    EOS_ADD_TO_LIST(ready_list[task_buffer->priority], task_buffer, scheduler);
    
    task_buffer->sync.parent_list = NULL;
    
    portEOS_ENABLE_ISR();
    
    return task_buffer;
}



