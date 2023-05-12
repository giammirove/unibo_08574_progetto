/**
 * \file storage.c
 * \brief Support Level flash devices driver
 *
 * \author Gianmaria Rovelli
 * \author Luca Tagliavini
 * \author Stefano Volpe
 * \date 25-06-2022
 */

#include "support/storage.h"
#include "os/const.h"
#include "os/ctypes.h"
#include "os/util.h"
#include "support/pager.h"
#include <uriscv/arch.h>

/** The number of the least significant bit of BLOCKNUMBER in the COMMAND field.
 */
#define FLASHCMDSHIFT 8

int read_flash(unsigned int dev, size_t block, void *dest)
{
    deactive_interrupts();
    dtpreg_t *const reg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, dev - 1);
    const size_t cmd = FLASHREAD | block << FLASHCMDSHIFT;
    reg->data0 = (memaddr)dest;
    const int res = SYSCALL(DOIO, (int)&reg->command, cmd, 0);
    active_interrupts();
    return res;
}

int write_flash(unsigned int dev, size_t block, void *src)
{
    deactive_interrupts();
    dtpreg_t *const reg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, dev - 1);
    const size_t cmd = FLASHWRITE | block << FLASHCMDSHIFT;
    reg->data0 = (memaddr)src;
    const int res = SYSCALL(DOIO, (int)&reg->command, cmd, 0);
    active_interrupts();
    return res;
}
