use slint::ComponentHandle;
use crate::create_slint_app;
pub(crate) fn simulator_main() -> ! {
    let _ = create_slint_app().run();

    loop {
        //do nothing...
    }
}