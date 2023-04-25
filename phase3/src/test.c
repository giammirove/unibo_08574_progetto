/**
 * \file test.c
 * \brief Phase 3 test program and helper functions
 *
 * \author Alessandro Frau
 * \author Gianmaria Rovelli \author Luca Tagliavini
 * \author Stefano Volpe
 * \date 25-06-2022
 */

#include "arch/devices.h"
#include "arch/processor.h"
#include "os/const.h"
#include "os/ctypes.h"
#include "os/list.h"
#include "os/types.h"
#include "os/util.h"
#include "support/pager.h"
#include "support/support.h"

/** A table from which Support Structures can be allocated. */
static support_t support_table[UPROCMAX];
/** The Support Structures which are currently not allocated. */
static list_head support_free;

/**
 * \brief Allocate a new Support Structure.
 * \return The newly allocated Support Structure.
 */
static inline support_t *allocate()
{
    list_head *const lh = list_next(&support_free);
    if (lh == NULL) {
        PANIC();
    }
    list_del(lh);
    support_t *const res = container_of(lh, support_t, p_list);
    return res;
}

/**
 * \brief Free a Support Structure which was previously allocated by the test
 * program.
 * \param[in,out] s The Support Structure to be deallocated.
 */
static inline void deallocate(support_t *s)
{
    if (s == NULL) {
        PANIC();
    }
    list_head *const lh = &s->p_list;
    if (list_contains(lh, &support_free)) {
        PANIC();
    }
    list_add(lh, &support_free);
}

/**
 * \brief Initialize \ref support_free.
 */
static inline void init_supports()
{
    INIT_LIST_HEAD(&support_free);
    for (size_t i = 0; i < UPROCMAX; i += 1)
        deallocate(support_table + i);
}

void test()
{
    state_t pstate;
    size_t support_status;
    size_t support_mie;
    memaddr ramtop;
    init_sys_semaphores();
    init_pager();
    init_supports();

    pstate.reg_sp = (memaddr)USERSTACKTOP;
    pstate.pc_epc = (memaddr)UPROCSTARTADDR;
    status_interrupts_on_process(&pstate.status);
    status_kernel_mode_off_process(&pstate.status);
    status_local_timer_on(&pstate.mie);
    status_il_on_all(&pstate.mie);

    support_status = pstate.status;
    support_mie = pstate.mie;
    status_interrupts_on_process(&support_status);
    status_kernel_mode_on_process(&support_status);
    status_local_timer_on(&support_mie);
    status_il_on_all(&support_mie);

    RAMTOP(ramtop);
    for (size_t i = 0; i < UPROCMAX; i += 1) {
        const size_t asid = i + 1;

        pstate.entry_hi = asid << ASIDSHIFT;

        // Support Structure initialization
        support_t *support_structure = allocate();
        support_structure->sup_asid = asid;
        if (!init_page_table(support_structure->sup_private_page_table, asid)) {
            PANIC();
        }
        support_structure->sup_except_context[PGFAULTEXCEPT].pc =
            (memaddr)support_tlb;
        support_structure->sup_except_context[PGFAULTEXCEPT].stack_ptr =
            ramtop - 2 * (asid)*PAGESIZE;
        support_structure->sup_except_context[PGFAULTEXCEPT].status =
            support_status;
        support_structure->sup_except_context[PGFAULTEXCEPT].mie = support_mie;
        support_structure->sup_except_context[GENERALEXCEPT].pc =
            (memaddr)support_generic;
        support_structure->sup_except_context[GENERALEXCEPT].stack_ptr =
            ramtop - 2 * asid * PAGESIZE + PAGESIZE;
        support_structure->sup_except_context[GENERALEXCEPT].status =
            support_status;
        support_structure->sup_except_context[GENERALEXCEPT].mie = support_mie;

        // Process creation
        SYSCALL(CREATEPROCESS, (int)&pstate, PROCESS_PRIO_LOW,
                (int)support_structure);
    }
    // Wait for each U-Proc to terminate before terminating yourself
    for (size_t i = 0; i < UPROCMAX; i += 1)
        master_semaphore_p();
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

void deallocate_support(support_t *s) { deallocate(s); }

void check()
{
    if (list_contains((const list_head *)(0x2000d530), &support_free)) {
        PANIC();
    }
}
