
#ifndef _EOS_H_
#define _EOS_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "eos_config.h"

#define _CONCAT(x,y) x ## y
#ifndef CONCAT
#define CONCAT(x,y) _CONCAT(x,y)
#endif

#ifndef EOS_ASSERT
#define EOS_ASSERT(cond)
#endif

//should be placed at top of any task or nest function
#define EOS_INIT(stack)                     \
    EOSJumperT *eos_jumper;                 \
    EOSTaskStateT *eos_task_state;          \
    EOS_STACK_POP(eos_task_state, stack);   \
    EOS_STACK_POP(eos_jumper, stack)         
    
//this goes after all context restoration, an points to the very beggining of the program
#define EOS_BEGIN()                                                                         \
    if(*eos_jumper == NULL) {*eos_jumper = &&EOS_BEGIN_LABEL;}                              \
    else {EOS_ASSERT(*eos_jumper >= &&EOS_BEGIN_LABEL && *eos_jumper <= &&EOS_END_LABEL);}      \
    goto **eos_jumper;                                                                      \
    EOS_BEGIN_LABEL:
  
//if an error happen... an nest function or a task should exit inmediately... call this routine  
#define EOS_EXIT()          \
    goto EOS_EXIT_LABEL

//should be placed at the bottom of the nest function or task before return or push copy operations
#define EOS_END()                       \
    EOS_EXIT_LABEL:                     \
    *eos_task_state = kEOSTaskEnded;    \
    EOS_END_LABEL:              

//Allow task yielding... this allow other task execution (copperative philosofy)
#define EOS_YIELD()                                                                 \
    do {                                                                            \
        *eos_jumper = &&CONCAT(EOS_YIELD_LABEL, __LINE__);                          \
        *eos_task_state = kEOSTaskYield;                                            \
        goto EOS_END_LABEL;                                                         \
        CONCAT(EOS_YIELD_LABEL, __LINE__):;                                         \
    } while(0)

//used to restore variables values from the stack. This is the pointer version
#define EOS_STACK_POP(ptr, stack)           \
    do{                                     \
        ptr = (void *)stack;                \
        stack += sizeof(*ptr);              \
    }while(0)
//used to return back a value to the stack... Normally this is not used in typical context implementation
#define EOS_STACK_PUSH(ptr, stack)          \
    do{                                     \
        stack -= sizeof(*ptr);              \
        ptr = NULL;                         \
    }while(0)
    
//used to restore variables values from the stack. This is the copy version
#define EOS_STACK_POP_COPY(var, stack)          \
    do{                                         \
        memcpy(&var, stack, sizeof(var));       \
        stack += sizeof(var);                   \
    }while(0)


//used to update a value to the stack... Normally this goes after EOS_END
#define EOS_STACK_PUSH_COPY(var, stack)         \
    do{                                         \
        stack -= sizeof(var);                   \
        memcpy(stack, &var, sizeof(var));       \
    }while(0)

//if you need to call a function that may yield or block the calling task. This should go before function call
#define EOS_NEST_BEGIN(stack)                               \
    *eos_jumper = &&CONCAT(EOS_NEST_FUNC_LABEL, __LINE__);  \
    do{                                                     \
        EOSJumperT *eos_temp_jumper;                        \
        EOSTaskStateT *eos_temp_task_state;                 \
        EOS_STACK_POP(eos_temp_task_state, stack);          \
        EOS_STACK_POP(eos_temp_jumper, stack);              \
        *eos_temp_jumper = NULL;                            \
        EOS_STACK_PUSH(eos_temp_task_state, stack);         \
        EOS_STACK_PUSH(eos_temp_jumper, stack);             \
    }while(0);                                              \
    CONCAT(EOS_NEST_FUNC_LABEL, __LINE__):

//if you need to call a function that may yield or block the calling task. This should go after function call    
#define EOS_NEST_END(stack) \
    do{ \
        EOSTaskStateT *eos_temp_task_stat;\
        EOS_STACK_POP(eos_temp_task_stat, stack); \
        if(*eos_temp_task_stat != kEOSTaskEnded){ \
            *eos_task_state = *eos_temp_task_stat; \
            goto EOS_END_LABEL; \
        }\
        EOS_STACK_PUSH(eos_temp_task_stat, stack); \
    }while(0)

//this is the minimun amout of bytes required per each stack an nested function
#define EOS_MIN_STACK (sizeof(EOSJumperT) + sizeof(EOSTaskStateT))
//to check stack over flow
#ifndef EOS_WATER_MARK_SYMBOL
#define EOS_WATER_MARK_SYMBOL 0x5a
#endif
#ifndef EOS_WATER_MARK_STACK_ROOM
#define EOS_WATER_MARK_STACK_ROOM 8
#endif

//if stack over flow checking is required... the this macro must be called before any task spawn
#define EOS_INIT_STACK(stack, size)                                                                             \
        do{                                                                                                     \
            memset(stack, 0, EOS_MIN_STACK);                                                                    \
            memset(&stack[EOS_MIN_STACK], EOS_WATER_MARK_SYMBOL, (size) - EOS_MIN_STACK);   \
        }while(0)
        

//used to remeber the last position "pc" in a task that is yielding
typedef void *EOSJumperT;
//this is the stack base type
typedef uint8_t EOSStaticStack;
//this is the type required in function or tasks
typedef EOSStaticStack *EOSStackT;
//posible task states
typedef enum{
    kEOSTaskEnded, //the task/function ended all its routines
    kEOSTaskYield,  //task is allowing to change execution task if required... but is a kind of ready signal
    kEOSTaskBlocked, //for those task which are waiting for a timeout
    kEOSTaskSuspended //indefinite block task
}EOSTaskStateT;

//check overflow routine
bool EOSCheckOverFlow(EOSStackT stack, uint32_t size);

//a typical task implementation with this basic features looks like
/*
#include <eos.h>

void thread (EOSStackT stack, void *args);

int main ()
{
    EOSStaticStack stack1[50];
    EOSStaticStack stack2[50];
    
    int id1 = 0;
    int id2 = 1;

    EOS_INIT_STACK(stack1, sizeof(stack1));
    EOS_INIT_STACK(stack2, sizeof(stack2));
    
    while (1)
    {
        thread (stack1, &id1);
        if(EOSCheckOverFlow(stack1, sizeof(stack1)))
        {
            printf("Stack over flow detected\n");
            break;
        }
        thread (stack2, &id2);
        if(EOSCheckOverFlow(stack2, sizeof(stack2)))
        {
            printf("Stack over flow detected\n");
            break;
        }
    }
    //should never reach here
    return 0;
}

void function2 (int origin)
{
    printf("fucntion2 called from %u\n", origin);
}

int function (EOSStackT stack, int origin)
{
    EOS_INIT(stack);
    
    int ret_val;
    
    EOS_STACK_POP_COPY(ret_val, stack);
    
    EOS_BEGIN();   
    
    ret_val = origin%2;
    
    EOS_YIELD();
    
    function2(origin);
  
    
    EOS_END ();
    
    EOS_STACK_PUSH_COPY(ret_val, stack);
    
    return ret_val;
}

void thread (EOSStackT stack, void *args)
{
    //local variables
    struct
    {
        int id;
        int count;
    } *l;
    
    int i;
    
    //should be called at top of fucntion
    EOS_INIT(stack);
    
    //restore local variables from stack... "contex switch"
    EOS_STACK_POP(l,stack);
    //for local copy... but volatile between yields ... so it is required a push at the end of task
    EOS_STACK_POP_COPY(i,stack);
    
    //here program starts
    EOS_BEGIN();
    
    l->count = 0;
    l->id = *(int *) args;
    
    printf ("thread %i begin\n", l->id);
    
    while (1)
    {
        switch (l->count)
        {
            case 0:
                for(i = 0; i<3; i++){
                    printf ("%i: 0\n", l->id);
                    EOS_YIELD ();
                }
                
                function2(l->id);
        
              break;
            case 1:
                printf ("%i: 1\n", l->id);
                
                EOS_NEST_BEGIN(stack);
                int is_odd = function(stack, l->id);
                EOS_NEST_END(stack);
                
                printf("%i: I am %s\n", l->id, is_odd?"odd":"even");
                
              break;
            default:
                printf ("%i: default\n", l->id);
              break;
        }
        printf ("%i: thread loop %i\n", l->id, l->count++);
        EOS_YIELD();
    }
    
    EOS_END ();
    
    EOS_STACK_PUSH_COPY(i,stack);
}
*/
#endif