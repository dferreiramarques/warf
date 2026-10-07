#pragma once

// Stable and Percussive retain the old parameter's normalised endpoints (0 and 1).
// Old APVTS states stored unnormalised Fast=1 and migrate that value to Percussive=2.
enum class TrackingMode { stable = 0, performance = 1, percussive = 2 };
