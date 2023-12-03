#![no_std]
#![cfg_attr(not(feature = "simulator"), no_main)]
//#![feature(type_alias_impl_trait)]
//#![feature(async_fn_in_trait)]

#[cfg(feature = "panic-probe")]
use defmt::*;
#[cfg(feature = "panic-probe")]
extern crate defmt_rtt;
#[cfg(feature = "panic-probe")]
extern crate panic_probe;

#[cfg(feature = "simulator")]
mod simulator_support;

#[cfg(not(feature = "simulator"))]
mod xpt2046;
#[cfg(not(feature = "simulator"))]
mod pico_backend;
//mod uc_support;

#[cfg(not(feature = "simulator"))]
mod display_interface_spi;

extern crate alloc;

slint::include_modules!();

fn create_slint_app() -> AppWindow {
    let ui = AppWindow::new().expect("Failed to load UI");

    //let ui_handle = ui.as_weak();

    /*
    ui.on_request_increase_value(move || {
        let ui = ui_handle.unwrap();
        //ui.set_counter(ui.get_counter() + 1);
    });
    */

    ui
}

#[cfg_attr(not(feature = "simulator"), rp_pico::entry)]
fn main() -> ! {

    #[cfg(not(feature = "simulator"))]
    pico_backend::init();

    let ui = create_slint_app();
    let timer = pico_backend::init_timers(ui.as_weak());
    ui.run().unwrap();
    loop {};

    //uc_support::uc_main();
}
