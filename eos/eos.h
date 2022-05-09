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

#ifndef _EOS_H_
#define _EOS_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "config.h"

#define _CONCAT(x,y) x ## y
#ifndef CONCAT
#define CONCAT(x,y) _CONCAT(x,y)
#endif


//should be placed at top of any task or nest function
#define EOS_INIT(stack)                                         \
    EOSJumperT *eos_jumper;                                     \
    EOSTaskStateT *eos_task_state;                              \
    EOSInternalInit(&(stack), &eos_jumper, &eos_task_state);

    
//this goes after all context restoration, an points to the very beggining of the program
#define EOS_BEGIN()     \
    goto *EOSInternalBegin(eos_jumper, &&EOS_BEGIN_LABEL, &&EOS_END_LABEL);                       \
    EOS_BEGIN_LABEL:
  
//if an error happen... an nest function or a task should exit inmediately... call this routine  
#define EOS_EXIT()                              \
        do {                                    \
            *eos_task_state = kEOSTaskEnded;    \
            goto EOS_END_LABEL;                 \
        }while(0)

//should be placed at the bottom of the nest function or task before return or push copy operations
#define EOS_END()                       \
    *eos_task_state = kEOSTaskEnded;    \
    EOS_END_LABEL:              

//Allow task yielding... this allows other task execution (cooperative philosophy)
#define EOS_YIELD()                                                                 \
    do {                                                                            \
        *eos_jumper = &&CONCAT(EOS_YIELD_LABEL, __LINE__);                          \
        *eos_task_state = kEOSTaskYield;                                            \
        goto EOS_END_LABEL;                                                         \
        CONCAT(EOS_YIELD_LABEL, __LINE__):;                                         \
    } while(0)

//used to restore variables values from the stack. This is the pointer version (recommended)
#define EOS_STACK_POP(ptr, stack)           \
    do{                                     \
        ptr = ((void *)stack);             \
        stack += sizeof(size_t) * (1 + ((sizeof(*ptr)- 1) / sizeof(size_t)));               \
    }while(0)
//used to return back a value to the stack... Normally this is not used in typical context implementation
#define EOS_STACK_PUSH(ptr, stack)          \
    do{                                     \
        stack -= sizeof(size_t) * (1 + ((sizeof(*ptr)- 1) / sizeof(size_t)));               \
        ptr = NULL;                         \
    }while(0)
    
//used to restore variables values from the stack. This is the copy version and should be only used 
//for variables that doen's requiere to "stay in memory" when task is no executing (like a index). 
//It is recommended for variables that doesn't give a memory reference to other functions.
#define EOS_STACK_POP_COPY(var, stack)          \
    {                                         \
        memcpy(&var, stack, sizeof(var));       \
        stack += sizeof(size_t) * (1 + ((sizeof(var)- 1) / sizeof(size_t)));                   \
    }while(0)


//used to update a value to the stack... Normally this goes after EOS_END
#define EOS_STACK_PUSH_COPY(var, stack)         \
    do{                                         \
        stack -= sizeof(size_t) * (1 + ((sizeof(var)- 1) / sizeof(size_t)));                    \
        memcpy(stack, &var, sizeof(var));       \
    }while(0)

//if you need to call a function that may yield or block the executing task. This should go before function call
#define EOS_NEST_BEGIN(stack)                               \
    EOSInternalNestBegin(stack, eos_jumper, &&CONCAT(EOS_NEST_START_LABEL, __LINE__));          \
    CONCAT(EOS_NEST_START_LABEL, __LINE__):

//if you need to call a function that may yield or block the calling task. This should go after function call    
#define EOS_NEST_END(stack)                                 \
    goto *EOSInternalNestEnd(stack, eos_task_state,       \
                &&CONCAT(EOS_NEST_END_LABEL, __LINE__),     \
                &&EOS_END_LABEL);                           \
    CONCAT(EOS_NEST_END_LABEL, __LINE__):
    

//this is the minimun amout of bytes required per each stack an nested function
#define EOS_MIN_STACK (sizeof(EOSJumperT) + sizeof(EOSTaskStateT))
//to check stack over flow


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
    kEOSTaskSuspended, //indefinite block task
    kEOSTaskRunning //only for task information purpuse. Not used in operation logic
}EOSTaskStateT;

//check overflow routine
bool EOSCheckOverFlow(EOSStackT stack, uint32_t size);

//Shouldn't be called directly by user
void *EOSInternalBegin(EOSJumperT *eos_jumper, void *begin, void *end);
void EOSInternalNestBegin(EOSStackT stack, EOSJumperT *jumper, void *fun_start);
void *EOSInternalNestEnd(EOSStackT stack, EOSTaskStateT *state, void *nest_end, void *task_end);
void EOSInternalInit(EOSStackT *stack, EOSJumperT **jumper, EOSTaskStateT **state);
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