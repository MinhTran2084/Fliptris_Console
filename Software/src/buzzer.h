/**
 * @file buzzer.h
 * @brief Non-blocking buzzer.
 *
 * The play_* functions add notes to a queue and return straight away.
 * Timer TCB1 plays the notes in the background, so the game keeps
 * running while sound plays. buzzer_tick() must be called every 1 ms.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void buzzer_init(void);
void buzzer_tick(void);
void play_block_fixed(void);
void play_lines_cleared(uint8_t lines);
void play_compaction(void);

#endif /* BUZZER_H */
