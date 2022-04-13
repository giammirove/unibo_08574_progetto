#include "os/scheduler.h"
#include "os/syscall.h"
#include "os/types.h"
#include "test/mock_init.h"
#include "test/mock_syscall.h"
#include "test/mock_iodev.h"
#include "test/test.h"
#include "os/const.h"
#include "os/util.h"
#include <stdio.h>
/* TESTING NSYS7 */

void p1(){
    /* It is not supposed to do anything*/
}

int main()
{
    mock_init();
    active_process = spawn_process(false);
    pcb_t *duplicate_active_process = active_process;
    state_t proc1;
    set_state(&proc1, (memaddr) p1);
    ensure("wait_for_clock syscall blocks current process on the right semaphore")
    {
        SYSCALL(CLOCKWAIT, 0, 0, 0);
        assert(duplicate_active_process->p_sem_add == get_timer_semaphore());
        assert(softblock_count == 1);
        assert(process_count == 1);
    }

    return 0;
}
