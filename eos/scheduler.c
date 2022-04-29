




#include "scheduler.h" 


#ifndef portEOS_DISABLE_ISR
#define portEOS_DISABLE_ISR()
#endif

#ifndef portEOS_ENABLE_ISR
#define portEOS_ENABLE_ISR()
#endif

#ifndef EOS_MAX_TASK_PRIORITY
#define EOS_MAX_TASK_PRIORITY 2
#endif


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

#define EOS_SET_LIST_INDEX(list, task)          \
    (list).index = task

#define EOS_GET_HEAD_FROM_LIST(list)     (list).head
#define EOS_GET_NEXT_FROM_ITEM(task)     (task)->next
#define EOS_GET_PREV_FROM_ITEM(task)     (task)->prev


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
    
#define EOS_DELAY_REMAIN(unblock_tick)                      \
    (eos_tick <= (unblock_tick) ?                           \
    ((unblock_tick) - eos_tick) :                           \
    ((0xffffffff - (eos_tick - (unblock_tick))) + 1))


typedef struct {
    EOSTaskT head;
    EOSTaskT tail;
    EOSTaskT index;
}EOSListT;

uint32_t eos_tick = 0;
EOSTaskT eos_running_task = NULL;

//0 is also a valid priority
EOSListT ready_list[EOS_MAX_TASK_PRIORITY + 1] = {{NULL, NULL, NULL}};
EOSListT blocked_list = {NULL, NULL, NULL};
EOSListT suspended_list = {NULL, NULL, NULL};



void EOSTickIncrement(void)
{
    EOSTaskT delayed_task;
    
    eos_tick++;
    
    portEOS_DISABLE_ISR();
    
    do{
        delayed_task = EOS_GET_HEAD_FROM_LIST(blocked_list);
        
        if(delayed_task == NULL)
            break; //no delayed task
            
        //check if its time to unblock
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
                delayed_task->ticks_to_delay = 0; //basically signals timeout
            } else 
            {
                break; //we just finish to unblock task
            }
        }
        
    }while(1);
        
    portEOS_ENABLE_ISR();
    
    
}


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


static EOSTaskT EOSGetNextTaskToRun(EOSTaskT actual_task)
{
    EOSTaskT task_to_run;
    uint8_t priority = EOS_MAX_TASK_PRIORITY;
    
    /*
        if actual_task is in the current most prioritized list
        this loop should found the owner list of the actual_task. So to no affect yield 
        operations, we return te actual_task (which is a task that was preselected to
        to run, but it isn't executed yet)
    */
    portEOS_DISABLE_ISR();
    do {
        task_to_run = EOS_GET_HEAD_FROM_LIST(ready_list[priority]);
        if(task_to_run != NULL){
            
            portEOS_ENABLE_ISR();
            
            if(actual_task == NULL || actual_task->priority < priority)
                return ((ready_list[priority].index == NULL) ? task_to_run : ready_list[priority].index);
                
            return actual_task;
        }
    } while(priority--);
    
     portEOS_ENABLE_ISR();
    
    //never should reach here as we always have idle task as the last task to execute
    EOS_ASSERT(0);
    
    return NULL;
}

void EOSScheduler(void)
{
    EOSTaskT process_task = NULL;
    EOSTaskT index_task = NULL;
  
    
    EOSStaticStack idle_stack[EOS_WATER_MARK_STACK_ROOM + EOS_MIN_STACK];
    EOSStaticTaskT idle_task_buffer;
    
    EOSCreateStaticTask(EOSIdleTask, NULL, 0, sizeof(idle_stack), idle_stack, &idle_task_buffer);

    while(eos_tick < 40)
    {
        //check it this precanditade it's the most prioritized
        eos_running_task = EOSGetNextTaskToRun(eos_running_task);
        
        eos_running_task->task(eos_running_task->stack, eos_running_task->args);
        EOS_ASSERT(!EOSCheckOverFlow(eos_running_task->stack, eos_running_task->stack_size));
        
        //before any removal... get the next one
        process_task = eos_running_task;
        
        portEOS_DISABLE_ISR();
        
        //get the nex precanditade to run in the same priority level
        eos_running_task = EOS_GET_NEXT_FROM_ITEM(process_task);
        //we save which is de actual "first" element of the list... useful for true yielding
        //even wen there is multiple preemption
        EOS_SET_LIST_INDEX(ready_list[process_task->priority], eos_running_task);
        
        switch(*((EOSTaskStateT *)process_task->stack))
        {
            case kEOSTaskEnded://the task/function ended all its routines
                EOS_REMOVE_FROM_LIST(ready_list[process_task->priority], process_task);
                break;
            
            //just go ahead
            case kEOSTaskYield:

                break;
            case kEOSTaskBlocked:
            
                EOS_REMOVE_FROM_LIST(ready_list[process_task->priority], process_task);
                
                process_task->unblock_tick = eos_tick + process_task->ticks_to_delay;
                process_task->tick_over_flow = process_task->unblock_tick < eos_tick;
                    
                
                //lock for a task that have more delay than process_task
                for(index_task = EOS_GET_HEAD_FROM_LIST(blocked_list); index_task != NULL; index_task = EOS_GET_NEXT_FROM_ITEM(index_task))
                {
                    if(EOS_DELAY_REMAIN(index_task->unblock_tick)>EOS_DELAY_REMAIN(process_task->unblock_tick))
                        break;
                }
                if(index_task == NULL)
                {
                    EOS_ADD_TO_LIST(blocked_list, process_task);
                } else {
                    EOS_INSERT_PREV_TO_ITEM_IN_LIST(blocked_list, process_task, index_task);
                }
                
                break;
            case kEOSTaskSuspended:
                EOS_REMOVE_FROM_LIST(ready_list[process_task->priority], process_task);
                EOS_ADD_TO_LIST(suspended_list, process_task);
                break;
            
            default:
                EOS_ASSERT(false);
        }
        portEOS_ENABLE_ISR();
        
        //virtual tick
        EOSTickIncrement();
    }
}

EOSTaskT EOSCreateStaticTask(EOSTask task, void *args, uint8_t priority, uint32_t stack_size, EOSStaticStack *stack_buffer, EOSStaticTaskT *task_buffer)
{
    task_buffer->stack = stack_buffer;
    task_buffer->stack_size = stack_size;
    task_buffer->args = args;
    task_buffer->task = task;
    task_buffer->state = kEOSTaskYield;
    task_buffer->unblock_tick = 0;
    task_buffer->priority = priority <= EOS_MAX_TASK_PRIORITY ? priority : EOS_MAX_TASK_PRIORITY;

    EOS_INIT_STACK(stack_buffer, stack_size);
    EOS_ADD_TO_LIST(ready_list[task_buffer->priority], task_buffer);
    
    return task_buffer;
}



