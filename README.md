# Sistema Embedded per l'Automazione Domestica

Progetto di un sistema di automazione domestica basato su Arduino, progettato per gestire illuminazione, cancello, sensori ambientali e interfaccia utente tramite due schede Arduino.

## Funzionalità principali

* Gestione dell'illuminazione dell'ingresso e della cucina tramite relè.
* Attivazione automatica della luce notturna tramite sensore PIR.
* Gestione automatica delle luci esterne tramite fotoresistore.
* Controllo del cancello tramite servomotore.
* Chiusura automatica del cancello dopo 30 secondi.
* Rilevamento di ostacoli durante la chiusura del cancello e riapertura automatica.
* Monitoraggio di temperatura e umidità tramite sensore DHT11.
* Visualizzazione dello stato dell'impianto tramite display LCD.
* Navigazione del menu tramite pulsanti.
* Impostazione dell'angolo del cancello tramite tastierino numerico.

## Elementi utilizzati

* 2 × Arduino
* Display LCD 16x2
* Tastierino numerico 4x4
* Modulo PCF8575 per espansione degli I/O
* Sensori PIR
* Fotoresistore
* Sensore DHT11
* Servomotore
* Moduli relè
* LED
* Pulsanti
* Breadboard e cablaggi

## Logica di funzionamento

Il sistema è suddiviso in due unità Arduino.

La **centralina principale** gestisce sensori e attuatori dell'impianto, occupandosi dell'illuminazione, del rilevamento del movimento, del monitoraggio ambientale e del controllo del cancello.

La seconda scheda Arduino gestisce l'**interfaccia utente**, composta da display LCD, pulsanti e tastierino numerico. Le due schede comunicano tramite collegamento seriale.

Il cancello può essere aperto e chiuso tramite l'interfaccia e viene richiuso automaticamente dopo 30 secondi. Durante la chiusura, un sensore PIR permette di rilevare eventuali ostacoli e comandare la riapertura del cancello.

## Librerie utilizzate

Le principali librerie utilizzate dal progetto sono:

* `Servo`
* `SoftwareSerial`
* `DHT`
* `LiquidCrystal`
* `Wire`
* `PCF8575`

Prima di compilare gli sketch nell'IDE Arduino, installare le librerie necessarie per i componenti utilizzati.

## Documentazione visiva

### Schema elettrico

![Schema elettrico](doc/schema_elettrico.png

### Rappresentazione grafica

![Rappresentazione grafica](doc/rappresentazione_grafica.png)
### Prototipo realizzato

![Foto del progetto](doc/soglia.jpg)

