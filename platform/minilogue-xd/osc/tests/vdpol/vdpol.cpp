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
 * File: vdpol.cpp
 *
 * Van der Pol Oscillator
 * 
 * Solve Van der Pol equation using the fourth Order Runge-Kutta method.
 *   
 * Solves the equation system
 *   x' = y
 *   y' = mu (1 - x^2) y - alpha x
 * where mu controls the daming, and alpha controls the oscillations frequency.  
 * 
 * https://en.wikipedia.org/wiki/Van_der_Pol_oscillator
 * https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods
 */

#include "userosc.h"
#define SAMPLING_FREQUENCY 48000

typedef struct State {
  float w0;
  float x;
  float y;
  float mu;
  uint8_t flags;
} State;

static State s_state;

enum {
  k_flags_none = 0,
  k_flag_reset = 1<<0,
};

void OSC_INIT(uint32_t platform, uint32_t api)
{
  s_state.w0 = 0.f;
  s_state.x = 1.f;
  s_state.y = 1.f;
  s_state.mu  = 0.f;
  s_state.flags = k_flags_none;
}

float g(float x, float y, float mu, float alpha) {
  return mu*(1 - x*x)*y - alpha*x;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{  
  const uint8_t flags = s_state.flags;
  s_state.flags = k_flags_none;

  float x = (flags & k_flag_reset) ? 1.f : s_state.x;
  float y = (flags & k_flag_reset) ? 1.f : s_state.y;

  const float w0 = 2.0f*PI*osc_notehzf((params->pitch)>>8);
  const float alpha = w0*w0;
  const float mu = s_state.mu;
  // Time step h is set as the sampling time 
  const float h = 1.0f / SAMPLING_FREQUENCY;

  float k0, l0, k1, l1, k2, l2, k3, l3;
  
  q31_t * __restrict z = (q31_t *)yn;
  const q31_t * z_e = z + frames;
  
  for (; z != z_e; ) {
    // Perform a step of Runge–Kutta fourth-order method
    // for every sample
    k0 = h*y;
    l0 = h*g(x, y, mu, alpha);

    k1 = h*(y + 0.5f*l0);
    l1 = h*g(x + 0.5f*k0, y + 0.5f*l0, mu, alpha);

    k2 = h*(y + 0.5f*l1);
    l2 = h*g(x + 0.5f*k1, y + 0.5f*l1, mu, alpha);

    k3 = h*(y + l2);
    l3 = h*g(x + k2, y + l2, mu, alpha);

    x = x + (1.0f / 6.0f)*(k0 + 2.0f*k1 + 2.0f*k2 + k3);
    y = y + (1.0f / 6.0f)*(l0 + 2.0f*l1 + 2.0f*l2 + l3);

    // Set the current sample
    const float sig  = osc_softclipf(0.05f, 0.5f*x);
    *(z++) = f32_to_q31(sig);
  }

  s_state.x = x;
  s_state.y = y;
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
  const float valf = param_val_to_f32(value);
  
  switch (index) {
  case k_user_osc_param_id1:
  case k_user_osc_param_id2:
  case k_user_osc_param_id3:
  case k_user_osc_param_id4:
  case k_user_osc_param_id5:
  case k_user_osc_param_id6:
    break;
  case k_user_osc_param_shape:
    s_state.mu = 10000.0f * valf;
    break;
  case k_user_osc_param_shiftshape:
    break;
  default:
    break;
  }
}

