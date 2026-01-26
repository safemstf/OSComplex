/* user/sysinfo.c - System Information
 *
 * Displays system information using various syscalls.
 * Tests: multiple syscalls, formatted output
 */

#include "start.h"

void main(void)
{
    write("\n");
    write("+----------------------------------+\n");
    write("|       System Information         |\n");
    write("+----------------------------------+\n");
    write("\n");

    write("  Process ID:     ");
    print_num(getpid());
    write("\n");

    write("  User Segment:   0x23 (Ring 3)\n");
    write("  Code Base:      0x08048000\n");
    write("  Stack Base:     0xBFFFFFFF\n");

    write("\n");
    write("  Syscalls available:\n");
    write("    - exit(code)\n");
    write("    - write(string)\n");
    write("    - getpid()\n");
    write("    - sleep(ms)\n");
    write("    - yield()\n");
    write("    - fork()\n");
    write("    - wait(status)\n");
    write("    - exec(path)\n");

    write("\n");
    write("+----------------------------------+\n");
    write("\n");
}
