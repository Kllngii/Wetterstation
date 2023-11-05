# Wetterstation

Mit einem WiFi-fähigen Mikrocontroller sollen verschiedene Sensordaten erfasst- und aufbereitet werden. 

Geplant ist die Verwendung eines ESP-32, um bei Bedarf zusätzliche Daten drahtlos senden und empfangen zu können.

Die erfassten Daten werden auf einem lokalen (Touch)-Display ausgegeben, durch die WLAN-Fähigkeit könnten die Daten zukünftig auch an eine lokale Datenbank übertragen werden (MySQL/Docker, graphische Aufbereitung mit Grafana o.ä.)

# Sensoren
Geplant ist derzeit die Erfassung folgender Messdaten:
- DHT22 (Temperatur, Luftfeuchtigkeit)
- BME/BMP280 (Temperatur, Luftfeuchtigkeit, Luftdruck)
- Evtl. externer Messfühler 
    - kabelgebunden als PT100/PT1000 [Zwei- oder Vierleitermessung, dazu Konstantstromquelle + rel. hochauflösender ADC (min. 16 bit)]
    - Zugekauftes RF-Modul mit Batteriebetrieb
- CCS811 (CO2 und flüchtige organische Verbindungen (TVOC))
- Empfang von Wetterdaten/Prognosen und Zeitsynchronisation über DCF77
- Evtl. Niederschlagsmessung als drahtlos angebundenes Zusatzmodul

# Struktur des Repos
- **LaTeX** [Dokumentation des Projekts](#dokumentation)
- **wetterstation-arduinosensor** [Arduino als Sensormodul](#arduino-als-sensormodul)
- **wetterstation-ui/** [Desktop App](#desktop-app)
- **wetterstation-rpico-ui/** [Raspberry Pico mit Touchscreen als Interface](#raspberry-pico-mit-touchscreen-als-interface)

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
