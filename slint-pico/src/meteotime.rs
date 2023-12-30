use alloc::boxed::Box;
use core::fmt::Display;
use defmt::{debug, Format};
use defmt::Display2Format;
use derive_more::Display;

#[derive(Debug, Format)]
pub(crate) struct TimeStamp {
    pub(crate) minute: u8,
    pub(crate) hour: u8,
    pub(crate) day: u8,
    pub(crate) month: u8,
}
struct Weather {
    region: u8, //Regions ID
    day: TimeStamp,
    day_weather: Option<WeatherType>, //Wetterprognose Tag
    night_weather: Option<WeatherType>, //Wetterprognose Nacht
    precipitation: Option<u8>, //Niederschlagswahrscheinlichkeit 24h
    wind: Option<Wind>, //Wind 24h
    temperature_day: Option<i32>, //Temperatur Tag
    temperature_night: Option<i32>, //Temperatur Nacht
}

#[derive(Debug, Format)]
enum WeatherType {
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

#[derive(Debug, Format, Display)]
enum WindDirection {
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

#[derive(Debug, Format)]
struct Wind {
    direction: Option<WindDirection>,
    force: u8,
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


/// Dekodiert die Regions-ID basierend auf dem Sendezeitpunkt `time`
pub(crate) fn decode_region(time: TimeStamp) -> u8 {

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

    region
}
fn decode_weather(meteotime_data: u32, time: TimeStamp) -> Weather {
    let weather = Weather {
        region: 0,
        day: time,
        day_weather: None,
        night_weather: None,
        precipitation: None,
        wind: None,
        temperature_day: None,
        temperature_night: None,
    };

    //TODO hier muss die Magie passieren

    weather
}