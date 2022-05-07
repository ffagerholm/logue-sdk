#include "usermodfx.h"
#include "float_math.h"


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
}


void MODFX_PROCESS(const float *main_xn, float *main_yn,
                   const float *sub_xn, float *sub_yn,
                   uint32_t frames)
{
    
    const int bit_depth = s_state.bit_depth;
    const int bit_rate = s_state.bit_rate;
    
    int max_value = pow(2, bit_depth) - 1;
    int step = SAMPLE_RATE / bit_rate;

    const float leftFirstSample = 0.f;
    const float rightFirstSample = 0.f;
 
    int i = 0;
    while (i < frames)
    {
        // Get the values from the left and right channels
        const float inLeft = main_xn[i * 2];
        const float inRight = main_xn[i * 2 + 1];

        // Reduce the bit_rate of the current sample 
        leftFirstSample = si_roundf((inLeft + 1.0) * max_value) / max_value - 1.0;
        rightFirstSample = si_roundf((inRight + 1.0) * max_value) / max_value - 1.0;
 
        // This loop simulates down-sampling of the signal.
        // The same sample is used in place of the `step` next samples
        for (int j = 0; j < step && i < frames; j++)
        {
            main_yn[i * 2] = leftFirstSample;
            main_yn[i * 2 + 1] = rightFirstSample;
            // move to next position in the output buffer
            i++;
        }
    }
}


void MODFX_PARAM(uint8_t index, int32_t value)
{
    // Convert fixed-point q31 format to float
    const float valf = param_val_to_f32(value);

    switch (index)
    {
        // Assign a value to rate when turning the time knob
        case k_user_modfx_param_time:
          s_state.bit_rate = si_roundf((SAMPLE_RATE - 1)*valf + 1);
          break;
        case k_user_modfx_param_depth:
          s_state.bit_depth = si_roundf(31*valf + 1);
          break;
        default:
          break;
    }
}