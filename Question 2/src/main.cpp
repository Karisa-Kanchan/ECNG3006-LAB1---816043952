#ifndef PIO_UNIT_TESTING
#include <Arduino.h>

extern "C" {
#include "calc.h"
}

void setup()
{
    SerialUSB.begin(115200);

    // Wait for the Serial Monitor to connect.
    while (!SerialUSB)
    {
    }

    int numbers[] = {-7, 7, 0, -128, 300};
    int count = sizeof(numbers) / sizeof(numbers[0]);

    SerialUSB.println("Magnitude results:");

    for (int i = 0; i < count; i++)
    {
        SerialUSB.print("magnitude(");
        SerialUSB.print(numbers[i]);
        SerialUSB.print(") = ");
        SerialUSB.println(magnitude(numbers[i]));
    }
}

void loop()
{
}

#endif