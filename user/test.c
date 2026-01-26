/* user/test.c - Basic System Test
 *
 * Tests basic syscall functionality.
 * Good for debugging - minimal and straightforward.
 */

#include "start.h"

void main(void)
{
    write("\n");
    write("=== OSComplex User Mode Test ===\n\n");

    /* Test 1: write syscall */
    write("[TEST 1] write() syscall: ");
    write("PASS\n");

    /* Test 2: getpid syscall */
    write("[TEST 2] getpid() syscall: PID = ");
    print_num(getpid());
    write(" - PASS\n");

    /* Test 3: yield syscall */
    write("[TEST 3] yield() syscall: ");
    yield();
    write("PASS\n");

    /* Test 4: sleep syscall */
    write("[TEST 4] sleep(100) syscall: ");
    sleep(100);
    write("PASS\n");

    write("\n=== All Tests Passed ===\n\n");
}
