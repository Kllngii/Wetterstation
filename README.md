# Wetterstation

Mit einem WiFi-fähigen Mikrocontroller sollen verschiedene Sensordaten erfasst- und aufbereitet werden. 

Geplant ist die Verwendung eines ESP-32, um bei Bedarf zusätzliche Daten drahtlos senden und empfangen zu können. 

Folgende Sensoren sind geplant:
- DHT22 (Temperatur, Luftfeuchtigkeit)
- BME/BMP280 (Temperatur, Luftfeuchtigkeit, Luftdruck)
- Evtl. externer Messfühler 
    - kabelgebunden als PT100/PT1000 [Zwei- oder Vierleitermessung, dazu Konstantstromquelle + rel. hochauflösender ADC (min. 16 bit)]
    - Zugekauftes RF-Modul mit Batteriebetrieb
- Messung von CO2 (Optional CO, Feinstaub)
- Empfang von Wetterdaten/Prognosen und Zeitsynchronisation über DCF77
- Evtl. Niederschlagsmessung, müsste dann aber drahtlos sein. 

Die erfassten Daten werden auf einem lokalen (Touch)-Display ausgegeben, durch die WLAN-Fähigkeit könnten die Daten zukünftig auch an eine lokale Datenbank übertragen werden (MySQL/Docker, graphische Aufbereitung mit Grafana o.ä.)

