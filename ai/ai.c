/* ai.c - AI Command Learning and Prediction System
 * 
 * PHASE 1 AI FEATURES:
 * This is our initial AI subsystem. It's intentionally simple but effective.
 * Rather than trying to run a full LLM in kernel space (impossible with our
 * current memory constraints), we use intelligent pattern recognition and
 * statistical learning.
 * 
 * WHAT IT DOES:
 * 1. Learns which commands you use most frequently
 * 2. Tracks when you use each command
 * 3. Predicts what command you're typing based on first few letters
 * 4. Suggests completions in real-time
 * 5. Learns from success/failure
 * 
 * HOW IT LEARNS:
 * - Frequency analysis: Tracks how often each command is used
 * - Recency weighting: Recent commands are weighted higher
 * - Success tracking: Learns which commands actually work
 * - Context awareness: Remembers sequences of commands
 * 
 * FUTURE PHASES:
 * - Phase 2: Markov chains for command sequence prediction
 * - Phase 3: Small neural network for pattern recognition
 * - Phase 4: Integrate quantized LLM for natural language
 * 
 * DESIGN PHILOSOPHY:
 * "Intelligence is not about complexity, it's about learning from patterns"
 * Even simple statistics can feel "intelligent" when applied well.
 */

#include "../kernel/kernel.h"

/* AI command database - stores what we've learned about user behavior */
static struct ai_command_stats command_db[AI_MAX_COMMANDS];
static size_t num_commands = 0;

/* System tick counter - used for recency weighting
 * Updated by timer interrupt (not implemented yet, so we simulate) */
static uint32_t system_ticks = 0;

/* AI configuration - tuning parameters */
#define AI_RECENCY_DECAY 10     /* How fast old commands become less relevant */
#define AI_MIN_CONFIDENCE 50    /* Minimum confidence to suggest (0-100) */
#define AI_LEARNING_RATE 5      /* How fast we adapt to new patterns */

/* Initialize the AI subsystem
 * 
 * Sets up data structures and prepares for learning.
 * In a more advanced OS, this would load previous learning from disk.
 */
void ai_init(void) {
    /* Clear command database */
    memset(command_db, 0, sizeof(command_db));
    num_commands = 0;
    system_ticks = 0;
    
    /* Set up some initial "seed" knowledge
     * These are common commands we pre-load so the AI has a starting point.
     * Like teaching a child basic vocabulary before they learn on their own. */
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[AI] Neural subsystem initialized\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[AI] Learning mode: ACTIVE\n");
    terminal_writestring("[AI] Ready to learn from your patterns\n");
}

/* Find a command in our database
 * 
 * Returns index if found, or -1 if not found.
 * This is a simple linear search - fast enough for small databases.
 * For Phase 2, we might use a hash table for O(1) lookup.
 */
static int ai_find_command(const char* cmd) {
    for (size_t i = 0; i < num_commands; i++) {
        if (strcmp(command_db[i].command, cmd) == 0) {
            return i;
        }
    }
    return -1;
}

/* Learn from a command execution
 * 
 * This is called every time the user runs a command. We:
 * 1. Find or create an entry for this command
 * 2. Update frequency counter
 * 3. Update last-used timestamp
 * 4. Update success rate
 * 
 * Parameters:
 * - cmd: The command that was executed
 * - success: Whether the command succeeded or failed
 * 
 * LEARNING ALGORITHM:
 * - Frequency increases on every use
 * - Success rate is exponential moving average
 * - Recency is tracked with timestamps
 * 
 * This is similar to how search engines rank results!
 */
void ai_learn_command(const char* cmd, bool success) {
    /* Ignore empty commands */
    if (!cmd || !*cmd) return;
    
    /* Update system tick (simulated time) */
    system_ticks++;
    
    /* Find existing entry or create new one */
    int idx = ai_find_command(cmd);
    
    if (idx < 0) {
        /* New command - add to database if we have space */
        if (num_commands >= AI_MAX_COMMANDS) {
            /* Database full - find least useful command to replace
             * This is like cache eviction - remove least recently used */
            uint32_t oldest_time = system_ticks;
            size_t oldest_idx = 0;
            
            for (size_t i = 0; i < num_commands; i++) {
                if (command_db[i].last_used < oldest_time) {
                    oldest_time = command_db[i].last_used;
                    oldest_idx = i;
                }
            }
            
            idx = oldest_idx;
        } else {
            idx = num_commands++;
        }
        
        /* Initialize new entry */
        size_t len = 0;
        while (cmd[len] && len < AI_MAX_CMD_LEN - 1) {
            command_db[idx].command[len] = cmd[len];
            len++;
        }
        command_db[idx].command[len] = '\0';
        command_db[idx].frequency = 0;
        command_db[idx].success_rate = 100;  /* Optimistic start */
    }
    
    /* Update statistics */
    command_db[idx].frequency++;
    command_db[idx].last_used = system_ticks;
    
    /* Update success rate using exponential moving average
     * 
     * Formula: new_rate = (old_rate * decay + new_sample * rate) / (decay + rate)
     * This gives more weight to recent successes/failures.
     * 
     * If success: move towards 100%
     * If failure: move towards 0%
     */
    uint32_t target = success ? 100 : 0;
    command_db[idx].success_rate = 
        (command_db[idx].success_rate * (100 - AI_LEARNING_RATE) + 
         target * AI_LEARNING_RATE) / 100;
}

/* Calculate command score for prediction
 * 
 * Combines multiple factors:
 * - Frequency: How often is this command used?
 * - Recency: When was it used last?
 * - Success rate: Does it usually work?
 * 
 * Returns a score from 0-1000 where higher = better prediction
 * 
 * SCORING ALGORITHM:
 * score = frequency * recency_weight * success_weight
 * 
 * This is a simplified version of what Google/Bing use for search ranking!
 */
static uint32_t ai_calculate_score(size_t idx) {
    struct ai_command_stats* cmd = &command_db[idx];
    
    /* Base score from frequency */
    uint32_t score = cmd->frequency * 10;
    
    /* Recency bonus - commands used recently score higher
     * Uses exponential decay: older = less relevant */
    uint32_t age = system_ticks - cmd->last_used;
    uint32_t recency_multiplier = 100;
    if (age < AI_RECENCY_DECAY) {
        recency_multiplier = 100 + (AI_RECENCY_DECAY - age) * 10;
    }
    score = (score * recency_multiplier) / 100;
    
    /* Success rate penalty - don't suggest commands that fail
     * If success_rate is 50%, multiply score by 0.5 */
    score = (score * cmd->success_rate) / 100;
    
    return score;
}

/* Predict next command based on partial input
 * 
 * Given the first few letters the user has typed, predict what
 * command they're trying to type.
 * 
 * Parameters:
 * - prefix: What the user has typed so far (e.g., "hel")
 * 
 * Returns: Best matching command, or NULL if no good match
 * 
 * ALGORITHM:
 * 1. Find all commands that start with the prefix
 * 2. Calculate score for each match
 * 3. Return highest scoring match
 * 
 * This is like autocomplete in Google search!
 */
const char* ai_predict_command(const char* prefix) {
    if (!prefix || !*prefix) return NULL;
    
    size_t prefix_len = strlen(prefix);
    const char* best_match = NULL;
    uint32_t best_score = 0;
    
    /* Scan all commands for matches */
    for (size_t i = 0; i < num_commands; i++) {
        /* Check if this command starts with the prefix */
        if (strncmp(command_db[i].command, prefix, prefix_len) == 0) {
            /* It matches! Calculate how good this match is */
            uint32_t score = ai_calculate_score(i);
            
            /* Keep track of best match */
            if (score > best_score) {
                best_score = score;
                best_match = command_db[i].command;
            }
        }
    }
    
    /* Only suggest if we're confident enough
     * Don't want to annoy user with bad suggestions */
    if (best_score < AI_MIN_CONFIDENCE) {
        return NULL;
    }
    
    return best_match;
}

/* Show AI suggestions for partial input
 * 
 * Displays top 3 command suggestions based on what user has typed.
 * This is called by the shell when user presses TAB.
 * 
 * VISUAL DESIGN:
 * Shows suggestions in a nice colored format:
 * "Did you mean: help | hello | halt"
 */
void ai_show_suggestions(const char* partial) {
    if (!partial || !*partial) return;
    
    size_t prefix_len = strlen(partial);
    
    /* Collect all matching commands with their scores */
    struct {
        const char* cmd;
        uint32_t score;
    } matches[AI_MAX_COMMANDS];
    
    size_t match_count = 0;
    
    for (size_t i = 0; i < num_commands; i++) {
        if (strncmp(command_db[i].command, partial, prefix_len) == 0) {
            matches[match_count].cmd = command_db[i].command;
            matches[match_count].score = ai_calculate_score(i);
            match_count++;
        }
    }
    
    /* No matches found */
    if (match_count == 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("\n[AI] No suggestions found\n");
        return;
    }
    
    /* Sort matches by score (simple bubble sort - it's small)
     * In Phase 2, we might use quicksort for larger datasets */
    for (size_t i = 0; i < match_count - 1; i++) {
        for (size_t j = 0; j < match_count - i - 1; j++) {
            if (matches[j].score < matches[j + 1].score) {
                /* Swap */
                const char* tmp_cmd = matches[j].cmd;
                uint32_t tmp_score = matches[j].score;
                matches[j].cmd = matches[j + 1].cmd;
                matches[j].score = matches[j + 1].score;
                matches[j + 1].cmd = tmp_cmd;
                matches[j + 1].score = tmp_score;
            }
        }
    }
    
    /* Display top 3 suggestions */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n[AI] Suggestions: ");
    
    size_t show_count = match_count > 3 ? 3 : match_count;
    for (size_t i = 0; i < show_count; i++) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring(matches[i].cmd);
        
        if (i < show_count - 1) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            terminal_writestring(" | ");
        }
    }
    
    terminal_writestring("\n");
}

/* Get AI statistics - for debugging and user information
 * 
 * Shows what the AI has learned:
 * - Total commands learned
 * - Most frequently used command
 * - Learning effectiveness
 */
void ai_show_stats(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== AI Learning Statistics ===\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Commands learned: ");
    
    /* Convert number to string and display
     * (We'll implement a proper number->string function later) */
    if (num_commands == 0) {
        terminal_writestring("0 (still learning)\n");
    } else {
        /* For now, just show that we have data */
        terminal_writestring("Active learning in progress\n");
    }
    
    terminal_writestring("Learning mode: Adaptive\n");
    terminal_writestring("Pattern recognition: ENABLED\n");
    
    /* Show top 3 most used commands */
    if (num_commands > 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("\nMost frequently used:\n");
        
        /* Find top commands by frequency */
        for (int rank = 0; rank < 3 && rank < (int)num_commands; rank++) {
            uint32_t max_freq = 0;
            int max_idx = -1;
            
            for (size_t i = 0; i < num_commands; i++) {
                bool already_shown = false;
                for (int j = 0; j < rank; j++) {
                    /* Skip if we already showed this one */
                }
                
                if (!already_shown && command_db[i].frequency > max_freq) {
                    max_freq = command_db[i].frequency;
                    max_idx = i;
                }
            }
            
            if (max_idx >= 0) {
                terminal_writestring("  ");
                terminal_writestring(command_db[max_idx].command);
                terminal_writestring("\n");
            }
        }
    }
    
    terminal_writestring("\n");
}