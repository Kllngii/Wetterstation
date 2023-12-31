extern crate alloc;

use crate::{OverviewAdapter, Value, TimeAdapter, TimeModel, DateModel, WeatherAdapter, BarTileModel};
use alloc::boxed::Box;
use alloc::rc::Rc;
use alloc::vec;
use core::cell::RefCell;
use core::char::decode_utf16;
use core::convert::Infallible;
use core::fmt::Display;
use core::time::Duration;
use cortex_m::interrupt::Mutex;
use cortex_m::prelude::_embedded_hal_blocking_i2c_WriteRead;
use cortex_m::prelude::_embedded_hal_serial_Read;
use cortex_m::singleton;
use embedded_alloc::Heap;
use embedded_hal::blocking::spi::Transfer;
use embedded_hal::digital::v2::{InputPin, OutputPin};
use embedded_hal::spi::FullDuplex;
use fugit::{Hertz, RateExtU32};
use hal::dma::{DMAExt, SingleChannel, WriteTarget};
use hal::uart::DataBits;
use hal::uart::StopBits;
use heapless::spsc::Queue;
use renderer::Rgb565Pixel;
use rp_pico::hal::gpio::{self, Interrupt as GpioInterrupt};
use rp_pico::hal::pac::interrupt;
use rp_pico::hal::timer::{Alarm, Alarm0};
use rp_pico::hal::I2C;
use rp_pico::hal::{self, pac, prelude::*, Clock, Timer};
use shared_bus::BusMutex;
use slint::platform::software_renderer as renderer;
use slint::platform::{PointerEventButton, WindowEvent};
use slint::{SharedString, TimerMode, ComponentHandle, Model, ModelRc, VecModel};

#[cfg(feature = "panic-probe")]
use defmt::*;
use st7789::Orientation;

use crate::xpt2046::XPT2046;
use crate::{display_interface_spi, xpt2046, AppWindow};
use crate::meteotime::{decode_region, TimeStamp};

#[cfg(feature = "panic-probe")]
extern crate defmt_rtt;
#[cfg(feature = "panic-probe")]
extern crate panic_probe;

// *** Allocator ***
const HEAP_SIZE: usize = 200 * 1024;
static mut HEAP: [u8; HEAP_SIZE] = [0; HEAP_SIZE];
#[global_allocator]
static ALLOCATOR: Heap = Heap::empty();

// berechnet nach der minimalen Zeit eines Schreibzyklus (siehe S.43) https://www.waveshare.com/w/upload/a/ae/ST7789_Datasheet.pdf
const SPI_ST7789VW_MAX_FREQ: Hertz<u32> = Hertz::<u32>::Hz(62_500_000);

const DISPLAY_SIZE: slint::PhysicalSize = slint::PhysicalSize::new(320, 240);

const DISPLAY_ORIENTATION: Orientation = Orientation::LandscapeSwapped;

const UART_RX_QUEUE_MAX_SIZE: usize = 256;

pub type TargetPixel = Rgb565Pixel;
type IrqPin = gpio::Pin<gpio::bank0::Gpio17, gpio::FunctionSio<gpio::SioInput>, gpio::PullUp>;
type SpiPins = (
    gpio::Pin<gpio::bank0::Gpio11, gpio::FunctionSpi, gpio::PullDown>,
    gpio::Pin<gpio::bank0::Gpio12, gpio::FunctionSpi, gpio::PullDown>,
    gpio::Pin<gpio::bank0::Gpio10, gpio::FunctionSpi, gpio::PullDown>,
);
type UartPins = (
    gpio::Pin<gpio::bank0::Gpio0, gpio::FunctionUart, gpio::PullNone>,
    gpio::Pin<gpio::bank0::Gpio1, gpio::FunctionUart, gpio::PullNone>,
);
type I2CPins = (
    gpio::Pin<gpio::bank0::Gpio20, gpio::FunctionI2C, gpio::PullUp>,
    gpio::Pin<gpio::bank0::Gpio21, gpio::FunctionI2C, gpio::PullUp>,
);
type EnabledSpi = hal::Spi<hal::spi::Enabled, pac::SPI1, SpiPins, 8>;
type EnabledUart = hal::uart::UartPeripheral<hal::uart::Enabled, pac::UART0, UartPins>;
type EnabledI2C = I2C<pac::I2C0, I2CPins>;

static ALARM0: Mutex<RefCell<Option<Alarm0>>> = Mutex::new(RefCell::new(None));
static TIMER: Mutex<RefCell<Option<Timer>>> = Mutex::new(RefCell::new(None));
static IRQ_PIN: Mutex<RefCell<Option<IrqPin>>> = Mutex::new(RefCell::new(None));
static GLOBAL_UART: Mutex<RefCell<Option<EnabledUart>>> = Mutex::new(RefCell::new(None));
static GLOBAL_I2C: Mutex<RefCell<Option<EnabledI2C>>> = Mutex::new(RefCell::new(None));
static UART_RX_QUEUE: UartQueueRx =
    UartQueueRx { mutex_cell_rx: Mutex::new(RefCell::new(Queue::new())) };

#[derive(Clone)]
struct SharedSpiWithFreq {
    mutex: &'static shared_bus::NullMutex<(EnabledSpi, Hertz<u32>)>,
    freq: Hertz<u32>,
}
struct UartQueueRx {
    mutex_cell_rx: Mutex<RefCell<Queue<u8, UART_RX_QUEUE_MAX_SIZE>>>,
}

impl SharedSpiWithFreq {
    fn lock<R, F: FnOnce(&mut EnabledSpi) -> R>(&self, f: F) -> R {
        self.mutex.lock(|(spi, old_freq)| {
            if *old_freq != self.freq {
                //Touch und Display-Ansteuerung brauchen verschiedene Baudraten...
                spi.set_baudrate(125_000_000u32.Hz(), self.freq);
                *old_freq = self.freq;
            }
            f(spi)
        })
    }
}
impl Transfer<u8> for SharedSpiWithFreq {
    type Error = <EnabledSpi as Transfer<u8>>::Error;

    fn transfer<'w>(&mut self, words: &'w mut [u8]) -> Result<&'w [u8], Self::Error> {
        self.lock(move |bus| bus.transfer(words))
    }
}

//TODO Wird diese impl noch benötigt? Eventuell entfernen?
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

pub fn init() {
    // *** Zugriff auf Systemressourcen übernehmen ***
    let mut pac = pac::Peripherals::take().unwrap();
    let core = pac::CorePeripherals::take().unwrap();

    let mut watchdog = hal::watchdog::Watchdog::new(pac.WATCHDOG);
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

    unsafe { ALLOCATOR.init(&mut HEAP as *const u8 as usize, core::mem::size_of_val(&HEAP)) }

    let mut delay = cortex_m::delay::Delay::new(core.SYST, clocks.system_clock.freq().raw());

    let sio = hal::sio::Sio::new(pac.SIO);
    let pins = rp_pico::Pins::new(pac.IO_BANK0, pac.PADS_BANK0, sio.gpio_bank0, &mut pac.RESETS);

    // *** Pins und benötigte Schnittstellen konfigurieren ***

    let rst = pins.gpio15.into_push_pull_output(); //Reset
    let bl = pins.gpio13.into_push_pull_output(); //Backlight

    let dc = pins.gpio8.into_push_pull_output(); //Data/Command
    let cs = pins.gpio9.into_push_pull_output(); //Display-Chipselect

    let spi_sclk = pins.gpio10.into_function::<gpio::FunctionSpi>();
    let spi_mosi = pins.gpio11.into_function::<gpio::FunctionSpi>();
    let spi_miso = pins.gpio12.into_function::<gpio::FunctionSpi>();

    let spi = hal::Spi::new(pac.SPI1, (spi_mosi, spi_miso, spi_sclk));
    let spi = spi.init(
        &mut pac.RESETS,
        clocks.peripheral_clock.freq(),
        SPI_ST7789VW_MAX_FREQ,
        embedded_hal::spi::MODE_3,
    );

    let (dc_copy, cs_copy) =
        unsafe { (core::ptr::read(&dc as *const _), core::ptr::read(&cs as *const _)) };
    let stolen_spi = unsafe { core::ptr::read(&spi as *const _) };

    let spi_mutex =
        singleton!(:shared_bus::NullMutex<(EnabledSpi, Hertz<u32>)> = shared_bus::NullMutex::create((spi, 0.Hz())))
            .unwrap();

    let display_spi = SharedSpiWithFreq { mutex: spi_mutex, freq: SPI_ST7789VW_MAX_FREQ };
    let di = display_interface_spi::SPIInterface::new(display_spi.clone(), dc, cs);

    let mut display = st7789::ST7789::new(
        di,
        Some(rst),
        Some(bl),
        DISPLAY_SIZE.width as _,
        DISPLAY_SIZE.height as _,
    );
    display.init(&mut delay).unwrap();
    display.set_orientation(DISPLAY_ORIENTATION).unwrap();

    let touch_irq = pins.gpio17.into_pull_up_input();
    touch_irq.set_interrupt_enabled(GpioInterrupt::LevelLow, true);

    cortex_m::interrupt::free(|cs| {
        IRQ_PIN.borrow(cs).replace(Some(touch_irq));
    });
    let touch = XPT2046::new(
        &IRQ_PIN,
        pins.gpio16.into_push_pull_output(),
        SharedSpiWithFreq { mutex: spi_mutex, freq: xpt2046::SPI_FREQ },
        DISPLAY_ORIENTATION,
    )
    .unwrap();

    let mut timer = Timer::new(pac.TIMER, &mut pac.RESETS, &clocks);
    let mut alarm0 = timer.alarm_0().unwrap();
    alarm0.enable_interrupt();

    cortex_m::interrupt::free(|cs| {
        ALARM0.borrow(cs).replace(Some(alarm0));
        TIMER.borrow(cs).replace(Some(timer));
    });

    unsafe {
        pac::NVIC::unmask(pac::Interrupt::IO_IRQ_BANK0);
        pac::NVIC::unmask(pac::Interrupt::TIMER_IRQ_0);
    }

    let dma = pac.DMA.split(&mut pac.RESETS);
    let pio = PioTransfer::Idle(
        dma.ch0,
        vec![Rgb565Pixel::default(); DISPLAY_SIZE.width as _].leak(),
        stolen_spi,
    );
    let buffer_provider = DrawBuffer {
        display,
        buffer: vec![Rgb565Pixel::default(); DISPLAY_SIZE.width as _].leak(),
        pio: Some(pio),
        stolen_pin: (dc_copy, cs_copy),
    };

    let _i2c_sda: gpio::Pin<_, gpio::FunctionI2C, gpio::PullUp> = pins.gpio20.reconfigure();
    let _i2c_scl: gpio::Pin<_, gpio::FunctionI2C, gpio::PullUp> = pins.gpio21.reconfigure();

    let i2c = I2C::new_controller(
        pac.I2C0,
        _i2c_sda,
        _i2c_scl,
        100.kHz(),
        &mut pac.RESETS,
        clocks.system_clock.freq(),
    );

    cortex_m::interrupt::free(|cs| {
        GLOBAL_I2C.borrow(cs).replace(Some(i2c));
    });

    let uart_pins = (
        pins.gpio0.reconfigure::<gpio::FunctionUart, gpio::PullNone>(),
        pins.gpio1.reconfigure::<gpio::FunctionUart, gpio::PullNone>(),
    );

    let mut uart = hal::uart::UartPeripheral::new(pac.UART0, uart_pins, &mut pac.RESETS)
        .enable(
            hal::uart::UartConfig::new(115200.Hz(), DataBits::Eight, None, StopBits::One),
            clocks.peripheral_clock.freq(),
        )
        .unwrap();

    unsafe {
        //UART Interrupt aktivieren
        pac::NVIC::unmask(pac::Interrupt::UART0_IRQ);
    }

    uart.enable_rx_interrupt(); //IRQ wenn Platz im TX Buffer
    cortex_m::interrupt::free(|cs| {
        GLOBAL_UART.borrow(cs).replace(Some(uart)); //Ab jetzt kein Zugriff mehr aus Main...
    });

    slint::platform::set_platform(Box::new(PicoBackend {
        window: Default::default(),
        buffer_provider: buffer_provider.into(),
        touch: touch.into(),
    }))
    .expect("backend already initialized");
}

pub fn init_timers(ui_handle: slint::Weak<AppWindow>) -> slint::Timer {
    let timer: slint::Timer = slint::Timer::default();
    let mut time: [u8; 7] = [0u8; 7];

    let id_ibme = [
        u8::try_from('I').unwrap(),
        u8::try_from('B').unwrap(),
        u8::try_from('M').unwrap(),
        u8::try_from('E').unwrap(),
    ];
    let id_ebme = [
        u8::try_from('E').unwrap(),
        u8::try_from('B').unwrap(),
        u8::try_from('M').unwrap(),
        u8::try_from('E').unwrap(),
    ];
    let id_co2 = [
        u8::try_from('C').unwrap(),
        u8::try_from('O').unwrap(),
        u8::try_from('2').unwrap(),
        u8::try_from('.').unwrap(),
    ];
    let id_time = [
        u8::try_from('T').unwrap(),
        u8::try_from('I').unwrap(),
        u8::try_from('M').unwrap(),
        u8::try_from('E').unwrap(),
    ];
    let id_meteo = [
        u8::try_from('M').unwrap(),
        u8::try_from('T').unwrap(),
        u8::try_from('E').unwrap(),
        u8::try_from('O').unwrap(),
    ];

    timer.start(TimerMode::Repeated, Duration::from_millis(1000), move || {
        let ui = ui_handle.upgrade().unwrap();

        let overview_adapter = ui.global::<OverviewAdapter>();
        let time_adapter = ui.global::<TimeAdapter>();
        let weather_adapter = ui.global::<WeatherAdapter>();

        static mut I2C: Option<EnabledI2C> = None;
        unsafe {
            if I2C.is_none() {
                cortex_m::interrupt::free(|cs| {
                    I2C = GLOBAL_I2C.borrow(cs).take();
                });
            }
            if let Some(i2c) = &mut I2C {

                i2c.write_read(0x68, &[0x00u8], &mut time).expect("I2C Fehler");
                //info!("{:02x}:{:02x}:{:02x} {:02x}.{:02x}.20{:02x}", time[2], time[1], time[0], time[4], time[5], time[6]);

                //TODO ebenfalls unnötige .into() hier, nach Linter-Update entfernen
                let weekday: SharedString = match time[3] {
                    1 => "Mo".into(),
                    2 => "Di".into(),
                    3 => "Mi".into(),
                    4 => "Do".into(),
                    5 => "Fr".into(),
                    6 => "Sa".into(),
                    7 => "So".into(),
                    _ => {
                        warn!("Ungültiger Wochentag!");
                        "Mo".into()
                    }
                };
                //TODO Makro oder impl für diese Umwandlung?
                let mut current_time: TimeModel = time_adapter.get_time(); //(i >> 4) * 10 + (i & 0xF)
                current_time.hours = ((time[2] >> 4) * 10 + (time[2] & 0xF)) as i32;
                current_time.minutes = ((time[1] >> 4) * 10 + (time[1] & 0xF)) as i32;
                current_time.seconds = ((time[0] >> 4) * 10 + (time[0] & 0xF)) as i32;

                let mut current_date: DateModel = time_adapter.get_date();
                current_date.day = ((time[4] >> 4) * 10 + (time[4] & 0xF)) as i32;
                current_date.month = ((time[5] >> 4) * 10 + (time[5] & 0xF)) as i32;
                current_date.year = ((time[6] >> 4) * 10 + (time[6] & 0xF)) as i32;
                current_date.weekday = weekday;

                decode_region(TimeStamp {minute: current_time.minutes.clone() as u8,
                    hour: current_time.hours.clone() as u8,
                    day: current_date.day.clone() as u8,
                    month: current_date.month.clone() as u8,
                });

                time_adapter.set_time(current_time);
                time_adapter.set_date(current_date);

                let weekdays = match time_adapter.get_date().weekday.as_str() {
                    "Mo" => ("Montag", "Di", "Mi", "Do"),
                    "Di" => ("Dienstag", "Mi", "Do", "Fr"),
                    "Mi" => ("Mittwoch", "Do", "Fr", "Sa"),
                    "Do" => ("Donnerstag", "Fr", "Sa", "So"),
                    "Fr" => ("Freitag", "Sa", "So", "Mo"),
                    "Sa" => ("Samstag", "So", "Mo", "Di"),
                    _ => ("Sonntag", "Mo", "Di", "Mi"),
                };

                weather_adapter.set_current_day(weekdays.0.into());
                let week = weather_adapter.get_week_model();
                let week_model = week.as_any().downcast_ref::<VecModel<BarTileModel>>().expect("muss gehen");
                let mut first_day = week_model.row_data(0).unwrap();
                let mut second_day = week_model.row_data(1).unwrap();
                let mut third_day = week_model.row_data(2).unwrap();

                first_day.title = weekdays.1.into();
                second_day.title = weekdays.2.into();
                third_day.title = weekdays.3.into();

                week_model.set_row_data(0, first_day);
                week_model.set_row_data(1, second_day);
                week_model.set_row_data(2, third_day);
            } else {
                warn!("I2C nicht initialisiert!");
            }
        }

        cortex_m::interrupt::free(|cs| {
            let cell_queue = UART_RX_QUEUE.mutex_cell_rx.borrow(cs);
            let mut queue = cell_queue.borrow_mut();

            let mut count = 0;
            let mut data: [u8; UART_RX_QUEUE_MAX_SIZE] = [0u8; UART_RX_QUEUE_MAX_SIZE];
            while let Some(byte) = queue.dequeue() {
                //info!("Byte empfangen: {}", byte);
                if count >= data.len() {
                    //Bei einem Overflow Fehlermeldung ausgeben
                    warn!("Maximale Länge eines UART-Telegramms überschritten! count = {}; max_len = {}", count, data.len());
                } else {
                    data[count] = byte;
                    count += 1;
                }
            }
            if count > 0 {
                info!("");
                let temperature_modelrc: ModelRc<Value> = overview_adapter.get_temperature_model();
                let humidity_modelrc: ModelRc<Value> = overview_adapter.get_humidity_model();
                let pressure_modelrc: ModelRc<Value> = overview_adapter.get_pressure_model();

                let temp_mod = temperature_modelrc.as_any().downcast_ref::<VecModel<Value>>().expect("Muss gehen!");
                let humi_mod = humidity_modelrc.as_any().downcast_ref::<VecModel<Value>>().expect("Muss gehen!");
                let pres_mod = pressure_modelrc.as_any().downcast_ref::<VecModel<Value>>().expect("Muss gehen!");

                //info!("count: {}", count);

                if let Some(ibme_start) = find_identifier::<u8>(&data, &id_ibme) {
                    info!("Internes Paket");
                    let temperature: f32 = data.to_f32(ibme_start);
                    let humidity: f32 = data.to_f32(ibme_start + 4);
                    let pressure: f32 = data.to_f32(ibme_start + 8);

                    let mut temp_data = temp_mod.row_data(0).unwrap();
                    let mut humi_data = humi_mod.row_data(0).unwrap();
                    let mut pres_data = pres_mod.row_data(0).unwrap();

                    temp_data.value = temperature;
                    humi_data.value = humidity;
                    pres_data.value = pressure;

                    temp_mod.set_row_data(0, temp_data);
                    humi_mod.set_row_data(0, humi_data);
                    pres_mod.set_row_data(0, pres_data);
                }
                if let Some(co2_start) = find_identifier::<u8>(&data, &id_co2) {
                    let co2: i16 = data.to_i16(co2_start);
                    match co2 {
                        -1 => warn!("Ungültiger CO2 Wert"),
                        co2 => {
                            let pressure_modelrc: ModelRc<Value> = overview_adapter.get_pressure_model();
                            let pres_mod = pressure_modelrc.as_any().downcast_ref::<VecModel<Value>>().expect("Muss gehen!");
                            let mut co2_data = pres_mod.row_data(1).unwrap();
                            info!("co2: {}ppm", co2);
                            co2_data.value = co2 as f32;
                            pres_mod.set_row_data(1, co2_data);
                            //TODO neue GUI anbinden
                        }
                    };


                }
                if let Some(ebme_start) = find_identifier::<u8>(&data, &id_ebme) {
                    info!("Externes Paket");
                    let temperature: f32 = data.to_f32(ebme_start);
                    let humidity: f32 = data.to_f32(ebme_start + 4);
                    //let _pressure: f32 = data.to_f32(ebme_start + 8);

                    let mut temp_data = temp_mod.row_data(1).unwrap();
                    let mut humi_data = humi_mod.row_data(1).unwrap();

                    temp_data.value = temperature;
                    humi_data.value = humidity;

                    temp_mod.set_row_data(1, temp_data);
                    humi_mod.set_row_data(1, humi_data);
                }
                if let Some(time_start) = find_identifier::<u8>(&data, &id_time) {
                    let hour: u8 = data[time_start];
                    let minute: u8 = data[time_start + 1];
                    let second: u8 = data[time_start + 2];
                    let year: i16 = data.to_i16(time_start + 3);
                    let month: u8 = data[time_start + 5];
                    let day: u8 = data[time_start + 6];

                    info!("Zeitstempel erhalten: {:02}:{:02}:{:02} {:02}.{:02}.{:04}", hour, minute, second, day, month, year);
                }
                if let Some(meteo_start) = find_identifier::<u8>(&data, &id_meteo) {
                    let data = data.to_u32(meteo_start);
                    info!("Meteo Paket {:#b}", data);
                    let day_weather: u8 = (data & 0x0f) as u8; //Bit 0-3
                    let night_weather: u8 = ((data & 0xf0) >> 4) as u8; //Bit 4-7
                    let extrema: u8 = ((data & 0x0f_00) >> 8) as u8; //Bit 8-11
                    let rainfall = (data & 0x70_00) >> 12; //Bit 12-14
                    let anomaly = !matches!(data & 0x80_00, 0); //Bit 15
                    let temperature = -22i32 + ((data & 0x3f_00_00) >> 16) as i32; //Bit 16-21

                    //TODO Wetterdaten auslesen...
                    //let day = get_weather_type(day_weather, extrema, true, anomaly);
                    //let night = get_weather_type(night_weather, extrema, false, anomaly);
                    //info!("day: {} {}", defmt::Display2Format(&day), day_weather);
                    //info!("night: {} {}", defmt::Display2Format(&night), night_weather);
                }
            }
        });
    });

    timer
}

trait NumbersFromSlice<T: PartialEq + Sized = Self> {
    fn to_f32(&self, start_index: usize) -> f32;
    fn to_i16(&self, start_index: usize) -> i16;
    fn to_u16(&self, start_index: usize) -> u16;
    fn to_u32(&self, start_index: usize) -> u32;
}

impl NumbersFromSlice for [u8; UART_RX_QUEUE_MAX_SIZE] {
    fn to_f32(&self, start_index: usize) -> f32 {
        f32::from_be_bytes([
            self[start_index + 3],
            self[start_index + 2],
            self[start_index + 1],
            self[start_index],
        ])
    }
    fn to_i16(&self, start_index: usize) -> i16 {
        i16::from_be_bytes([self[start_index + 1], self[start_index]])
    }
    fn to_u16(&self, start_index: usize) -> u16 {
        u16::from_be_bytes([self[start_index + 1], self[start_index]])
    }
    fn to_u32(&self, start_index: usize) -> u32 {
        u32::from_be_bytes([self[start_index + 3], self[start_index + 2], self[start_index + 1], self[start_index]])
    }
}

fn find_identifier<T>(haystack: &[T], needle: &[T]) -> Option<usize>
where
    for<'a> &'a [T]: PartialEq,
{
    haystack
        .windows(needle.len())
        .position(|window| window == needle)
        .map(|index| index + needle.len())
}

struct PicoBackend<DrawBuffer, Touch> {
    window: RefCell<Option<Rc<renderer::MinimalSoftwareWindow>>>,
    buffer_provider: RefCell<DrawBuffer>,
    touch: RefCell<Touch>,
}
impl<
        DI: display_interface::WriteOnlyDataCommand,
        RST: OutputPin<Error = Infallible>,
        BL: OutputPin<Error = Infallible>,
        TO: WriteTarget<TransmittedWord = u8> + FullDuplex<u8>,
        CH: SingleChannel,
        DC_: OutputPin<Error = Infallible>,
        CS_: OutputPin<Error = Infallible>,
        IRQ: InputPin<Error = Infallible>,
        CS: OutputPin<Error = Infallible>,
        SPI: Transfer<u8>,
    > slint::platform::Platform
    for PicoBackend<
        DrawBuffer<st7789::ST7789<DI, RST, BL>, PioTransfer<TO, CH>, (DC_, CS_)>,
        XPT2046<IRQ, CS, SPI>,
    >
{
    fn create_window_adapter(
        &self,
    ) -> Result<Rc<dyn slint::platform::WindowAdapter>, slint::PlatformError> {
        let window =
            renderer::MinimalSoftwareWindow::new(renderer::RepaintBufferType::ReusedBuffer);
        self.window.replace(Some(window.clone()));
        Ok(window)
    }

    fn run_event_loop(&self) -> Result<(), slint::PlatformError> {
        let mut last_touch = None;

        self.window.borrow().as_ref().unwrap().set_size(DISPLAY_SIZE);

        loop {
            slint::platform::update_timers_and_animations();

            if let Some(window) = self.window.borrow().clone() {
                window.draw_if_needed(|renderer| {
                    let mut buffer_provider = self.buffer_provider.borrow_mut();
                    renderer.render_by_line(&mut *buffer_provider);
                    buffer_provider.flush_frame();
                });

                // handle touch event
                let button = PointerEventButton::Left;
                if let Some(event) = self
                    .touch
                    .borrow_mut()
                    .read()
                    .map_err(|_| ())
                    .unwrap()
                    .map(|point| {
                        let position = slint::PhysicalPosition::new(
                            (point.x * DISPLAY_SIZE.width as f32) as _,
                            (point.y * DISPLAY_SIZE.height as f32) as _,
                        )
                        .to_logical(window.scale_factor());
                        match last_touch.replace(position) {
                            Some(_) => WindowEvent::PointerMoved { position },
                            None => WindowEvent::PointerPressed { position, button },
                        }
                    })
                    .or_else(|| {
                        last_touch
                            .take()
                            .map(|position| WindowEvent::PointerReleased { position, button })
                    })
                {
                    let is_pointer_release_event =
                        matches!(event, WindowEvent::PointerReleased { .. });

                    window.dispatch_event(event);

                    //Eventuellen Hover-State über Widgets zurücknehmen
                    if is_pointer_release_event {
                        window.dispatch_event(WindowEvent::PointerExited);
                    }
                    //Nach Touch-Input keinen Sleep auslösen
                    continue;
                }

                if window.has_active_animations() {
                    //Bei laufenden Animationen keinen Sleep auslösen
                    continue;
                }
            }

            //voraussichtliche wfe-Zeit berechnen 💤
            let sleep_duration = match slint::platform::duration_until_next_timer_update() {
                None => None,
                Some(d) => {
                    let micros = d.as_micros() as u32;
                    if micros < 10 {
                        //Wenn man weniger als 10µs schlafen will, merk es dir mit einem REIM, NEIN!
                        continue;
                    } else {
                        Some(fugit::MicrosDurationU32::micros(micros))
                    }
                }
            };

            //Gute Nacht 😴💤
            cortex_m::interrupt::free(|cs| {
                if let Some(duration) = sleep_duration {
                    ALARM0.borrow(cs).borrow_mut().as_mut().unwrap().schedule(duration).unwrap();
                }

                IRQ_PIN
                    .borrow(cs)
                    .borrow()
                    .as_ref()
                    .unwrap()
                    .set_interrupt_enabled(GpioInterrupt::LevelLow, true);
            });
            cortex_m::asm::wfe();
        }
    }

    fn duration_since_start(&self) -> Duration {
        let counter = cortex_m::interrupt::free(|cs| {
            TIMER.borrow(cs).borrow().as_ref().map(|t| t.get_counter().ticks()).unwrap_or_default()
        });
        Duration::from_micros(counter)
    }

    fn debug_log(&self, arguments: core::fmt::Arguments) {
        use alloc::string::ToString; //TODO vom Linter fälschlicherweise als "unused" markiert
        debug!("{=str}", arguments.to_string());
    }
}

enum PioTransfer<TO: WriteTarget, CH: SingleChannel> {
    Idle(CH, &'static mut [TargetPixel], TO),
    Running(hal::dma::single_buffer::Transfer<CH, PartialReadBuffer, TO>),
}

impl<TO: WriteTarget<TransmittedWord = u8> + FullDuplex<u8>, CH: SingleChannel>
    PioTransfer<TO, CH>
{
    fn wait(self) -> (CH, &'static mut [TargetPixel], TO) {
        match self {
            PioTransfer::Idle(a, b, c) => (a, b, c),
            PioTransfer::Running(dma) => {
                let (a, b, mut to) = dma.wait();
                // Nach DMA Operation FIFO leeren bis zum Err(WouldBlock). Sonst macht der Touchcontroller Schwachsinn
                while to.read().is_ok() {}
                (a, b.0, to)
            }
        }
    }
}

struct DrawBuffer<Display, PioTransfer, Stolen> {
    display: Display,
    buffer: &'static mut [TargetPixel],
    pio: Option<PioTransfer>,
    stolen_pin: Stolen,
}

impl<
        DI: display_interface::WriteOnlyDataCommand,
        RST: OutputPin<Error = Infallible>,
        BL: OutputPin<Error = Infallible>,
        TO: WriteTarget<TransmittedWord = u8> + FullDuplex<u8>,
        CH: SingleChannel,
        DC_: OutputPin<Error = Infallible>,
        CS_: OutputPin<Error = Infallible>,
    > renderer::LineBufferProvider
    for &mut DrawBuffer<st7789::ST7789<DI, RST, BL>, PioTransfer<TO, CH>, (DC_, CS_)>
{
    type TargetPixel = TargetPixel;

    fn process_line(
        &mut self,
        line: usize,
        range: core::ops::Range<usize>,
        render_fn: impl FnOnce(&mut [TargetPixel]),
    ) {
        render_fn(&mut self.buffer[range.clone()]);

        // Little zu Big-Endian für DMA
        for x in &mut self.buffer[range.clone()] {
            *x = Rgb565Pixel(x.0.to_be())
        }
        let (ch, mut b, spi) = self.pio.take().unwrap().wait();
        self.stolen_pin.1.set_high().unwrap();

        core::mem::swap(&mut self.buffer, &mut b);

        self.display
            .set_pixels(
                range.start as u16,
                line as _,
                range.end as u16,
                line as u16,
                core::iter::empty(),
            )
            .unwrap();

        self.stolen_pin.1.set_low().unwrap();
        self.stolen_pin.0.set_high().unwrap();
        let mut dma = hal::dma::single_buffer::Config::new(ch, PartialReadBuffer(b, range), spi);
        dma.pace(hal::dma::Pace::PreferSink);
        self.pio = Some(PioTransfer::Running(dma.start()));
    }
}

impl<
        DI: display_interface::WriteOnlyDataCommand,
        RST: OutputPin<Error = Infallible>,
        BL: OutputPin<Error = Infallible>,
        TO: WriteTarget<TransmittedWord = u8> + FullDuplex<u8>,
        CH: SingleChannel,
        DC_: OutputPin<Error = Infallible>,
        CS_: OutputPin<Error = Infallible>,
    > DrawBuffer<st7789::ST7789<DI, RST, BL>, PioTransfer<TO, CH>, (DC_, CS_)>
{
    fn flush_frame(&mut self) {
        let (ch, b, spi) = self.pio.take().unwrap().wait();
        self.pio = Some(PioTransfer::Idle(ch, b, spi));
        self.stolen_pin.1.set_high().unwrap();
    }
}

struct PartialReadBuffer(&'static mut [Rgb565Pixel], core::ops::Range<usize>);
unsafe impl embedded_dma::ReadBuffer for PartialReadBuffer {
    type Word = u8;

    unsafe fn read_buffer(&self) -> (*const <Self as embedded_dma::ReadBuffer>::Word, usize) {
        let act_slice = &self.0[self.1.clone()];
        //TODO laut Linter kann das optimiert werden zu core::mem::size_of_val(act_slice)
        (act_slice.as_ptr() as *const u8, act_slice.len() * core::mem::size_of::<Rgb565Pixel>())
    }
}

#[interrupt]
fn IO_IRQ_BANK0() {
    cortex_m::interrupt::free(|cs| {
        let mut pin = IRQ_PIN.borrow(cs).borrow_mut();
        let pin = pin.as_mut().unwrap();
        pin.set_interrupt_enabled(GpioInterrupt::LevelLow, false);
        pin.clear_interrupt(GpioInterrupt::LevelLow);
    });
}

#[interrupt]
fn TIMER_IRQ_0() {
    cortex_m::interrupt::free(|cs| {
        ALARM0.borrow(cs).borrow_mut().as_mut().unwrap().clear_interrupt();
    });
}

#[interrupt]
fn UART0_IRQ() {
    /* Dank `#[interrupt]` Makro ist ein Aufruf ohne `unsafe` möglich. Diese Funktion ist nicht eintritt-invariant / reentrant,
     * durch die Funktionsweise des NVIC kann dieser Problemfall aber auch nie auftreten.
     */
    static mut UART: Option<EnabledUart> = None;

    if UART.is_none() {
        cortex_m::interrupt::free(|cs| {
            *UART = GLOBAL_UART.borrow(cs).take();
        });
    }

    if let Some(uart) = UART {
        while let Ok(byte) = uart.read() {
            cortex_m::interrupt::free(|cs| {
                let cell_queue = UART_RX_QUEUE.mutex_cell_rx.borrow(cs);
                let mut queue = cell_queue.borrow_mut();
                if queue.enqueue(byte).is_err() {
                    warn!("Fehler beim Beschreiben der RX Queue!");
                }
            });
        }
    } else {
        warn!("Uart nicht initialisiert!");
    }
    //Durch das Event sollte der Main-Thread immer wieder aufwachen...
    cortex_m::asm::sev();
}
