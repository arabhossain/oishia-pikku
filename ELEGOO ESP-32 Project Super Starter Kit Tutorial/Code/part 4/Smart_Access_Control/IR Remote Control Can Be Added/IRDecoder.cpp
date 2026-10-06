#include "IRDecoder.h"

const int IR = IR_RECEIVER_PIN;

// New: Maximum timeout for single decoding (100ms, avoid long-term blocking)
#define DECODE_TIMEOUT 100000  // 100,000 microseconds = 100 milliseconds

/**
 * @brief Non-blocking read of specified level duration (core fix)
 * @param level Target level (HIGH/LOW)
 * @param maxDuration Maximum allowed duration (in microseconds)
 * @return Duration of the level (in microseconds), returns 0 if timed out
 */
static unsigned long readPulseNonBlocking(int level, unsigned long maxDuration) {
  unsigned long start = micros();
  while (digitalRead(IR) == level) {
    // 1. Exceed target duration: return current duration (for leader code/data bit judgment)
    if (micros() - start > maxDuration) {
      return micros() - start;
    }
    // 2. Exceed total single decoding timeout: return 0 directly to exit decoding (avoid blocking)
    if (micros() - start > DECODE_TIMEOUT) {
      return 0;
    }
    delayMicroseconds(10);  // Minor delay to reduce CPU usage
  }
  return micros() - start;
}

void IRDecoder_Init() {
  pinMode(IR, INPUT);
  Serial.println("IR Receiver initialized successfully (Pin GPIO25)");
}

/**
 * @brief Non-blocking IR decoding (core fix, no main loop blocking)
 * @return Decoded 32-bit NEC protocol code, returns 0 if decoding fails/timeouts
 */
unsigned long IRDecoder_Decode() {
  unsigned long decodeStart = micros();  // Record decoding start time

  // 1. Wait for NEC leader code low level (9ms), return quickly if timed out
  if (digitalRead(IR) != LOW) {
    return 0;
  }
  unsigned long lowTime = readPulseNonBlocking(LOW, 10000);  // Max wait 10ms
  if (lowTime < 8000 || lowTime > 10000) {  // Leader code low level must be 8-10ms
    return 0;
  }

  // 2. Read leader code high level (4.5ms), return quickly if timed out
  unsigned long highTime = readPulseNonBlocking(HIGH, 5000);  // Max wait 5ms
  if (highTime < 4000 || highTime > 5000) {  // Leader code high level must be 4-5ms
    return 0;
  }

  // 3. Read 32-bit data (non-blocking, each bit has timeout)
  unsigned long code = 0;
  for (int i = 0; i < 32; i++) {
    // 3.1 Read bit start low level (560us)
    unsigned long bitLow = readPulseNonBlocking(LOW, 700);  // Max wait 700us
    if (bitLow < 400 || bitLow > 700) {
      return 0;
    }

    // 3.2 Read bit high level (0=560us, 1=1680us)
    unsigned long bitHigh = readPulseNonBlocking(HIGH, 1800);  // Max wait 1800us
    if (bitHigh < 400 || bitHigh > 1800) {
      return 0;
    }

    // 3.3 Assemble data bit
    code <<= 1;
    code |= (bitHigh > 1000) ? 1 : 0;

    // 4. Check total single decoding timeout (avoid infinite loop)
    if (micros() - decodeStart > DECODE_TIMEOUT) {
      return 0;
    }
  }

  // 5. Verify stop bit (560us low level)
  unsigned long stopLow = readPulseNonBlocking(LOW, 700);
  if (stopLow < 400 || stopLow > 700) {
    return 0;
  }

  // 6. Debounce delay (shortened to 50ms, reduce waiting time)
  delay(50);

  return code;
}