#include <RH_ASK.h>
#include <SPI.h>

#define WEAPON_PIN 13
#define CAZOLETA_PIN 6
#define PUSH_NEEDED_TIME 200
#define LOOP_ITERATION_TIME 0.012

int PUSH_NEEDED_ITERATIONS = PUSH_NEEDED_TIME * LOOP_ITERATION_TIME;

RH_ASK rf_driver;

void setup() {
  rf_driver.init();
  //Serial.begin(9600);
  //Serial.println("Started");
}

void loop() {
  uint8_t touche = digitalRead(WEAPON_PIN);
  uint8_t cazoleta = digitalRead(CAZOLETA_PIN);
  uint8_t value = touche << cazoleta;
  uint8_t *data;
  int count = 0;
  if (touche == HIGH || cazoleta == HIGH) {
    count++;
    if (count >= PUSH_NEEDED_ITERATIONS) {
      //Serial.println("touché!");
      data = &value;
      rf_driver.send(data, sizeof(value));
      rf_driver.waitPacketSent();
      count = 0;
    }
  }
}
