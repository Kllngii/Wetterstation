#![no_std]
#![no_main]

use alloc::vec::Vec;
use core::cell::RefCell;
use core::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use cortex_m::asm::wfi;
use cortex_m::interrupt::{free, Mutex};
use embedded_alloc::Heap;
use embedded_hal::digital::v2::InputPin;
use rp_pico::hal::{gpio, Clock, Timer};
use rp_pico::{entry, hal, pac};

use defmt::*;
use fugit::MicrosDurationU32;
use rp_pico::hal::gpio::Interrupt::{EdgeHigh, EdgeLow};
use rp_pico::hal::timer::{Alarm, Alarm0, Instant};
use rp_pico::pac::{interrupt, Interrupt};
use wheelbuf::WheelBuf;

extern crate defmt_rtt;
extern crate panic_probe;
extern crate alloc;

// *** Allocator ***
const HEAP_SIZE: usize = 200 * 1024;
static mut HEAP: [u8; HEAP_SIZE] = [0; HEAP_SIZE];
#[global_allocator]
static ALLOCATOR: Heap = Heap::empty();

type DCFPin = gpio::Pin<gpio::bank0::Gpio13, gpio::FunctionSioInput, gpio::PullDown>;

type IrqBorrow = (DCFPin, Timer);

static GLOBAL_PINS: Mutex<RefCell<Option<IrqBorrow>>> = Mutex::new(RefCell::new(None));

#[entry]
fn main() -> ! {
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

    unsafe {
        ALLOCATOR.init(
            &mut HEAP as *const u8 as usize,
            core::mem::size_of_val(&HEAP),
        )
    }

    let _delay = cortex_m::delay::Delay::new(core.SYST, clocks.system_clock.freq().raw());
    let mut timer = Timer::new(pac.TIMER, &mut pac.RESETS, &clocks);
    let sio = hal::sio::Sio::new(pac.SIO);
    let pins = rp_pico::Pins::new(
        pac.IO_BANK0,
        pac.PADS_BANK0,
        sio.gpio_bank0,
        &mut pac.RESETS,
    );

    let dcf_pin: DCFPin = pins.gpio13.into_pull_down_input();

    dcf_pin.set_interrupt_enabled(EdgeLow, true);
    dcf_pin.set_interrupt_enabled(EdgeHigh, true);

    free(|cs| {
        GLOBAL_PINS.borrow(cs).replace(Some((dcf_pin, timer)));
    });

    unsafe {
        pac::NVIC::unmask(Interrupt::IO_IRQ_BANK0);
    }

    loop {
        wfi();
    }
}

#[derive(Debug, Format)]
struct SignalEdge {
    rising: bool,
    time: u32,
}

impl SignalEdge {
    pub const fn new() -> Self {
        Self {
            rising: false,
            time: 0,
        }
    }
    pub fn new_initialized(rising: bool, time: u32) -> Self {
        Self { rising, time }
    }
}

//static GLOBAL_DCF77_SIGNAL: SignalEdge = SignalEdge::new();
static GLOBAL_TIMER_TICK: AtomicBool = AtomicBool::new(false);

static GLOBAL_DCF77_VALUE

static mut GLOBAL_ALARM: Option<Alarm0> = None;
const FRAMES_PER_SECOND: u32 = 20;

#[allow(non_snake_case)] //TODO Wird das überhaupt benötigt?
#[interrupt]
fn IO_IRQ_BANK0() {
    static mut IRQ_BORROW: Option<IrqBorrow> = None;

    if IRQ_BORROW.is_none() {
        free(|cs| {
            *IRQ_BORROW = GLOBAL_PINS.borrow(cs).take();
        });
    }

    if let Some(irq) = IRQ_BORROW {
        let (pin, timer) = irq;

        let now = timer.get_counter_low();
        let is_low = pin.is_low().unwrap();

        let signal_edge = SignalEdge::new_initialized(!is_low, now);

        info!("Signal: {}", signal_edge);

        pin.clear_interrupt(if is_low { EdgeLow } else { EdgeHigh });
    }
}

#[interrupt]
unsafe fn TIMER_IRQ_0() {
    GLOBAL_TIMER_TICK.store(true, Ordering::Release);

    let alarm = GLOBAL_ALARM.as_mut().unwrap();
    alarm.clear_interrupt();
    alarm
        .schedule(MicrosDurationU32::micros(
            1_000_000 / FRAMES_PER_SECOND as u32,
        ))
        .unwrap();
}
