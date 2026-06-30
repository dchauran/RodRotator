#pragma once

#include <Arduino.h>

void setupMotion();
void configureStepRate(float rpm);
void setDriverEnabled(bool enabled);
void setMotorRunning(bool running);
void setDirection(bool forward);
void setSpeedIndex(uint8_t nextSpeedIndex);
void setTargetRpm(float nextTargetRpm, uint8_t nextSpeedIndex);
void pollSpeedRamp();
uint8_t speedBucketForRpm(float rpm);
