#![allow(stable_features, unknown_lints, async_fn_in_trait)]

use cortex_m::prelude::_embedded_hal_serial_Read;
use heapless::spsc::Queue;
use core::cell::RefCell;
use alloc::fmt::format;
use alloc::vec;
use cortex_m::prelude::_embedded_hal_blocking_i2c_WriteRead;
// Pull in any important traits
use core::time;
#[cfg(feature = "panic-probe")]
use defmt::*;
use embedded_hal::prelude::_embedded_hal_blocking_i2c_Read;
use embedded_hal::prelude::_embedded_hal_blocking_i2c_Write;
use fugit::RateExtU32;
use rp_pico::hal;
use rp_pico::hal::gpio;
use rp_pico::hal::gpio::PullType;
use rp_pico::hal::pac;
use rp_pico::hal::prelude::*;
use rp_pico::hal::I2C;
use rp_pico::hal::spi::ValidSpiPinout;
use rp_pico::hal::uart::{DataBits, StopBits};
use rp_pico::pac::{interrupt, SPI1};
use cortex_m::interrupt::Mutex;
use cortex_m::prelude::_embedded_hal_serial_Write;
use core::fmt::Write;

use crate::{create_slint_app, display_interface_spi, xpt2046};
use slint::platform::WindowEvent;
use slint::{format, SharedString, Timer, TimerMode};

#[cfg(feature = "panic-probe")]
extern crate defmt_rtt;
#[cfg(feature = "panic-halt")]
extern crate panic_halt;
#[cfg(feature = "panic-probe")]
extern crate panic_probe;

struct MyPlatform {
    window: alloc::rc::Rc<slint::platform::software_renderer::MinimalSoftwareWindow>,
    timer: hal::Timer,
}

impl slint::platform::Platform for MyPlatform {
    fn create_window_adapter(
        &self,
    ) -> Result<alloc::rc::Rc<dyn slint::platform::WindowAdapter>, slint::PlatformError> {
        Ok(self.window.clone())
    }
    fn duration_since_start(&self) -> core::time::Duration {
        //TODO funktioniert das noch? mit rp-pico 0.5 gab es kein .ticks()
        core::time::Duration::from_micros(self.timer.get_counter().ticks())
    }
}

type UartPins = (
    hal::gpio::Pin<hal::gpio::bank0::Gpio0, hal::gpio::FunctionUart, hal::gpio::PullNone>,
    hal::gpio::Pin<hal::gpio::bank0::Gpio1, hal::gpio::FunctionUart, hal::gpio::PullNone>,
);
type Uart = hal::uart::UartPeripheral<hal::uart::Enabled, pac::UART0, UartPins>;

struct UartQueueTx {
    mutex_cell_tx: Mutex<RefCell<Queue<u8, 64>>>,
    interrupt: pac::Interrupt,
}
struct UartQueueRx {
    mutex_cell_rx: Mutex<RefCell<Queue<u8, 256>>>,
}

impl UartQueueTx {
    fn read_byte(&self) -> Option<u8> {
        cortex_m::interrupt::free(|cs| {
            let cell_queue = self.mutex_cell_tx.borrow(cs);
            let mut queue = cell_queue.borrow_mut();
            queue.dequeue()
        })
    }

    /// Peek at the next byte in the queue without removing it.
    fn peek_byte(&self) -> Option<u8> {
        cortex_m::interrupt::free(|cs| {
            let cell_queue = self.mutex_cell_tx.borrow(cs);
            let queue = cell_queue.borrow_mut();
            queue.peek().cloned()
        })
    }

    fn len(&self) -> usize {
        cortex_m::interrupt::free(|cs| {
            let cell_queue = self.mutex_cell_tx.borrow(cs);
            let queue = cell_queue.borrow_mut();
            queue.len()
        })
    }

    /// Write some data to the queue, spinning until it all fits.
    fn write_bytes_blocking(&self, data: &[u8]) {
        // Go through all the bytes we need to write.
        for byte in data.iter() {
            // Keep trying until there is space in the queue. But release the
            // mutex between each attempt, otherwise the IRQ will never run
            // and we will never have space!
            let mut written = false;
            while !written {
                // Grab the mutex, by turning interrupts off. NOTE: This
                // doesn't work if you are using Core 1 as we only turn
                // interrupts off on one core.
                cortex_m::interrupt::free(|cs| {
                    // Grab the mutex contents.
                    let cell_queue = self.mutex_cell_tx.borrow(cs);
                    // Grab mutable access to the queue. This can't fail
                    // because there are no interrupts running.
                    let mut queue = cell_queue.borrow_mut();
                    // Try and put the byte in the queue.
                    if queue.enqueue(*byte).is_ok() {
                        // It worked! We must have had space.
                        if !pac::NVIC::is_enabled(self.interrupt) {
                            unsafe {
                                // Now enable the UART interrupt in the *Nested
                                // Vectored Interrupt Controller*, which is part
                                // of the Cortex-M0+ core. If the FIFO has space,
                                // the interrupt will run as soon as we're out of
                                // the closure.
                                pac::NVIC::unmask(self.interrupt);
                                // We also have to kick the IRQ in case the FIFO
                                // was already below the threshold level.
                                pac::NVIC::pend(self.interrupt);
                            }
                        }
                        written = true;
                    }
                });
            }
        }
    }
}

impl UartQueueRx {
    fn read_byte(&self) -> Option<u8> {
        cortex_m::interrupt::free(|cs| {
            let cell_queue = self.mutex_cell_rx.borrow(cs);
            let mut queue = cell_queue.borrow_mut();
            queue.dequeue()
        })
    }
    fn peek_byte(&self) -> Option<u8> {
        cortex_m::interrupt::free(|cs| {
            let cell_queue = self.mutex_cell_rx.borrow(cs);
            let queue = cell_queue.borrow_mut();
            queue.peek().cloned()
        })
    }
    fn len(&self) -> usize {
        cortex_m::interrupt::free(|cs| {
            let cell_queue = self.mutex_cell_rx.borrow(cs);
            let queue = cell_queue.borrow_mut();
            queue.len()
        })
    }
}

impl core::fmt::Write for &UartQueueTx {
    /// This function allows us to `writeln!` on our global static UART queue.
    /// Note we have an impl for &UartQueue, because our global static queue
    /// is not mutable and `core::fmt::Write` takes mutable references.
    fn write_str(&mut self, data: &str) -> core::fmt::Result {
        self.write_bytes_blocking(data.as_bytes());
        Ok(())
    }
}

/// This how we transfer the UART into the Interrupt Handler
static GLOBAL_UART: Mutex<RefCell<Option<Uart>>> = Mutex::new(RefCell::new(None));

/// This is our outbound UART queue. We write to it from the main thread, and
/// read from it in the UART IRQ.
static UART_TX_QUEUE: UartQueueTx = UartQueueTx {
    mutex_cell_tx: Mutex::new(RefCell::new(Queue::new())),
    interrupt: hal::pac::Interrupt::UART0_IRQ,
};

static UART_RX_QUEUE: UartQueueRx = UartQueueRx {
    mutex_cell_rx: Mutex::new(RefCell::new(Queue::new())),
};

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
    let pins = rp_pico::Pins::new(pac.IO_BANK0, pac.PADS_BANK0, sio.gpio_bank0, &mut pac.RESETS);

    let mut timer = hal::Timer::new(pac.TIMER, &mut pac.RESETS, &clocks);

    //let (mut pio, sm0, _, _, _) = pac.PIO0.split(&mut pac.RESETS);
    let _i2c_sda: hal::gpio::Pin<_, gpio::FunctionI2C, gpio::PullUp> = pins.gpio20.reconfigure();
    let _i2c_scl: hal::gpio::Pin<_, gpio::FunctionI2C, gpio::PullUp> = pins.gpio21.reconfigure();

    let mut i2c = I2C::new_controller(
        pac.I2C0,
        _i2c_sda,
        _i2c_scl,
        100.kHz(),
        &mut pac.RESETS,
        clocks.system_clock.freq(),
    );

    let uart_pins = (
        pins.gpio0.reconfigure::<hal::gpio::FunctionUart, hal::gpio::PullNone>(),
        pins.gpio1.reconfigure::<hal::gpio::FunctionUart, hal::gpio::PullNone>(),
    );

    let mut uart = hal::uart::UartPeripheral::new(pac.UART0, uart_pins, &mut pac.RESETS)
        .enable(
            hal::uart::UartConfig::new(115200.Hz(), DataBits::Eight, None,StopBits::One),
            clocks.peripheral_clock.freq(),
        )
        .unwrap();


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
    display.set_orientation(st7789::Orientation::LandscapeSwapped).unwrap();

    // touch screen
    /*
    let touch_irq = pins.gpio17.into_pull_up_input();
    //TODO statt Polling lieber IRQ wie im Beispiel https://github.com/slint-ui/slint/blob/master/examples/mcu-board-support/pico_st7789.rs
    let mut touch =
        xpt2046::XPT2046::new(touch_irq, pins.gpio16.into_push_pull_output(), spi.acquire_spi()).unwrap();
    */

    // *** UART Config ***
    uart.enable_tx_interrupt(); //IRQ wenn Platz im TX Buffer
    cortex_m::interrupt::free(|cs| {
        GLOBAL_UART.borrow(cs).replace(Some(uart)); //Ab jetzt kein Zugriff mehr aus Main...
    });

    // *** Slint Backend Setup ***
    let window = slint::platform::software_renderer::MinimalSoftwareWindow::new(Default::default());
    slint::platform::set_platform(alloc::boxed::Box::new(MyPlatform {
        window: window.clone(),
        timer,
    }))
    .unwrap();

    #[cfg(not(feature = "wifi-ap"))]
    info!("Embedded-HAL Initialisierung fertig, beginne mit Aufbau der UI!");

    #[cfg(feature = "wifi-ap")]
    info!("Embedded-HAL Initialisierung fertig, starte Wi-Fi Access Point!");
    #[cfg(feature = "wifi-ap")]
    wifi_ap_startup();
    #[cfg(feature = "wifi-ap")]
    info!("Wi-Fi AP gestartet, beginne mit Aufbau der UI!");

    // *** UI Config ***
    // (need to be done after the call to slint::platform::set_platform)
    let ui = create_slint_app();

    // *** Initialisierung des RTC-Moduls ***
    // siehe https://files.waveshare.com/upload/9/9b/DS3231.pdf
    //                         REG,    sec,  min,  hour, wd,   day,  mon,  year
    //i2c.write(0x68, &[0x00u8, 0x30, 0x55, 0x22, 0x02, 0x28, 0x11, 0x23]).expect("I2C Fehler"); // Zeit schreiben
    //i2c.write(0x68, &[0x0eu8, 0x00, 0x08]).expect("I2C Fehler"); //EOSC Bit setzen & OSF zurücksetzen

    // *** Event loop ***
    let mut line = [slint::platform::software_renderer::Rgb565Pixel(0); 320];
    //let mut last_touch = None;

    let timer = Timer::default();
    let mut time: [u8; 7] = [0u8; 7];
    //let mut control: [u8; 2] = [0u8; 2];
    timer.start(TimerMode::Repeated, time::Duration::from_millis(1000), move || {
        i2c.write_read(0x68, &[0x00u8], &mut time).expect("I2C Fehler");
        //i2c.write_read(0x68, &[0x0eu8], &mut control).expect("I2C Fehler");

        info!(
            "{:02x}:{:02x}:{:02x} {:02x}.{:02x}.20{:02x}",
            time[2], time[1], time[0], time[4], time[5], time[6]
        );
        /*
        writeln!(&UART_TX_QUEUE, "{:02x}:{:02x}:{:02x} {:02x}.{:02x}.20{:02x}",
                                           time[2], time[1], time[0], time[4], time[5], time[6]).unwrap();
         */
        let time_str: SharedString = SharedString::from(slint::format!("{:02x}:{:02x}:{:02x}", time[2], time[1], time[0]));

        let date_str: SharedString = SharedString::from(slint::format!("{:02x}.{:02x}.20{:02x}", time[4], time[5], time[6]));
        ui.set_time(time_str);
        ui.set_date(date_str);
        /*
        cortex_m::interrupt::free(|cs| {
            // Grab the mutex contents.
            let cell_queue = UART_RX_QUEUE.mutex_cell_rx.borrow(cs);
            // Grab mutable access to the queue. This can't fail
            // because there are no interrupts running.
            let mut queue = cell_queue.borrow_mut();
            // Try and put the byte in the queue.
            while let Some(byte) = queue.dequeue() {
                info!("Byte empfangen: {}", byte);
            }
        });
         */
    });

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
                    //TODO durch DMA ersetzen
                    // https://github.com/slint-ui/slint/blob/master/examples/mcu-board-support/pico_st7789.rs
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

        /*

        // handle touch event
        let button = slint::platform::PointerEventButton::Left;
        if let Some(event) = touch
            .read()
            .map_err(|_| ())
            .unwrap()
            .map(|point| {
                let position =
                    slint::PhysicalPosition::new((point.x * 320.) as _, (point.y * 240.) as _)
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

         */

        if window.has_active_animations() {
            continue;
        }

        // TODO: we could save battery here by going to sleep up to
        //   slint::platform::duration_until_next_timer_update()
        // or until the next touch interrupt, whatever comes first
        // cortex_m::asm::wfe();
    }
}

#[interrupt]
fn UART0_IRQ() {
    // This variable is special. It gets mangled by the `#[interrupt]` macro
    // into something that we can access without the `unsafe` keyword. It can
    // do this because this function cannot be called re-entrantly. We know
    // this because the function's 'real' name is unknown, and hence it cannot
    // be called from the main thread. We also know that the NVIC will not
    // re-entrantly call an interrupt.
    static mut UART: Option<hal::uart::UartPeripheral<hal::uart::Enabled, pac::UART0, UartPins>> = None;

    // This is one-time lazy initialisation. We steal the variable given to us
    // via `GLOBAL_UART`.
    if UART.is_none() {
        cortex_m::interrupt::free(|cs| {
            *UART = GLOBAL_UART.borrow(cs).take();
        });
    }

    // Check if we have a UART to work with
    if let Some(uart) = UART {
        // Check if we have data to transmit
        while let Some(byte) = UART_TX_QUEUE.peek_byte() {
            if uart.write(byte).is_ok() {
                // The UART took it, so pop it off the queue.
                let _ = UART_TX_QUEUE.read_byte();
            } else {
                break;
            }
        }

        while let Ok(byte) = uart.read() {
            cortex_m::interrupt::free(|cs| {
                // Grab the mutex contents.
                let cell_queue = UART_RX_QUEUE.mutex_cell_rx.borrow(cs);
                // Grab mutable access to the queue. This can't fail
                // because there are no interrupts running.
                let mut queue = cell_queue.borrow_mut();
                // Try and put the byte in the queue.
                if !queue.enqueue(byte).is_ok() {
                    warn!("Fehler beim Beschreiben der RX Queue!");
                }
            });
        }

        if UART_TX_QUEUE.peek_byte().is_none() {
            pac::NVIC::mask(UART_TX_QUEUE.interrupt);
        }
    }

    // Set an event to ensure the main thread always wakes up, even if it's in
    // the process of going to sleep.
    cortex_m::asm::sev();
}


#[cfg(feature = "wifi-ap")]
#[inline]
fn wifi_ap_startup() {}
