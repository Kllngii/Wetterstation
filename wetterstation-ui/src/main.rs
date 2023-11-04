use chrono::offset::Utc;
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
    let dht_deque = Arc::new(Mutex::new(VecDeque::<SensorData>::new()));

    initialize_callbacks(&main_window);
    let timer = initialize_update_timer(&main_window, dht_deque.clone());
    let thread_handle = initialize_serial(dht_deque.clone());

    main_window.run().expect("Fehler beim Anzeigen des Fensters!");
}

fn initialize_callbacks(main_window: &MainWindow) {
    let weak_main = main_window.as_weak().unwrap();
    main_window.global::<CallbackExporter>().on_temperature_touch(move || {
        weak_main.set_show_temperature(!weak_main.get_show_temperature());
    });

    let weak_main = main_window.as_weak().unwrap();
    main_window.global::<CallbackExporter>().on_humidity_touch(move || {
        weak_main.set_show_humidity(!weak_main.get_show_humidity());
    });

    let weak_main = main_window.as_weak().unwrap();
    main_window.global::<CallbackExporter>().on_pressure_touch(move || {
        weak_main.set_show_pressure(!weak_main.get_show_pressure());
    });
}

fn initialize_update_timer(main_window: &MainWindow, dht_deque: Arc<Mutex<VecDeque<SensorData>>>) -> Timer {
    let refresh_timer = Timer::default();
    let weak_main = main_window.as_weak().unwrap();

    refresh_timer.start(TimerMode::Repeated, std::time::Duration::from_millis(200), move || {
        let system_time = SystemTime::now();
        let datetime: DateTime<Utc> = system_time.into();

        match dht_deque.try_lock() {
            Ok(mut dht_deque) => {
                if let Some(dht_data) = dht_deque.pop_front() {
                    match dht_data.temperature {
                        Some(temperature) => {
                            weak_main.set_temperature(SharedString::from(format!("{:.1}", temperature)));
                        },
                        None => {
                            weak_main.set_temperature(SharedString::from("N/A"));
                        },
                    }
                    match dht_data.humidity {
                        Some(humidity) => {
                            weak_main.set_humidity(SharedString::from(format!("{:.1}", humidity)));
                        },
                        None => {
                            weak_main.set_humidity(SharedString::from("N/A"));
                        },
                    }
                    match dht_data.pressure {
                        Some(pressure) => {
                            weak_main.set_pressure(SharedString::from(format!("{:.1}", pressure)));
                        },
                        None => {
                            weak_main.set_pressure(SharedString::from("N/A"));
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

    let temperature_progress = (((temperature - TEMPERATURE_0) / (TEMPERATURE_100 - TEMPERATURE_0))*100f64) as i32;
    let humidity_progress = (((humidity - HUMIDITY_0) / (HUMIDITY_100 - HUMIDITY_0))*100f64) as i32;
    let pressure_progress = (((pressure - PRESSURE_0) / (PRESSURE_100 - PRESSURE_0))*100f64) as i32;

    println!("Temperature: {} -> {}", temperature, temperature_progress);
    println!("Humidity: {} -> {}", humidity, humidity_progress);
    println!("Pressure: {} -> {}", pressure, pressure_progress);

    main_window.set_temperature_progress(temperature_progress.clamp(0, 100));
    main_window.set_humidity_progress(humidity_progress.clamp(0, 100));
    main_window.set_pressure_progress(pressure_progress.clamp(0, 100));
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