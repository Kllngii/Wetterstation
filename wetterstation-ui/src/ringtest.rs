use ringbuffer::{AllocRingBuffer, RingBuffer};

use crate::DHT_Data;

pub(crate) fn run() {
    let mut buffer = AllocRingBuffer::<DHT_Data>::new(10);
    buffer.fill_default();
    buffer.push(DHT_Data { temperature: Some(0f64), humidity: Some(0f64) });
    buffer.push(DHT_Data { temperature: Some(1f64), humidity: Some(1f64) });
    buffer.push(DHT_Data { temperature: Some(2f64), humidity: Some(2f64) });
    buffer.push(DHT_Data { temperature: Some(3f64), humidity: Some(3f64) });
    buffer.push(DHT_Data { temperature: Some(4f64), humidity: Some(4f64) });
    buffer.push(DHT_Data { temperature: Some(5f64), humidity: Some(5f64) });
    buffer.push(DHT_Data { temperature: Some(6f64), humidity: Some(6f64) });
    buffer.push(DHT_Data { temperature: Some(7f64), humidity: Some(7f64) });
    buffer.push(DHT_Data { temperature: Some(8f64), humidity: Some(8f64) });
    buffer.push(DHT_Data { temperature: Some(9f64), humidity: Some(9f64) });
    buffer.push(DHT_Data { temperature: Some(10f64), humidity: Some(10f64) });
    buffer.push(DHT_Data { temperature: Some(11f64), humidity: Some(11f64) });
    buffer.push(DHT_Data { temperature: Some(12f64), humidity: Some(12f64) });
    buffer.iter().for_each(|x| {println!("{:#?}", x);});
}