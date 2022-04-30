
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
extern EOSListT blocked_list;
extern EOSListT suspended_list;


void EOSMailSend(EOSTaskT task, uint32_t mail)
{
    portEOS_DISABLE_ISR();
    task->mail_value = mail;
    task->mail_count++;
    if(task->block_source == kEOSBlockSrcMail){
        task->block_source = kEOSBlockSrcNone;
        if(!EOS_ITEM_BELONG_TO_LIST(ready_list[task->priority], task) && task->parent_list != NULL)
        {
            //reomove from either, block or suspended
            EOS_REMOVE_FROM_LIST(*((EOSListT *)task->parent_list), task);
            EOS_ADD_TO_LIST(ready_list[task->priority], task);
        }
    }   
    portEOS_ENABLE_ISR();
}