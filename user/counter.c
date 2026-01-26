/* user/counter.c - Counting Program
 *
 * Counts from 1 to 10 with delays between each number.
 * Tests: loops, sleep syscall, multiple writes
 */

#include "start.h"

void main(void)
{
    int i;

    write("\n");
    write("Counter Program (PID ");
    print_num(getpid());
    write(")\n");
    write("Counting from 1 to 10...\n\n");

    for (i = 1; i <= 10; i++) {
        write("  Count: ");
        print_num(i);
        write("\n");

        /* Sleep 500ms between counts */
        sleep(500);
    }

    write("\nDone counting!\n\n");
}
