# Wetterstation

Mit einem WiFi-fähigen Mikrocontroller sollen verschiedene Sensordaten erfasst- und aufbereitet werden. 

Für die WiFi-Anbindung wird ein Raspberry Pi Pico W verwendet, der mit zwei Kernen und einem WLAN/Bluetooth Modul genug Leistung
zur Verfügung stellen kann. 

Die erfassten Daten werden auf einem lokalen (Touch)-Display ausgegeben, durch die WLAN-Fähigkeit könnten die Daten zukünftig auch an eine lokale Datenbank übertragen werden (MySQL/Docker, graphische Aufbereitung mit Grafana o.ä.)

# Sensoren
Folgende Sensoren werden verwendet (das Projekt ist beliebig skalierbar):
## Interne Messgruppe
- BME280 (Temperatur, Luftfeuchtigkeit, Luftdruck)
- MH-Z19C (CO2)
## Externe Messgruppe
- BME280 (Tempertur, Luftfeuchtigkeit, Luftdruck)
- DCF77-Empfänger (Uhrzeit und Wettervorhersage)
    - Der DCF77-Empfänger empfängt das Signal eines Zeitzeichensenders, das neben aktuellen Zeitinformationen eine Wettervorhersage beinhaltet. Diese ist allerdings verschlüsselt. verfügbaren GPIO Pins und Rechenleistung wird die Entschlüsselung auf dem internen Modul vorgenommen. Mit der Nutzung des ESP8266 als Außenmodul ist das ganz sinnvoll, weil der nicht so viele GPIO Pins frei verfügbar hat, ein MSP430 sollte das aber ohne grö0eren Rechenaufwand schaffen. Für die Entschlüsselung der Daten wird ein HKW581 Modul benötigt. 
## Eventuelle Erweiterungen
- DHT22 (Temperatur, Luftfeuchtigkeit)
- PT100/PT1000 (Zwei- oder Vierleitermessung, dazu Konstantstromquelle + rel. hochauflösender ADC (min. 16 bit))
- CCS811 (CO2 und flüchtige organische Verbindungen (TVOC))
- Niederschlagsmessung
- Windgeschwindigkeit und Windrichtung

# Struktur des Repos
- **LaTeX** [Dokumentation des Projekts](#dokumentation)
- **wetterstation-arduinosensor** [Arduino als Sensormodul](#arduino-als-sensormodul)
- **wetterstation-ui/** [Desktop App](#desktop-app)
- **slint-pico/** [Raspberry Pico mit Touchscreen als Interface](#raspberry-pico-mit-touchscreen-als-interface)
- **pcbs/** Hier findet man Schaltpläne und PCBs der Module als [KiCad](https://www.kicad.org) Projekt

# Desktop App
![Platform Darwin](https://img.shields.io/badge/platform-macOS-orange.svg)
![Platform Windows](https://img.shields.io/badge/platform-Windows-orange.svg)
![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)
![Apache-2.0 License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)
![Version](https://img.shields.io/badge/version-0.1.0-green.svg)

*TODO* Hier müsste man erklären was die Desktop-App ist 

# Dokumentation

Das LaTeX Dokument in diesem Ordner ist die Dokumentation des Projekts.

# Arduino als Sensormodul
![Platform Arduino](https://img.shields.io/badge/platform-arduino--uno-orange.svg)
![Version](https://img.shields.io/badge/version-0.1.0-green.svg)

Ein kleiner Beispiel-Sketch, der einen Arduino (Uno) benutzt, um die I2C- und Onewire-Sensoren anzusteuern.

# Raspberry Pico mit Touchscreen als Interface 
![Platform RPico](https://img.shields.io/badge/platform-RP2040-orange.svg)
![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)
![Apache-2.0 License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)
![Version](https://img.shields.io/badge/version-0.0.1-green.svg)

*TODO* Hier müsste man erklären was die Raspberry-Pi-Pico-App ist

# Aktivität
![Alt](https://repobeats.axiom.co/api/embed/44e0efac31b7cbafcf5f86047e12328fdb639fd2.svg "Repobeats analytics image")
