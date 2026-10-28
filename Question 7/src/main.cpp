/*
 * Name: Karisa Kanchan
 * Student ID: 816043952
 *
 * Arduino interface for the C FSM in q7.c.
 * Serial is the USB connection to the computer.
 * Serial1 copies received characters onto the UART TX pin so they can be observed with an oscilloscope in Q8.
 */

#include <Arduino.h>
#include <stdint.h>
#define LED_PIN 2

extern "C"
{
    void q7_init(void);
    void q7_tick(uint32_t now_ms);

    int serial_character_available(void);
    int serial_read_character(void);
    void led_write(int on);
    void report_message(const char *message);
}

int serial_character_available(void)
{
    return Serial.available();
}

int serial_read_character(void)
{
    int received = Serial.read();

    if (received >= 0)
    {
        Serial1.write((uint8_t)received);
    }

    return received;
}

void led_write(int on)
{
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void report_message(const char *message)
{
    Serial.println(message);
}

void setup(void)
{
    pinMode(LED_PIN, OUTPUT);

    Serial.begin(115200);
    Serial1.begin(9600);

    q7_init();
}

void loop(void)
{
    static uint32_t previous_tick = 0U;

    uint32_t current_tick = millis();

    /* Run the FSM only after the system tick changes. */
    if (current_tick != previous_tick)
    {
        previous_tick = current_tick;
        q7_tick(current_tick);
    }
}