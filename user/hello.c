/* user/hello.c - Hello World
 *
 * The simplest user program - just prints a message and exits.
 */

#include "start.h"

void main(void)
{
    write("\n");
    write("Hello from user space!\n");
    write("PID: ");
    print_num(getpid());
    write("\n\n");
}
