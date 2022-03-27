/**
 * \file
 * \brief Schedule awating processes.
 *
 * \author Luca Tagliavini
 * \author Stefano Volpe
 * \date 20-03-2022
 */

#include "os/scheduler.h"
#include "os/asl.h"
#include "os/list.h"
#include "os/pcb.h"
#include "os/util.h"
#include "umps/libumps.h"

int running_count;
int blocked_count;
list_head ready_queue_hi, ready_queue_lo;
pcb_t *active_process;

/* Always points to the pid of the most recently created process */
static int pid_count = 0;

inline pcb_t *P(int *sem_addr, pcb_t *p)
{
    if (*sem_addr > 0) {
        *sem_addr = *sem_addr - 1;
        return p;
    } else {
        /* TODO: dequeing here is useless if the p is the current_process */
        dequeue_process(p);
        int r = insert_blocked(sem_addr, p);

        if (r > 0) {

            if(r == 3 && p->p_sem_add == sem_addr) {
                stderr("PASSEREN same addr %d\n", r);
                PANIC();
            }else  {
            //scheduler_panic("PASSEREN failed %d");
                stderr("PASSEREN failed %d\n", r);
                PANIC();
            }
        }

        return NULL;
    }
}

inline pcb_t *V(int *sem_addr)
{
    pcb_t *p = remove_blocked(sem_addr);
    if (p == NULL) { /* means that sem_proc is empty */
        *sem_addr = *sem_addr + 1;
    } else {
        enqueue_process(p);
    }

    return p;
}

void init_scheduler()
{
    running_count = 0;
    blocked_count = 0;
    mk_empty_proc_q(&ready_queue_hi);
    mk_empty_proc_q(&ready_queue_lo);
    active_process = NULL;
}

pcb_t *spawn_process(bool priority)
{
    pcb_t *p = alloc_pcb();
    if (p == NULL) {
        return NULL;
    }
    p->p_pid =
        ++pid_count; /* TODO: Change this with the actual implementation */
    p->p_prio = priority;
    ++running_count;
    enqueue_process(p);
    return p;
}

inline void enqueue_process(pcb_t *p)
{
    insert_proc_q(p->p_prio ? &ready_queue_hi : &ready_queue_lo, p);
}

inline void dequeue_process(pcb_t *p)
{
    out_proc_q(p->p_prio ? &ready_queue_hi : &ready_queue_lo, p);
}

void kill_process(pcb_t *p)
{
    --running_count;

    /* In case it is blocked by a semaphore*/
    out_blocked(p);

    /* In case it is in the ready queue */
    dequeue_process(p);

    /* Set pcb as free */
    free_pcb(p);
}

void schedule()
{
    if (!list_empty(&ready_queue_hi))
        active_process = remove_proc_q(&ready_queue_hi);
    else if (!list_empty(&ready_queue_lo))
        active_process = remove_proc_q(&ready_queue_lo);
    else
        scheduler_wait();

    /* This point should never be reached unless processes have been
     * re-scheduled (i.e. when waiting for events in a soft blocked state ) */
    scheduler_takeover();
}
