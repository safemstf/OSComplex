/* drivers/keyboard.c - PS/2 Keyboard driver with AI-powered features
 *
 * THE KEYBOARD HARDWARE:
 * The PS/2 keyboard communicates through the keyboard controller
 * which uses two I/O ports:
 * - 0x60: Data port (read scancodes here)
 * - 0x64: Status/command port
 *
 * HOW IT WORKS:
 * 1. User presses a key
 * 2. Keyboard sends scancode to controller
 * 3. Controller triggers IRQ 1
 * 4. Our interrupt handler reads the scancode from port 0x60
 * 5. We convert scancode to ASCII
 * 6. Store character in buffer for shell to read
 *
 * SCANCODES:
 * Scancodes are NOT ASCII! They're keyboard-specific codes.
 * - Scancode set 1 (most common): One byte per key
 * - If bit 7 is SET (scancode & 0x80): Key was RELEASED
 * - If bit 7 is CLEAR: Key was PRESSED
 *
 * We use a lookup table to convert scancodes to ASCII.
 *
 * AI FEATURES:
 * - Learns typing patterns
 * - Predicts next characters
 * - Suggests command completions
 */

#include "../kernel/kernel.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

/* Keyboard input buffer
 * Stores characters until the shell reads them
 * This is a simple circular buffer (ring buffer) */
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_read_pos = 0;
static volatile size_t buffer_write_pos = 0;

/* Keyboard state flags */
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;
static bool extended_scancode = false;

/* US QWERTY keyboard layout - scancode to ASCII mapping
 *
 * This table maps scancode (index) to ASCII character (value).
 * Extended to cover all common scancodes (0x00-0x58).
 *
 * Special scancodes:
 * - 0x1D: Left Ctrl
 * - 0x2A: Left Shift
 * - 0x36: Right Shift
 * - 0x38: Alt
 * - 0x3A: Caps Lock
 * - 0x1C: Enter
 * - 0x0E: Backspace
 * - 0x39: Space
 * - 0x0F: Tab
 */
static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',      /* 0x00-0x07 */
    '7', '8', '9', '0', '-', '=', '\b', '\t', /* 0x08-0x0F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   /* 0x10-0x17 */
    'o', 'p', '[', ']', '\n', 0, 'a', 's',    /* 0x18-0x1F (0x1C=Enter, 0x1D=Ctrl) */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 0x20-0x27 */
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',   /* 0x28-0x2F (0x2A=LShift) */
    'b', 'n', 'm', ',', '.', '/', 0, '*',     /* 0x30-0x37 (0x36=RShift) */
    0, ' ', 0, 0, 0, 0, 0, 0,                 /* 0x38-0x3F (0x38=Alt, 0x39=Space, 0x3A=CapsLock) */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x40-0x47 */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x48-0x4F */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x50-0x57 */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x58-0x5F */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x60-0x67 */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x68-0x6F */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x70-0x77 */
    0, 0, 0, 0, 0, 0, 0, 0                    /* 0x78-0x7F */
};

/* Shifted characters (when Shift is held)
 * For keys that change with Shift (numbers -> symbols, etc.) */
static const char scancode_to_ascii_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',      /* 0x00-0x07 */
    '&', '*', '(', ')', '_', '+', '\b', '\t', /* 0x08-0x0F */
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',   /* 0x10-0x17 */
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',    /* 0x18-0x1F */
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   /* 0x20-0x27 */
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',     /* 0x28-0x2F */
    'B', 'N', 'M', '<', '>', '?', 0, '*',     /* 0x30-0x37 */
    0, ' ', 0, 0, 0, 0, 0, 0,                 /* 0x38-0x3F */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x40-0x47 */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x48-0x4F */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x50-0x57 */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x58-0x5F */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x60-0x67 */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x68-0x6F */
    0, 0, 0, 0, 0, 0, 0, 0,                   /* 0x70-0x77 */
    0, 0, 0, 0, 0, 0, 0, 0                    /* 0x78-0x7F */
};

/* Add character to keyboard buffer
 *
 * This is a circular buffer - when we reach the end, we wrap around.
 * If buffer is full, we drop the character (could also overwrite oldest).
 */
static void keyboard_buffer_push(char c)
{
    size_t next = (buffer_write_pos + 1) % KEYBOARD_BUFFER_SIZE;

    /* Check if buffer is full */
    if (next == buffer_read_pos)
    {
        /* Buffer full - drop character */
        return;
    }

    keyboard_buffer[buffer_write_pos] = c;
    buffer_write_pos = next;
}

/* Read character from keyboard buffer
 *
 * Returns the next character, or 0 if buffer is empty.
 * The shell calls this to get user input.
 */
char keyboard_buffer_pop(void)
{
    /* Check if buffer is empty */
    if (buffer_read_pos == buffer_write_pos)
    {
        return 0; /* No data available */
    }

    char c = keyboard_buffer[buffer_read_pos];
    buffer_read_pos = (buffer_read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

/* Check if character is a letter (a-z, A-Z) */
static bool is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Keyboard interrupt handler
 *
 * Called by IRQ 1 handler when a key is pressed or released.
 *
 * PROCESS:
 * 1. Read scancode from port 0x60
 * 2. Check if it's a key press (bit 7 clear) or release (bit 7 set)
 * 3. Handle special keys (Shift, Ctrl, Caps Lock)
 * 4. Convert scancode to ASCII
 * 5. Add to keyboard buffer
 *
 * AI INTEGRATION:
 * - Track key patterns for prediction
 * - Learn common key combinations
 * - Provide smart suggestions
 */
void keyboard_handler(void)
{
    /* Read scancode from keyboard data port
     * IMPORTANT: We MUST read this even if we don't use it!
     * If we don't, the keyboard controller won't send more interrupts. */

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* Check if this is a key release (bit 7 set) */
    if (scancode & 0x80)
    {
        /* Key release - clear the bit to get the base scancode */
        scancode &= 0x7F;

        /* Update modifier key states */
        if (scancode == 0x2A || scancode == 0x36)
        {
            shift_pressed = false; /* Shift released */
        }
        else if (scancode == 0x1D)
        {
            ctrl_pressed = false; /* Ctrl released */
        }
        else if (scancode == 0x38)
        {
            alt_pressed = false; /* Alt released */
        }

        /* Don't process key releases further */
        return;
    }

    /* Key press event - handle special keys first */

    /* Shift key */
    if (scancode == 0x2A || scancode == 0x36)
    {
        shift_pressed = true;
        return;
    }

    /* Ctrl key */
    if (scancode == 0x1D)
    {
        ctrl_pressed = true;
        return;
    }

    /* Alt key */
    if (scancode == 0x38)
    {
        alt_pressed = true;
        return;
    }

    /* Caps Lock - toggle state */
    if (scancode == 0x3A)
    {
        caps_lock = !caps_lock;
        /* TODO: Update keyboard LEDs to show Caps Lock state */
        return;
    }

    /* Handle extended scancode prefix */
    if (scancode == 0xE0)
    {
        extended_scancode = true;
        return;
    }

    /* Process extended scancode */
    if (extended_scancode)
    {
        extended_scancode = false;

        /* Ignore key releases */
        if (scancode & 0x80)
            return;

        switch (scancode)
        {
        case 0x49: /* Page Up */
            terminal_scrollback_page_up();
            return;

        case 0x51: /* Page Down */
            terminal_scrollback_page_down();
            return;

        default:
            return;
        }
    }

    /* Convert scancode to ASCII character */
    char c = 0;

    /* Bounds check - ensure scancode is valid */
    if (scancode < 128)
    {
        /* First get the base character (without shift/caps) */
        char base_char = scancode_to_ascii[scancode];

        if (base_char == 0)
        {
            /* Invalid/unmapped scancode */
            return;
        }

        /* Determine which character to use based on modifiers */
        if (is_letter(base_char))
        {
            /* For letters: Caps Lock XOR Shift determines case */
            bool make_uppercase = shift_pressed ^ caps_lock;

            if (make_uppercase)
            {
                c = scancode_to_ascii_shift[scancode];
            }
            else
            {
                c = scancode_to_ascii[scancode];
            }
        }
        else
        {
            /* For non-letters: Only Shift matters, Caps Lock is ignored */
            if (shift_pressed)
            {
                c = scancode_to_ascii_shift[scancode];
            }
            else
            {
                c = scancode_to_ascii[scancode];
            }
        }

        /* Handle Ctrl key combinations
         * Ctrl+C = 0x03, Ctrl+D = 0x04, etc. */
        if (ctrl_pressed && c >= 'a' && c <= 'z')
        {
            c = c - 'a' + 1; /* Convert to control character */
        }
        else if (ctrl_pressed && c >= 'A' && c <= 'Z')
        {
            c = c - 'A' + 1; /* Convert to control character */
        }
    }

    /* If we got a valid character, add to buffer */
    if (c)
    {
        keyboard_buffer_push(c);

        /* AI: Learn from this keystroke
         * Track patterns, common sequences, typing speed, etc.
         * This data will be used for prediction and autocomplete */
        // TODO: ai_learn_keystroke(c);
    }
}

/* Initialize keyboard driver
 *
 * Sets up the keyboard interrupt handler and initializes state.
 */
void keyboard_init(void)
{
    /* Install our handler for IRQ 1 (keyboard interrupt)
     * This connects keyboard_handler to the interrupt system */
    irq_install_handler(IRQ_KEYBOARD, keyboard_handler);

    /* Initialize buffer pointers */
    buffer_read_pos = 0;
    buffer_write_pos = 0;

    /* Clear keyboard state */
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;

    terminal_writestring("[KEYBOARD] Driver initialized\n");
}

/* Check if keyboard has data available
 * Returns true if there's a character waiting to be read */
bool keyboard_has_data(void)
{
    return buffer_read_pos != buffer_write_pos;
}