
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

#ifndef _EOS_LIST_H_
#define _EOS_LIST_H_


#include "scheduler.h"
//task are handled in list... so the next macros is for list handling

//Add a task to list tail
#define EOS_ADD_TO_LIST(list, task)         \
    if((list).head == NULL)                 \
    {                                       \
        (list).head = task;                 \
        (task)->prev = NULL;                \
        (task)->next = NULL;                \
        (list).tail = task;                 \
        (task)->parent_list = &(list);      \
    } else if((list).tail != NULL)          \
    {                                       \
        (list).tail->next = task;           \
        (task)->prev = (list).tail;         \
        (task)->next = NULL;                \
        (list).tail = task;                 \
        (task)->parent_list = &(list);      \
    } else                                  \
    {                                       \
        EOS_ASSERT(false);                  \
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
    (task)->parent_list = NULL

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

#define EOS_ITEM_BELONG_TO_LIST(list, task)     ((task)->parent_list == (&(list)))

//insert operations

//intem <=> item <=> ref_task <=> item <=> item
//intem <=> item <=> ref_task <=> new_task <=> item <=> item
#define EOS_INSERT_NEXT_TO_ITEM_IN_LIST(list, ref_task, new_task)   \
    if((ref_task) != NULL)                                          \
    {                                                               \
        (new_task)->next = (ref_task)->next;                        \
        (ref_task)->next = new_task;                                \
        (new_task)->prev = ref_task;                                \
        (new_task)->parent_list = &(list);                          \
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
        (new_task)->parent_list = &(list);                          \
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



#endif