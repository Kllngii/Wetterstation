fn main() {
    slint_build::compile_with_config(
        //"ui/appwindow.slint",
        "ui/mcu_window.slint",
        slint_build::CompilerConfiguration::new()
            .embed_resources(slint_build::EmbedResourcesKind::EmbedForSoftwareRenderer),
    )
    .unwrap();
}
