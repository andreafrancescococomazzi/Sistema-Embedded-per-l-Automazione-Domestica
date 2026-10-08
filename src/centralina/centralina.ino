/*
 * Sistema Embedded per l'Automazione Domestica
 * 
 * Codice della centralina
 * Comunica cambi di stato allo Slave in modo efficiente
 */

#include <Servo.h>
#include <SoftwareSerial.h>
#include "DHT.h"

// ---------- SOFTWARE SERIAL PER SLAVE/Display ----------
SoftwareSerial displaySerial(8, 9); // RX=8, TX=9

// ---------- SERVO CANCELLO ----------
Servo cancello;
const int ANGLE_CHIUSO = 0;
const int ANGLE_APERTO = 90;
int currentAngle = ANGLE_CHIUSO;
int targetAngle = ANGLE_CHIUSO;
unsigned long lastServoStep = 0;
const unsigned long SERVO_STEP_DELAY = 10;
bool servoAttivo = false;

// ---------- STATO CANCELLO ----------
enum GateState {IDLE, OPENING, OPEN, CLOSING};
GateState gateState = IDLE;
unsigned long openStartMillis = 0;
const unsigned long AUTO_CLOSE_MS = 30000; // 30 sec

// ---------- LDR / Luci Esterne ----------
const int LDR_PIN = A0;
const int SOGLIA_BUIO = 300;
const int SOGLIA_LUCE = 550;
bool luceAccesa = false;
unsigned long lastLDRRead = 0;
const unsigned long LDR_INTERVAL = 200;

// ---------- PIN Relè ----------
const int releIng = 7;
const int relePir = 6;   // Luce Notturna / PIR interno
const int pirPin = 2;    // PIR interno
const int releC = 11;
const int releCancello = 4;
const int ledCancello = 5;
const int pirCancello = 10;
const int releLuci = 12;

// ---------- PIR interno = Luce Notturna ----------
bool pirAttivo = false;         
bool luceNotturnaAccesa = false;


// ---------- DHT ----------
#define DHTPIN A1
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
unsigned long ultimoTempo = 0;
#define INTERVALLO_LETTURA 5000
float tempEMA = 0;
float humEMA = 0;
bool emaInizializzata = false;
#define ALPHA 0.2
float tempOld = -100;
float humOld = -100;

// ---------- Stato ingressi ----------
bool ingresso = false;
bool cucina = false;

// ---------- Funzioni Forward ----------
void gestioneCancello(unsigned long now);
void gestioneChiusuraAutomatica(unsigned long now);
void gestioneLuceNotturna(unsigned long now);
void gestioneLDR(unsigned long now);
void gestioneDHT(unsigned long now);
void comandiSeriali(unsigned long now);
void inviaAlSlave(String messaggio);
void leggiComandiSlave();

void setup() {
  // PinMode
  pinMode(releIng, OUTPUT); digitalWrite(releIng, HIGH);
  pinMode(relePir, OUTPUT); digitalWrite(relePir, HIGH);
  pinMode(releC, OUTPUT); digitalWrite(releC, HIGH);
  pinMode(releCancello, OUTPUT); digitalWrite(releCancello, HIGH);
  pinMode(releLuci, OUTPUT); digitalWrite(releLuci, HIGH);
  pinMode(pirPin, INPUT);
  pinMode(pirCancello, INPUT);
  pinMode(ledCancello, OUTPUT); digitalWrite(ledCancello, LOW);
  pinMode(LDR_PIN, INPUT);

  // Seriali
  Serial.begin(9600);
  displaySerial.begin(9600);

  // Servo
  cancello.attach(3);
  cancello.write(currentAngle);
  cancello.detach();

  // DHT
  dht.begin();

  Serial.println("Master pronto");
  displaySerial.println("Sistema Avviato");
}

void loop() {
  unsigned long now = millis();

  gestioneCancello(now);
  gestioneChiusuraAutomatica(now);
  gestioneLuceNotturna(now);
  gestioneLDR(now);
  gestioneDHT(now);
  comandiSeriali(now);
  leggiComandiSlave();
}

// ---------- FUNZIONI ----------

// Funzione per inviare al Slave/Display
void inviaAlSlave(String messaggio) {
  displaySerial.println(messaggio);
  Serial.println(messaggio);
}

// Gestione Cancello
void gestioneCancello(unsigned long now) {
  if (gateState == OPENING || gateState == CLOSING) {
    if (!servoAttivo) {
      cancello.attach(3);
      servoAttivo = true;
      digitalWrite(releCancello, LOW);
      digitalWrite(ledCancello, LOW);
    }
    if (now - lastServoStep >= SERVO_STEP_DELAY) {
      lastServoStep = now;
      if (gateState == OPENING) {
        if (currentAngle < targetAngle) currentAngle++;
        else {
          gateState = OPEN;
          cancello.detach(); servoAttivo = false;
          digitalWrite(releCancello, HIGH);
          digitalWrite(ledCancello, HIGH);
          openStartMillis = now;
          inviaAlSlave("CANCELLO;OPEN");
        }
        cancello.write(currentAngle);
      } else if (gateState == CLOSING) {
        if (digitalRead(pirCancello) == HIGH) {
          targetAngle = ANGLE_APERTO;
          gateState = OPENING;
          Serial.println("Ostacolo! Riapertura Cancello");
          inviaAlSlave("CANCELLO;RIAPERTURA");
          return;
        }
        if (currentAngle > targetAngle) currentAngle--;
        else {
          gateState = IDLE;
          cancello.detach(); servoAttivo = false;
          digitalWrite(releCancello, HIGH);
          digitalWrite(ledCancello, LOW);
          inviaAlSlave("CANCELLO;CLOSE");
        }
        cancello.write(currentAngle);
      }
    }
  }
}

// Chiusura automatica
void gestioneChiusuraAutomatica(unsigned long now) {
  if (gateState == OPEN && now - openStartMillis >= AUTO_CLOSE_MS) {
    targetAngle = ANGLE_CHIUSO;
    gateState = CLOSING;
  }
}

// ---------- Gestione Luce Notturna con PIR ----------
void gestioneLuceNotturna(unsigned long now) {
  if (pirAttivo) {
    if (digitalRead(pirPin) == HIGH) {
      if (!luceNotturnaAccesa) {
        luceNotturnaAccesa = true;
        digitalWrite(relePir, LOW);  // accendi luce
        inviaAlSlave("LUCE_NOTTE;ON");
      }
    } else {
      if (luceNotturnaAccesa) {
        luceNotturnaAccesa = false;
        digitalWrite(relePir, HIGH); // spegni luce
        inviaAlSlave("LUCE_NOTTE;OFF");
      }
    }
  }
  else {
    // PIR spento --> luce sempre spenta
    if (luceNotturnaAccesa) {
      luceNotturnaAccesa = false;
      digitalWrite(relePir, HIGH);
      inviaAlSlave("LUCE_NOTTE;OFF");
    }
  }
}

// ---------- Gestione LDR ----------
void gestioneLDR(unsigned long now) {
  if (now - lastLDRRead < LDR_INTERVAL) return;
  lastLDRRead = now;

  int val = analogRead(LDR_PIN);
  bool luceNow = luceAccesa;
  if (!luceAccesa && val < SOGLIA_BUIO) luceNow = true;
  else if (luceAccesa && val > SOGLIA_LUCE) luceNow = false;

  if (luceNow != luceAccesa) {
    luceAccesa = luceNow;
    digitalWrite(releLuci, luceAccesa ? LOW : HIGH);
    inviaAlSlave(String("ESTERNE;") + (luceAccesa ? "ON" : "OFF"));
  }
}

// ---------- Gestione DHT ----------
void gestioneDHT(unsigned long now) {
  if (now - ultimoTempo < INTERVALLO_LETTURA) return;
  ultimoTempo = now;

  float temperatura = dht.readTemperature();
  float umidita = dht.readHumidity();
  if (isnan(temperatura) || isnan(umidita)) return;

  if (!emaInizializzata) { tempEMA = temperatura; humEMA = umidita; emaInizializzata = true; }
  else { tempEMA = ALPHA*temperatura + (1-ALPHA)*tempEMA; humEMA = ALPHA*umidita + (1-ALPHA)*humEMA; }

  if (abs(tempEMA - tempOld) > 0.5) { inviaAlSlave("TEMPERATURA;" + String(tempEMA,1)); tempOld = tempEMA; }
  if (abs(humEMA - humOld) > 2) { inviaAlSlave("UMIDITA;" + String(humEMA,1)); humOld = humEMA; }
}

// ---------- Comandi Seriali ----------
void comandiSeriali(unsigned long now) {
  while (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim(); msg.toUpperCase();
    
    if (msg.startsWith("CANCELLO_ANGLE;")) {

  int sep = msg.indexOf(';');
  String val = msg.substring(sep + 1);
  int angle = val.toInt();

  if (angle >= 0 && angle <= 90) {

    targetAngle = angle;

    if (angle > currentAngle)
      gateState = OPENING;
    else if (angle < currentAngle)
      gateState = CLOSING;

    inviaAlSlave("CANCELLO;ANGLE_" + String(angle));
  }
}
    else if (msg == "APRI_CANCELLO") { targetAngle = ANGLE_APERTO; gateState = OPENING; inviaAlSlave("CANCELLO;OPEN"); }
    else if (msg == "CHIUDI_CANCELLO") { targetAngle = ANGLE_CHIUSO; gateState = CLOSING; inviaAlSlave("CANCELLO;CLOSE"); }
    else if (msg == "INGRESSO_ON") { digitalWrite(releIng, LOW); inviaAlSlave("INGRESSO;ON"); }
    else if (msg == "INGRESSO_OFF") { digitalWrite(releIng, HIGH); inviaAlSlave("INGRESSO;OFF"); }
    else if (msg == "CUCINA_ON") { digitalWrite(releC, LOW); inviaAlSlave("CUCINA;ON"); }
    else if (msg == "CUCINA_OFF") { digitalWrite(releC, HIGH); inviaAlSlave("CUCINA;OFF"); }
    else if (msg == "LN_ON") { pirAttivo = true; }
    else if (msg == "LN_OFF") { pirAttivo = false; luceNotturnaAccesa = false; digitalWrite(relePir, HIGH); inviaAlSlave("LUCE_NOTTE;OFF"); }
  }
}

// ---------- Leggi comandi dallo Slave ----------
void leggiComandiSlave() {
  while (displaySerial.available()) {
    String cmd = displaySerial.readStringUntil('\n');
    cmd.trim(); cmd.toUpperCase();

 if (cmd.startsWith("CANCELLO_ANGLE;")) {

      int sep = cmd.indexOf(';');
      int angle = cmd.substring(sep + 1).toInt();

      angle = constrain(angle, 0, 180);

      targetAngle = angle;

      if (angle > currentAngle) {
        gateState = OPENING;
      }
      else if (angle < currentAngle) {
        gateState = CLOSING;
      }

      Serial.print("Nuovo angolo target: ");
      Serial.println(targetAngle);
    }
  
    else if (cmd == "APRI_CANCELLO") { targetAngle = ANGLE_APERTO; gateState = OPENING; inviaAlSlave("CANCELLO;OPEN"); }
    else if (cmd == "CHIUDI_CANCELLO") { targetAngle = ANGLE_CHIUSO; gateState = CLOSING; inviaAlSlave("CANCELLO;CLOSE"); }
    else if (cmd == "INGRESSO_ON") { digitalWrite(releIng, LOW); inviaAlSlave("INGRESSO;ON"); }
    else if (cmd == "INGRESSO_OFF") { digitalWrite(releIng, HIGH); inviaAlSlave("INGRESSO;OFF"); }
    else if (cmd == "CUCINA_ON") { digitalWrite(releC, LOW); inviaAlSlave("CUCINA;ON"); }
    else if (cmd == "CUCINA_OFF") { digitalWrite(releC, HIGH); inviaAlSlave("CUCINA;OFF"); }
    else if (cmd == "LN_ON") { pirAttivo = true; }
    else if (cmd == "LN_OFF") { pirAttivo = false; luceNotturnaAccesa = false; digitalWrite(relePir, HIGH); inviaAlSlave("LUCE_NOTTE;OFF"); }
  }
}
