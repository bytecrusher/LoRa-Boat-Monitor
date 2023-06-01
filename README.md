# LoRa-Boat-Monitor
Fork of "LoRa Boat Monitor" from GitHub from Norbert Walter

Small hardware changes:
  - cut the +5V line to the GPS module, and wire the GPS +5V pin to one the relay pins (X3-3).
  - The Other (X3-2) connect to +5V of the Board.
This gives you the option, to shutdown the GPS Modul in sleep mode.

Another change that i made is the related to the IN1 (alarm1):
 - connect X1-4 to GND
 - connect X1-3 to the output of your Batterie switch (turn Main Batterie on, will start the Lora-Boat-Monitor, turn of your switch will turn the Lora-Boat-Monitor into Standby)
    (i change the IN1/alarm1 logic from "low active" to "high active".

