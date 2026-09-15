// No dialog/fs/midi Tauri plugins needed here - app.html talks to the OS through plain Web APIs
// (getUserMedia for the mic, navigator.requestMIDIAccess() for MIDI, <input type=file> and a
// Blob download for the file-conversion/recording-export paths), all of which WebView2 supports
// directly. Kerf's own desktop app already relies on Web MIDI the same way for its input handling.
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .setup(|app| {
      if cfg!(debug_assertions) {
        app.handle().plugin(
          tauri_plugin_log::Builder::default()
            .level(log::LevelFilter::Info)
            .build(),
        )?;
      }
      Ok(())
    })
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}
