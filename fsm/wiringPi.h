#ifndef WIRINGPI_H
#define WIRINGPI_H

#define LOW 0
#define HIGH 1

#define INPUT 0
#define OUTPUT 1


#define wiringPiISR(gpio, edge, function)
#define wiringPiSetup()


void digitalWrite (int gpio, int value);
void pinMode(int gpio, int value);


#endif