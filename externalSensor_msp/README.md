# Entwicklung eingestellt. 

Der Plan war eigentlich, statt dem ESP8266 einen MSP430G2553 einzusetzen, der mit 1 MHz Takt eigentlich genügend schnell sein sollte und wesentlich weniger Energie verbraucht. Allerdings ist die Entwicklung damit wesentlich komplexer, weil keine offiziellen Treiber für I2C/UART und den BME280 existieren oder die verfügbaren Treiber nicht ohne weiteres eingesetzt werden können. 

Das Programm hier hat einen UART und I2C Treiber aus dem Internet integiert (Link zum Repo sollte da irgendwo sein), zusammen mit einem BME280 Interface von Git (Link sollte da auch irgendwo sein), konnten aber nur unsinnige Werte gelesen werden. Aufgrund von Zeitdruck wird deshalb ein ESP8266 statt MSP430 eingesetzt, der zwar mehr Energie verbraucht, dafür aber funktioniert. 