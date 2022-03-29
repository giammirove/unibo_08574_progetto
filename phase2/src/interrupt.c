/**
 * \file interrupt.c
 * \brief Interrupt and trap handler
 *
 * TODO: Implement all the syscalls.
 * \author Alessandro Frau
 * \author Gianmaria Rovelli
 * \author Luca Tagliavini
 * \date 20-03-2022
 */

#include "interrupt.h"
#include "interrupt_impl.h"
#include "native_scheduler.h"
#include "os/asl.h"
#include "os/const.h"
#include "os/pcb.h"
#include "os/scheduler.h"
#include "os/util.h"
#include "semaphores.h"
#include "syscall.h"
#include "umps/cp0.h"
#include "umps/types.h"
#include <umps/arch.h>
#include <umps/libumps.h>

/*find_device_number() viene utilizzato per identificare il numero del device
 * che ha sollevato l'interrupt*/
int find_device_number(memaddr *bitmap)
{
    int device_n = 0;

    while (*bitmap > 1) {
        device_n++;
        *bitmap >>= 1;
    }
    return device_n;
}

static inline memaddr *get_terminal_transm_status(int devicenumber)
{
    return (memaddr *)TERMINAL_TRANSM_STATUS(devicenumber);
}

static inline memaddr *get_terminal_recv_status(int devicenumber)
{
    return (memaddr *)TERMINAL_RECV_STATUS(devicenumber);
}

static inline memaddr *get_terminal_transm_command(int devicenumber)
{
    return (memaddr *)TERMINAL_TRANSM_COMMAND(devicenumber);
}

static inline memaddr *get_terminal_recv_command(int devicenumber)
{
    return (memaddr *)TERMINAL_RECV_COMMAND(devicenumber);
}

/* TODO: now is hardcoded */
static inline scheduler_control_t interrupt_handler()
{

    // pcb_t *unblocked; /* Puntatore a processo sbloccato */

    int cause = getCAUSE();
    int act_pid = -1;
    if (active_process != NULL) {
        act_pid = active_process->p_pid;
    }

    if (CAUSE_IP_GET(cause, IL_IPI)) {
        pandos_kprintf("<< INTERRUPT(IPI)\n");
        /* ACK_IPI; */
        /* TODO */
    }

    else if (CAUSE_IP_GET(cause, IL_LOCAL_TIMER)) {
        pandos_kprintf("<< INTERRUPT(LOCAL_TIMER, (%d) , %d)\n", act_pid,
                       (int)getTIMER());

        reset_plt();

        /* recalls scheduler */
        return CONTROL_RESCHEDULE;
    }

    else if (CAUSE_IP_GET(cause, IL_TIMER)) {
        /*Exctract pcb and put them into the ready queue*/
        int tod = *((int *)TODLOADDR);
        pandos_kprintf("<< INTERRUPT(TIMER, (%d) , %p)\n", act_pid, tod);
        reset_timer();

        pcb_t *p;
        while ((p = V(&timer_semaphore)) != NULL) {
            verbose("REMOVE from clock %d\n", act_pid);
        }

        timer_semaphore = 0;

        return CONTROL_PRESERVE(active_process);
    } else if (CAUSE_IP_GET(cause, IL_DISK) || CAUSE_IP_GET(cause, IL_FLASH) ||
               CAUSE_IP_GET(cause, IL_ETHERNET) ||
               CAUSE_IP_GET(cause, IL_PRINTER)) {
        pandos_kprintf("<< INTERRUPT(GENERIC)\n");

        /* TODO */
        int il = IL_DISK;
        int *sem[] = {disk_semaphores, tape_semaphores, ethernet_semaphores,
                      printer_semaphores};
        /* inverse priority */
        for (int i = IL_DISK; i < IL_PRINTER; i++) {
            if (CAUSE_IP_GET(cause, i)) {
                il = i;
                break;
            }
        }

        int devicenumber = find_device_number((memaddr *)CDEV_BITMAP_ADDR(il));
        pcb_t *p = V(&sem[il - IL_DISK][devicenumber]);
        scheduler_control_t ctrl = mask_V(p);
        /* 		if( (unblocked = V(&sem_ethernet[devicenumber])) !=
           NULL){
                                        //Se un processo ha chiamato la SYS dei
           TAPE, V() lo sblocca e aggiorna lo status unblocked->p_s.reg_v0 =
           *DEVICE_STATUS(il, devicenumber);
                                        insertProcQ(&ready_queue[unblocked->numCPU],
           unblocked);
                                } */

        /* ACK al device */
        *DEVICE_COMMAND(il, devicenumber) = DEV_C_ACK;
        return ctrl;

    } else if (CAUSE_IP_GET(cause, IL_TERMINAL)) {
        pandos_kprintf("<< INTERRUPT(TERMINAL, (%d))\n", act_pid);

        /*Scandiamo la BITMAP_TERMINALDEVICE per identificare quale terminale ha
         * sollevato l'interrupt*/
        int devicenumber =
            find_device_number((memaddr *)CDEV_BITMAP_ADDR(IL_TERMINAL));

        /* TODO : order is important, check */
        memaddr *(*get_status[2])(int dev) = {get_terminal_transm_status,
                                              get_terminal_recv_status};
        memaddr *(*get_cmd[2])(int dev) = {get_terminal_transm_command,
                                           get_terminal_recv_command};
        int *sem[] = {termw_semaphores, termr_semaphores};

        // pandos_kfprintf(&kverb, "\n[-] TERM INT START (%d)\n", act_pid);
        for (int i = 0; i < 2; i++) {
            int status = *get_status[i](devicenumber);
            if ((status & TERMSTATMASK) != DEV_S_READY) {
                pcb_t *p = V(&sem[i][devicenumber]);
                scheduler_control_t ctrl = mask_V(p);
                // int pid = 0;
                if (p == NULL) {
                    active_process->p_s.reg_v0 = status;
                    // pid = active_process->p_pid;
                } else {
                    p->p_s.reg_v0 = status;
                    // pid = p->p_pid;
                    // pandos_kfprintf(&kverb, "   SIZE (%p)\n",
                    // list_size(&ready_queue_lo));
                }

                // pandos_kfprintf(&kverb, "   STATUS of (%d) (%p)\n", pid,
                // status);

                stdout("SENT ACK\n");
                *get_cmd[i](devicenumber) = DEV_C_ACK;
                /* do the first one */
                return ctrl;
            }
        }

        pandos_kfprintf(&kverb, "WTF TERMINAL (%d)\n", act_pid);

        // pandos_kfprintf(&kverb, "--------- TERM INT END -------- (%p)\n");

    } else
        pandos_kprintf("<< INTERRUPT(UNKNOWN)\n");

    /* The
newly unblocked pcb is enqueued back on the Ready Queue and control
is returned to the Current Process unless the newly unblocked process
has higher prority of the Current Process. */

    return CONTROL_RESCHEDULE;
}

static inline scheduler_control_t tbl_handler()
{
    if (active_process->p_support == NULL)
        kill_process(active_process);
    else {
        support_t *s = active_process->p_support;
        pandos_kprintf(">> SUPPORT(%d)\n", s->sup_asid);
        /* TODO: tell the scheduler to handoff the control to s->sup_asid;
         * with the appropriate state found in s->sup_except_state ??????
         * read the docs i guess
         */
    }

    return CONTROL_RESCHEDULE;
}

/* TODO: fill me */
static inline scheduler_control_t trap_handler()
{
    int pid = -1;
    if (active_process != NULL)
        pid = active_process->p_pid;
    pandos_kprintf("<< TRAP (%d)\n", pid);

    if (active_process->p_support == NULL)
        kill_process(active_process);
    else
        PANIC();
    return CONTROL_BLOCK;
}

void exception_handler()
{
    scheduler_control_t ctrl = CONTROL_RESCHEDULE;

    if (active_process != NULL)
        memcpy(&active_process->p_s, (state_t *)BIOSDATAPAGE, sizeof(state_t));
    else
        stdout("ACTIVE IS NULL (start) (%d)\n", CAUSE_GET_EXCCODE(getCAUSE()));
    /* p_s.cause could have been used instead of getCAUSE() */
    switch (CAUSE_GET_EXCCODE(getCAUSE())) {
        case 0:
            ctrl = interrupt_handler();
            break;
        case 1:
        case 2:
        case 3:
            ctrl = tbl_handler();
            break;
        case 8:
            if (active_process == NULL)
                scheduler_panic(
                    "A syscall happened while active_process was NULL\n");
            /* ALWAYS increment the PC to prevent system call loops */
            active_process->p_s.pc_epc += WORD_SIZE;
            active_process->p_s.reg_t9 += WORD_SIZE;
            ctrl = syscall_handler();
            break;
        default: /* 4-7, 9-12 */
            ctrl = trap_handler();
            break;
    }
    schedule(ctrl.pcb, ctrl.enqueue);
}
