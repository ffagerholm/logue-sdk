#include "usermodfx.h"


static const int SAMPLE_RATE = 48000;

typedef struct State {
  int bit_depth;
  int bit_rate;
} State;

static State s_state;


// Initialization function
void MODFX_INIT(uint32_t platform, uint32_t api)
{
  s_state.bit_depth = 32;
  s_state.bit_rate = SAMPLE_RATE;

  (void)platform;
  (void)api;
}


void MODFX_PROCESS(const float *main_xn, float *main_yn,
                   const float *sub_xn, float *sub_yn,
                   uint32_t frames)
{
    const float * mx = main_xn;
    float * __restrict my = main_yn;
    const float * my_e = my + 2*frames;

    const int bit_depth = s_state.bit_depth;
    const int bit_rate = s_state.bit_rate;
    
    const float max_value = (float)((1 << bit_depth) - 1);
    const int step = (int)(SAMPLE_RATE / bit_rate);

    float inLeft;
    float inRight;

    float leftFirstSample;
    float rightFirstSample;

    for (; my < my_e; )
    {
        // Get the values from the left and right channels
        inLeft = *(mx++);
        inRight = *(mx++);

        // Reduce the bit_rate of the current sample 
        leftFirstSample = si_roundf((inLeft + 1.f) * max_value) / max_value - 1.f;
        rightFirstSample = si_roundf((inRight + 1.f) * max_value) / max_value - 1.f;
 
        // This loop simulates down-sampling of the signal.
        // The same sample is used in place of the `step` next samples
        for (int j = 0; j < step && my < my_e; j++)
        {
            *(my++) = leftFirstSample;
            *(my++) = rightFirstSample;
            // move to next position in the input and output buffers
            mx++;
            mx++;
        }
    }
}


void MODFX_PARAM(uint8_t index, int32_t value)
{
    // Convert 10-bit int32_t value to float in range [0, 1]
    const float valf = q31_to_f32(value);

    switch (index)
    {
        // Assign a value to rate when turning the time knob
        case k_user_modfx_param_time:
          s_state.bit_rate = (int)((SAMPLE_RATE - 1.f)*valf + 1.f);
          break;
        case k_user_modfx_param_depth:
          s_state.bit_depth = (int)(29.f*valf + 2.f);
          break;
        default:
          break;
    }
}