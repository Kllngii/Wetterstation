#![allow(stable_features, unknown_lints, async_fn_in_trait)]

// Pull in any important traits
#[cfg(feature = "panic-probe")]
use defmt::*;
#[cfg(feature = "panic-probe")]
use defmt_rtt as _;
use fugit::RateExtU32;
#[cfg(feature = "panic-halt")]
use panic_halt as _;
#[cfg(feature = "panic-probe")]
use panic_probe as _;

use rp_pico::hal;
use rp_pico::hal::gpio;
use rp_pico::hal::pac;
use rp_pico::hal::prelude::*;

use crate::{create_slint_app, xpt2046};
use slint::platform::WindowEvent;

pub(crate) fn uc_main() -> ! {
    // *** Allocator Setup ***
    const HEAP_SIZE: usize = 200 * 1024;
    static mut HEAP: [u8; HEAP_SIZE] = [0; HEAP_SIZE];
    #[global_allocator]
    static ALLOCATOR: embedded_alloc::Heap = embedded_alloc::Heap::empty();
    unsafe { ALLOCATOR.init(&mut HEAP as *const u8 as usize, core::mem::size_of_val(&HEAP)) };

    // *** Peripherals Setup ***
    let mut pac = pac::Peripherals::take().unwrap();
    let core = pac::CorePeripherals::take().unwrap();
    let mut watchdog = hal::Watchdog::new(pac.WATCHDOG);

    let clocks = hal::clocks::init_clocks_and_plls(
        rp_pico::XOSC_CRYSTAL_FREQ,
        pac.XOSC,
        pac.CLOCKS,
        pac.PLL_SYS,
        pac.PLL_USB,
        &mut pac.RESETS,
        &mut watchdog,
    )
    .ok()
    .unwrap();

    let mut delay = cortex_m::delay::Delay::new(core.SYST, clocks.system_clock.freq().raw());

    let sio = hal::sio::Sio::new(pac.SIO);
    //let pins = rp_pico::Pins::new(pac.IO_BANK0, pac.PADS_BANK0, sio.gpio_bank0, &mut pac.RESETS);
    let pins = rp_pico::Pins::new(pac.IO_BANK0, pac.PADS_BANK0, sio.gpio_bank0, &mut pac.RESETS);

    let mut timer = hal::Timer::new(pac.TIMER, &mut pac.RESETS, &clocks);

    let _spi_sclk: hal::gpio::Pin<_, gpio::FunctionSpi, gpio::PullNone> = pins.gpio10.reconfigure();
    let _spi_mosi: hal::gpio::Pin<_, gpio::FunctionSpi, gpio::PullNone> = pins.gpio11.reconfigure();
    let _spi_miso: hal::gpio::Pin<_, gpio::FunctionSpi, gpio::PullNone> = pins.gpio12.reconfigure();

    let spi = hal::spi::Spi::<_, _, _, 8>::new(pac.SPI1, (_spi_mosi, _spi_miso, _spi_sclk));
    let spi = spi.init(
        &mut pac.RESETS,
        clocks.peripheral_clock.freq(),
        62_500_000.Hz(),
        embedded_hal::spi::MODE_3,
    );

    let spi = shared_bus::BusManagerSimple::new(spi);

    let bl = pins.gpio13.into_push_pull_output();
    let rst = pins.gpio15.into_push_pull_output();
    let dc = pins.gpio8.into_push_pull_output();
    let cs = pins.gpio9.into_push_pull_output();
    let di = display_interface_spi::SPIInterface::new(spi.acquire_spi(), dc, cs);

    let mut display = st7789::ST7789::new(di, Some(rst), Some(bl), 320, 240);

    display.init(&mut delay).unwrap();
    display.set_orientation(st7789::Orientation::Landscape).unwrap();

    // touch screen
    let touch_irq = pins.gpio17.into_pull_up_input();
    let mut touch =
        xpt2046::XPT2046::new(touch_irq, pins.gpio16.into_push_pull_output(), spi.acquire_spi())
            .unwrap();

    // -------- Setup the Slint backend --------
    let window = slint::platform::software_renderer::MinimalSoftwareWindow::new(Default::default());
    slint::platform::set_platform(alloc::boxed::Box::new(MyPlatform {
        window: window.clone(),
        timer,
    }))
    .unwrap();

    struct MyPlatform {
        window: alloc::rc::Rc<slint::platform::software_renderer::MinimalSoftwareWindow>,
        timer: hal::Timer,
    }

    impl slint::platform::Platform for MyPlatform {
        fn create_window_adapter(
            &self,
        ) -> Result<alloc::rc::Rc<dyn slint::platform::WindowAdapter>, slint::PlatformError>
        {
            Ok(self.window.clone())
        }
        fn duration_since_start(&self) -> core::time::Duration {
            //TODO funktioniert das noch? mit rp-pico 0.5 gab es kein .ticks()
            core::time::Duration::from_micros(self.timer.get_counter().ticks())
        }
    }
    info!("embedded-hal init fertig, starte Wi-Fi Access Point");

    // -------- Configure the UI --------
    // (need to be done after the call to slint::platform::set_platform)
    let _ui = create_slint_app();

    // -------- Event loop --------
    let mut line = [slint::platform::software_renderer::Rgb565Pixel(0); 320];
    let mut last_touch = None;
    loop {
        slint::platform::update_timers_and_animations();
        window.draw_if_needed(|renderer| {
            use embedded_graphics_core::prelude::*;
            struct DisplayWrapper<'a, T>(
                &'a mut T,
                &'a mut [slint::platform::software_renderer::Rgb565Pixel],
            );
            impl<T: DrawTarget<Color = embedded_graphics_core::pixelcolor::Rgb565>>
                slint::platform::software_renderer::LineBufferProvider for DisplayWrapper<'_, T>
            {
                type TargetPixel = slint::platform::software_renderer::Rgb565Pixel;
                fn process_line(
                    &mut self,
                    line: usize,
                    range: core::ops::Range<usize>,
                    render_fn: impl FnOnce(&mut [Self::TargetPixel]),
                ) {
                    let rect = embedded_graphics_core::primitives::Rectangle::new(
                        Point::new(range.start as _, line as _),
                        Size::new(range.len() as _, 1),
                    );
                    render_fn(&mut self.1[range.clone()]);
                    // NOTE! this is not an efficient way to send pixel to the screen, but it is kept simple on this template.
                    // It would be much faster to use the DMA to send pixel in parallel.
                    // See the example in https://github.com/slint-ui/slint/blob/master/examples/mcu-board-support/pico_st7789.rs
                    self.0
                        .fill_contiguous(
                            &rect,
                            self.1[range.clone()].iter().map(|p| {
                                embedded_graphics_core::pixelcolor::raw::RawU16::new(p.0).into()
                            }),
                        )
                        .map_err(drop)
                        .unwrap();
                }
            }
            renderer.render_by_line(DisplayWrapper(&mut display, &mut line));
        });

        // handle touch event
        let button = slint::platform::PointerEventButton::Left;
        if let Some(event) = touch
            .read()
            .map_err(|_| ())
            .unwrap()
            .map(|point| {
                let position =
                    slint::PhysicalPosition::new((point.0 * 320.) as _, (point.1 * 240.) as _)
                        .to_logical(window.scale_factor());
                match last_touch.replace(position) {
                    Some(_) => WindowEvent::PointerMoved { position },
                    None => WindowEvent::PointerPressed { position, button },
                }
            })
            .or_else(|| {
                last_touch.take().map(|position| WindowEvent::PointerReleased { position, button })
            })
        {
            window.dispatch_event(event);
            // Don't go to sleep after a touch event that forces a redraw
            continue;
        }

        if window.has_active_animations() {
            continue;
        }

        // TODO: we could save battery here by going to sleep up to
        //   slint::platform::duration_until_next_timer_update()
        // or until the next touch interrupt, whatever comes first
        // cortex_m::asm::wfe();
    }
}
