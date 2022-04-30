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


#ifndef _EOS_SETTINGS_H_
#define _EOS_SETTINGS_H_


#include "eos_config.h"

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


#ifndef EOS_TASK_MAX_NAME_LEN 
#define EOS_TASK_MAX_NAME_LEN 4
#endif

#ifndef EOS_ASSERT
#define EOS_ASSERT(x)
#endif

#ifndef EOS_WATER_MARK_SYMBOL
#define EOS_WATER_MARK_SYMBOL 0x5a
#endif
#ifndef EOS_WATER_MARK_STACK_ROOM
#define EOS_WATER_MARK_STACK_ROOM 8
#endif

#endif