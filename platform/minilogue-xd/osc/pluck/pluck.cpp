
/*
 * File: pluck.cpp
 * Author: Luciano Esteban Notarfrancesco (github.com/len)
 * Modified by Fredrik Fagerholm (github.com/ffagerholm)
 * 
 * Karplus-Strong plucked string algorithm
 *
 */

#include "userosc.h"
#include "delayline.hpp"
#include "biquad.hpp"

enum {
  k_flags_none    = 0,
  k_flag_reset    = 1<<0
};

#define DELAY_BUFFER_SIZE 4096 // 2048 is too small to fit the lowest octave, and it must be a power of 2 due to DelayLine implementation
static float delay_buffer[DELAY_BUFFER_SIZE];

typedef struct State {
  dsp::DelayLine delay;
  dsp::BiQuad impulse_filter;
  float attack, damping;
  uint32_t burst;
  float lfo, lfoz;
  uint32_t flags:8;
} State;

static State s;

void OSC_INIT(uint32_t platform, uint32_t api)
{
  s.delay.setMemory(delay_buffer, DELAY_BUFFER_SIZE);
  s.impulse_filter.mCoeffs.setPoleLP(0.9f);
  s.attack = 10; // 10 milliseconds
  s.damping = .5f;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
  // Handle events.
  {
    const uint32_t flags = s.flags;
    s.flags = k_flags_none;
    
    if (flags & k_flag_reset) {
      s.delay.clear();
      s.burst = 48.f*s.attack; // milliseconds at 48khz
    }
    
    s.lfo = q31_to_f32(params->shape_lfo);
  }
  
  dsp::BiQuad &impulse_filter = s.impulse_filter;
  dsp::DelayLine &delay = s.delay;

  const float length = clipminmaxf(2.f, 1.f / osc_w0f_for_note(
    (params->pitch)>>8, params->pitch & 0xFF), DELAY_BUFFER_SIZE);

  uint32_t burst = s.burst;

  float lfoz = s.lfoz;
  const float lfo_inc = (s.lfo - lfoz) / frames;
  
  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;
  float lastSig = 0;
  float impulse = 0;
  // Iterate over samples in inpuf buffer
  for (; y != y_e; ) {
    // Read a sample from the delay line at a fractional position
    // from current write index. Values are interpolated.
    float sig = delay.readFrac(length);

    // TODO: Replace by 2 order lowpass filter
    // Apply running mean low-pass filter for damping
    const float damping = clipminmaxf(.000001f, s.damping + lfoz, .999999f);
    sig = (sig*damping + lastSig*(1.f - damping));

    // If we are at the begining of a note a burst of
    // white noise should be added to exite the model.
    if (burst>0) {
      burst--;
      sig += impulse_filter.process_fo(osc_white());
    }

    // Add current value to the delay line
    delay.write(osc_softclipf(0.05f, sig));
    // Apply softclipping
    sig = osc_softclipf(0.05f, sig);
    // Add sample to output buffer
    *(y++) = f32_to_q31(sig);

    lastSig = sig; 

    lfoz += lfo_inc;
  }
  
  s.burst = burst;
  s.lfoz = lfoz;
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
  s.flags |= k_flag_reset;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
  (void)params;
}

/*
index    Parameter ID. See user_osc_param_id_t.
value    Parameter value.
         10 bit resolution for shape/shift-shape.

Use param_val_to_f32 to convert the value to a float in the range 0 to 1
*/
void OSC_PARAM(uint16_t index, uint16_t value)
{ 
  switch (index) {
  case k_osc_param_id1:
  case k_osc_param_id2:
  case k_osc_param_id3:
  case k_osc_param_id4:
  case k_osc_param_id5:
  case k_osc_param_id6:
    break;
    
  case k_osc_param_shape:
    s.damping = 1.f - clipminmaxf(.0000001f, param_val_to_f32(value), .999999f); // 1 to 0
    break;
    
  case k_osc_param_shiftshape:
    {
      // Convert param value to range 0-1
      const float x = 1.0 - param_val_to_f32(value);
      // more resolution near 1
      float perc = clipminmaxf(.0000001f, 1.0 - x*x*x, .999999f); 
      s.impulse_filter.mCoeffs.setPoleLP(perc); 
    }
    break;
 
  default:
    break;
  }
}

