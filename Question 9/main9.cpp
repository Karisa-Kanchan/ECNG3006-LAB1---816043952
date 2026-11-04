/*
 * Name: Karisa Kanchan
 * Student ID: 816043952
 *
 * Lab 1 - Question 9
 *
 * Samples A0 at 1000 Hz using hardware timer TC5.
 * Hardware averaging is disabled.
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


// Configure A0 for 12-bit measurements without averaging.
static void configureADC()
{
    // Use the Arduino core to select A0 and initialise the ADC.
    analogReadResolution(12);
    analogReference(AR_DEFAULT);
    (void)analogRead(A0);

    ADC->CTRLA.bit.ENABLE = 0;
    waitADC();

    // ADC clock: 48 MHz / 64 = 750 kHz.
    ADC->CTRLB.reg =
        ADC_CTRLB_PRESCALER_DIV64 |
        ADC_CTRLB_RESSEL_12BIT;

    waitADC();

    // One conversion per result: hardware averaging OFF.
    ADC->AVGCTRL.reg =
        ADC_AVGCTRL_SAMPLENUM_1 |
        ADC_AVGCTRL_ADJRES(0);

    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(5);

    ADC->CTRLA.bit.ENABLE = 1;
    waitADC();

    // Discard one initial conversion.
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
    Serial1.println("--- Q9 RESULTS ---");

    Serial1.println("Hardware averaging: OFF (1 sample)");
    Serial1.println("Target sampling rate: 1000 Hz");

    Serial1.print("Samples collected: ");
    Serial1.println((unsigned int)samplesCollected);

    Serial1.print("Signal (mean): ");
    printThreeDecimals(meanMilli);
    Serial1.println(" ADC counts");

    Serial1.print("Noise (standard deviation): ");
    printThreeDecimals(noiseMilli);
    Serial1.println(" ADC counts");

    if (noiseMilli != 0) {
        Serial1.print("SNR (linear): ");

        uint64_t snrMilli = meanMilli * 1000 / noiseMilli;

        printThreeDecimals(snrMilli);
        Serial1.println();

        Serial1.println("Calculate ENOB = log(SNR) / log(2).");
    } else {
        Serial1.println("Noise unresolved in this dataset.");
        Serial1.println("SNR and ENOB cannot be calculated.");
    }

    Serial1.println("--- END RESULTS ---");
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