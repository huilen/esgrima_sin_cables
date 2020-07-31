#include <RH_ASK.h>
#include <SPI.h>
#include <SoftwareSerial.h>

// pins
#define RX_PIN 8
#define TX_PIN 7
#define LED_MASTER_PIN 3
#define LED_SLAVE_PIN 2
#define CAZOLETA_PIN 6
#define WEAPON_PIN 13

#define RF_LATENCY 850
#define LOOP_ITERATION_TIME 0.017

/* 
 * VALORES DE REGLAMENTO
 *
 * hay que ajustar estos 3 valores según el reglamento
 * 
 * DOUBLE_TOLERANCE_INTERVAL = es el tiempo de tolerancia que hay para
 * considerar que hubo toque doble, en milisegundos
 *
 * WAIT_AFTER_TOUCHE = es el tiempo que se espera para reiniciar el asalto, en
 * milisegundos
 *
 * PUSH_NEEDED_TIME = es el tiempo que tiene que estar presionado el
 * interruptor de la espada para que se considere válido el toque, en
 * milisegundos
 *
 */
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
  // se establecen los modos de los pines de salida
  pinMode(TX_PIN, OUTPUT);
  pinMode(LED_SLAVE_PIN, OUTPUT);
  pinMode(LED_MASTER_PIN, OUTPUT);
  
  // inicia módulo RF para comunicación con componente slave
  // por defecto usa el pin D11 para TX
  rf_driver.init();
  
  // inicia módulo bluetooth
  hc05.begin(9600);
  
  Serial.begin(9600);
  Serial.println("Started master...");
}

// esta función lee los pines de espada y cazoleta del master
void readMaster() {
  // lee del pin de la espada para ver si hubo toque
  if (digitalRead(WEAPON_PIN)) {
    if (!values[MT])
      m_time = millis(); // guarda el tiempo, para comparar en caso de dobles
    values[MT] = 1;
    master_push_iterations++; // guarda la cantidad de iteraciones presionado
  }
  if (!values[MC])
    values[MC] = digitalRead(CAZOLETA_PIN); // lee del pin de la cazoleta
}

// esta función recibe por RF el toque o cazoleta del slave
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

/*
 *
 * esta función procesa un vector compuesto por los siguientes valores:
 *
 * {MT, ST, MC, SC}
 * MT = Master Touche
 * ST = Slave Touche
 * MC = Master Cazoleta
 * SC = Slave Cazoleta
 *
 * cada combinación de valores tiene como resultado un valor que puede ser:
 *
 * -1 = no hubo toque
 * 1 = toque de master
 * 0 = toque de slave
 * 2 = doble
 *
 * las combinaciones con sus respectivos resultados están definidos en la
 * constante MATRIX
 *
 */
int process(int *values) {
  for (int i = 0; i < 4; i++)
    Serial.print(values[i]);
  Serial.print('\n');

  if (master_push_iterations < PUSH_NEEDED_ITERATIONS) {
    master_push_iterations = 0;
    values[MP] = 0; // toque ignorado porque no se presionó suficiente tiempo
  }

  for (int i = 0; i < 16; i++) {
    // toma la combinación que corresponde de la matriz de combinatorias
    if (arraysEqual(MATRIX[i], values, 4)) {
      if (MATRIX[i][4] == 2) {
        Serial.println(abs(m_time - s_time));
        // en caso de doble se tiene en cuenta el tiempo
        if (abs(m_time - s_time) <= DOUBLE_TOLERANCE_INTERVAL)
          return 2; // es doble si está dentro del intervalo de tolerancia
        return 1 ? m_time < s_time : 0; // si no, es punto del primero que tocó
      }
      return MATRIX[i][4];
    }
  }
}

// esta función transmite el valor final por bluetooth al teléfono
void emitToServer(int value) {
  Serial.println(value);
  
  // se encienden estos ledes con fines de facilitar el testeo
  if (value == 0) {
    digitalWrite(LED_SLAVE_PIN, HIGH);
  } else if (value == 1) {
    digitalWrite(LED_MASTER_PIN, HIGH);
  } else if (value == 2) {
    digitalWrite(LED_MASTER_PIN, HIGH);
    digitalWrite(LED_SLAVE_PIN, HIGH);
  }
  
  hc05.println(value);
  
  // luego de transmitir se espera unos segundos antes de volver a empezar
  delay(WAIT_AFTER_TOUCHE);
  
  digitalWrite(LED_SLAVE_PIN, LOW);
  digitalWrite(LED_MASTER_PIN, LOW);
  
  hc05.println(ALLEZ_SIGNAL);
}

void loop() {
  readMaster();
  readSlave();
  
  now = millis();
  
  // si hay toque, espera para ver si se produce doble o si hubo cazoleta
  if ((m_time && (now - m_time > WAIT_INTERVAL)) || (s_time && (now - s_time > WAIT_INTERVAL))) {
    value = process(values);
    
    // resetea todos los valores
    values[MT] = 0;
    values[ST] = 0;
    values[MC] = 0;
    values[SC] = 0;

    m_time = 0;
    s_time = 0;

    emitToServer(value);
  }
}
