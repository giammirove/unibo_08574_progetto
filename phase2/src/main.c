/**
 * \file main.c
 * \brief Entrypoint for the PandOS+ kernel.
 *
 * \author Luca Tagliavini
 * \author Stefano Volpe
 * \date 17-03-2022
 */

#include "arch/processor.h"
#include "exception.h"
#include "os/const.h"
#include "os/init.h"
#include "os/puod.h"
#include "os/scheduler.h"
#include "os/util.h"
#include "support/pager.h"
#include "support/support.h"
#include "support/test.h"
#include "uriscv/arch.h"
#include <uriscv/liburiscv.h>

int main(int argc)
{
    init((memaddr)tlb_refill_handler, (memaddr)exception_handler,
         (memaddr)test);
    schedule(NULL, false);
    return 1;
}
