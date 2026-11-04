/*
 * Name: Karisa Kanchan
 * Student ID: 816043952
 *
 * Lab 1 - Question 11
 *
 * Samples A0 at 1000 Hz using hardware timer TC5.
 * Hardware averaging is selectable: 1 or 16 samples.
 *
 * Signal = mean of 1000 readings.
 * Noise = population standard deviation.
 *
 * Calculations use integers, not floating point.
 * Results and commands use hardware UART Serial1 only.
 * TX: pin 14. RX: pin 13. Baud rate: 115200, 8-N-1.
 * USB is used only for power/programming, not data output.
 */

#include <Arduino.h>

constexpr uint16_t SAMPLE_COUNT = 1000;
// Use 1 for no averaging, or 16 for hardware averaging.
constexpr uint8_t AVERAGING_SAMPLES = 16;

volatile uint16_t DataBuf[SAMPLE_COUNT];
volatile uint32_t TimeBuf[SAMPLE_COUNT];

volatile uint16_t samplesCollected = 0;


// Wait for ADC register synchronisation.
static void waitADC()
{
    while (ADC->STATUS.bit.SYNCBUSY) {
    }
}


// Start a conversion and busy-wait for its result.
static uint16_t readADC()
{
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;

    ADC->SWTRIG.bit.START = 1;
    waitADC();

    while (!ADC->INTFLAG.bit.RESRDY) {
    }

    return ADC->RESULT.reg;
}


// Configure A0 with the selected hardware averaging setting.
static void configureADC()
{
    analogReadResolution(12);
    analogReference(AR_DEFAULT);
    (void)analogRead(A0);

    ADC->CTRLA.bit.ENABLE = 0;
    waitADC();

    if (AVERAGING_SAMPLES == 16) {
        // Averaging mode uses the 16-bit result register.
        ADC->CTRLB.reg =
            ADC_CTRLB_PRESCALER_DIV64 |
            ADC_CTRLB_RESSEL_16BIT;

        waitADC();

        // Accumulate 16 conversions and divide by 16.
        // The final result remains on the 0-4095 count scale.
        ADC->AVGCTRL.reg =
            ADC_AVGCTRL_SAMPLENUM_16 |
            ADC_AVGCTRL_ADJRES(4);
    } else {
        ADC->CTRLB.reg =
            ADC_CTRLB_PRESCALER_DIV64 |
            ADC_CTRLB_RESSEL_12BIT;

        waitADC();

        ADC->AVGCTRL.reg =
            ADC_AVGCTRL_SAMPLENUM_1 |
            ADC_AVGCTRL_ADJRES(0);
    }

    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(5);

    ADC->CTRLA.bit.ENABLE = 1;
    waitADC();

    // Discard an initial result.
    (void)readADC();
}


// Timer interrupt: collect one measurement every 1 ms.
extern "C" void TC5_Handler(void)
{
    if (TC5->COUNT16.INTFLAG.bit.MC0) {
        TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

        uint16_t i = samplesCollected;

        if (i < SAMPLE_COUNT) {
            TimeBuf[i] = micros();
            DataBuf[i] = readADC();

            samplesCollected = i + 1;
        }
    }
}


// Configure and start TC5 at 1000 Hz.
static void startTimer()
{
    PM->APBCMASK.reg |= PM_APBCMASK_TC5;

    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_ID(GCM_TC4_TC5) |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_CLKEN;

    while (GCLK->STATUS.bit.SYNCBUSY) {
    }

    NVIC_DisableIRQ(TC5_IRQn);

    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY ||
           TC5->COUNT16.CTRLA.bit.SWRST) {
    }

    TC5->COUNT16.CTRLA.reg =
        TC_CTRLA_MODE_COUNT16 |
        TC_CTRLA_WAVEGEN_MFRQ |
        TC_CTRLA_PRESCALER_DIV64;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }

    // 48,000,000 / (64 * (749 + 1)) = 1000 Hz.
    TC5->COUNT16.CC[0].reg = 749;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }

    TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
    TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

    NVIC_ClearPendingIRQ(TC5_IRQn);
    NVIC_SetPriority(TC5_IRQn, 2);
    NVIC_EnableIRQ(TC5_IRQn);

    TC5->COUNT16.CTRLA.bit.ENABLE = 1;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }
}


// Stop sampling before calculating or printing results.
static void stopTimer()
{
    NVIC_DisableIRQ(TC5_IRQn);

    TC5->COUNT16.INTENCLR.reg = TC_INTENCLR_MC0;
    TC5->COUNT16.CTRLA.bit.ENABLE = 0;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }

    NVIC_ClearPendingIRQ(TC5_IRQn);
}


// Calculate the integer square root without floating point.
static uint64_t integerSqrt(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = UINT64_C(1) << 62;

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }

        bit >>= 2;
    }

    return result;
}


// Display a value stored as an integer multiplied by 1000.
static void printThreeDecimals(uint64_t scaledValue)
{
    Serial1.print((unsigned long)(scaledValue / 1000));
    Serial1.print('.');

    uint16_t fraction = scaledValue % 1000;

    if (fraction < 100) {
        Serial1.print('0');
    }

    if (fraction < 10) {
        Serial1.print('0');
    }

    Serial1.print(fraction);
}

// Calculate log2(SNR), scaled by 1000, using integer arithmetic.
// Input is SNR multiplied by 1000.
static int32_t calculateEnobMilli(uint64_t snrMilli)
{
    uint64_t x = snrMilli * 65536 / 1000;
    int32_t exponent = 0;
    uint32_t fraction = 0;

    // Normalise x to the range [1, 2), in Q16 format.
    while (x >= 131072) {
        x >>= 1;
        ++exponent;
    }

    while (x < 65536) {
        x <<= 1;
        --exponent;
    }

    // Calculate 16 fractional bits of the logarithm.
    for (uint8_t i = 0; i < 16; ++i) {
        x = (x * x) >> 16;

        if (x >= 131072) {
            x >>= 1;
            fraction |= (1UL << (15 - i));
        }
    }

    return exponent * 1000 +
           (int32_t)((uint64_t)fraction * 1000 / 65536);
}

// Print the number of times each ADC value occurred.
// Call only after sampling has stopped.
static void printPMF(Print &output)
{
    uint16_t lowest = 4095;
    uint16_t highest = 0;

    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i) {
        uint16_t value = DataBuf[i];

        if (value < lowest) {
            lowest = value;
        }

        if (value > highest) {
            highest = value;
        }
    }

    output.println();
    output.print("PMF_BEGIN,averaging=");
    output.println((unsigned int)AVERAGING_SAMPLES);

    output.println("ADC_value,Count");

    for (uint32_t value = lowest; value <= highest; ++value) {
        uint16_t count = 0;

        for (uint16_t i = 0; i < SAMPLE_COUNT; ++i) {
            if (DataBuf[i] == value) {
                ++count;
            }
        }

        output.print((unsigned long)value);
        output.print(',');
        output.println((unsigned int)count);
    }

    output.println("PMF_END");
}

// Collect a block, calculate statistics and print the results.
static void collectAndReport()
{
    Serial1.println("Collecting 1000 samples...");
    Serial1.flush();

    samplesCollected = 0;
    startTimer();

    while (samplesCollected < SAMPLE_COUNT) {
    }

    stopTimer();

    uint64_t sum = 0;
    uint64_t sumSquares = 0;

    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i) {
        uint32_t value = DataBuf[i];

        sum += value;
        sumSquares += (uint64_t)value * value;
    }

    // Mean multiplied by 1000.
    uint64_t meanMilli = sum * 1000 / SAMPLE_COUNT;

    // Population variance:
    // variance = (N * sum(x*x) - sum(x)^2) / N^2.
    uint64_t numerator =
        SAMPLE_COUNT * sumSquares - sum * sum;

    // Scale variance so its square root retains 3 decimal places.
    uint64_t varianceScaled =
        numerator * UINT64_C(1000000) /
        ((uint64_t)SAMPLE_COUNT * SAMPLE_COUNT);

    uint64_t noiseMilli = integerSqrt(varianceScaled);

    Serial1.println();
    Serial1.println("--- Q11 RESULTS ---");

    Serial1.print("Hardware averaging window: ");
    Serial1.println((unsigned int)AVERAGING_SAMPLES);
    Serial1.println("Target sampling rate: 1000 Hz");

    Serial1.print("Samples collected: ");
    Serial1.println((unsigned int)samplesCollected);

    Serial1.print("Signal (mean): ");
    printThreeDecimals(meanMilli);
    Serial1.println(" ADC counts");

    Serial1.print("Noise (standard deviation): ");
    printThreeDecimals(noiseMilli);
    Serial1.println(" ADC counts");

    if (noiseMilli != 0 && meanMilli != 0) {
        uint64_t snrMilli = meanMilli * 1000 / noiseMilli;

        if (snrMilli != 0) {
            Serial1.print("SNR (linear): ");
            printThreeDecimals(snrMilli);
            Serial1.println();

            int32_t enobMilli = calculateEnobMilli(snrMilli);

            Serial1.print("ENOB: ");

            if (enobMilli < 0) {
                Serial1.print('-');
                enobMilli = -enobMilli;
            }

            printThreeDecimals((uint64_t)enobMilli);
            Serial1.println(" bits");
        } else {
            Serial1.println("SNR below reporting precision.");
            Serial1.println("ENOB unavailable.");
        }
    } else {
        Serial1.println("SNR and ENOB unavailable: zero signal or noise.");
    }

    Serial1.println("--- END RESULTS ---");

    // Q10: Calculate the intervals between consecutive samples.
    uint32_t minimumInterval = UINT32_MAX;
    uint32_t maximumInterval = 0;

    for (uint16_t i = 1; i < SAMPLE_COUNT; ++i) {
        uint32_t interval = TimeBuf[i] - TimeBuf[i - 1];

        if (interval < minimumInterval) {
            minimumInterval = interval;
        }

        if (interval > maximumInterval) {
            maximumInterval = interval;
        }
    }

    // Peak-to-peak jitter is the largest interval minus the smallest.
    uint32_t jitter = maximumInterval - minimumInterval;

    Serial1.println();
    Serial1.println("--- Q11 TIMING RESULTS ---");

    Serial1.println("Target sampling interval: 1000 us");
    Serial1.println("Sampling intervals checked: 999");

    Serial1.print("Minimum sampling interval: ");
    Serial1.print((unsigned long)minimumInterval);
    Serial1.println(" us");

    Serial1.print("Maximum sampling interval: ");
    Serial1.print((unsigned long)maximumInterval);
    Serial1.println(" us");

    Serial1.print("Peak-to-peak sampling jitter: ");
    Serial1.print((unsigned long)jitter);
    Serial1.println(" us");

    Serial1.println("--- END Q11 RESULTS ---");

    // Export the PMF through hardware UART.
    printPMF(Serial1);



    Serial1.println("Send r through UART to collect another block.");
}


void setup()
{
    Serial1.begin(115200);

    // Hardware UART starts without waiting for a USB connection.

    configureADC();

    delay(500);

    collectAndReport();
}


void loop()
{
    if (Serial1.available()) {
        char command = Serial1.read();

        if (command == 'r' || command == 'R') {
            collectAndReport();
        }
    }
}