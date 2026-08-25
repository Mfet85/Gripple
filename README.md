# Gripple: Grip-Strength Monitoring Glove

A wearable glove that tracks grip strength and finger movement speed to help monitor rheumatoid arthritis (RA) disease activity over time. Built as a group project.

**[Watch the product advert](https://www.youtube.com/watch?v=j_Db0eZFdeM)**

## What it does

The user squeezes a force sensor built into the palm of the glove across 5 reps per session. Gripple measures:
- **Grip force** (via an FSR sensor, calibrated to Newtons)
- **Movement initiation speed** (time between hand movement and force application, via an onboard accelerometer)

A baseline session is recorded on first use. Every session after that is compared against the baseline, classifying the user into an RA activity zone (Dangerous, Low, Moderate, High, Remission) and tracking trends over time, all displayed live on an onboard OLED screen.

## Hardware

- Adafruit Circuit Playground Express (built-in LIS3DH accelerometer)
- FSR (force-sensitive resistor) on the palm, via a voltage divider
- SSD1306 128x64 OLED display
- Hand-knitted glove body

## How it works

1. **Calibration:** Upon first use, user sets biological gender (used for grip-force reference ranges) and records a baseline session
2. **Grip test:** 5 reps done each morning. open hand → move toward sensor (accelerometer-triggered) → squeeze and hold for 3 seconds
3. **Zone classification:** <ean grip force is mapped to an RA activity zone using fixed thresholds
4. **Feedback:** Results are compared to baseline and shown on device

## Accuracy & false-positive handling

The sensor was validated against a resting hand, a repeated grip test, and two confounding-variable tests (hand tapping the table, and wrist movement without gripping) to check for false triggers. Both confounders produced no meaningful signal change compared to the resting baseline, confirming the sensor doesn't false-positive on incidental hand movement.

Several safeguards are built into the code to reduce false positives/negatives:
- A minimum force threshold before a squeeze is registered as a rep
- A sustained-hold requirement (rather than a single brief spike) before a squeeze counts
- A drain timeout to clear residual pressure between menu taps
- An accelerometer movement threshold to avoid triggering on drift or vibration

## Code

See [`Gripple.ino`](./Gripple.ino) — written in the Arduino IDE 2 environment for the Circuit Playground Express, using the `Adafruit_CircuitPlayground`, `Adafruit_GFX`, and `Adafruit_SSD1306` libraries.
