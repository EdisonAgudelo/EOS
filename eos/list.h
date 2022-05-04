
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
#define EOS_ADD_TO_LIST(list, task, link)               \
    do {                                                \
        if((list).head == NULL)                         \
        {                                               \
            (list).head = task;                         \
            (task)->link.prev = NULL;                   \
            (task)->link.next = NULL;                   \
            (list).tail = task;                         \
            (task)->link.parent_list = &(list);         \
        } else if((list).tail != NULL)                  \
        {                                               \
            (list).tail->link.next = task;              \
            (task)->link.prev = (list).tail;            \
            (task)->link.next = NULL;                   \
            (list).tail = task;                         \
            (task)->link.parent_list = &(list);         \
        } else                                          \
        {                                               \
            EOS_ASSERT(false);                          \
        }                                               \
    }while(0)

//remove a task from a list
#define EOS_REMOVE_FROM_LIST(list, task, link)              \
    do{                                                     \
        if((task)->link.prev == NULL)                           \
        {                                                       \
            (list).head = (task)->link.next;                    \
            if((list).head)                                     \
                (list).head->link.prev = NULL;                  \
        } else                                                  \
        {                                                       \
            (task)->link.prev->link.next = (task)->link.next;   \
        }                                                       \
        if((task)->link.next == NULL)                           \
        {                                                       \
            (list).tail = (task)->link.prev;                    \
            if((list).tail)                                     \
                (list).tail->link.next = NULL;                  \
        } else                                                  \
        {                                                       \
            (task)->link.next->link.prev = (task)->link.prev;   \
        }                                                       \
        (task)->link.next = NULL;                               \
        (task)->link.prev = NULL;                               \
        (task)->link.parent_list = NULL;                        \
    } while(0)

//index is "list depending" meaning. Eg. for ready_list
//it holds the next task to execute to allows true yielding 
//even when higer priority list preempt actual list
#define EOS_SET_LIST_INDEX(list, task)          \
    (list).index = task

//to get custom pointer from list
#define EOS_GET_INDEX_FROM_LIST(list)            ((list).index)
//the starting point of the list
#define EOS_GET_HEAD_FROM_LIST(list)            ((list).head)
//to go foward
#define EOS_GET_NEXT_FROM_ITEM(task, link)      ((task)->link.next)
//to go backward
#define EOS_GET_PREV_FROM_ITEM(task, link)      ((task)->link.prev)

#define EOS_ITEM_BELONG_TO_LIST(list, task, link)     ((task)->link.parent_list == (&(list)))

//insert operations

//intem <=> item <=> ref_task <=> item <=> item
//intem <=> item <=> ref_task <=> new_task <=> item <=> item
#define EOS_INSERT_NEXT_TO_ITEM_IN_LIST(list, ref_task, new_task, link)             \
    do{                                                                             \
        if((ref_task) != NULL)                                                      \
        {                                                                           \
            (new_task)->link.next = (ref_task)->link.next;                          \
            (ref_task)->link.next = new_task;                                       \
            (new_task)->link.prev = ref_task;                                       \
            (new_task)->link.parent_list = &(list);                                 \
            if((new_task)->link.next == NULL)                                       \
                (list).tail = new_task;                                             \
            else                                                                    \
                (new_task)->link.next->link.prev = new_task;                        \
        }                                                                           \
    }while(0)

//intem <=> item <=> ref_task <=> item <=> item
//intem <=> item <=> new_task <=> ref_task <=> item <=> item
#define EOS_INSERT_PREV_TO_ITEM_IN_LIST(list, new_task, ref_task, link)             \
    do{                                                                             \
        if((ref_task) != NULL)                                                      \
        {                                                                           \
            (new_task)->link.prev = (ref_task)->link.prev;                          \
            (ref_task)->link.prev = new_task;                                       \
            (new_task)->link.next = ref_task;                                       \
            (new_task)->link.parent_list = &(list);                                 \
            if((new_task)->link.prev == NULL)                                       \
                (list).head = new_task;                                             \
            else                                                                    \
                (new_task)->link.prev->link.next = new_task;                        \
        }                                                                           \
    }while(0)


#define EOS_TIME_DIFFERENCE(ref, act)           \
    ((ref) <= (act) ?                           \
    ((act) - (ref)) :                           \
    ((0xffffffff - ((ref) - (act))) + 1))


//calculate how many ticks is left even if ticks overflow //useful for long term applications
#define EOS_DELAY_REMAIN(unblock_tick)                      \
    EOS_TIME_DIFFERENCE(eos_tick, unblock_tick)

//list body
typedef struct {
    EOSTaskT head;
    EOSTaskT tail;
    EOSTaskT index;
}EOSListT;



#endif