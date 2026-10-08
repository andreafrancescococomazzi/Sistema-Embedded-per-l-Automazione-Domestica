/*
 * Sistema Embedded per l'Automazione Domestica
 * Codice Interfaccia Utente
 */

#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <PCF8575.h>

// ---------- KEYPAD ----------

PCF8575 pcf(0x20);

const uint8_t rows[4] = {0,1,2,3};
const uint8_t cols[4] = {4,5,6,7};

char keymap[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

String angleBuffer = "";
bool enteringAngle = false;

// ---------- LCD + SERIAL ----------
LiquidCrystal lcd(7,6,5,4,3,2);
SoftwareSerial domoticaSerial(8,9);

// ---------- PULSANTI ----------
const int btnSu = A0;
const int btnGiu = A1;
const int btnSeleziona = A2;
const int btnBack = A3;

// ---------- STATO CASA ----------
struct CasaState {
  bool ingresso = false;
  bool cucina = false;
  bool luciEsterne = false;
  bool luceNotturna = false;

  enum Gate {CHIUSO, APERTO};
  Gate cancello = CHIUSO;

  float temperatura = 0;
  float umidita = 0;
};

CasaState casa;

// ---------- MENU ----------
enum Screen {HOME, MENU, DETTAGLIO};
Screen currentScreen = HOME;
Screen prevScreen = HOME;

const char* menuItems[] = {
  "Ingresso",
  "Cucina",
  "Luce Notturna",
  "Cancello",
  "Luci Esterne",
  "Temperatura",
  "Umidita"
};

const int MAX_MENU = 7;
int menuIndex = 0;

// ---------- OVERLAY ----------
String systemMessage = "";
bool showMessage = false;
unsigned long messageTimer = 0;

const unsigned long TIMEOUT_DETTAGLIO = 5000;
unsigned long dettaglioTimer = 0;

// ---------- DEBOUNCE ----------
const unsigned long debounceDelay = 50;

unsigned long lastDebounceSu = 0;
unsigned long lastDebounceGiu = 0;
unsigned long lastDebounceSel = 0;
unsigned long lastDebounceBack = 0;

bool lastSuState = HIGH;
bool lastGiuState = HIGH;
bool lastSelState = HIGH;
bool lastBackState = HIGH;

// =====================================================
// SETUP
// =====================================================
void setup() {

  lcd.begin(16,2);

  pinMode(btnSu, INPUT_PULLUP);
  pinMode(btnGiu, INPUT_PULLUP);
  pinMode(btnSeleziona, INPUT_PULLUP);
  pinMode(btnBack, INPUT_PULLUP);

  domoticaSerial.begin(9600);

  lcd.print("DOMOTICA BOOT...");
  delay(1500);
  lcd.clear();

  Wire.begin();
  pcf.begin();

for(int i=0;i<4;i++){
  pcf.write(cols[i], HIGH);
  pcf.write(rows[i], HIGH);
}
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  readButtons();
  readSerial();
  render();
  handleKeypad();
  
}

// =====================================================
// LETTURA PULSANTI
// =====================================================
void readButtons() {

  unsigned long now = millis();

  // -------- SU --------
  bool readingSu = digitalRead(btnSu);
  if (readingSu != lastSuState) {
    lastDebounceSu = now;
    lastSuState = readingSu;
  }
  if ((now - lastDebounceSu) > debounceDelay && readingSu == LOW) {
    if (currentScreen == MENU) {
      menuIndex--;
      if (menuIndex < 0) menuIndex = MAX_MENU - 1;
    }
    lastDebounceSu = now;
  }

  // -------- GIU --------
  bool readingGiu = digitalRead(btnGiu);
  if (readingGiu != lastGiuState) {
    lastDebounceGiu = now;
    lastGiuState = readingGiu;
  }
  if ((now - lastDebounceGiu) > debounceDelay && readingGiu == LOW) {
    if (currentScreen == MENU) {
      menuIndex++;
      if (menuIndex >= MAX_MENU) menuIndex = 0;
    }
    lastDebounceGiu = now;
  }

  // -------- SELEZIONA --------
  bool readingSel = digitalRead(btnSeleziona);
  if (readingSel != lastSelState) {
    lastDebounceSel = now;
    lastSelState = readingSel;
  }

  if ((now - lastDebounceSel) > debounceDelay && readingSel == LOW) {

    if (currentScreen == HOME) {
      currentScreen = MENU;
    }
    else if (currentScreen == MENU) {
      currentScreen = DETTAGLIO;
      dettaglioTimer = now;
    }
    else if (currentScreen == DETTAGLIO) {

      String cmd = "";

      switch(menuIndex) {

        case 0:
          casa.ingresso = !casa.ingresso;
          cmd = casa.ingresso ? "INGRESSO_ON" : "INGRESSO_OFF";
          break;

        case 1:
          casa.cucina = !casa.cucina;
          cmd = casa.cucina ? "CUCINA_ON" : "CUCINA_OFF";
          break;

      case 2:
         casa.luceNotturna = !casa.luceNotturna;
         cmd = casa.luceNotturna ? "LN_ON" : "LN_OFF";
         domoticaSerial.println(cmd); 

           // Mostra subito lo stato in modo leggibile sul display
         systemMessage = "Luce Notte: " + String(casa.luceNotturna ? "ON" : "OFF");
         showMessage = true;
         messageTimer = now;
         break;

        case 3:
          casa.cancello = (casa.cancello == CasaState::APERTO)
                          ? CasaState::CHIUSO
                          : CasaState::APERTO;

          cmd = (casa.cancello == CasaState::APERTO)
                ? "APRI_CANCELLO"
                : "CHIUDI_CANCELLO";
          break;
      }

      if (cmd != "") {
        domoticaSerial.println(cmd);
      }

      dettaglioTimer = now;
    }

    lastDebounceSel = now;
  }

  // -------- BACK --------
  bool readingBack = digitalRead(btnBack);
  if (readingBack != lastBackState) {
    lastDebounceBack = now;
    lastBackState = readingBack;
  }

  if ((now - lastDebounceBack) > debounceDelay && readingBack == LOW) {

    if (currentScreen == DETTAGLIO)
      currentScreen = MENU;
    else if (currentScreen == MENU)
      currentScreen = HOME;

    lastDebounceBack = now;
  }
}

// =====================================================
// SERIAL
// =====================================================
void readSerial() {

  while (domoticaSerial.available()) {

    String msg = domoticaSerial.readStringUntil('\n');
    msg.trim();
    processIncoming(msg);
  }
}

void processIncoming(String msg) {

  int sep = msg.indexOf(';');
  if (sep == -1) return;

  String device = msg.substring(0, sep);
  String value = msg.substring(sep + 1);

  if (device == "INGRESSO") casa.ingresso = (value == "ON");
  if (device == "CUCINA") casa.cucina = (value == "ON");
  if (device == "ESTERNE") casa.luciEsterne = (value == "ON");
  if (device == "LUCE_NOTTURNA") casa.luceNotturna = (value == "ON");

  if (device == "CANCELLO") {
    casa.cancello = (value == "OPEN") ?
                    CasaState::APERTO :
                    CasaState::CHIUSO;
  }

  if (device == "TEMPERATURA") casa.temperatura = value.toFloat();
  if (device == "UMIDITA") casa.umidita = value.toFloat();

  systemMessage = device + ": " + value;
  showMessage = true;
  messageTimer = millis();
}

// =====================================================
// RENDER (NO SFARFALLIO)
// =====================================================
void render() {

  static Screen lastScreen = HOME;

  if (currentScreen != lastScreen) {
    lcd.clear();
    lastScreen = currentScreen;
  }

  if (showMessage) {
    lcd.setCursor(0,0);
    lcd.print("                ");
    lcd.setCursor(0,0);
    lcd.print(systemMessage);

    lcd.setCursor(0,1);
    lcd.print("                ");

    if (millis() - messageTimer > 2000) {
      showMessage = false;
      lcd.clear();
    }
    return;
  }

  if (currentScreen == DETTAGLIO &&
      millis() - dettaglioTimer > TIMEOUT_DETTAGLIO) {
      currentScreen = HOME;
      return;
  }

  switch(currentScreen) {
    case HOME: drawHome(); break;
    case MENU: drawMenu(); break;
    case DETTAGLIO: drawDettaglio(); break;
  }
}

// =====================================================
// SCHERMATE
// =====================================================
void drawHome() {

  lcd.setCursor(0,0);
  lcd.print("Casa Andrea     ");

  lcd.setCursor(0,1);
  lcd.print("Gate: ");
  lcd.print(casa.cancello==CasaState::APERTO ? "OPEN  " : "CLOSE ");
}

void drawMenu() {

  lcd.setCursor(0,0);
  lcd.print("> ");
  lcd.print(menuItems[menuIndex]);
  lcd.print("                ");

  lcd.setCursor(0,1);
  lcd.print("Sel=OK Back=<-  ");
}

void drawDettaglio() {

  lcd.setCursor(0,0);
  lcd.print(menuItems[menuIndex]);
  lcd.print(":              ");

  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);

  switch(menuIndex) {

    case 0: lcd.print(casa.ingresso ? "ON " : "OFF"); break;
    case 1: lcd.print(casa.cucina ? "ON " : "OFF"); break;
    case 2: lcd.print(casa.luceNotturna ? "ON " : "OFF"); break;
    case 3:   if(enteringAngle) {
                    lcd.print("Angle: ");
                    lcd.print(angleBuffer);
                  } else {
                       lcd.print(casa.cancello == CasaState::APERTO ? "OPEN" : "CLOSE");} break;
    case 4: lcd.print(casa.luciEsterne ? "ON " : "OFF"); break;
    case 5: lcd.print(casa.temperatura,1); lcd.print(" C"); break;
    case 6: lcd.print(casa.umidita,1); lcd.print(" %"); break;
  }
}

// =====================================================
// KEYPAD
// =====================================================

char getKey() {
  for(int c=0; c<4; c++){
    pcf.write(cols[c], LOW);  // seleziona la colonna corrente
    
    for(int r=0; r<4; r++){
      if(pcf.read(rows[r]) == 0){ // tasto premuto
        delay(50);                // debounce
        while(pcf.read(rows[r]) == 0); // aspetta rilascio tasto
        pcf.write(cols[c], HIGH); // reset colonna
        return keymap[r][c];
      }
    }
    
    pcf.write(cols[c], HIGH); // reset colonna
  }
  return '\0';
}

void handleKeypad() {

  char key = getKey();
  if(key == '\0') return;

  // Se siamo nel dettaglio del cancello
  if(currentScreen == DETTAGLIO && menuIndex == 3) {

    enteringAngle = true;

    if(key >= '0' && key <= '9') {

      if(angleBuffer.length() < 3) {
        angleBuffer += key;
      }
    }

    else if(key == '*') {
      angleBuffer = ""; // cancella
    }

    else if(key == '#') {

      int angle = angleBuffer.toInt();

      if(angle >= 0 && angle <= 180) {

        String cmd = "CANCELLO_ANGLE;";
        cmd += angle;
        domoticaSerial.println(cmd);

        systemMessage = "Gate -> " + String(angle) + " deg";
        showMessage = true;
        messageTimer = millis();
      }

      angleBuffer = "";
      enteringAngle = false;
    }
  }
}
