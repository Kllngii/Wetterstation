use alloc::boxed::Box;
use core::fmt::Display;
use cortex_m::interrupt::free;
use defmt::{debug, Format, info};
use defmt::Display2Format;
use derive_more::Display;
use crate::meteotime::MeteoPackageType::{ANOMALY_WIND_4, HIGH_1, HIGH_2, HIGH_3, HIGH_4, LOW_1, LOW_2, LOW_3};

#[derive(Debug, Format, Clone)]
pub(crate) struct TimeStamp {
    pub(crate) minute: u8,
    pub(crate) hour: u8,
    pub(crate) day: u8,
    pub(crate) month: u8,
}

#[derive(Debug, Format, Clone)]
pub(crate) struct Forecast {
    pub(crate) region: u8,
    pub(crate) day1_weather: Option<WeatherType>,
    pub(crate) night1_weather: Option<WeatherType>,
    pub(crate) precipitation1: Option<u8>,
    pub(crate) wind1: Option<Wind>,
    pub(crate) temperature_day1: Option<i32>, //Temperatur Tag
    pub(crate) temperature_night1: Option<i32>, //Temperatur Nacht

    pub(crate) day2_weather: Option<WeatherType>,
    pub(crate) night2_weather: Option<WeatherType>,
    pub(crate) precipitation2: Option<u8>,
    pub(crate) wind2: Option<Wind>,
    pub(crate) temperature_day2: Option<i32>, //Temperatur Tag
    pub(crate) temperature_night2: Option<i32>, //Temperatur Nacht

    pub(crate) day3_weather: Option<WeatherType>,
    pub(crate) night3_weather: Option<WeatherType>,
    pub(crate) precipitation3: Option<u8>,
    pub(crate) wind3: Option<Wind>,
    pub(crate) temperature_day3: Option<i32>, //Temperatur Tag
    pub(crate) temperature_night3: Option<i32>, //Temperatur Nacht

    pub(crate) day4_weather: Option<WeatherType>,
    pub(crate) night4_weather: Option<WeatherType>,
    pub(crate) precipitation4: Option<u8>,
    pub(crate) wind4: Option<Wind>,
    pub(crate) temperature_day4: Option<i32>, //Temperatur Tag
    pub(crate) temperature_night4: Option<i32>, //Temperatur Nacht
}
#[derive(Format, Debug, Clone)]
pub(crate) struct Weather {
    pub(crate) region: u8, //Regions ID
    pub(crate) meteo_package_type: MeteoPackageType,
    pub(crate) time: TimeStamp,
    pub(crate) day_weather: Option<WeatherType>, //Wetterprognose Tag
    pub(crate) night_weather: Option<WeatherType>, //Wetterprognose Nacht
    pub(crate) precipitation: Option<u8>, //Niederschlagswahrscheinlichkeit 24h
    pub(crate) wind: Option<Wind>, //Wind 24h
    pub(crate) temperature_day: Option<i32>, //Temperatur Tag
    pub(crate) temperature_night: Option<i32>, //Temperatur Nacht
}

#[derive(Debug, Format, Clone)]
pub(crate) enum WeatherType {
    Error,
    Sonnig,
    Klar,
    LeichtBewölkt,
    VorwiegendBewölkt,
    Bedeckt,
    Hochnebel,
    Nebel,
    Regenschauer,
    LeichterRegen,
    StarkerRegen,
    FrontenGewitter,
    WärmeGewitter,
    SchneeregenSchauer,
    Schneeschauer,
    Schneeregen,
    Schneefall,
    //Extrema
    GroßeHitze,
    DichterNebel,
    KurzerStarkregen,
    ExtremeNiederschläge,
    StarkeGewitter,
    StarkeNiederschläge,

    //Schweres Wetter
    Sturm(#[defmt(Display2Format)] Box<WeatherType>, bool, bool), //(Grundwetter, Tag, Nacht)
    Böen(#[defmt(Display2Format)]Box<WeatherType>, bool), //(Grundwetter, Tag)
    Eisregen(#[defmt(Display2Format)]Box<WeatherType>, bool, bool), //(Grundwetter, Vormittag, Nachmittag)
    Feinstaub(#[defmt(Display2Format)]Box<WeatherType>), //(Grundwetter)
    Ozon(#[defmt(Display2Format)]Box<WeatherType>), //(Grundwetter)
    Radiation(#[defmt(Display2Format)]Box<WeatherType>), //(Grundwetter)
    Hochwasser(#[defmt(Display2Format)]Box<WeatherType>), //(Grundwetter)

}

impl Display for WeatherType {
    //TODO eventuell per derive_more #[derive(Display) realisierbar]
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            WeatherType::Error => core::write!(f, "Error"),
            WeatherType::Sonnig => core::write!(f, "Sonnig"),
            WeatherType::Klar => core::write!(f, "Klar"),
            WeatherType::LeichtBewölkt => core::write!(f, "Leicht Bewölkt"),
            WeatherType::VorwiegendBewölkt => core::write!(f, "Vorwiegend Bewölkt"),
            WeatherType::Bedeckt => core::write!(f, "Bedeckt"),
            WeatherType::Hochnebel => core::write!(f, "Hochnebel"),
            WeatherType::Nebel => core::write!(f, "Nebel"),
            WeatherType::Regenschauer => core::write!(f, "Regenschauer"),
            WeatherType::LeichterRegen => core::write!(f, "Leichter Regen"),
            WeatherType::StarkerRegen => core::write!(f, "Starker Regen"),
            WeatherType::FrontenGewitter => core::write!(f, "Frontengewitter"),
            WeatherType::WärmeGewitter => core::write!(f, "Wärmegewitter"),
            WeatherType::SchneeregenSchauer => core::write!(f, "Schneeregenschauer"),
            WeatherType::Schneeschauer => core::write!(f, "Schneeschauer"),
            WeatherType::Schneeregen => core::write!(f, "Schneeregen"),
            WeatherType::Schneefall => core::write!(f, "Schneefall"),
            WeatherType::GroßeHitze => core::write!(f, "Große Hitze"),
            WeatherType::DichterNebel => core::write!(f, "Dichter Nebel"),
            WeatherType::KurzerStarkregen => core::write!(f, "Kurzer Starkregen"),
            WeatherType::ExtremeNiederschläge => core::write!(f, "Extreme Niederschläge"),
            WeatherType::StarkeGewitter => core::write!(f, "Starke Gewitter"),
            WeatherType::StarkeNiederschläge => core::write!(f, "Starke Niederschläge"),
            WeatherType::Sturm(wetter, tag, nacht) => {
                if *tag {
                    if *nacht {
                        core::write!(f, "Sturm 24h + {}", wetter)
                    } else {
                        core::write!(f, "Sturm Tag + {}", wetter)
                    }
                } else {
                    core::write!(f, "Sturm Nacht + {}", wetter)
                }
            },
            WeatherType::Böen(wetter, tag) => {
                if *tag {
                    core::write!(f, "Böen Tag + {}", wetter)
                } else {
                    core::write!(f, "Böen Nacht + {}", wetter)
                }
            },
            WeatherType::Eisregen(wetter, vormittag, nachmittag) => {
                if *vormittag {
                    core::write!(f, "Eisregen Vormittag + {}", wetter)
                } else if *nachmittag {
                    core::write!(f, "Eisregen Nachmittag + {}", wetter)
                } else {
                    core::write!(f, "Eisregen Nacht + {}", wetter)
                }
            },
            WeatherType::Feinstaub(wetter) => {
                core::write!(f, "Feinstaub + {}", wetter)
            },
            WeatherType::Ozon(wetter) => {
                core::write!(f, "Ozon + {}", wetter)
            },
            WeatherType::Radiation(wetter) => {
                core::write!(f, "Radiation + {}", wetter)
            },
            WeatherType::Hochwasser(wetter) => {
                core::write!(f, "Hochwasser + {}", wetter)
            },
        }
    }
}

#[derive(Debug, Format, Display, Copy, Clone)]
pub(crate) enum WindDirection {
    N,
    NO,
    O,
    SO,
    S,
    SW,
    W,
    NW,
    Wechselnd,
    Föhn,
    Biswind,
    Mistral,
    Scirocco,
    Tramontana,
}

#[derive(Debug, Format, Display, Clone)]
pub(crate) enum MeteoPackageType {
    HIGH_1,
    LOW_1,
    HIGH_2,
    LOW_2,
    HIGH_3,
    LOW_3,
    HIGH_4,
    ANOMALY_WIND_4,
}

#[derive(Debug, Format, Copy, Clone)]
pub(crate) struct Wind {
    pub(crate) direction: Option<WindDirection>,
    pub(crate) force: u8,
}

fn decode_wind_direction(data: u8) -> Option<WindDirection> {
    match data % 16 {
        0 => Some(WindDirection::N),
        1 => Some(WindDirection::NO),
        2 => Some(WindDirection::O),
        3 => Some(WindDirection::SO),
        4 => Some(WindDirection::S),
        5 => Some(WindDirection::SW),
        6 => Some(WindDirection::W),
        7 => Some(WindDirection::NW),
        8 => Some(WindDirection::Wechselnd),
        9 => Some(WindDirection::Föhn),
        10 => Some(WindDirection::Biswind),
        11 => Some(WindDirection::Mistral),
        12 => Some(WindDirection::Scirocco),
        13 => Some(WindDirection::Tramontana),
        _ => None,
    }
}
fn decode_wind(data: u8) -> Option<Wind> {
    match data {
        0..=15 => Some(Wind { force: 0, direction: None}),
        16..=31 => Some(Wind { force: 1, direction: decode_wind_direction(data)}),
        32..=47 => Some(Wind { force: 2, direction: decode_wind_direction(data)}),
        48..=63 => Some(Wind { force: 3, direction: decode_wind_direction(data)}),
        64..=79 => Some(Wind { force: 1, direction: decode_wind_direction(data)}),
        80..=95 => Some(Wind { force: 1, direction: decode_wind_direction(data)}),
        96..=111 => Some(Wind { force: 1, direction: decode_wind_direction(data)}),
        112..=127 => Some(Wind { force: 1, direction: decode_wind_direction(data)}),
        _ => None,
    }
}

fn decode_extrem_weather_type(weather: WeatherType) -> WeatherType {
    match weather {
        WeatherType::Sonnig => WeatherType::GroßeHitze,
        WeatherType::Nebel => WeatherType::DichterNebel,
        WeatherType::Regenschauer => WeatherType::KurzerStarkregen,
        WeatherType::StarkerRegen => WeatherType::ExtremeNiederschläge,
        WeatherType::FrontenGewitter => WeatherType::StarkeGewitter,
        WeatherType::SchneeregenSchauer => WeatherType::StarkeNiederschläge,
        WeatherType::Schneeregen => WeatherType::StarkeNiederschläge,
        WeatherType::Schneefall => WeatherType::StarkeNiederschläge,
        _ => weather,
    }
}
fn decode_weather_type(weather: u8, extrema: u8, is_day: bool, anomaly: bool) -> WeatherType {
    //u8 day_weather, u8 night_weather, u8 extrema, u8 rainfall, u8 anomaly, u8 temperature
    let weather_type = match weather {
        1 => {
            if is_day {WeatherType::Sonnig} else {WeatherType::Klar}
        },
        2 => WeatherType::LeichtBewölkt,
        3 => WeatherType::VorwiegendBewölkt,
        4 => WeatherType::Bedeckt,
        5 => WeatherType::Hochnebel,
        6 => WeatherType::Nebel,
        7 => WeatherType::Regenschauer,
        8 => WeatherType::LeichterRegen,
        9 => WeatherType::StarkerRegen,
        10 => WeatherType::FrontenGewitter,
        11 => WeatherType::WärmeGewitter,
        12 => WeatherType::SchneeregenSchauer,
        13 => WeatherType::Schneeschauer,
        14 => WeatherType::Schneeregen,
        15 => WeatherType::Schneefall,
        _ => WeatherType::Error,
    };

    debug!("vorläufiges Wetter: {}", Display2Format(&weather_type));

    if !anomaly {
        match extrema {
            0 => weather_type, //Kein Extremwetter
            1 => { //24h Extremwetter
                decode_extrem_weather_type(weather_type)
            },
            2 => { //Nur am Tag Extremwetter
                if is_day {decode_extrem_weather_type(weather_type)} else {weather_type}
            },
            3 => { //Nur in der Nacht Extremwetter
                if !is_day {decode_extrem_weather_type(weather_type)} else {weather_type}
            },
            4  => {
                WeatherType::Sturm(Box::from(weather_type), true, true)
            },
            5  => {
                WeatherType::Sturm(Box::from(weather_type), true, false)
            },
            6  => {
                WeatherType::Sturm(Box::from(weather_type), false, true)
            },
            7  => {
                WeatherType::Böen(Box::from(weather_type), true)
            },
            8  => {
                WeatherType::Böen(Box::from(weather_type), false)
            },
            9  => {
                WeatherType::Eisregen(Box::from(weather_type), true, false)
            },
            10  => {
                WeatherType::Eisregen(Box::from(weather_type), false, true)
            },
            11  => {
                WeatherType::Eisregen(Box::from(weather_type), false, false)
            },
            12  => {
                WeatherType::Feinstaub(Box::from(weather_type))
            },
            13  => {
                WeatherType::Ozon(Box::from(weather_type))
            },
            14  => {
                WeatherType::Radiation(Box::from(weather_type))
            },
            15  => {
                WeatherType::Hochwasser(Box::from(weather_type))
            }
            _ => WeatherType::Error
        }
    } else {
        //TODO Extremwetter für anomaly = 1 hinzufügen
        weather_type
    }
}

fn decode_temperature(data: u8) -> i32 {
    -22 + (data as i32)
}


/// Dekodiert die Regions-ID basierend auf dem Sendezeitpunkt `time`
fn decode_region(time: TimeStamp) -> (u8, MeteoPackageType) {

    //FIXME 22:00 bezieht sich auf UTC, der Zeitstempel hat UTC+1 oder UTC+2
    let start_time = 23; //Bei Sommerzeit 24:00

    let minutes_since_start: u32 = if time.hour >= start_time {
        ((time.hour-start_time) as u32)*60 + time.minute as u32
    } else {
        ((time.hour+(24-start_time)) as u32)*60 + time.minute as u32
    };
    debug!("Bei der Zeit {} sind {} Minuten seit {}:00 vergangen", time, minutes_since_start, start_time);

    let region = (minutes_since_start / 3 % 60) as u8;
    debug!("Das entspricht Region {}", region);

    let mpt = match time.hour % 24 {
        //TODO könnte bei der ersten/letzten Region zu Problemen führen, das muss getestet werden
        0..=1 => HIGH_1,
        2..=4 => LOW_1,
        5..=7 => HIGH_2,
        8..=10 => LOW_2,
        11..=13 => HIGH_3,
        14..=16 => LOW_3,
        17..=19 => HIGH_4,
        20..=22 => ANOMALY_WIND_4,
        //TODO Support für '2 Tage Regionen' und Anomaly_Wind Zeiten prüfen
        _ => HIGH_1
    };

    (region, mpt)
}
pub(crate) fn decode_weather(data: u32, time: TimeStamp) -> Weather {
    let mut weather = Weather {
        region: 0,
        meteo_package_type: LOW_1,
        time: time.clone(),
        day_weather: None,
        night_weather: None,
        precipitation: None,
        wind: None,
        temperature_day: None,
        temperature_night: None,
    };

    (weather.region, weather.meteo_package_type) = decode_region(time.clone());

    weather.day_weather = Some(decode_weather_type(
        (data & 0x0f) as u8,
        ((data>>8) & 0x0f) as u8,
        true,
        ((data>>15) & 0x01) == 1
    ));
    weather.night_weather = Some(decode_weather_type(
        ((data>>4) & 0x0f) as u8,
        ((data>>8) & 0x0f) as u8,
        false,
        ((data>>15) & 0x01) == 1
    ));
    match weather.meteo_package_type {
        HIGH_1 | HIGH_2 | HIGH_3 | HIGH_4 => {
            weather.temperature_day = Some(decode_temperature(((data >> 16) & 0x3f) as u8))
        }
        LOW_1 | LOW_2 | LOW_3 => {
            weather.temperature_night = Some(decode_temperature(((data >> 16) & 0x3f) as u8))
        }
        ANOMALY_WIND_4 => {}
    }


    info!("{}", weather);
    weather
}