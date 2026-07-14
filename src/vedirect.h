#ifndef VEDIRECT_H
#define VEDIRECT_H

#include <Arduino.h>

void sendVEdirect();
void sendBinaryValue(String type, int value);
void sendVEdirectBinary();
bool queueVEdirectBinary();
bool isVEdirectBinaryBusy();

#endif
