use ringbuffer::{AllocRingBuffer, RingBuffer};

use crate::SensorData;

pub(crate) fn run() {
    let mut buffer = AllocRingBuffer::<SensorData>::new(10);
    buffer.fill_default();
    buffer.push(SensorData { temperature: Some(0f64), humidity: Some(0f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(1f64), humidity: Some(1f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(2f64), humidity: Some(2f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(3f64), humidity: Some(3f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(4f64), humidity: Some(4f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(5f64), humidity: Some(5f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(6f64), humidity: Some(6f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(7f64), humidity: Some(7f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(8f64), humidity: Some(8f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(9f64), humidity: Some(9f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(10f64), humidity: Some(10f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(11f64), humidity: Some(11f64), ..Default::default()});
    buffer.push(SensorData { temperature: Some(12f64), humidity: Some(12f64), ..Default::default()});
    buffer.iter().for_each(|x| {println!("{:#?}", x);});
}