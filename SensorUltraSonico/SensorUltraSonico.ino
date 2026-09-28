#define TRIG_PIN 18
#define ECHO_PIN 19

void setup() {

  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, HIGH);

  long duracion = pulseIn(ECHO_PIN, HIGH);

  if (duracion == 0) {

    Serial.println("No se detectó eco");

  } else {
    float distancia = duracion * 0.0343 / 2;;

    Serial.print("Distancia: ");
    Serial.print(distancia);
    Serial.println(" cm");
  }

  delay(500);
}