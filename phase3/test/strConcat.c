
/* concatenates two strings together and prints them out */

#include <uriscv/liburiscv.h>

#include "test/print.h"
#include "test/tconst.h"

void main()
{

    print(WRITETERMINAL, "Strcat Test starts\n");

    print(WRITETERMINAL, "\n");
    print(WRITETERMINAL, "Enter another string: ");


    print(WRITETERMINAL, "\n\nStrcat concluded\n");

    /* Terminate normally */
    SYSCALL(TERMINATE, 0, 0, 0);
}
