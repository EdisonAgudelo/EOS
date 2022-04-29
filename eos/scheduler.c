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


#include "scheduler.h" 

//this allows critical section execution
//if there is no a general function to do that, you can manually disables 
//thos peripheral that may generate any EOS notification... for example the tick interrupt
#ifndef portEOS_DISABLE_ISR
#define portEOS_DISABLE_ISR()
#endif

#ifndef portEOS_ENABLE_ISR
#define portEOS_ENABLE_ISR()
#endif

//how many individual priorities is gonna exist. Task may share same priority number
#ifndef EOS_MAX_TASK_PRIORITY
#define EOS_MAX_TASK_PRIORITY 2
#endif

//task are handled in list... so the next macros is for list handling

//Add a task to list tail
#define EOS_ADD_TO_LIST(list, task)     \
    if((list).head == NULL)             \
    {                                   \
        (list).head = task;             \
        (task)->prev = NULL;            \
        (task)->next = NULL;            \
        (list).tail = task;             \
        (task)->belong_to_list = &list; \
    } else if((list).tail != NULL)      \
    {                                   \
        (list).tail->next = task;       \
        (task)->prev = (list).tail;     \
        (task)->next = NULL;            \
        (list).tail = task;             \
        (task)->belong_to_list = &list; \
    } else                              \
    {                                   \
        EOS_ASSERT(false);                   \
    }

//remove a task from a list
#define EOS_REMOVE_FROM_LIST(list, task)        \
    if((task)->prev == NULL)                    \
    {                                           \
        (list).head = (task)->next;             \
        if((list).head)                         \
            (list).head->prev = NULL;           \
    } else                                      \
    {                                           \
        (task)->prev->next = (task)->next;      \
    }                                           \
    if((task)->next == NULL)                    \
    {                                           \
        (list).tail = (task)->prev;             \
        if((list).tail)                         \
            (list).tail->next = NULL;           \
    } else                                      \
    {                                           \
        (task)->next->prev = (task)->prev;      \
    }                                           \
    (task)->next = NULL;                        \
    (task)->prev = NULL;                        \
    (task)->belong_to_list = NULL

//index is "list depending" meaning. Eg. for ready_list
//it holds the next task to execute to allows true yielding 
//even when higer priority list preempt actual list
#define EOS_SET_LIST_INDEX(list, task)          \
    (list).index = task

//the starting point of the list
#define EOS_GET_HEAD_FROM_LIST(list)     (list).head
//to go foward
#define EOS_GET_NEXT_FROM_ITEM(task)     (task)->next
//to go backward
#define EOS_GET_PREV_FROM_ITEM(task)     (task)->prev

//insert operations

//intem <=> item <=> ref_task <=> item <=> item
//intem <=> item <=> ref_task <=> new_task <=> item <=> item
#define EOS_INSERT_NEXT_TO_ITEM_IN_LIST(list, ref_task, new_task)   \
    if((ref_task) != NULL)                                          \
    {                                                               \
        (new_task)->next = (ref_task)->next;                        \
        (ref_task)->next = new_task;                                \
        (new_task)->prev = ref_task;                                \
        (new_task)->belong_to_list = &list;                         \
        if((new_task)->next == NULL)                                \
            (list).tail = new_task;                                 \
        else                                                        \
            (new_task)->next->prev = new_task;                      \
    }

//intem <=> item <=> ref_task <=> item <=> item
//intem <=> item <=> new_task <=> ref_task <=> item <=> item
#define EOS_INSERT_PREV_TO_ITEM_IN_LIST(list, new_task, ref_task)   \
    if((ref_task) != NULL)                                          \
    {                                                               \
        (new_task)->prev = (ref_task)->prev;                        \
        (ref_task)->prev = new_task;                                \
        (new_task)->next = ref_task;                                \
        (new_task)->belong_to_list = &list;                         \
        if((new_task)->prev == NULL)                                \
            (list).head = new_task;                                 \
        else                                                        \
            (new_task)->prev->next = new_task;                      \
    }

//calculate how many ticks is left even if ticks overflow //useful for long term applications
#define EOS_DELAY_REMAIN(unblock_tick)                      \
    (eos_tick <= (unblock_tick) ?                           \
    ((unblock_tick) - eos_tick) :                           \
    ((0xffffffff - (eos_tick - (unblock_tick))) + 1))

//list body
typedef struct {
    EOSTaskT head;
    EOSTaskT tail;
    EOSTaskT index;
}EOSListT;

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
                EOS_REMOVE_FROM_LIST(blocked_list, delayed_task);
                EOS_ADD_TO_LIST(ready_list[delayed_task->priority], delayed_task);
                delayed_task->ticks_to_delay = 0; //signals timeout
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
            
            return ((ready_list[priority].index == NULL) ? task_to_run : ready_list[priority].index);
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
    
    EOSCreateStaticTask(EOSIdleTask, NULL, 0, sizeof(idle_stack), idle_stack, &idle_task_buffer);

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
        EOS_SET_LIST_INDEX(ready_list[eos_running_task->priority], EOS_GET_NEXT_FROM_ITEM(eos_running_task));
        
        //then we can update task state moving through list owner
        switch(*((EOSTaskStateT *)eos_running_task->stack))
        {
            case kEOSTaskEnded://the task/function ended all its routines
                EOS_REMOVE_FROM_LIST(ready_list[eos_running_task->priority], eos_running_task);
                break;
            
            //just go ahead
            case kEOSTaskYield:

                break;
            case kEOSTaskBlocked:
            
                EOS_REMOVE_FROM_LIST(ready_list[eos_running_task->priority], eos_running_task);
                
                eos_running_task->unblock_tick = eos_tick + eos_running_task->ticks_to_delay;
                eos_running_task->tick_over_flow = eos_running_task->unblock_tick < eos_tick;
                    
                
                //lock for a task that have more delay than eos_running_task
                for(index_task = EOS_GET_HEAD_FROM_LIST(blocked_list); index_task != NULL; index_task = EOS_GET_NEXT_FROM_ITEM(index_task))
                {
                    if(EOS_DELAY_REMAIN(index_task->unblock_tick)>EOS_DELAY_REMAIN(eos_running_task->unblock_tick))
                        break;
                }
                if(index_task == NULL)
                {
                    EOS_ADD_TO_LIST(blocked_list, eos_running_task);
                } else {
                    EOS_INSERT_PREV_TO_ITEM_IN_LIST(blocked_list, eos_running_task, index_task);
                }
                
                break;
            case kEOSTaskSuspended:
                EOS_REMOVE_FROM_LIST(ready_list[eos_running_task->priority], eos_running_task);
                EOS_ADD_TO_LIST(suspended_list, eos_running_task);
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
EOSTaskT EOSCreateStaticTask(EOSTaskFunction task, void *args, uint8_t priority, uint32_t stack_size, EOSStaticStack *stack_buffer, EOSStaticTaskT *task_buffer)
{
    portEOS_DISABLE_ISR();
    
    task_buffer->stack = stack_buffer;
    task_buffer->stack_size = stack_size;
    task_buffer->args = args;
    task_buffer->task = task;
    task_buffer->state = kEOSTaskYield;
    task_buffer->unblock_tick = 0;
    task_buffer->priority = priority <= EOS_MAX_TASK_PRIORITY ? priority : EOS_MAX_TASK_PRIORITY;

    EOS_INIT_STACK(stack_buffer, stack_size);
    EOS_ADD_TO_LIST(ready_list[task_buffer->priority], task_buffer);
    
    portEOS_ENABLE_ISR();
    
    return task_buffer;
}



