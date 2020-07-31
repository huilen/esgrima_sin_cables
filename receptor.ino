#include <RH_ASK.h>
#include <SPI.h>
#include <SoftwareSerial.h>

#define RX_PIN 8
#define TX_PIN 7

#define LED_MASTER_PIN 3
#define LED_SLAVE_PIN 2

#define CAZOLETA_PIN 6
#define WEAPON_PIN 13

#define RF_LATENCY 850
#define LOOP_ITERATION_TIME 0.017

#define DOUBLE_TOLERANCE_INTERVAL 600
#define WAIT_AFTER_TOUCHE 5000
#define PUSH_NEEDED_TIME 200

#define ALLEZ_SIGNAL 3

#define MT 0 // Master Touche
#define ST 1 // Slave Touche
#define MC 2 // Master Cazoleta
#define SC 3 // Slave Cazoleta

RH_ASK rf_driver;

SoftwareSerial hc05(RX_PIN, TX_PIN);

int WAIT_INTERVAL = DOUBLE_TOLERANCE_INTERVAL + RF_LATENCY;
int PUSH_NEEDED_ITERATIONS = PUSH_NEEDED_TIME * LOOP_ITERATION_TIME;

int MATRIX[16][5] = {
  // MT ST MC SC RESULT
  {0, 0, 0, 0, -1},
  {0, 0, 0, 1, -1},
  {0, 0, 1, 0, 0},
  {0, 1, 0, 0, 0},
  {1, 0, 0, 0, 1},
  {0, 0, 1, 1, -1},
  {0, 1, 1, 0, -1},
  {1, 1, 0, 0, 2},
  {1, 0, 0, 1, 1},
  {0, 1, 1, 1, -1},
  {1, 1, 1, 0, 1},
  {1, 0, 1, 1, -1},
  {1, 1, 0, 1, 0},
  {1, 1, 1, 1, -1},
  {1, 0, 1, 0, 1},
  {0, 1, 0, 1, 0}
};

int values[] = {0, 0, 0, 0, 0};
int value = 0;
long s_time = 0;
long m_time = 0;
long now = 0;
long master_push_iterations = 0;

void setup() {
  pinMode(TX_PIN, OUTPUT);
  pinMode(LED_SLAVE_PIN, OUTPUT);
  pinMode(LED_MASTER_PIN, OUTPUT);
  
  rf_driver.init();
  
  hc05.begin(9600);
  
  Serial.begin(9600);
  Serial.println("Started master...");
}

void readMaster() {
  if (digitalRead(WEAPON_PIN)) {
    if (!values[MT])
      m_time = millis();
    values[MT] = 1;
    master_push_iterations++;
  }
  if (!values[MC])
    values[MC] = digitalRead(CAZOLETA_PIN);
}

void readSlave() {
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);
  
  if (rf_driver.recv(buf, &buflen)) {
    if (!values[ST] && buf[0] == 1) {
      values[ST] = 1;
      s_time = millis() - RF_LATENCY;
    }
    if (buf[0] == 0)
      if (!values[SC])
        values[SC] = 1;
  }
}

bool arraysEqual(int a[], int b[], int n) {
  for (int i = 0; i < n; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

int process(int *values) {
  for (int i = 0; i < 4; i++)
    Serial.print(values[i]);
  Serial.print('\n');

  if (master_push_iterations < PUSH_NEEDED_ITERATIONS)
    master_push_iterations = 0;

  for (int i = 0; i < 16; i++) {
    if (arraysEqual(MATRIX[i], values, 4)) {
      if (MATRIX[i][4] == 2) {
        Serial.println(abs(m_time - s_time));
        if (abs(m_time - s_time) <= DOUBLE_TOLERANCE_INTERVAL)
          return 2;
        return 1 ? m_time < s_time : 0;
      }
      return MATRIX[i][4];
    }
  }
}

void emitToServer(int value) {
  Serial.println(value);
  
  if (value == 0) {
    digitalWrite(LED_SLAVE_PIN, HIGH);
  } else if (value == 1) {
    digitalWrite(LED_MASTER_PIN, HIGH);
  } else if (value == 2) {
    digitalWrite(LED_MASTER_PIN, HIGH);
    digitalWrite(LED_SLAVE_PIN, HIGH);
  }
  
  hc05.println(value);
  
  delay(WAIT_AFTER_TOUCHE);
  
  digitalWrite(LED_SLAVE_PIN, LOW);
  digitalWrite(LED_MASTER_PIN, LOW);
  
  hc05.println(ALLEZ_SIGNAL);
}

void loop() {
  readMaster();
  readSlave();
  
  now = millis();
  
  if ((m_time && (now - m_time > WAIT_INTERVAL)) || (s_time && (now - s_time > WAIT_INTERVAL))) {
    value = process(values);
    
    values[MT] = 0;
    values[ST] = 0;
    values[MC] = 0;
    values[SC] = 0;
    
    m_time = 0;
    s_time = 0;

    emitToServer(value);
  }
}
