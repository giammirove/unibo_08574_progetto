/** \file
 * \brief Type definitions
 *
 * \author Luca Tagliavini
 * \date 17-01-2022
 */

#ifndef PANDOS_TYPES_H
#define PANDOS_TYPES_H

#include "arch/processor.h"
#include "const.h"
#include "list.h"
#include "os/ctypes.h"

typedef signed int cpu_t;
/* Avoid re-defining size_t on a modern architecture */

/* Page Table Entry descriptor */
typedef struct pte_entry_t {
    unsigned int pte_entry_hi;
    unsigned int pte_entry_lo;
} pte_entry_t;

/* Support level context */
typedef struct context_t {
    unsigned int stack_ptr;
    unsigned int status;
    unsigned int pc;
} context_t;

/* Support level descriptor */
typedef struct support_t {
    int sup_asid;                    /* process ID */
    state_t sup_except_state[2];     /* old state exceptions */
    context_t sup_except_context[2]; /* new contexts for passing up	*/
    pte_entry_t sup_private_page_table[USERPGTBLSIZE]; /* user page table */
} support_t;

typedef memaddr pandos_pid_t; /* NOTE: the type has been changed to bool */

/* process table entry type */
typedef struct pcb_t {
    /* process queue  */
    list_head p_list;

    /* process tree fields */
    struct pcb_t *p_parent; /* ptr to parent	*/
    list_head p_child;      /* children list */
    list_head p_sib;        /* sibling list  */

    /* process status information */
    state_t p_s;  /* processor state */
    cpu_t p_time; /* cpu time used by proc */

    /* Pointer to the semaphore the process is currently blocked on */
    int *p_sem_add;

    /* Pointer to the support struct */
    support_t *p_support;

    /* Indicator of priority; 0 - low, 1 - high */
    bool p_prio; /* NOTE: the type has been changed to bool */

    /* process id */
    pandos_pid_t p_pid;
} pcb_t;

/* semaphore descriptor (SEMD) data structure */
typedef struct semd_t {
    /* Semaphore key */
    int *s_key;
    /* Queue of PCBs blocked on the semaphore */
    list_head s_procq;

    /* Semaphore list */
    list_head s_link;
} semd_t;

typedef struct scheduler_control {
    pcb_t *pcb;
    bool enqueue;
} scheduler_control_t;

#define CONTROL_BLOCK                                                          \
    (scheduler_control_t) { NULL, false }
#define CONTROL_PRESERVE(p)                                                    \
    (scheduler_control_t) { p, false }
#define CONTROL_RESCHEDULE                                                     \
    (scheduler_control_t) { active_process, true }

#endif /* PANDOS_TYPES_H */
