#pragma once

#include <Arduino.h>

void initializeFirmwareBootHealth(const char *currentVersion);
void armFirmwareBootValidation(const char *targetVersion);
void confirmFirmwareBootHealthy();
bool isFirmwareBootValidationPending();
bool isRecoverySafeMode();
uint8_t getFirmwareBootAttemptCount();
String getFirmwareBootHealthStatus();

