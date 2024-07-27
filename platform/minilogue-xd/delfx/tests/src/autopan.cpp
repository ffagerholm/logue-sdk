/*
 * File: autopan.cpp
 *
 * 
 * 
 * 
 * 2018 (c) Korg
 *
 */

#include "userdelfx.h"
#include "simplelfo.hpp"

static dsp::SimpleLFO s_lfo_left;
static dsp::SimpleLFO s_lfo_right;

enum {
  k_sin_bi_off = 0,
  k_tri_bi_off,
  k_saw_bi_off,
  k_sqr_bi_off,
  k_wave_count
};

static uint8_t s_lfo_wave;
static float s_mix = 0.5f;
static float s_param = 0.f;
static const float s_fs_recip = 1.f / 48000.f;

void DELFX_INIT(uint32_t platform, uint32_t api)
{
  s_lfo_left.reset();
  s_lfo_left.setF0(1.f, s_fs_recip);
  s_lfo_right.reset();
  s_lfo_right.setF0(1.f, s_fs_recip);
}

void DELFX_PROCESS(float *xn, uint32_t frames)
{
  float * __restrict x = xn;
  const float * x_e = x + 2*frames;
  
  for (; x != x_e; ) {  

    s_lfo_left.cycle();
    s_lfo_right.cycle();

    float wave_left = 0.f;
    float wave_right = 0.f;
    
    switch (s_lfo_wave) {
      case k_sin_bi_off:
        wave_left = s_lfo_left.sine_bi();
        wave_right = s_lfo_right.sine_bi_off(s_param);
        break;
        
      case k_tri_bi_off:
        wave_left = s_lfo_left.triangle_bi();
        wave_right = s_lfo_right.triangle_bi_off(s_param);
        break;
        
      case k_saw_bi_off:
        wave_left = s_lfo_left.saw_bi();
        wave_right = s_lfo_right.saw_bi_off(s_param);
        break;
        
      case k_sqr_bi_off:
        wave_left = s_lfo_left.square_bi();
        wave_right = s_lfo_right.square_bi_off(s_param);
        break;  
    }
    
    // Scale down the wave, full swing is way too loud. (polyphony headroom)
    wave_left  = (wave_left + 1.f) / 2.f;
    wave_right = (wave_right + 1.f) / 2.f;
    
    const float dry = 1.f - s_mix;
    const float wet = s_mix;
    // process left channel
    *x = dry * (*x) + wet * wave_left*(*x);
    ++x;
    // process right channel
    *x = dry * (*x) + wet * wave_right*(*x);
    ++x;
  }
}


void DELFX_PARAM(uint8_t index, int32_t value)
{
  const float valf = q31_to_f32(value);
  switch (index) {
  case k_user_delfx_param_time:
    s_lfo_wave = (uint8_t)si_roundf(valf * (k_wave_count - 1));
    break;
  case k_user_delfx_param_depth:
    //s_mix = valf;
    s_param = valf;
    break;
  case k_user_delfx_param_shift_depth:
    s_lfo_left.setF0(0.1f + 10.f * valf, s_fs_recip);
    s_lfo_right.setF0(0.1f + 10.f * valf, s_fs_recip);
    break;
  default:
    break;
  }
}

