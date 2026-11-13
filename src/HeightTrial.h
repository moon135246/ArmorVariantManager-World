#pragma once

namespace worlddiff::height_trial {
void Install();
void Tick();
struct Controls { bool available, enabled; float cm; };
Controls GetControls();
void SetControls(bool enabled, float cm);
}
