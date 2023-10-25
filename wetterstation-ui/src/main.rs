use slint::private_unstable_api::re_exports::PointerEvent;
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

    let a = main_window.global::<CallbackExporter>().on_temperature_touch(|pe: PointerEvent| {
        println!("{pe:#?}");
    });

    main_window.run().expect("Fehler beim Anzeigen des Fensters!");

}
