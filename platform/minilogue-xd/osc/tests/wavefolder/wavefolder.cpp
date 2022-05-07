/*
    BSD 3-Clause License

    Copyright (c) 2018, KORG INC.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//*/

/*
 * File: wavefolder.cpp
 * Author: Fredrik Fagerholm
 * Created: 2022-05-06
 * 
 * Sine oscillator with naive wavefolder.
 * 
 * No anti-aliasing is applied!
 * Based on https://ccrma.stanford.edu/~jatin/ComplexNonlinearities/Wavefolder.html
 */

#include "userosc.h"
typedef __uint32_t uint32_t;

typedef struct State {
  float w0;
  float phase;
  float dist;
  float ff_drive;
  float fb_drive;
  float wf_gain, ff_gain, fb_gain;
  float z;
  float lfo, lfoz;
  uint8_t flags;
} State;

static State s_state;

enum {
  k_flags_none = 0,
  k_flag_reset = 1<<0,
};

void OSC_INIT(uint32_t platform, uint32_t api)
{
  s_state.w0    = 0.f;
  s_state.phase = 0.f;
  s_state.dist  = 0.f;
  s_state.ff_drive = 1.f;
  s_state.fb_drive = 1.f;
  s_state.wf_gain = -0.38461538f;
  s_state.ff_gain = 0.61538462f;
  s_state.fb_gain = 0.76923077f;
  s_state.z     = 0.f;
  s_state.lfo = s_state.lfoz = 0.f;
  s_state.flags = k_flags_none;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{  
  const uint8_t flags = s_state.flags;
  s_state.flags = k_flags_none;
  
  const float w0 = s_state.w0 = osc_w0f_for_note((params->pitch)>>8, params->pitch & 0xFF);
  float phase = (flags & k_flag_reset) ? 0.f : s_state.phase;
  
  const float dist  = s_state.dist;
  const float ff_drive = s_state.ff_drive;
  const float fb_drive  = s_state.fb_drive;

  const float gain_sum = s_state.wf_gain + s_state.ff_gain + s_state.fb_gain;
  const float wf_gain_norm = s_state.wf_gain / gain_sum;
  const float ff_gain_norm = s_state.ff_gain / gain_sum;
  const float fb_gain_norm = s_state.fb_gain / gain_sum;

  float z = s_state.z;

  const float lfo = s_state.lfo = q31_to_f32(params->shape_lfo);
  float lfoz = (flags & k_flag_reset) ? lfo : s_state.lfoz;
  const float lfo_inc = (lfo - lfoz) / frames;

  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;
  
  for (; y != y_e; ) {
    const float dist_mod = dist + lfoz * dist;

    float p = phase;
    p = (p <= 0) ? 1.f - p : p - (uint32_t)p;

    static float x = osc_sinf(p);
    static float wf = osc_sinf(fmod(dist_mod*x, 1.0f));
    static float ff = osc_softclipf(0.05f, ff_drive * x);
    static float fb = osc_softclipf(0.05f, fb_drive * z);
    z = wf_gain_norm*wf + ff_gain_norm*ff + fb_gain_norm*fb;
    // Main signal
    const float sig = osc_softclipf(0.05f, z);
    *(y++) = f32_to_q31(sig);
    
    phase += w0;
    phase -= (uint32_t)phase;

    lfoz += lfo_inc;
  }

  s_state.z = z;
  s_state.phase = phase;
  s_state.lfoz = lfoz;
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
  s_state.flags |= k_flag_reset;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
  (void)params;
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
  
  switch (index) {
  case k_user_osc_param_id1:
    {
      const int16_t bipolar = value - 100; // recover signed representation
      s_state.wf_gain = bipolar * 0.01f; // scale to [-1.f,1.f]
    }
    break;
  case k_user_osc_param_id2:
    {
      const int16_t bipolar = value - 100; // recover signed representation
      s_state.ff_gain = bipolar * 0.01f; // scale to [-1.f,1.f]
    }
    break;
  case k_user_osc_param_id3:
    {
      const int16_t bipolar = value - 100; // recover signed representation
      s_state.fb_gain = bipolar * 0.01f; // scale to [-1.f,1.f]
    }
    break;
  case k_user_osc_param_id4:
    const float valf = param_val_to_f32(value);
    s_state.fb_drive = 1.f + valf; 
  case k_user_osc_param_id5:
  case k_user_osc_param_id6:
    break;
  case k_user_osc_param_shape:
    const float valf = param_val_to_f32(value);
    s_state.dist = 3.0f * valf;
    break;
  case k_user_osc_param_shiftshape:
    const float valf = param_val_to_f32(value);
    s_state.ff_drive = 1.f + valf; 
    break;
  default:
    break;
  }
}

