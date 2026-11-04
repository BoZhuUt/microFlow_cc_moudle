#include "flow_scale.h"
#include <math.h>
#include <stddef.h>

/* User-supplied JDL5135 example, NOT a calibration of the installed sensor.
 * Normalize the example's 0..10 SLM shape to the configured full scale.
 * Voltage offsets use its zero-flow output, 0.51 V. Absolute example volts
 * and SLM are never applied directly to the installed sensor.
 */
static const float fraction[] = {0, .01f, .105f, .239f, .306f,
                                 .424f, .552f, .706f, .9f, 1};
static const float rise[] = {0, .54f, 1.84f, 2.38f, 2.61f,
                             2.87f, 3.10f, 3.34f, 3.81f, 4.01f};

static float interpolate(const float *x, const float *y, float value)
{
    unsigned i;
    for (i = 1; i < 9 && value > x[i]; ++i) {}
    /* Last segment extrapolates above range instead of falsely reporting full scale. */
    return y[i-1] + (value-x[i-1]) * (y[i]-y[i-1]) / (x[i]-x[i-1]);
}

int FlowScale_ValidateCalibration(uint16_t model, float lowFlow,
                                  float referenceFlow, float zeroVoltage,
                                  float referenceVoltage, float fullScale)
{
    if (!isfinite(lowFlow) ||
        !isfinite(referenceFlow) || !isfinite(zeroVoltage) ||
        !isfinite(referenceVoltage) || lowFlow < 0.0f || lowFlow > 30.0f ||
        referenceFlow < 0.0f || referenceFlow > 30.0f || lowFlow == referenceFlow ||
        zeroVoltage < 0.0f || zeroVoltage > 3.3f ||
        referenceVoltage < 0.0f || referenceVoltage > 3.3f ||
        fabsf(referenceVoltage-zeroVoltage) < 0.001f)
        return 0;
    if (model == 0U) return 1;
    if (model == 1U)
        return lowFlow == 0.0f && isfinite(fullScale) && fullScale > 0.0f &&
               fullScale <= 30.0f && referenceFlow > 0.0f && referenceFlow <= fullScale &&
               referenceVoltage > zeroVoltage;
    return 0;
}

int FlowScale_Convert(uint16_t model, float voltage, float lowFlow,
                      float referenceFlow, float zeroVoltage,
                      float referenceVoltage, float fullScale, float *output)
{
    float value, referenceRise;
    if (output == NULL || !isfinite(voltage) ||
        !FlowScale_ValidateCalibration(model, lowFlow, referenceFlow,
                                        zeroVoltage, referenceVoltage, fullScale))
        return 0;
    if (model == 0U) {
        value = lowFlow + (voltage-zeroVoltage) * (referenceFlow-lowFlow) /
                         (referenceVoltage-zeroVoltage);
    } else if (model == 1U) {
        referenceRise = interpolate(fraction, rise, referenceFlow/fullScale);
        value = voltage <= zeroVoltage ? 0.0f : fullScale *
            interpolate(rise, fraction,
                (voltage-zeroVoltage) * referenceRise / (referenceVoltage-zeroVoltage));
    } else return 0;
    if (!isfinite(value)) return 0;
    *output = value;
    return 1;
}
