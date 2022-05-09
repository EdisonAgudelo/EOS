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

#include "eos.h"


bool EOSCheckOverFlow(EOSStackT stack, uint32_t size)
{
    uint8_t i = 0;
    while(--size && i < EOS_WATER_MARK_STACK_ROOM)
    {
        if(stack[size] != EOS_WATER_MARK_SYMBOL)
            return true;
        i++;   
    }
    return false;
}

void *EOSInternalBegin(EOSJumperT *eos_jumper, void *begin, void *end)
{
    EOS_ASSERT(eos_jumper != NULL);
    if(*eos_jumper == NULL) {
        *eos_jumper = begin;
    }
    //EOS_ASSERT(*eos_jumper >= begin);
    EOS_ASSERT((((size_t)*eos_jumper) & 0x1) == 0);
    return *eos_jumper;
}

void EOSInternalNestBegin(EOSStackT stack, EOSJumperT *jumper, void *fun_start)
{                                             
    EOSJumperT *nest_jumper;
    EOSTaskStateT *nest_state;
    
    EOS_STACK_POP(nest_state, stack);
    EOS_STACK_POP(nest_jumper, stack);
    EOS_ASSERT(nest_jumper != NULL);
    *nest_jumper = NULL;     
    *jumper = fun_start;
}

void *EOSInternalNestEnd(EOSStackT stack, EOSTaskStateT *state, void *nest_end, void *task_end)
{
    EOSTaskStateT *nest_stat;
    EOS_STACK_POP(nest_stat, stack);
    EOS_ASSERT(nest_stat != NULL);
    if(*nest_stat != kEOSTaskEnded){
        *state = *nest_stat;
        return  task_end;
    }
    return nest_end;
}

void EOSInternalInit(EOSStackT *stack, EOSJumperT **jumper, EOSTaskStateT **state){
    EOS_STACK_POP(*state, (*stack));
    EOS_STACK_POP(*jumper, (*stack));
    
    EOS_ASSERT((((size_t)*stack) & 0x1) == 0);
}