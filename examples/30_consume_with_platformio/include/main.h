#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

/*
 * Open the serial port of the protocol, configure the board and report the start of the device role.
 */
void setup();

/*
 * Feed the received bytes of the serial port to the device role and maintain it, once per pass of the framework.
 */
void loop();

#endif // SERCOM_MAIN_H
