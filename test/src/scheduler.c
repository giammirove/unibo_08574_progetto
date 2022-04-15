/**
 * \file scheduler.c
 * \brief Unit tests for the scheduler core.
 *
 * \author Luca Tagliavini
 * \date 11-04-2022
 */

#include "os/scheduler.h"
#include "os/const.h"
#include "os/pcb.h"
#include "os/scheduler_impl.h"
#include "os/types.h"
#include "test/mock_init.h"
#include "test/test.h"
#include <math.h>

int main()
{
    mock_init();
    /* The two following tests are to prevent regressions */
    ensure("enqueue_process does nothing with wrong input")
    {
        enqueue_process(NULL);
        assert(list_size(&ready_queue_hi) == 0);
        assert(list_size(&ready_queue_lo) == 0);
    }
    ensure("enqueue_process does not increment the running count")
    {
        size_t rc = process_count;
        pcb_t *p = alloc_pcb();
        enqueue_process(p);
        assert(process_count == rc);
        dequeue_process(p);
        free_pcb(p);
    }
    ensure("enqueue_process does not break a previous queue")
    {
        list_head head = LIST_HEAD_INIT(head);
        pcb_t *p = alloc_pcb();
        insert_proc_q(&head, p);
        assert(!list_null(&p->p_list));
        assert(!list_empty(&head));
        assert(list_contains(&p->p_list, &head));
        enqueue_process(p);
        assert(!list_null(&p->p_list));
        assert(list_empty(&head));
        assert(!list_contains(&p->p_list, &head));
        dequeue_process(p);
        free_pcb(p);
    }
    ensure("enqueue_process inserts the process in the appropriate queue")
    {
        pcb_t *p1 = alloc_pcb();
        pcb_t *p2 = alloc_pcb();
        p1->p_prio = true;
        p2->p_prio = false;
        enqueue_process(p1);
        enqueue_process(p2);
        assert(list_contains(&p1->p_list, &ready_queue_hi));
        assert(!list_contains(&p1->p_list, &ready_queue_lo));
        assert(list_contains(&p2->p_list, &ready_queue_lo));
        assert(!list_contains(&p2->p_list, &ready_queue_hi));
        dequeue_process(p1);
        dequeue_process(p2);
        free_pcb(p1);
        free_pcb(p2);
    }

    ensure("dequeue_process errors with wrong input")
    {
        assert(dequeue_process(NULL) == NULL);
        pcb_t *p = alloc_pcb();
        assert(list_null(&p->p_list));
        assert(dequeue_process(NULL) == NULL);
        assert(list_null(&p->p_list));

        /* The process is in a list but it's not a ready list */
        list_head head = LIST_HEAD_INIT(head);
        insert_proc_q(&head, p);
        assert(!list_null(&p->p_list));
        assert(!list_empty(&head));
        assert(list_contains(&p->p_list, &head));
        dequeue_process(p);
        assert(!list_null(&p->p_list));
        assert(!list_empty(&head));
        assert(list_contains(&p->p_list, &head));
        free_pcb(p);
    }
    ensure("dequeue_process correctly removes a process from its queue")
    {
        pcb_t *p1 = spawn_process(false);
        assert(process_count == 1);
        assert(!list_null(&p1->p_list));
        assert(list_contains(&p1->p_list, &ready_queue_lo));
        dequeue_process(p1);
        assert(list_empty(&ready_queue_lo));
        assert(!list_contains(&p1->p_list, &ready_queue_lo));
        kill_progeny(p1);
        --process_count;

        pcb_t *p2 = spawn_process(true);
        assert(process_count == 1);
        assert(!list_null(&p2->p_list));
        assert(list_contains(&p2->p_list, &ready_queue_hi));
        dequeue_process(p2);
        assert(list_empty(&ready_queue_hi));
        assert(!list_contains(&p2->p_list, &ready_queue_hi));
        kill_progeny(p2);
        --process_count;
    }

    ensure("the constant MAX_PROC_BITS is right")
    {
        assert(pow(2, MAX_PROC_BITS) >= MAX_PROC);
        assert(MAX_PROC_BITS < sizeof(pandos_pid_t) * BYTELENGTH);
    }
    ensure("spawn_process generates the right pid")
    {
        size_t recycle = get_recycle_count();
        int first_id =
            head_proc_q((list_head *)get_pcb_free()) - get_pcb_table();
        pcb_t *p1 = spawn_process(false);
        int second_id =
            head_proc_q((list_head *)get_pcb_free()) - get_pcb_table();
        pcb_t *p2 = spawn_process(false);
        assert(p1->p_pid != -1);
        assert(p2->p_pid != -1);
        assert(mask_pid_id(p1->p_pid) == first_id);
        assert(mask_recycle(p1->p_pid) == recycle);
        assert(mask_pid_id(p2->p_pid) == second_id);
        assert(mask_recycle(p2->p_pid) == recycle + 1);
        kill_progeny(p1);
        int third_id =
            head_proc_q((list_head *)get_pcb_free()) - get_pcb_table();
        p1 = spawn_process(false);
        assert(mask_pid_id(p1->p_pid) == third_id);
        assert(mask_recycle(p1->p_pid) == recycle + 2);
        kill_progeny(p1);
        kill_progeny(p2);
    }
    return 0;
}
