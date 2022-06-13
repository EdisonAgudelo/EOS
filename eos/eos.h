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

/**
 * @file eos.h
 * @author Edison Agudelo
 * @version 1.0.0
 * @copyright Copyright (c) 2022 Edison Agudelo
 * @brief RTOS Kernel.
 * 
 * This file defines backbone elements of EOS. It basically allows handling 
 * context, pc, and stack "restoration", and defines the most basic element 
 * for "multithreading", yield operation. All other files rely on this 
 * implementation. It is not required to include eos.h directly or in any specific 
 * order, unless you just use this API.
 * 
 */

///@cond
#ifndef _EOS_H_
#define _EOS_H_
///@endcond

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "config.h"


/**
 * @brief Macro Name Concatenation.
 * 
 * It is similar to \ref CONCAT, but this will concatenate anything inside 
 * without expand first its values (in cases where any of both 
 * concatenation items are a macro).
 * 
 * This is not used directly in EOS library.
 */
#define _CONCAT(x,y) x ## y
#ifndef CONCAT
/**
 * @brief Macro Concatenation.
 * 
 * This Macro concatenates macro names with any other macro or symbol. 
 * If Macro has any value, that value will be concatenated. This is 
 * really important for OES kernel jumps (due to it helps to generate 
 * unique labels with __LINE__ macro).
 * 
 * For example, if there is this piece of code:
 * @code
 *  //...       
 *  
 *  CONCAT(MY_LABEL_JUMP, __LINE__):
 *  
 *  //...
 * @endcode 
 * 
 * Considering that __LINE___ is 26, the macro will expand to:
 * @code
 *  //...
 * 
 *  MY_LABEL_JUMP26:
 * 
 *  //...
 * 
 * @endcode
 * 
 */
#define CONCAT(x,y) _CONCAT(x,y)
#endif


/**
 * @brief INIT EOS Context.
 * 
 * This Macro should be placed in every function where it is going to 
 * handle any kind of EOS API. Normally Task functions and nested functions. 
 * There is a known exception when it can be ignored, and that is when a 
 * function is just a wrapper.
 * 
 * It should go before any function body, and can go after variable declaration.
 * 
 * @code
 * //recommended way.
 * void Task1(EOSStackT stack) {
 *  int i;
 * 
 *  EOS_INIT(stack);
 * 
 *  //...
 * }
 * 
 * //it is also ok.
 * void Task2(EOSStackT stack) {
 *  EOS_INIT(stack);
 * 
 *  int i;
 * 
 *  //...
 * }
 * 
 * //here EOS_INIT can be ignored
 * int FunctionWrapper(EOSStackT stack, int option){
 *      switch (option) {
 *          case 0:
 *              return Function0(stack);
 *          case 23:
 *              return Function23(stack);
 * 
 *          //...
 *      }
 * }
 * 
 * @endcode
 * 
 * @param[in] stack Stack variable to initialize. 
 *                  Every Task function or nested function must be provided with one @ref EOSStackT.
 * @return Nothing.
 */
#define EOS_INIT(stack)                                         \
    EOSJumperT *eos_jumper;                                     \
    EOSTaskStateT *eos_task_state;                              \
    EOSInternalInit(&(stack), &eos_jumper, &eos_task_state);

    
/**
 * @brief EOS Task or nest function start point.
 * 
 * All code placed after this macro is going to execute as a normal 
 * task with the expected behaviors. What it does internally is to 
 * go to the last execution point where it was in a previous context 
 * switch command, If it is the first call just start executing from 
 * "the beginning".
 * 
 * @warning Any code placed before @ref EOS_BEGIN macro has no granted 
 * behavior except for cases where documentation explicitly request it.
 * 
 * This macro must be called if @ref EOS_INIT is placed, normally after @ref EOS_INIT.
 * 
 * @code
 * void Task1(EOSStackT stack) {
 *  int i;
 * 
 *  EOS_INIT(stack);
 *  
 *  //any instruction coding before this macro has no granted behavior.
 *  EOS_BEGIN();
 * 
 *  //...
 * }
 * @endcode
 * 
 * @return Nothing.
 * 
 */
#define EOS_BEGIN()     \
    goto *EOSInternalBegin(eos_jumper, &&EOS_BEGIN_LABEL, &&EOS_END_LABEL);                       \
    EOS_BEGIN_LABEL:


/**
 * @brief Task or nest function end point or return point.
 * 
 * @ref EOS_END tells the kernel where the function or task has ended. It must 
 * be placed ant the button of all instruction if @ref EOS_INIT is called.
 * 
 * @warning Any code after @ref EOS_END has no granted behavior except for 
 * cases where documentation explicitly requests it.
 * 
 * @code 
 * 
 * void Task1(EOSStackT stack) {
 *  int i;
 * 
 *  EOS_INIT(stack);
 *  
 *  EOS_BEGIN();
 * 
 *  //...
 * 
 *  // "Normal" execution code
 * 
 *  //...
 * 
 *  EOS_END();
 *  //any instruction coding after this macro has no granted behavior.
 * }
 * @endcode
 * 
 * 
 * @warning All tasks or nested functions must always end in the same place. And that 
 * is marked by @ref EOS_END macro. This means that any return statement before 
 * @ref EOS_END macro must be avoided.
 * 
 * @warning It is only possible to have 1 @ref EOS_END macro call per function 
 * or task. If you need multiple endpoints or return points, use @ref EOS_EXIT 
 * instead of return. If you need to return any constant or variable, make sure 
 * that the only return statement returns a variable preloaded with the desired 
 * return value.
 * 
 * @code 
 * 
 * int Task1(EOSStackT stack) {
 *  int ret_val;
 * 
 *  EOS_INIT(stack);
 *  EOS_BEGIN();
 * 
 *  //...
 * 
 *  if (BadCondition)
 *     return -1;   // wrong. Not allowed!
 * 
 * // Use this instead
 *  if (BadCondition) {
 *      ret_val = -1;
 *      EOS_EXIT(); //this jumps directly to EOS_END
 * }
 * 
 *  //...
 * 
 *  //Consider that if the code reaches this point, then no error has 
 *  //occurred, so return a no error value. In this example, 0.
 *  ret_val = 0;
 * 
 *  EOS_END();
 *  return ret_val; //Always goes at the end of function
 * }
 * 
 * @endcode 
 * 
 * @return Nothing.
 * 
 */
#define EOS_END()                       \
    *eos_task_state = kEOSTaskEnded;    \
    EOS_END_LABEL:

/**
 * @brief Abort function execution.
 * 
 * When you have to return inside a function body, the only way to do this is by 
 * this macro. This is because, as @ref EOS_END explains, there is a constraint 
 * on where the return label can be placed. So this helps to "fix" that constraint.
 * 
 * @note \ref EOS_EXIT can be called on a "success" or in an "error" event. The 
 * user itself has this liberty yet. \ref EOS_EXIT only makes the program jump to 
 * the end part of the function and notify the EOS kernel that that function has 
 * just ended.
 * 
 * @warning Also note that this is normally called in nested functions, as tasks 
 * are normally running forever. Calling this in a task body will finish its 
 * execution immediately.
 * 
 * @return Nothing.
 * 
 */
#define EOS_EXIT()                              \
        do {                                    \
            *eos_task_state = kEOSTaskEnded;    \
            goto EOS_END_LABEL;                 \
        }while(0)

         
/**
 * @brief Allow task switching. 
 * 
 * This is the most basic multi-thread element available in EOS. When a task 
 * starts to execute, the only way for EOS can perform “task switching”, is 
 * when the running task has been blocked or has issued a yielding command. 
 * The yield itself doesn't block the task execution, it just advises the EOS 
 * that it can run another task if available (It may look for a higher priority 
 * task if scheduler is used or run other tasks in the same priority level).
 * 
 */
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