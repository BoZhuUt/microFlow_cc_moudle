#ifndef FLOW_SCALE_H
#define FLOW_SCALE_H
#include <stdint.h>

#define FLOW_SCALE_VERSION 0x5301U
/* Shared by runtime conversion and persisted-parameter validation. */
int FlowScale_ValidateCalibration(uint16_t model, float lowFlow,
                                  float referenceFlow, float zeroVoltage,
                                  float referenceVoltage, float fullScale);
/* Returns 0 for invalid calibration/input; output is untouched on failure. */
int FlowScale_Convert(uint16_t model, float voltage, float lowFlow,
                      float referenceFlow, float zeroVoltage,
                      float referenceVoltage, float fullScale, float *output);
#endif
