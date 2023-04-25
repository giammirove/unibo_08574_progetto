/* Does nothing but outputs to the printer and terminates */

#include <uriscv/liburiscv.h>

#include "test/print.h"
#include "test/tconst.h"

void main()
{
    print(WRITETERMINAL, "printTest is ok\n");

    print(WRITETERMINAL, "Test number 4 is ok\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}
