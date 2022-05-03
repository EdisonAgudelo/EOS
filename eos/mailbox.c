
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


#include "mailbox.h"
#include "list.h"


extern EOSListT ready_list[];


//return tru if higer priority was woken
bool EOSMailSendISR(EOSTaskT task, uint32_t mail)
{
    bool high_priority = false;
    portEOS_DISABLE_ISR();
    task->mail_value = mail;
    task->mail_count++;
    if(task->block_source == kEOSBlockSrcMail){
        high_priority = task->priority > eos_running_task->priority;
        task->block_source = kEOSBlockSrcNone;
        if(task->scheduler.parent_list != NULL && !EOS_ITEM_BELONG_TO_LIST(ready_list[task->priority], task, scheduler))
        {
            //reomove from either, block or suspended
            EOS_REMOVE_FROM_LIST(*((EOSListT *)task->scheduler.parent_list), task, scheduler);
            EOS_ADD_TO_LIST(ready_list[task->priority], task, scheduler);
        }
    }   
    portEOS_ENABLE_ISR();
    return high_priority;
}

//when call this function 
//eos_running_task->ticks_to_delay = (ticks_to_wait);
//and success has a default value of true
//shouldn't be called directly by user
void *EOSInternalMailWait(uint32_t *message, EOSJumperT *jumper, EOSTaskStateT *state, bool *success,
                            void *wait, void *mail_end, void *task_end)
{           
    //we enter in a critical section as all of below can fail if EOSMailSendISR is called
    portEOS_DISABLE_ISR(); 
    /*
    First check mail content for any recepction mail count. (wich indicates if any mailbox 
    was received). Take into account that if a  mail wait exipires  and  blocked task starts to 
    execute and right before call this function, EOSMailSendISR is called, this logic will
    make a success return (even with the expired time).
    */                                      
    if(eos_running_task->mail_count != 0){                       
        eos_running_task->mail_count--;                          
        *message = eos_running_task->mail_value;                 
        portEOS_ENABLE_ISR();                                  
        return mail_end;                                  
    }
    //if wait mail expires or no ticks to wait given... 
    if((eos_running_task->ticks_to_delay) == 0)                                     
    {                                                                               
        *success = false;      
        portEOS_ENABLE_ISR();                                                  
        return mail_end;                                  
    }
    //block task until EOSMailSendISR or time expire
    *jumper = wait;                          
    eos_running_task->block_source = kEOSBlockSrcMail;                              
    *state = ((eos_running_task->ticks_to_delay) == EOS_INFINITE_TICKS) ?                     
                        kEOSTaskSuspended : kEOSTaskBlocked;  
    portEOS_ENABLE_ISR();                   
    return task_end;                                                                   
}