#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

/*
 * Open the serial port and configure the board.
 */
void setup();

/*
 * Feed the received bytes to the device application and maintain it.
 */
void loop();

#endif // SERCOM_MAIN_H
