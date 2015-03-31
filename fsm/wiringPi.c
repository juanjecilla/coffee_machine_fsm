#include <stdio.h>
#include "wiringPi.h"

void digitalWrite(int gpio, int value){
	printf("PIN %d tiene un valor de %d\n", gpio, value);
}

void pinMode(int gpio, int value){}
