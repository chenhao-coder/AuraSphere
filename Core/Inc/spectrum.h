#ifndef SPECTRUM_H_
#define SPECTRUM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    RHYTHM_MODEL1 = 0,
    RHYTHM_MODEL2,
    RHYTHM_MODEL3,
    RHYTHM_MODEL4,
} RhythmMode;


void Spectrum_SetMode(RhythmMode mode);
void Spectrum_Draw(uint8_t *band);

#ifdef __cplusplus
}
#endif

#endif