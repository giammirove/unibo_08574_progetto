#include "support/support.h"
#include "arch/processor.h"
#include "os/const.h"
#include "os/ctypes.h"
#include "os/scheduler.h"
#include "os/semaphores.h"
#include "os/syscall.h"
#include "os/util.h"
#include "support/pager.h"
#include "support/print.h"
#include "test/tconst.h"
#include "umps/arch.h"
#include "umps/const.h"
#include "umps/cp0.h"

inline void support_generic()
{
    // pandos_kprintf("Start of support generic\n");
    support_t *current_support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *saved_state = &current_support->sup_except_state[GENERALEXCEPT];
    int id = CAUSE_GET_EXCCODE(
        current_support->sup_except_state[GENERALEXCEPT].cause);
    switch (id) {
        case 8: /*Syscall*/
            support_syscall(current_support);
            break;
        default:
            support_trap();
    }

    // pandos_kprintf("End of support generic\n");
    saved_state->pc_epc += WORD_SIZE;
    saved_state->reg_t9 += WORD_SIZE;
    load_state(saved_state);
}

/* TODO: */
void support_trap() { pandos_kprintf("!!!!!support_trap\n"); }

void sys_get_tod()
{
    cpu_t time;
    STCK(time);
    /* Points to the current state POPS 8.5*/
    ((state_t *)BIOSDATAPAGE)->reg_v0 = time;
}

/* hardware constants */
#define PRINTCHR 2
#define RECVD 5

size_t sys_write_printer()
{
    support_t *current_support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    size_t termid = current_support->sup_asid;
    char *s = (char *)current_support->sup_except_state[GENERALEXCEPT].reg_a1;
    size_t len =
        (size_t)current_support->sup_except_state[GENERALEXCEPT].reg_a2;

    int *sem_term_mut = get_semaphore(IL_PRINTER, (int)termid, true);

    dtpreg_t *device = (dtpreg_t *)DEV_REG_ADDR(IL_PRINTER, (int)termid);
    unsigned int status;

    SYSCALL(PASSEREN, (int)&sem_term_mut, 0, 0);
    size_t i;
    for (i = 0; i < len; ++i) {
        device->data0 = s[i];
        status = SYSCALL(DOIO, (int)&device->command, (int)PRINTCHR, 0);
        if (device->status != DEV_STATUS_READY) {
            current_support->sup_except_state[GENERALEXCEPT].reg_v0 =
                -(status & TERMSTATMASK);
            return -device->status;
        }
    }
    SYSCALL(VERHOGEN, (int)&sem_term_mut, 0, 0);
    current_support->sup_except_state[GENERALEXCEPT].reg_v0 = len;
    return i;
}

size_t sys_write_terminal()
{
    support_t *current_support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    size_t asid = current_support->sup_asid;
    char *s = (char *)current_support->sup_except_state[GENERALEXCEPT].reg_a1;
    size_t len =
        (size_t)current_support->sup_except_state[GENERALEXCEPT].reg_a2;
    return syscall_writer((void *)(asid), s, len);
}

#define RECEIVE_CHAR 2
#define RECEIVE_STATUS(v) ((v)&0xF)
#define RECEIVE_VALUE(v) ((v) >> 8)
size_t sys_read_terminal_v2()
{
    size_t read = 0;
    char r = EOS;
    support_t *current_support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    termreg_t *base = (termreg_t *)(DEV_REG_ADDR(
        IL_TERMINAL, (int)current_support->sup_asid));
    char *vbuf =
        (char *)current_support->sup_except_state[GENERALEXCEPT].reg_v1;
    char *buf = (char *)virtual_to_physical(current_support, (memaddr)vbuf);
    while (r != '\n') {
        size_t status =
            SYSCALL(DOIO, (int)&base->recv_command, (int)RECEIVE_CHAR, 0);
        if (RECEIVE_STATUS(status) != DEV_STATUS_TERMINAL_OK) {
            return -RECEIVE_STATUS(status);
        }
        r = RECEIVE_VALUE(status);
        pandos_kfprintf(&kdebug, "wiring %c to %p\n", r, (buf + read));
        *(buf + read++) = r;
    }
    *(buf + read++) = EOS;
    pandos_kfprintf(&kdebug, "read: %d\n", read);
    return read;
}

void support_syscall(support_t *current_support)
{
    pandos_kprintf("!!!!!support_syscall\n");
    // state_t *saved_state = (state_t *)BIOSDATAPAGE;
    const int id = (int)current_support->sup_except_state[GENERALEXCEPT].reg_a0;
    pandos_kprintf("Code %d\n", id);
    size_t res = -1;
    switch (id) {
        case GET_TOD:
            sys_get_tod();
            break;
        case TERMINATE:
            SYSCALL(TERMPROCESS, 0, 0, 0);
            break;
        case WRITEPRINTER:
            res = sys_write_printer();
            break;
        case WRITETERMINAL:
            res = sys_write_terminal();
            break;
        case READTERMINAL:

            break;
        default:
            /*idk*/
            break;
    }
    current_support->sup_except_state[GENERALEXCEPT].reg_v0 = res;
    /*
        TODO:   the Support Level’s SYSCALL exception handler must also incre-
                ment the PC by 4 in order to return control to the instruction
       after the SYSCALL instruction.
    */
}

size_t syscall_writer(void *termid, char *msg, size_t len)
{

    int *sem_term_mut = get_semaphore(IL_TERMINAL, (int)termid, true);

    termreg_t *device = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, (int)termid);
    char *s = msg;
    unsigned int status;

    SYSCALL(PASSEREN, (int)&sem_term_mut, 0, 0); /* P(sem_term_mut) */
    while (*s != EOS) {
        unsigned int value = PRINTCHR | (((unsigned int)*s) << 8);
        status = SYSCALL(DOIO, (int)&device->transm_command, (int)value, 0);
        if ((status & TERMSTATMASK) != RECVD) {
            return -(status & TERMSTATMASK);
        }
        s++;
    }
    SYSCALL(VERHOGEN, (int)&sem_term_mut, 0, 0); /* V(sem_term_mut) */
    return len;
}

size_t syscall_reader(void *termid, char *s)
{

    int *sem_term_mut = get_semaphore(IL_TERMINAL, (int)termid, false);

    termreg_t *device = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, (int)termid);
    device->recv_command = RECEIVE_CHAR;
    unsigned int status;
    size_t len = 0;

    SYSCALL(PASSEREN, (int)&sem_term_mut, 0, 0); /* P(sem_term_mut) */
    do {
        unsigned int value = PRINTCHR | (((unsigned int)*s) << 8);
        status = SYSCALL(DOIO, (int)&device->transm_command, (int)value, 0);
        ++len;
        if ((status & TERMSTATMASK) != RECVD) {
            return -(status & TERMSTATMASK);
        }
    } while (*s++ != EOS);
    SYSCALL(VERHOGEN, (int)&sem_term_mut, 0, 0); /* V(sem_term_mut) */
    return len;
}
