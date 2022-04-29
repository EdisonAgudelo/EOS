

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
