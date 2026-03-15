#include <Wire.h>
#include <VL53L1X.h>

#define I2C_SDA       5
#define I2C_SCL       6
#define MUX_RST_PIN   4
#define PEDAL_PIN     12
#define MUX_ADDR      0x70

VL53L1X sensors[6];
float latestDistances[6] = {999.0, 999.0, 999.0, 999.0, 999.0, 999.0};

// --- ตัวแปร Logic หลัก ---
float currentMinDist = 999.0;
float prevMinDist = 999.0;
float velocity = 0.0;
float highestPointInSet = 0.0;

bool isPlayerPresent = false;
bool isBypassed = false;
bool isTriggered = false;
bool isLiftingStarted = false;

// --- Timers ---
unsigned long lastReadTime = 0;
unsigned long presenceTimer = 0;
unsigned long t1Timer = 0;
unsigned long t3Timer = 0;
unsigned long bypassTimer = 0;
unsigned long triggerTimer = 0;

int noiseRejectCount = 0;

// --- ตัวแปรปุ่มเหยียบ (แก้ใหม่) ---
unsigned long pedalPressTime = 0;
bool lastPedalReading = LOW;
bool pedalStableState = LOW;
unsigned long pedalDebounceTime = 0;
bool pedalHeld = false;
bool wakeHandled = false;

void tcaSelect(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(MUX_ADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

void resetMultiplexer() {
  digitalWrite(MUX_RST_PIN, LOW); delay(10);
  digitalWrite(MUX_RST_PIN, HIGH); delay(10);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  pinMode(MUX_RST_PIN, OUTPUT);
  pinMode(PEDAL_PIN, INPUT);  // ปุ่ม 3 ขา ไม่ใช้ PULLUP
  resetMultiplexer();

  for (int i = 0; i < 6; i++) {
    tcaSelect(i);
    sensors[i].setTimeout(500);
    if (sensors[i].init()) {
      sensors[i].setDistanceMode(VL53L1X::Short);
      sensors[i].startContinuous(33);
    }
  }
  lastReadTime = millis();
}

void loop() {
  handleFootPedal();

  if (isTriggered && (millis() - triggerTimer >= 60000)) {
    resetSystem();
    Serial.println(F("[SYSTEM] AUTO WAKE UP."));
  }

  if (isBypassed && (millis() - bypassTimer >= 60000)) {
    isBypassed = false;
    Serial.println(F("[STATE] Bypass Ended."));
  }

  readAndFilterSensors();
  checkPresence();

  if (isPlayerPresent && !isBypassed && !isTriggered) {
    evaluateSafetyThresholds();
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 100) {
    printDebugInfo();
    lastPrint = millis();
  }
}

void readAndFilterSensors() {
  bool anyNewData = false;
  for (int i = 0; i < 6; i++) {
    tcaSelect(i);
    if (sensors[i].dataReady()) {
      sensors[i].read(false);
      float d = sensors[i].ranging_data.range_mm / 10.0;
      latestDistances[i] = (d > 1.0 && d < 130.0) ? d : 999.0;
      anyNewData = true;
    }
  }
  if (!anyNewData) return;

  float rawMin = 999.0;
  for (int i = 0; i < 6; i++) {
    if (latestDistances[i] < rawMin) rawMin = latestDistances[i];
  }
  if (rawMin == 999.0) return;

  unsigned long now = millis();
  float dt = (now - lastReadTime) / 1000.0;

  if (dt >= 0.05) {
    float rawVel = 0.0;

    if (prevMinDist != 999.0) {
      rawVel = ((rawMin - prevMinDist) / 100.0) / dt;

      if (abs(rawMin - prevMinDist) > 40.0 || abs(rawVel) > 4.0) {
        if (++noiseRejectCount > 3) {
          currentMinDist = rawMin;
          velocity = rawVel;
          prevMinDist = rawMin;
          noiseRejectCount = 0;
        }
        lastReadTime = now;
        return;
      }

      velocity = (velocity * 0.4) + (rawVel * 0.6);
    } else {
      velocity = 0.0;
    }

    currentMinDist = rawMin;
    prevMinDist = rawMin;
    noiseRejectCount = 0;
    lastReadTime = now;
  }
}

void checkPresence() {
  if (currentMinDist > 60.0) {
    if (presenceTimer == 0) presenceTimer = millis();
    if (millis() - presenceTimer >= 5000) {
      isPlayerPresent = false;
      isLiftingStarted = false;
      highestPointInSet = 0;
    }
  } else {
    isPlayerPresent = true;
    presenceTimer = 0;

    if (currentMinDist > highestPointInSet) highestPointInSet = currentMinDist;

    if (!isLiftingStarted && (highestPointInSet - currentMinDist > 10.0)) {
      isLiftingStarted = true;
      Serial.println(F("[SYSTEM] LIFTING DETECTED!"));
    }
  }
}

void evaluateSafetyThresholds() {
  if (velocity <= -0.80) { triggerSystem("T2: RAPID DROP"); return; }

  if (currentMinDist >= 4.5 && currentMinDist <= 13.0) {
    if (t3Timer == 0) t3Timer = millis();
    if (millis() - t3Timer >= 4000) { triggerSystem("T3: UNCONSCIOUS"); return; }
  } else t3Timer = 0;

  if (isLiftingStarted) {
    if (abs(velocity) <= 0.15) {
      if (t1Timer == 0) t1Timer = millis();
      unsigned long stuckTime = millis() - t1Timer;
      if (currentMinDist >= 4.0 && currentMinDist <= 17.0 && stuckTime >= 1500) triggerSystem("T1: STICKING (DANGER)");
      else if (currentMinDist > 17.0 && stuckTime >= 2000) triggerSystem("T1: STICKING (NORMAL)");
    } else t1Timer = 0;
  }
}

// ==========================================
// ฟังก์ชันจัดการปุ่มเหยียบ (แก้ใหม่ — ปุ่ม 3 ขา)
// ==========================================
void handleFootPedal() {
  unsigned long now = millis();
  bool reading = digitalRead(PEDAL_PIN);

  // Debounce
  if (reading != lastPedalReading) {
    pedalDebounceTime = now;
  }
  if (now - pedalDebounceTime > 100) {
    pedalStableState = reading;
  }
  lastPedalReading = reading;

  bool pressed = (pedalStableState == HIGH);  // ปุ่ม 3 ขา HIGH = กด

  if (pressed) {
    if (!pedalHeld) {
      pedalPressTime = now;
      pedalHeld = true;
      wakeHandled = false;
    }
  } else {
    // ปล่อยปุ่ม → เช็คว่ากดนานพอไหม
    if (pedalHeld && !wakeHandled) {
      unsigned long pressDuration = now - pedalPressTime;

      if (pressDuration >= 2000) {
        // กดค้าง 2 วิ → reset
        resetSystem();
        wakeHandled = true;
        Serial.println(F("[SYSTEM] MANUAL RESET."));
      } else if (pressDuration > 50) {
        // กดสั้น → bypass
        isBypassed = true;
        bypassTimer = now;
        isTriggered = false;
        wakeHandled = true;
        Serial.println(F("[SYSTEM] BYPASSED."));
      }
    }
    pedalHeld = false;
  }
}

void resetSystem() {
  isTriggered = false; isBypassed = false; isLiftingStarted = false;
  highestPointInSet = 0; t1Timer = 0; t3Timer = 0;
}

void triggerSystem(const char* r) {
  if (isTriggered) return;
  isTriggered = true; triggerTimer = millis();
  Serial.println(F("\n========================================="));
  Serial.print(F("!!! TRIGGER: ")); Serial.println(r);
  Serial.println(F("=========================================\n"));
}

void printDebugInfo() {
  Serial.print(F("Distance: "));
  if (currentMinDist == 999.0) Serial.print(F("---  "));
  else Serial.print(currentMinDist, 1);

  Serial.print(F(" cm | Velocity: ")); Serial.print(velocity, 2);

  Serial.print(F(" m/s | PeakHeight: ")); Serial.print(highestPointInSet, 1);

  Serial.print(F(" cm | State: ["));
  if (isTriggered) Serial.print(F("TRIGGERED"));
  else if (isBypassed) Serial.print(F("BYPASSED"));
  else if (!isPlayerPresent) Serial.print(F("NO_PLAYER"));
  else if (!isLiftingStarted) Serial.print(F("STANDBY"));
  else Serial.print(F("ACTIVE"));
  Serial.println(F("]"));
}