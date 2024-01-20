# Raspberry Pico Wetterstation-UI
![Platform RPico](https://img.shields.io/badge/platform-RP2040-orange.svg)
![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)
![Apache-2.0 License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)
![Version](https://img.shields.io/badge/version-0.0.1-green.svg)

Nutzt [Slint](https://slint-ui.com), um eine hübsche UI auf einem Touch-Display anzuzeigen.

## Usage

### Auf einem PC testen (Simulator)
```
cargo run --features simulator
```

### Auf einem [RaspberryPi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) mit [2.8 inch Waveshare Touch Screen](https://www.waveshare.com/pico-restouch-lcd-2.8.htm) ausführen:

#### 1. Target RP2040 hinzufügen:
```
rustup self update
rustup update stable
rustup target add thumbv6m-none-eabi
```

#### 2. UF2-Images-Tool für den RP2040 installieren:
```
cargo install elf2uf2-rs --locked
``` 

#### 3. Korrekten Runner auswählen:
In der Datei `.cargo/config.toml` muss der richtige Runner ausgewählt werden,
indem alle anderen Runner auskommentiert werden.
- Wird der Pico im USB-Bootloader-Modus angeschlossen: `elf2uf2-rs` auswählen
- Der Pico wird über eine Debug Probe geflasht: `probe-run` wählen

#### 4. Auf RPico ausführen:
```
cargo run --target=thumbv6m-none-eabi --features=pico --release
```