/* user/spin.c - Spinner/Progress Indicator
 *
 * Shows a spinning animation to test continuous execution.
 * Tests: loops, sleep, character output
 */

#include "start.h"

void main(void)
{
    int i;
    const char spinner[] = {'|', '/', '-', '\\'};

    write("\n");
    write("Spinner Demo (PID ");
    print_num(getpid());
    write(")\n");
    write("Running for 20 iterations...\n\n");

    for (i = 0; i < 20; i++) {
        char c[4];
        c[0] = spinner[i % 4];
        c[1] = ' ';
        c[2] = '\0';

        write("  Progress: ");
        write(c);
        print_num((i + 1) * 5);
        write("%\n");

        sleep(200);
    }

    write("\nComplete!\n\n");
}
