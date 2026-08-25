#include <Adafruit_CircuitPlayground.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define TOTAL_REPS 5
#define HOLD_DURATION 3000
#define REST_DURATION 5000

#define FSR_PIN A1
#define FSR_THRESHOLD 100
#define FSR_TAP_THRESHOLD 150
#define FSR_DRAIN_TIMEOUT 2000
#define SAMPLE_INTERVAL 50

#define ACCEL_THRESHOLD 1.5
#define ACCEL_BASELINE_MS 500

#define MAX_SESSIONS 20

#define MALE_F1 568.98
#define MALE_R1 766.0
#define FEMALE_F1 370.3
#define FEMALE_R1 746.0

#define ZONE_DANGEROUS_MAX 174.32
#define ZONE_LOW_MAX 201.40
#define ZONE_MODERATE_MAX 234.46
#define ZONE_REMISSION_MIN 327.65

float R0 = 0;
char userName[30] = "";
char userSex = ' ';
float F1_cal = 0;
float R1_cal = 0;
bool isFirstTime = true;

float baselineMeanForce = 0;
float baselineMeanInit = 0;
int baselineZone = -1;

float sessionForces[MAX_SESSIONS];
float sessionInits[MAX_SESSIONS];
int sessionCount = 0;

float repMeans[TOTAL_REPS];
float repInitTimes[TOTAL_REPS];
float repForces[TOTAL_REPS];

int getZone(float forceN) {
  if (forceN < ZONE_DANGEROUS_MAX) return 0;
  if (forceN < ZONE_LOW_MAX) return 1;
  if (forceN < ZONE_MODERATE_MAX) return 2;
  if (forceN < ZONE_REMISSION_MIN) return 3;
  return 4;
}

const char* getZoneName(int zone) {
  switch (zone) {
    case 0: return "DANGEROUS";
    case 1: return "Low activity";
    case 2: return "Moderate";
    case 3: return "High activity";
    case 4: return "REMISSION";
    default: return "Unknown";
  }
}

void drawZoneBar(int currentZone, int baseZone) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("RA Activity Zone:");

  int barY = 16;
  int barH = 10;
  int segW = 25;
  int barX = 2;
  const char* labels[] = {"DNG", "LOW", "MOD", "HGH", "REM"};

  for (int z = 0; z < 5; z++) {
    int x = barX + z * segW;
    display.drawRect(x, barY, segW, barH, SSD1306_WHITE);
    if (z == currentZone) {
      display.fillRect(x + 1, barY + 1, segW - 2, barH - 2, SSD1306_WHITE);
    }
    display.setCursor(x + 1, barY - 8);
    display.print(labels[z]);
  }

  int arrowX = barX + currentZone * segW + segW / 2;
  int arrowY = barY + barH + 2;
  display.drawFastVLine(arrowX, arrowY, 6, SSD1306_WHITE);
  display.drawFastVLine(arrowX - 1, arrowY + 1, 4, SSD1306_WHITE);
  display.drawFastVLine(arrowX + 1, arrowY + 1, 4, SSD1306_WHITE);
  display.drawFastVLine(arrowX - 2, arrowY + 2, 2, SSD1306_WHITE);
  display.drawFastVLine(arrowX + 2, arrowY + 2, 2, SSD1306_WHITE);

  if (baseZone >= 0 && baseZone != currentZone) {
    int bx = barX + baseZone * segW + segW / 2 - 2;
    display.setCursor(bx, arrowY);
    display.print("B");
  }

  display.setCursor(0, 38);
  display.setTextSize(1);
  display.print("Zone: ");
  display.println(getZoneName(currentZone));

  if (baseZone >= 0) {
    display.setCursor(0, 50);
    if (currentZone > baseZone) {
      display.print("^ Improved from ");
      display.println(getZoneName(baseZone));
    } else if (currentZone < baseZone) {
      display.print("v Worse than ");
      display.println(getZoneName(baseZone));
    } else {
      display.print("= Same as baseline");
    }
  } else {
    display.setCursor(0, 50);
    display.print("Baseline zone set!");
  }

  display.display();
}

void waitForFSRTap(const char* prompt = "Squeeze to continue", int settleMs = 600) {
  unsigned long drainStart = millis();
  while (analogRead(FSR_PIN) > FSR_TAP_THRESHOLD) {
    if (millis() - drainStart > FSR_DRAIN_TIMEOUT) break;
    delay(20);
  }
  delay(settleMs);

  int dotCount = 0;
  bool pressed = false;

  while (true) {
    char dots[5] = "    ";
    for (int d = 0; d < (dotCount % 4); d++) dots[d] = '.';

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
    display.drawRect(52, 38, 24, 16, SSD1306_WHITE);
    display.drawRect(52, 28, 6, 12, SSD1306_WHITE);
    display.drawRect(60, 24, 6, 16, SSD1306_WHITE);
    display.drawRect(68, 24, 6, 16, SSD1306_WHITE);
    display.drawRect(76, 28, 6, 12, SSD1306_WHITE);
    display.drawRect(44, 40, 10, 8, SSD1306_WHITE);
    display.setCursor(8, 8);
    display.println(prompt);
    display.setCursor(40, 50);
    display.println(dots);
    display.display();

    dotCount++;
    int val = analogRead(FSR_PIN);
    if (!pressed && val > FSR_TAP_THRESHOLD) pressed = true;
    if (pressed && val <= FSR_TAP_THRESHOLD) {
      display.clearDisplay();
      display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(40, 28);
      display.println("Got it!");
      display.display();
      delay(400);
      return;
    }
    delay(120);
  }
}

void oledText(const char* l1, const char* l2 = "", const char* l3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  if (strlen(l1) > 0) { display.setCursor(4, 8);  display.println(l1); }
  if (strlen(l2) > 0) { display.setCursor(4, 28); display.println(l2); }
  if (strlen(l3) > 0) { display.setCursor(4, 48); display.println(l3); }
  display.display();
}

void oledProgressBar(const char* label, float fraction, bool paused) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 6);
  display.println(label);
  display.drawRect(4, 24, 120, 14, SSD1306_WHITE);
  int fillW = (int)(116.0 * fraction);
  if (fillW > 0) display.fillRect(6, 26, fillW, 10, SSD1306_WHITE);

  char pct[16];
  if (paused) {
    sprintf(pct, "PAUSED");
    display.setCursor(44, 44);
  } else {
    sprintf(pct, "%d%%", (int)(fraction * 100));
    display.setCursor(56, 44);
  }
  display.println(pct);

  if (!paused && fraction > 0.85) {
    display.setCursor(100, 44);
    display.println("<3");
  }
  display.display();
}

void oledCountdown(int seconds, const char* label) {
  for (int i = seconds; i >= 1; i--) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(3);
    char num[4];
    sprintf(num, "%d", i);
    int numX = (i >= 10) ? 40 : 52;
    display.setCursor(numX, 12);
    display.println(num);
    display.setTextSize(1);
    display.setCursor(20, 50);
    display.println(label);
    display.display();
    delay(1000);
  }
}

#define GRAPH_X1 20
#define GRAPH_Y1 8
#define GRAPH_X2 124
#define GRAPH_Y2 54
#define GRAPH_W (GRAPH_X2 - GRAPH_X1)
#define GRAPH_H (GRAPH_Y2 - GRAPH_Y1)

float arrayMin(float* arr, int n) {
  float mn = arr[0];
  for (int i = 1; i < n; i++) if (arr[i] < mn) mn = arr[i];
  return mn;
}

float arrayMax(float* arr, int n) {
  float mx = arr[0];
  for (int i = 1; i < n; i++) if (arr[i] > mx) mx = arr[i];
  return mx;
}

int valueToY(float val, float minVal, float maxVal) {
  if (maxVal == minVal) return (GRAPH_Y1 + GRAPH_Y2) / 2;
  float norm = (val - minVal) / (maxVal - minVal);
  return GRAPH_Y2 - (int)(norm * GRAPH_H);
}

int sessionToX(int idx, int total) {
  if (total <= 1) return (GRAPH_X1 + GRAPH_X2) / 2;
  return GRAPH_X1 + (idx * GRAPH_W) / (total - 1);
}

void drawForceGraph() {
  if (sessionCount < 1) return;

  float allVals[MAX_SESSIONS + 1];
  allVals[0] = baselineMeanForce;
  for (int i = 0; i < sessionCount; i++) allVals[i + 1] = sessionForces[i];
  int totalVals = sessionCount + 1;

  float mn = arrayMin(allVals, totalVals);
  float mx = arrayMax(allVals, totalVals);
  float pad = (mx - mn) * 0.1; if (pad < 1) pad = 5;
  mn -= pad; mx += pad;

  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("Force (N)");
  display.drawLine(GRAPH_X1, GRAPH_Y1, GRAPH_X1, GRAPH_Y2, SSD1306_WHITE);
  display.drawLine(GRAPH_X1, GRAPH_Y2, GRAPH_X2, GRAPH_Y2, SSD1306_WHITE);

  char yLo[8], yHi[8];
  sprintf(yLo, "%.0f", mn); sprintf(yHi, "%.0f", mx);
  display.setCursor(0, GRAPH_Y2 - 6); display.println(yLo);
  display.setCursor(0, GRAPH_Y1);     display.println(yHi);

  int baseY = valueToY(baselineMeanForce, mn, mx);
  for (int x = GRAPH_X1; x <= GRAPH_X2; x += 4) {
    display.drawPixel(x, baseY, SSD1306_WHITE);
    display.drawPixel(x + 1, baseY, SSD1306_WHITE);
  }
  display.setCursor(GRAPH_X2 - 10, baseY - 8); display.println("BL");

  for (int i = 0; i < sessionCount; i++) {
    int x = sessionToX(i, sessionCount);
    int y = valueToY(sessionForces[i], mn, mx);
    display.fillCircle(x, y, 2, SSD1306_WHITE);
    if (i < sessionCount - 1) {
      int nx = sessionToX(i + 1, sessionCount);
      int ny = valueToY(sessionForces[i + 1], mn, mx);
      display.drawLine(x, y, nx, ny, SSD1306_WHITE);
    }
    if (sessionCount <= 10) {
      char sn[4]; sprintf(sn, "%d", i + 1);
      display.setCursor(x - 2, GRAPH_Y2 + 2); display.println(sn);
    }
  }

  int lx = sessionToX(sessionCount - 1, sessionCount);
  int ly = valueToY(sessionForces[sessionCount - 1], mn, mx);
  display.drawCircle(lx, ly, 4, SSD1306_WHITE);

  display.setCursor(0, 56); display.println("Squeeze = next");
  display.display();
}

void drawInitGraph() {
  if (sessionCount < 1) return;

  float allVals[MAX_SESSIONS + 1];
  allVals[0] = baselineMeanInit;
  for (int i = 0; i < sessionCount; i++) allVals[i + 1] = sessionInits[i];
  int totalVals = sessionCount + 1;

  float mn = arrayMin(allVals, totalVals);
  float mx = arrayMax(allVals, totalVals);
  float pad = (mx - mn) * 0.1; if (pad < 1) pad = 10;
  mn -= pad; mx += pad;

  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("Speed (ms)");
  display.drawLine(GRAPH_X1, GRAPH_Y1, GRAPH_X1, GRAPH_Y2, SSD1306_WHITE);
  display.drawLine(GRAPH_X1, GRAPH_Y2, GRAPH_X2, GRAPH_Y2, SSD1306_WHITE);

  char yLo[8], yHi[8];
  sprintf(yLo, "%.0f", mn); sprintf(yHi, "%.0f", mx);
  display.setCursor(0, GRAPH_Y2 - 6); display.println(yLo);
  display.setCursor(0, GRAPH_Y1);     display.println(yHi);

  int baseY = valueToY(baselineMeanInit, mn, mx);
  for (int x = GRAPH_X1; x <= GRAPH_X2; x += 4) {
    display.drawPixel(x, baseY, SSD1306_WHITE);
    display.drawPixel(x + 1, baseY, SSD1306_WHITE);
  }
  display.setCursor(GRAPH_X2 - 10, baseY - 8); display.println("BL");

  for (int i = 0; i < sessionCount; i++) {
    int x = sessionToX(i, sessionCount);
    int y = valueToY(sessionInits[i], mn, mx);
    display.fillCircle(x, y, 2, SSD1306_WHITE);
    if (i < sessionCount - 1) {
      int nx = sessionToX(i + 1, sessionCount);
      int ny = valueToY(sessionInits[i + 1], mn, mx);
      display.drawLine(x, y, nx, ny, SSD1306_WHITE);
    }
    if (sessionCount <= 10) {
      char sn[4]; sprintf(sn, "%d", i + 1);
      display.setCursor(x - 2, GRAPH_Y2 + 2); display.println(sn);
    }
  }

  int lx = sessionToX(sessionCount - 1, sessionCount);
  int ly = valueToY(sessionInits[sessionCount - 1], mn, mx);
  display.drawCircle(lx, ly, 4, SSD1306_WHITE);

  display.setCursor(0, 56); display.println("Squeeze = done");
  display.display();
}

void beep(int freq, int duration) { CircuitPlayground.playTone(freq, duration); }

void startBeep() {
  beep(600, 150); delay(80); beep(800, 150); delay(80); beep(1000, 250);
}

void doneBeep() {
  beep(523, 100); delay(30); beep(523, 100); delay(30);
  beep(523, 100); delay(30); beep(523, 300); delay(50);
  beep(415, 200); delay(30); beep(466, 200); delay(30);
  beep(523, 200); delay(30); beep(466, 100); delay(20);
  beep(523, 600);
}

void progressBeep() {
  beep(392, 100); delay(20); beep(523, 100); delay(20);
  beep(659, 100); delay(20); beep(784, 400); delay(30);
  beep(784, 100); delay(20); beep(784, 400);
}

void regressBeep() {
  beep(523, 200); delay(30); beep(466, 200); delay(30);
  beep(415, 200); delay(30); beep(349, 500);
}

float toNewtons(float R) {
  if (R <= R0) return 0.0;
  return F1_cal * (R - R0) / (R1_cal - R0);
}

float measureR0() {
  oledText("Lift hand", "off sensor", "fully.");
  Serial.println("\nLift hand off sensor. Measuring in 3s...");
  oledCountdown(3, "measuring...");

  long total = 0; int count = 0;
  unsigned long start = millis();
  while (millis() - start < 500) {
    total += analogRead(FSR_PIN); count++; delay(20);
  }
  float r0 = (count > 0) ? (float)total / count : 0.0;
  Serial.print("R0 = "); Serial.println(r0);
  return r0;
}

String readSerialLine() {
  String input = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (input.length() > 0) return input;
      } else {
        input += c; Serial.print(c);
      }
    }
  }
}

void firstTimeSetup() {
  oledText("Welcome to", "Gripple!", "");
  Serial.println("Welcome to Gripple!");
  delay(2500);

  oledText("First-time", "setup!", "");
  delay(2000);

  oledText("Enter your", "name in", "Serial below:");
  delay(1000);
  Serial.println("Enter your name:");
  String name = readSerialLine();
  name.toCharArray(userName, 30);

  char shortName[9];
  strncpy(shortName, userName, 8);
  shortName[8] = '\0';
  char welcome[20];
  sprintf(welcome, "Hi %s!", shortName);
  oledText(welcome, "Nice to", "meet you!");
  delay(2000);

  oledText("Biological", "sex for grip", "M or F?");
  Serial.println("\nBiological sex:");
  Serial.println("  M = Male   F = Female");

  while (true) {
    String sex = readSerialLine();
    sex.toUpperCase();
    if (sex == "M") {
      userSex = 'M'; F1_cal = MALE_F1; R1_cal = MALE_R1;
      oledText("Male set!", "Good to go.", "");
      Serial.println("Male calibration set.");
      delay(1500); break;
    } else if (sex == "F") {
      userSex = 'F'; F1_cal = FEMALE_F1; R1_cal = FEMALE_R1;
      oledText("Female set!", "Good to go.", "");
      Serial.println("Female calibration set.");
      delay(1500); break;
    } else {
      Serial.println("Please type M or F.");
    }
  }

  oledText("Baseline", "test next!", "Any key...");
  Serial.println("\nBaseline test coming up. Press any key...");
  readSerialLine();

  isFirstTime = true;
}

void returningUserGreeting() {
  char shortName[9];
  strncpy(shortName, userName, 8);
  shortName[8] = '\0';
  char line1[20];
  sprintf(line1, "Hey %s!", shortName);
  oledText(line1, "Welcome", "back!");
  Serial.print("Welcome back, "); Serial.println(userName);
  delay(3000);

  char f[16], t[16];
  sprintf(f, "%.1f N", baselineMeanForce);
  sprintf(t, "%.0f ms", baselineMeanInit);
  oledText("Your baseline:", f, t);
  delay(2500);

  waitForFSRTap("Squeeze to start", 500);
  isFirstTime = false;
}

void morningGreeting() {
  char shortName[9];
  strncpy(shortName, userName, 8);
  shortName[8] = '\0';
  char line2[20];
  sprintf(line2, "%s!", shortName);
  oledText("Good morning,", line2, "");
  Serial.println("\n");
  Serial.print("Good morning, "); Serial.println(userName);
  delay(2500);

  oledText("Time for your", "daily test!", "");
  Serial.println("Time for your daily test!");
  delay(2000);

  waitForFSRTap("Squeeze to begin", 500);
}

float getBaselineZ(int durationMs) {
  float total = 0; int count = 0;
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    total += CircuitPlayground.motionZ(); count++; delay(20);
  }
  return (count > 0) ? total / count : 0.0;
}

unsigned long waitForMovement(float baselineZ) {
  while (true) {
    if (abs(CircuitPlayground.motionZ() - baselineZ) >= ACCEL_THRESHOLD)
      return millis();
    delay(10);
  }
}

unsigned long waitForPress() {
  while (analogRead(FSR_PIN) < FSR_THRESHOLD) delay(10);
  return millis();
}

float collectHold() {
  long total = 0;
  int samples = 0;
  long activeMs = 0;
  unsigned long lastTick = millis();

  while (activeMs < HOLD_DURATION) {
    unsigned long now = millis();
    unsigned long dt = now - lastTick;
    lastTick = now;

    int val = analogRead(FSR_PIN);
    bool pressing = (val >= FSR_THRESHOLD);

    if (pressing) {
      activeMs += dt;
      total += val;
      samples++;
      float frac = (float)activeMs / HOLD_DURATION;
      oledProgressBar("Hold squeeze! <3", frac, false);
    } else {
      float frac = (float)activeMs / HOLD_DURATION;
      oledProgressBar("Hold squeeze! <3", frac, true);
    }
    delay(SAMPLE_INTERVAL);
  }
  return (samples > 0) ? (float)total / samples : 0.0;
}

void runSession() {
  Serial.println("\n--- Starting Session ---");

  if (isFirstTime) {
    oledText("Baseline", "session!", "Here we go~");
    Serial.println("*** BASELINE SESSION ***");
    delay(2000);
  }

  R0 = measureR0();
  delay(500);

  oledText("Get ready!", "Starting...", "");
  startBeep();
  delay(800);

  for (int repCount = 0; repCount < TOTAL_REPS; repCount++) {
    char repLine[16];
    sprintf(repLine, "Rep %d of %d", repCount + 1, TOTAL_REPS);
    oledText(repLine, "Open hand!", "");
    Serial.print("\nRep "); Serial.print(repCount + 1);
    Serial.println(" - open hand, then squeeze");
    delay(1000);

    float baselineZ = getBaselineZ(ACCEL_BASELINE_MS);

    oledText(repLine, "Now move!", "");
    Serial.println("  Waiting for movement...");
    unsigned long moveTime = waitForMovement(baselineZ);
    Serial.println("  Movement detected!");

    unsigned long pressTime = waitForPress();
    float initiationMs = (float)(pressTime - moveTime);
    repInitTimes[repCount] = initiationMs;
    Serial.print("  Init: "); Serial.print(initiationMs); Serial.println(" ms");

    float meanRaw = collectHold();
    repMeans[repCount] = meanRaw;
    float forceN = toNewtons(meanRaw);
    repForces[repCount] = forceN;

    char fStr[16];
    sprintf(fStr, "%.1f N", forceN);
    oledText(repLine, "Done!", fStr);
    Serial.print("  Force: "); Serial.print(forceN, 1); Serial.println(" N");
    delay(1200);

    if (repCount < TOTAL_REPS - 1) {
      Serial.println("  Resting...");
      oledCountdown(REST_DURATION / 1000, "next rep soon");
    }
  }

  oledText("Calculating", "results...", "");
  doneBeep();
  delay(1500);
}

void printResults() {
  float totalForce = 0, totalInit = 0;
  for (int i = 0; i < TOTAL_REPS; i++) {
    totalForce += repForces[i];
    totalInit += repInitTimes[i];
  }
  float meanForce = totalForce / TOTAL_REPS;
  float meanInit = totalInit / TOTAL_REPS;

  Serial.println("\n--- Results ---");
  Serial.println("Rep | Force(N) | Init(ms)");
  Serial.println("----|----------|--------");
  for (int i = 0; i < TOTAL_REPS; i++) {
    Serial.print("  "); Serial.print(i + 1);
    Serial.print("   |   "); Serial.print(repForces[i], 1);
    Serial.print("    |  "); Serial.println(repInitTimes[i], 0);
  }
  Serial.println("----|----------|--------");
  Serial.print("Mean|   "); Serial.print(meanForce, 1);
  Serial.print("    |  "); Serial.println(meanInit, 0);

  // Baseline session
  if (isFirstTime) {
    baselineMeanForce = meanForce;
    baselineMeanInit = meanInit;
    baselineZone = getZone(meanForce);

    Serial.println("\nBaseline recorded!");
    Serial.print("Baseline grip: "); Serial.print(baselineMeanForce, 1); Serial.println(" N");
    Serial.print("Baseline init: "); Serial.print(baselineMeanInit, 0); Serial.println(" ms");
    Serial.print("Baseline zone: "); Serial.println(getZoneName(baselineZone));

    char f[16], t[16];
    sprintf(f, "Grip: %.1fN", meanForce);
    sprintf(t, "Spd: %.0fms", meanInit);
    oledText("Baseline saved!", f, t);
    delay(3000);

    drawZoneBar(baselineZone, -1);
    delay(4000);
    waitForFSRTap("Squeeze to continue", 1000);
    return;
  }

  if (sessionCount < MAX_SESSIONS) {
    sessionForces[sessionCount] = meanForce;
    sessionInits[sessionCount] = meanInit;
    sessionCount++;
  }

  int currentZone = getZone(meanForce);
  float forceDiff = meanForce - baselineMeanForce;
  float forcePct = (baselineMeanForce > 0) ? (forceDiff / baselineMeanForce) * 100.0 : 0;
  float initDiff = meanInit - baselineMeanInit;
  float initPct = (baselineMeanInit > 0) ? (initDiff / baselineMeanInit) * 100.0 : 0;

  bool forceImproved = forceDiff >= 0;
  bool speedImproved = initDiff <= 0;

  Serial.println("\n--- Progress vs Baseline ---");
  Serial.print("Grip:  "); Serial.print(forcePct >= 0 ? "+" : ""); Serial.print(forcePct, 1); Serial.println("%");
  Serial.print("Speed: "); Serial.print(abs(initPct), 1);
  Serial.println(speedImproved ? "% faster" : "% slower");
  Serial.print("Zone:  "); Serial.print(getZoneName(currentZone));
  Serial.print(" (baseline: "); Serial.print(getZoneName(baselineZone)); Serial.println(")");

  if (forceImproved && speedImproved) {
    progressBeep();
    oledText("Both improved!", "Great session.", "");
    Serial.println("Both grip and speed improved!");
  } else if (forceImproved && !speedImproved) {
    progressBeep();
    oledText("Good grip!", "Speed was low.", "Keep it up!");
    Serial.println("Good grip. Speed lower.");
  } else if (!forceImproved && speedImproved) {
    regressBeep();
    oledText("Fast today!", "Grip dipped.", "Note stiffness");
    Serial.println("Good speed. Grip lower - note stiffness.");
  } else {
    regressBeep();
    oledText("Below baseline.", "Note pain.", "Tell your Dr.");
    Serial.println("Both below baseline. Tell your rheumatologist.");
  }

  waitForFSRTap("Squeeze to continue", 3500);
  drawZoneBar(currentZone, baselineZone);
  waitForFSRTap("Squeeze to continue", 4000);

  char f[20], fb[20];
  sprintf(f, "Grip:  %.1fN", meanForce);
  sprintf(fb, "Base:  %.1fN", baselineMeanForce);
  oledText(f, fb, forcePct >= 0 ? "^ better!" : "v lower");
  waitForFSRTap("Squeeze to continue", 3500);

  char t[20], tb[20];
  sprintf(t, "Spd: %.0fms", meanInit);
  sprintf(tb, "Base:%.0fms", baselineMeanInit);
  oledText(t, tb, initPct <= 0 ? "^ faster!" : "v slower");
  waitForFSRTap("Squeeze to continue", 3500);

  if (userSex == 'M') {
    oledText("Norm (Male):", "475-570 N", "");
    Serial.println("Reference (Male): 475-570 N");
  } else {
    oledText("Norm (Fem):", "280-380 N", "");
    Serial.println("Reference (Female): 280-380 N");
  }
  waitForFSRTap("Squeeze to continue", 3000);

  waitForFSRTap("Squeeze for graphs", 500);
  drawForceGraph();
  waitForFSRTap("Squeeze for speed", 15000);
  drawInitGraph();
  waitForFSRTap("Squeeze to finish", 15000);

  oledText("Bye bye!", "See you", "tomorrow!");
  Serial.println("\nBye bye! See you tomorrow!");
  delay(5000);
}

void setup() {
  CircuitPlayground.begin();
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) {
      CircuitPlayground.setPixelColor(0, 255, 0, 0);
      delay(300);
      CircuitPlayground.setPixelColor(0, 0, 0, 0);
      delay(300);
    }
  }

  oledText("Gripple", "Loading...", "");
  delay(500);

  if (strlen(userName) == 0) {
    while (!Serial) { delay(100); }
    firstTimeSetup();
  } else {
    returningUserGreeting();
  }

  runSession();
  printResults();
  isFirstTime = false;
}

void loop() {
  morningGreeting();
  runSession();
  printResults();
}
