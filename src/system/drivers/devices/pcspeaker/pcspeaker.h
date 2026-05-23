#ifndef PCSPEAKER_H
#define PCSPEAKER_H

#include <stdint.h>

// Initialize the PC speaker
void pcspeaker_init(void);

// Play a tone at the specified frequency (in Hz)
// If frequency is 0, stops the sound
void pcspeaker_play(uint32_t frequency);

// Stop playing any sound
void pcspeaker_stop(void);

// Play a beep for a specified duration (in milliseconds)
void pcspeaker_beep(uint32_t frequency, uint32_t duration_ms);

// Play a simple melody (for testing)
void pcspeaker_test_melody(void);

#endif // PCSPEAKER_H
