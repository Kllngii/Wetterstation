use chrono::{offset::Utc, Local};
use chrono::DateTime;

use serde::Deserialize;
use serde_json::Value;
use serialport::{SerialPort, available_ports, SerialPortInfo};
use slint::SharedString;
use std::{time::SystemTime, io::Read, collections::VecDeque, str::FromStr, sync::{Arc, Mutex}};
use clokwerk::{Scheduler, TimeUnits, Job};

use slint::{Timer, TimerMode};

mod ringtest;

//TODO per Ringbuffer letzte n Werte merken und in Graph einfügen
//TODO PieChart anhand der Messwerte füllen

slint::include_modules!();

const TEMPERATURE_0: f64 = -10.0;
const TEMPERATURE_100: f64 = 40.0;
const HUMIDITY_0: f64 = 0.0;
const HUMIDITY_100: f64 = 100.0;
const PRESSURE_0: f64 = 900.0;
const PRESSURE_100: f64 = 1100.0;
const CO2_0: f64 = 400.0;
const CO2_100: f64 = 4000.0;
const TVOC_0: f64 = 0.0;
const TVOC_100: f64 = 1200.0; //TODO Werte für CO2 und TVOC überprüfen

enum ValidData {
    DHT11(f64),
    BME280(f64),
    BOTH(f64, f64),
}

#[derive(Debug, Default)]
struct SensorData {
    temperature: Option<f64>,
    humidity: Option<f64>,
    pressure: Option<f64>,
    co2: Option<f64>,
    tvoc: Option<f64>,
}
impl FromStr for SensorData {
    type Err = Box<dyn std::error::Error>;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let a: Value = serde_json::from_str(s)?;

        //TODO DHT11 und BME280 Werte auslesen
        let temperature = a["bme-temperature"].as_f64();
        let humidity = a["bme-humidity"].as_f64();
        let pressure = a["bme-pressure"].as_f64();
        let co2 = a["ccs-co2"].as_f64();
        let tvoc = a["ccs-tvoc"].as_f64();

        Ok(SensorData {
            temperature,
            humidity,
            pressure,
            co2,
            tvoc,
        })
    }
}

fn startup() {
    match option_env!("CARGO_PKG_VERSION") {
        Some(version_string) => println!("Starte Wetterstation-UI v{version_string}!"),
        None => println!("Starte Wetterstation-UI!"),
    };
}

fn main() {
    ringtest::run();
    startup();
    let main_window = MainWindow::new().unwrap();
    let senor_deque = Arc::new(Mutex::new(VecDeque::<SensorData>::new()));

    let timer = initialize_update_timer(&main_window, senor_deque.clone());
    let thread_handle = initialize_serial(senor_deque.clone());

    main_window.run().expect("Fehler beim Anzeigen des Fensters!");
}

fn initialize_update_timer(main_window: &MainWindow, sensor_deque: Arc<Mutex<VecDeque<SensorData>>>) -> Timer {
    let refresh_timer = Timer::default();
    let weak_main = main_window.as_weak().unwrap();

    refresh_timer.start(TimerMode::Repeated, std::time::Duration::from_millis(200), move || {
        let datetime = Local::now();

        match sensor_deque.try_lock() {
            Ok(mut sensor_deque) => {
                if let Some(sensor_data) = sensor_deque.pop_front() {
                    match sensor_data.temperature {
                        Some(temperature) => {
                            weak_main.set_temperature(SharedString::from(format!("{:.1}", temperature)));
                        },
                        None => {
                            weak_main.set_temperature(SharedString::from("N/A"));
                        },
                    }
                    match sensor_data.humidity {
                        Some(humidity) => {
                            weak_main.set_humidity(SharedString::from(format!("{:.1}", humidity)));
                        },
                        None => {
                            weak_main.set_humidity(SharedString::from("N/A"));
                        },
                    }
                    match sensor_data.pressure {
                        Some(pressure) => {
                            weak_main.set_pressure(SharedString::from(format!("{:.1}", pressure)));
                        },
                        None => {
                            weak_main.set_pressure(SharedString::from("N/A"));
                        },
                    }
                    match sensor_data.co2 {
                        Some(co2) => {
                            weak_main.set_co2(SharedString::from(format!("{:.1}", co2)));
                        },
                        None => {
                            weak_main.set_co2(SharedString::from("N/A"));
                        },
                    }
                    match sensor_data.tvoc {
                        Some(tvoc) => {
                            weak_main.set_tvoc(SharedString::from(format!("{:.1}", tvoc)));
                        },
                        None => {
                            weak_main.set_tvoc(SharedString::from("N/A"));
                        },
                    }
                    //TODO CO2 und TVOC anzeigen
                    value_changed(weak_main.as_weak().unwrap());
                }
            },
            Err(_) => {},
        }
        weak_main.set_time(SharedString::from(datetime.format("%T").to_string()));
    });

    refresh_timer
}

fn value_changed(main_window: MainWindow) {
    let temperature = main_window.get_temperature().parse::<f64>().unwrap_or(0.0);
    let humidity = main_window.get_humidity().parse::<f64>().unwrap_or(0.0);
    let pressure = main_window.get_pressure().parse::<f64>().unwrap_or(0.0);
    let co2 = main_window.get_co2().parse::<f64>().unwrap_or(0.0);
    let tvoc = main_window.get_tvoc().parse::<f64>().unwrap_or(0.0);

    let temperature_progress = (((temperature - TEMPERATURE_0) / (TEMPERATURE_100 - TEMPERATURE_0))*100f64) as i32;
    let humidity_progress = (((humidity - HUMIDITY_0) / (HUMIDITY_100 - HUMIDITY_0))*100f64) as i32;
    let pressure_progress = (((pressure - PRESSURE_0) / (PRESSURE_100 - PRESSURE_0))*100f64) as i32;
    let co2_progress = (((co2 - CO2_0) / (CO2_100 - CO2_0))*100f64) as i32;
    let tvoc_progress = (((tvoc - TVOC_0) / (TVOC_100 - TVOC_0))*100f64) as i32;

    println!("Temperature: {} -> {}", temperature, temperature_progress);
    println!("Humidity: {} -> {}", humidity, humidity_progress);
    println!("Pressure: {} -> {}", pressure, pressure_progress);
    println!("CO2: {} -> {}", co2, co2_progress);
    println!("TVOC: {} -> {}", tvoc, tvoc_progress);

    main_window.set_temperature_progress(temperature_progress.clamp(0, 100));
    main_window.set_humidity_progress(humidity_progress.clamp(0, 100));
    main_window.set_pressure_progress(pressure_progress.clamp(0, 100));
    main_window.set_co2_progress(co2_progress.clamp(0, 100));
    main_window.set_tvoc_progress(tvoc_progress.clamp(0, 100));
}

fn initialize_serial(mut dht_deque: Arc<Mutex<VecDeque<SensorData>>>) -> Option<clokwerk::ScheduleHandle>{
    let mut found_port: Option<SerialPortInfo> = None;
    for port in available_ports().unwrap() {
        //TODO für Windows anpassen & eventuell andere Mikrocontroller außer Arduino unterstützen
        if port.port_name.contains("tty.usbmodem") {
            println!("Mikrocontroller auf Port {} gefunden!", port.port_name);
            found_port = Some(port);
            break;
        }
    }
    match found_port {
        Some(port) => {
            let mut serial_port = serialport::new(port.port_name.as_str(), 115200)
                .timeout(std::time::Duration::from_millis(100))
                .open_native();
            match serial_port {
                Ok(mut serial) => {
                    println!("Serieller Port erfolgreich geöffnet!");
                    
                    let mut scheduler = Scheduler::new();
                    let mut buf: [u8; 512] =  [0; 512];
                    scheduler.every(1.seconds()).plus(1.seconds()).run(move || {
                        println!();
                        println!("Lese Daten vom seriellen Port!");
                        match  serial.read(&mut buf) {
                            Ok(bytes_read) => {
                                println!("Es wurden {} Bytes gelesen!", bytes_read);
                                let data = String::from_utf8_lossy(&buf);
                                let data = data.trim();
                                serial.clear(serialport::ClearBuffer::All).unwrap();
                                if let Some(start_idx) = data.find('{') {
                                    if let Some(end_idx) = data[start_idx..].find('}') {
                                        let result = &data[start_idx..start_idx + end_idx + 1];
                                        dht_deque.lock().unwrap().push_back(result.parse::<SensorData>().unwrap_or(SensorData {
                                            temperature: None,
                                            humidity: None,
                                            pressure: None,
                                            co2: None,
                                            tvoc: None,
                                        }));
                                    }
                                }
                            },
                            Err(error) => {
                                println!("Fehler beim Lesen vom seriellen Port: {}", error);
                            },
                        }
                    });
                    Some(scheduler.watch_thread(std::time::Duration::from_millis(100)))
                    
                }
                Err(error) => {
                    println!("Fehler beim Öffnen des seriellen Ports: {}", error);
                    None
                },
            }
        },
        None => {
            None
        }
    }
}