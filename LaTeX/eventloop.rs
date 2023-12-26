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

    //voraussichtliche wfe-Zeit berechnen
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

    //Sleep bis zum nächsten Event auslösen
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