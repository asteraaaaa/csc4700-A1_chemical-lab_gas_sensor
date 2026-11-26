/****************************************************
 * CSC4700 IoT Hazard Monitoring Prototype (FINAL)
 * Hardware: ESP32, MQ135 (AO), Piezo Buzzer
 * IoT Platform: Blynk IoT
 *
 * PHYSICAL (REAL): MQ135 baseline algorithm
 *  - Warm-up 2 min -> Baseline 1 min -> Monitor every 5s
 *  - Alarm if MQ > baseline*1.30 for 3 consecutive samples
 *
 * VIRTUAL (SIMULATED): CO, NH3, CH4, H2S, NO2
 *  - Switch ON starts scenario (WARNING stage)
 *  - WARNING -> ALARM only when thresholds crossed
 *  - Switch OFF acknowledges/resets to NORMAL
 *  - During scenario, gases ACCUMULATE (no baseline reset each second)
 *  - When no scenario, gases show safe baseline + small noise (looks alive)
 *
 * BLYNK PIN MAP (matches your datastream table):
 *  V0  Air Quality (MQ135 raw/index)
 *  V1  CO ppm
 *  V2  NH3 ppm
 *  V3  CH4 ppm
 *  V4  H2S ppm
 *  V5  NO2 ppm
 *  V6  Scenario (String)
 *  V7  Alarm Status (String): NORMAL / WARNING / ALARM
 *  V10 Combustion Event (Switch 0/1)
 *  V11 Gas Line Leak (Switch 0/1)
 *  V12 Chemical Spill (Switch 0/1)
 *  V13 Toxic Gas Release (Switch 0/1)
 *  V20 Auto Demo (Switch 0/1)  [optional]
 *  V21 Mute (Switch 0/1)       [optional]
 ****************************************************/

/************ BLYNK DEFINES ************/
#define BLYNK_TEMPLATE_ID   ""
#define BLYNK_TEMPLATE_NAME "Chemical Warehouse Air Monitor"
#define BLYNK_AUTH_TOKEN    ""
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

/************ WIFI ************/
char ssid[] = "";
char pass[] = "";

/************ TIMER ************/
BlynkTimer timer;

/************ PINS ************/
const int MQ135_PIN  = 34;
const int BUZZER_PIN = 27;

/************ BUZZER PWM (ESP32 core v3.3.x) ************/
const uint32_t BUZZER_FREQ = 3000;
const uint8_t  BUZZER_RES  = 8;

/************ TIMING ************/
const unsigned long SAMPLE_INTERVAL = 5000;     // MQ sample every 5s
const unsigned long WARMUP_TIME     = 120000;   // 2 min
const unsigned long BASELINE_TIME   = 60000;    // 1 min

/************ MQ135 DETECTION ************/
const float DRIFT_FACTOR    = 1.30;
const int   REQUIRED_SPIKES = 3;

/************ MQ135 STATE ************/
unsigned long startTime      = 0;
unsigned long lastSampleTime = 0;

float baselineSum   = 0.0f;
int   baselineCount = 0;
float baseline      = 0.0f;
bool  baselineReady = false;

int   spikeCount   = 0;
bool  mqAlarm      = false;

int latestMQ = 0;

/************ VIRTUAL GAS VALUES ************/
float CO_ppm  = 0;
float NH3_ppm = 0;
float CH4_ppm = 0;
float H2S_ppm = 0;
float NO2_ppm = 0;

/************ VIRTUAL GAS THRESHOLDS ************/
const float TH_CO  = 50;
const float TH_NH3 = 25;
const float TH_CH4 = 5000;
const float TH_H2S = 10;
const float TH_NO2 = 3;

/************ SCENARIOS ************/
enum ScenarioType {
  SC_NONE,
  SC_COMBUSTION,
  SC_GAS_LEAK,
  SC_CHEM_SPILL,
  SC_TOXIC_RELEASE
};

ScenarioType currentScenario = SC_NONE;
bool scenarioActive = false;
unsigned long scenarioStartMs = 0;
String scenarioName = "NONE";
String scenarioMsg  = "";

/************ WARNING/ALARM STATE ************/
bool warningActive = false;
bool alarmActive   = false;
String alarmStatus = "NORMAL";

// Separate flags so WARNING and ALARM both generate events
bool warningEventSent = false;
bool alarmEventSent   = false;

/************ OPTIONAL CONTROLS ************/
bool muteActive    = false;  // V21
bool autoDemo      = false;  // V20
unsigned long demoLastSwitchMs = 0;
int demoStage = 0;

/************ DETERMINISTIC NOISE ************/
uint32_t prngState = 123456789;
float noise(float amp) {
  prngState = 1664525UL * prngState + 1013904223UL;
  float r = (prngState & 0xFFFF) / 65535.0f; // 0..1
  return (r * 2.0f - 1.0f) * amp;            // -amp..+amp
}

/************ BUZZER HELPERS ************/
void buzzerOn()  { if (!muteActive) ledcWrite(BUZZER_PIN, 128); }
void buzzerOff() { ledcWrite(BUZZER_PIN, 0); }

/************ SAFE BASELINE (when NO scenario) ************/
void idleBaselineVirtualGases() {
  CO_ppm  = 2.0  + noise(0.5);
  CH4_ppm = 2.0  + noise(1.0);
  H2S_ppm = 0.1  + noise(0.05);
  NO2_ppm = 0.05 + noise(0.03);

  float aqNorm = latestMQ / 4095.0f;
  NH3_ppm = 1.0 + aqNorm * 3.0 + noise(0.5);
  if (NH3_ppm < 0) NH3_ppm = 0;
}

/************ RESET VIRTUAL GAS VALUES ************/
void resetVirtualGasesToSafe() {
  idleBaselineVirtualGases();
}

/************ START/STOP SCENARIO ************/
void startScenario(ScenarioType sc, const String& name, const String& msg) {
  // Stop any previous scenario
  scenarioActive = false;
  currentScenario = SC_NONE;

  // Set new scenario
  currentScenario = sc;
  scenarioActive = (sc != SC_NONE);
  scenarioStartMs = millis();
  scenarioName = name;
  scenarioMsg  = msg;

  // WARNING stage begins immediately
  warningActive = scenarioActive;
  alarmActive   = false;
  alarmStatus   = warningActive ? "WARNING" : "NORMAL";

  // Reset event flags so WARNING and ALARM can both fire
  warningEventSent = false;
  alarmEventSent   = false;

  // For scenarios, start from safe baseline then accumulate
  resetVirtualGasesToSafe();

  // Send WARNING event once
  if (scenarioActive && !warningEventSent) {

    if (Blynk.connected()) {
      Serial.println("Sending WARNING event to Blynk");
      Blynk.logEvent("gas_alert", "WARNING: " + scenarioMsg);
      warningEventSent = true;
    } else {
      Serial.println("Blynk NOT connected - event skipped");
    }

  }
}

void stopScenarioAcknowledge() {
  scenarioActive = false;
  currentScenario = SC_NONE;
  scenarioName = "NONE";
  scenarioMsg  = "";

  warningActive = false;
  alarmActive   = false;
  alarmStatus   = "NORMAL";

  warningEventSent = false;
  alarmEventSent   = false;

  buzzerOff();
  resetVirtualGasesToSafe();
}

/************ SCENARIO ENGINE (accumulates 1Hz) ************/
void updateScenario1Hz() {
  if (!scenarioActive) {
    // keep dashboard alive in safe state
    idleBaselineVirtualGases();
    return;
  }

  // Accumulate plume (do NOT reset baselines here)
  switch (currentScenario) {
    case SC_COMBUSTION:
      CO_ppm  += 3.0;   // ppm/sec (demo tuning)
      NO2_ppm += 0.3;
      break;

    case SC_GAS_LEAK:
      CH4_ppm += 900.0;
      break;

    case SC_CHEM_SPILL:
      NH3_ppm += 4.0;
      H2S_ppm += 0.2;
      break;

    case SC_TOXIC_RELEASE:
      H2S_ppm += 2.5;
      break;

    default:
      break;
  }

  // Clamp to your dashboard max so gauges don’t blow past range
  if (CO_ppm  < 0) CO_ppm = 0;   if (CO_ppm  > 500)   CO_ppm  = 500;
  if (NH3_ppm < 0) NH3_ppm = 0;  if (NH3_ppm > 200)   NH3_ppm = 200;
  if (CH4_ppm < 0) CH4_ppm = 0;  if (CH4_ppm > 20000) CH4_ppm = 20000;
  if (H2S_ppm < 0) H2S_ppm = 0;  if (H2S_ppm > 100)   H2S_ppm = 100;
  if (NO2_ppm < 0) NO2_ppm = 0;  if (NO2_ppm > 20)    NO2_ppm = 20;
}

/************ MQ135 SAMPLING (every 5s) ************/
void sampleMQ135() {
  unsigned long now = millis();

  latestMQ = analogRead(MQ135_PIN);

  // Warm-up
  if (now - startTime < WARMUP_TIME) {
    mqAlarm = false;
    spikeCount = 0;
    return;
  }

  // Baseline
  if (now - startTime < (WARMUP_TIME + BASELINE_TIME)) {
    baselineSum += latestMQ;
    baselineCount++;
    mqAlarm = false;
    spikeCount = 0;
    return;
  }

  // Finalize baseline once
  if (!baselineReady && baselineCount > 0) {
    baseline = baselineSum / baselineCount;
    baselineReady = true;
    Serial.print("Baseline established: ");
    Serial.println(baseline);
  }

  // Monitoring
  if (baselineReady && latestMQ > baseline * DRIFT_FACTOR) {
    spikeCount++;
    if (spikeCount >= REQUIRED_SPIKES) mqAlarm = true;
  } else {
    spikeCount = 0;
    mqAlarm = false;
  }
}

/************ WARNING -> ALARM ESCALATION ************/
void evaluateAlarm() {
  bool virtualThresholdCrossed =
    (CO_ppm  > TH_CO)  ||
    (NH3_ppm > TH_NH3) ||
    (CH4_ppm > TH_CH4) ||
    (H2S_ppm > TH_H2S) ||
    (NO2_ppm > TH_NO2);

  // WARNING state exists when scenario is active and no ALARM yet
  if (scenarioActive && !alarmActive) {
    warningActive = true;
  }

  // Escalate to ALARM if thresholds crossed
  if ((scenarioActive && virtualThresholdCrossed) || mqAlarm) {
    alarmActive = true;
    warningActive = false;
  }

  // Determine display status + buzzer
  if (alarmActive) {
    alarmStatus = "ALARM";
    buzzerOn();

    if (!alarmEventSent) {

      if (Blynk.connected()) {
        Serial.println("Sending ALARM event to Blynk");
        Blynk.logEvent(
          "gas_alert",
          "ALARM: Hazardous concentration reached (" + scenarioName + ")"
        );
        alarmEventSent = true;
      } else {
        Serial.println("Blynk NOT connected - alarm skipped");
      }

    }


  } else if (warningActive) {
    alarmStatus = "WARNING";
    buzzerOff();

  } else {
    alarmStatus = "NORMAL";
    buzzerOff();
  }

  // If scenario stops (switch OFF), we should be NORMAL
  if (!scenarioActive && !mqAlarm) {
    alarmActive = false;
    warningActive = false;
    alarmStatus = "NORMAL";
    alarmEventSent = false;
    // warningEventSent stays false; next scenario will set it
  }
}

/************ AUTO DEMO (optional) ************/
void runAutoDemo1Hz() {
  if (!autoDemo) return;

  unsigned long now = millis();
  if (now - demoLastSwitchMs < 20UL * 1000UL) return; // switch every 20s
  demoLastSwitchMs = now;

  demoStage = (demoStage + 1) % 4;
  switch (demoStage) {
    case 0: startScenario(SC_COMBUSTION, "COMBUSTION EVENT", "CO & NO2 leak detected (Combustion Event)"); break;
    case 1: startScenario(SC_GAS_LEAK,   "GAS LINE LEAK",    "Methane (CH4) gas line leak detected"); break;
    case 2: startScenario(SC_CHEM_SPILL, "CHEMICAL SPILL",   "Ammonia (NH3) chemical spill detected"); break;
    case 3: startScenario(SC_TOXIC_RELEASE, "TOXIC GAS RELEASE", "Hydrogen Sulfide (H2S) toxic release detected"); break;
  }
}

/************ BLYNK SWITCHES ************/
BLYNK_WRITE(V10) {
  if (param.asInt()) startScenario(SC_COMBUSTION, "COMBUSTION EVENT", "CO & NO2 leak detected (Combustion Event)");
  else stopScenarioAcknowledge();
}

BLYNK_WRITE(V11) {
  if (param.asInt()) startScenario(SC_GAS_LEAK, "GAS LINE LEAK", "Methane (CH4) gas line leak detected");
  else stopScenarioAcknowledge();
}

BLYNK_WRITE(V12) {
  if (param.asInt()) startScenario(SC_CHEM_SPILL, "CHEMICAL SPILL", "Ammonia (NH3) chemical spill detected");
  else stopScenarioAcknowledge();
}

BLYNK_WRITE(V13) {
  if (param.asInt()) startScenario(SC_TOXIC_RELEASE, "TOXIC GAS RELEASE", "Hydrogen Sulfide (H2S) toxic release detected");
  else stopScenarioAcknowledge();
}

BLYNK_WRITE(V20) { autoDemo = (param.asInt() == 1); }
BLYNK_WRITE(V21) { muteActive = (param.asInt() == 1); if (muteActive) buzzerOff(); }

/************ BLYNK PUSH (1Hz) ************/
void sendToBlynk() {
  Blynk.virtualWrite(V0, latestMQ);
  Blynk.virtualWrite(V1, CO_ppm);
  Blynk.virtualWrite(V2, NH3_ppm);
  Blynk.virtualWrite(V3, CH4_ppm);
  Blynk.virtualWrite(V4, H2S_ppm);
  Blynk.virtualWrite(V5, NO2_ppm);
  Blynk.virtualWrite(V6, scenarioName);
  Blynk.virtualWrite(V7, alarmStatus);
}

/************ SETUP ************/
void setup() {
  Serial.begin(115200);

  // ADC config
  analogReadResolution(12);
  analogSetPinAttenuation(MQ135_PIN, ADC_11db);

  // Buzzer PWM attach
  bool ok = ledcAttach(BUZZER_PIN, BUZZER_FREQ, BUZZER_RES);
  if (!ok) Serial.println("ERROR: ledcAttach() failed for buzzer pin");
  buzzerOff();

  // Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Timers
  timer.setInterval(1000L, sendToBlynk);        // real-time dashboard
  timer.setInterval(1000L, updateScenario1Hz);  // simulation engine
  timer.setInterval(1000L, runAutoDemo1Hz);     // optional

  startTime = millis();

  // Initialize safe baselines
  idleBaselineVirtualGases();

  Serial.println("=== Hazard Monitoring Prototype READY ===");
}

/************ LOOP ************/
void loop() {
  Blynk.run();
  timer.run();

  unsigned long now = millis();
  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = now;
    sampleMQ135();
  }

  evaluateAlarm();
}
