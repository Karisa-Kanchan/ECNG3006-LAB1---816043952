/*
 * Name: Karisa Kanchan
 * Student ID: 816043952
 *
 * Question 7: Serial receive debounce FSM.
 *
 * Strategy: switch-case.
 * Only lowercase 'a' is valid.
 * Each valid character restarts the debounce interval, including a repeated character that is ignored.
 * Invalid characters do not change the state or timer.
 *
 * q7_tick() is called only when a new system tick is observed.
 */

#include <stdint.h>

#define VALID_CHARACTER  'a'
#define DEBOUNCE_MS      500UL

typedef enum
{
    STATE_OFF,
    STATE_ON
} led_state_t;

/* Functions supplied by main.cpp. */
extern int serial_character_available(void);
extern int serial_read_character(void);
extern void led_write(int on);
extern void report_message(const char *message);

static led_state_t state = STATE_OFF;
static uint32_t last_valid_ms = 0U;
static int valid_seen = 0;

void q7_init(void)
{
    state = STATE_OFF;
    last_valid_ms = 0U;
    valid_seen = 0;

    led_write(0);
}

void q7_tick(uint32_t now_ms)
{
    while (serial_character_available())
    {
        int received = serial_read_character();

        if (received != VALID_CHARACTER)
        {
            report_message("Invalid character: no change.");
            continue;
        }

        uint32_t interval = now_ms - last_valid_ms;

        /* Reset on every valid 'a', even if it is ignored. */
        last_valid_ms = now_ms;

        if (valid_seen && interval <= DEBOUNCE_MS)
        {
            report_message("Repeated a within 500 ms: ignored.");
            continue;
        }

        valid_seen = 1;

        switch (state)
        {
            case STATE_OFF:
                state = STATE_ON;
                led_write(1);
                report_message("Valid a: LED ON.");
                break;

            case STATE_ON:
                state = STATE_OFF;
                led_write(0);
                report_message("Valid a: LED OFF.");
                break;

            default:
                state = STATE_OFF;
                led_write(0);
                break;
        }
    }
}