use chrono::offset::Utc;
use chrono::DateTime;
use slint::SharedString;
use std::time::SystemTime;

use slint::{Timer, TimerMode};

slint::include_modules!();
#[forbid(unsafe_code)]

fn startup() {
    match option_env!("CARGO_PKG_VERSION") {
        Some(version_string) => println!("Starte Wetterstation-UI v{version_string}!"),
        None => println!("Starte Wetterstation-UI!"),
    };
}

fn main() {
    startup();
    let main_window = MainWindow::new().unwrap();

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

    let clock_refresh_timer = Timer::default();
    let weak_main = main_window.as_weak().unwrap();

    clock_refresh_timer.start(TimerMode::Repeated, std::time::Duration::from_millis(200), move || {
        let system_time = SystemTime::now();
        let datetime: DateTime<Utc> = system_time.into();
        
        weak_main.set_time(SharedString::from(datetime.format("%T").to_string()));
    });


    main_window.run().expect("Fehler beim Anzeigen des Fensters!");
}
