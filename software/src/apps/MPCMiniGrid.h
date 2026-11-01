#pragma once

static constexpr uint8_t MPC_MINI_GRID_ROWS = 8;
static constexpr uint8_t MPC_MINI_GRID_COLS = 8;
static constexpr uint8_t MPC_MINI_GRID_STEPS = MPC_MINI_GRID_ROWS * MPC_MINI_GRID_COLS;

// MPC Mini pads are typically laid out in an 8x8 matrix.
// Base note is 36 (C2), and the default 64 pad layout covers notes 36..99.
static constexpr uint8_t MPC_MINI_PAD_NOTE_LOW = 36;
static constexpr uint8_t MPC_MINI_PAD_NOTE_HIGH = 99;

namespace OC {

OC_APP_CLASS(AppMPCMiniGrid, TWOCCS("MP"), "MPC Mini", "MPC") {
public:
  OC_APP_INTERFACE_DECLARE(AppMPCMiniGrid, MPC_MINI_GRID_STEPS * 2);

  void Init() {
    selected_row_ = 0;
    selected_col_ = 0;
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i) {
      pad_active_[i] = false;
      pad_velocity_[i] = 0;
    }
  }

  void Process(OC::IOFrame *ioframe) {
    // Control-surface-only application. This app watches the MPC Mini USB MIDI input
    // and updates its internal pad state, but does not generate CV or gate output by itself.
    (void)ioframe;

    process_midi_in(usbMIDI);
#ifdef ARDUINO_TEENSY41
    process_midi_in(usbHostMIDI[0]);
    process_midi_in(usbHostMIDI[1]);
    process_midi_in(MIDI1);
#endif
  }

  void GetIOConfig(OC::IOConfig &ioconfig) const {
    // This app is intentionally a control surface only; it does not claim DAC outputs.
    ioconfig.digital_inputs[DIGITAL_INPUT_1].set("MPC Clock");
    ioconfig.digital_inputs[DIGITAL_INPUT_2].set("MPC Start");
    ioconfig.digital_inputs[DIGITAL_INPUT_3].set("MPC Stop");
    ioconfig.digital_inputs[DIGITAL_INPUT_4].set("MPC Hold");

    // Leave CV output ports undefined for the control-surface-only build.
  }

  void HandleButtonEvent(const UI::Event &event) {
    if (UI::EVENT_BUTTON_PRESS == event.type) {
      switch (event.control) {
        case CONTROL_BUTTON_L:
          clear_grid();
          break;
        case CONTROL_BUTTON_R:
          // Step through rows for the selected pad bank.
          selected_row_ = (selected_row_ + 1) % MPC_MINI_GRID_ROWS;
          break;
        case CONTROL_BUTTON_UP:
          selected_col_ = (selected_col_ + 1) % MPC_MINI_GRID_COLS;
          break;
        case CONTROL_BUTTON_DOWN:
          selected_row_ = (selected_row_ + MPC_MINI_GRID_ROWS - 1) % MPC_MINI_GRID_ROWS;
          break;
        default:
          break;
      }
    }
  }

  void HandleEncoderEvent(const UI::Event &event) {
    if (CONTROL_ENCODER_L == event.control) {
      selected_row_ = constrain(selected_row_ + event.value, 0, MPC_MINI_GRID_ROWS - 1);
    } else if (CONTROL_ENCODER_R == event.control) {
      selected_col_ = constrain(selected_col_ + event.value, 0, MPC_MINI_GRID_COLS - 1);
    }
  }

  void DrawMenu() const {
    gfxHeader("MPC Mini");

    graphics.setPrintPos(6, 18);
    graphics.print("Pad ");
    graphics.print(selected_row_ + 1);
    graphics.print(":");
    graphics.print(selected_col_ + 1);

    graphics.setPrintPos(6, 32);
    graphics.print("Mapped pads: ");
    graphics.print(count_active_pads());

    graphics.setPrintPos(6, 46);
    graphics.print("L clear   R row");
  }

  void DrawScreensaver() const {
    for (int row = 0; row < MPC_MINI_GRID_ROWS; ++row) {
      for (int col = 0; col < MPC_MINI_GRID_COLS; ++col) {
        const int step = row * MPC_MINI_GRID_COLS + col;
        const int x = 4 + col * 14;
        const int y = 12 + row * 8;
        if (pad_active_[step])
          graphics.drawRect(x, y, 10, 5);
        else
          graphics.drawFrame(x, y, 10, 5);
      }
    }
  }

  void DrawDebugInfo() const { }

  void HandleAppEvent(OC::AppEvent event) {
    switch (event) {
      case APP_EVENT_RESUME:
      case APP_EVENT_SUSPEND:
      case APP_EVENT_SCREENSAVER_ON:
      case APP_EVENT_SCREENSAVER_OFF:
        break;
      default:
        break;
    }
  }

  void Loop() { }

  size_t SaveAppData(util::StreamBufferWriter &stream_buffer) const {
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i) {
      stream_buffer.write(pad_active_[i]);
      stream_buffer.write(pad_velocity_[i]);
    }
    return stream_buffer.written();
  }

  size_t RestoreAppData(util::StreamBufferReader &stream_buffer) {
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i) {
      if (stream_buffer.underflow())
        break;
      pad_active_[i] = stream_buffer.read();
      if (stream_buffer.underflow())
        break;
      pad_velocity_[i] = stream_buffer.read();
    }
    return stream_buffer.read();
  }

private:
  uint8_t selected_row_;
  uint8_t selected_col_;
  bool pad_active_[MPC_MINI_GRID_STEPS];
  uint8_t pad_velocity_[MPC_MINI_GRID_STEPS];

  int count_active_pads() const {
    int total = 0;
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i)
      total += pad_active_[i] ? 1 : 0;
    return total;
  }

  int pad_index_from_note(uint8_t note) const {
    if (note < MPC_MINI_PAD_NOTE_LOW || note > MPC_MINI_PAD_NOTE_HIGH)
      return -1;

    // Row-major note mapping: 8 columns across each row. This matches the usual 8x8 pad matrix.
    const int offset = note - MPC_MINI_PAD_NOTE_LOW;
    const int index = offset;
    if (index < 0 || index >= MPC_MINI_GRID_STEPS)
      return -1;
    return index;
  }

  void clear_grid() {
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i) {
      pad_active_[i] = false;
      pad_velocity_[i] = 0;
    }
  }

  template <typename T>
  void process_midi_in(T &device) {
    while (device.read()) {
      const uint8_t message = device.getType();
      const uint8_t channel = device.getChannel();
      const uint8_t data1 = device.getData1();
      const uint8_t data2 = device.getData2();

      (void)channel;

      if (message == HEM_MIDI_NOTE_ON && data2 > 0) {
        const int index = pad_index_from_note(data1);
        if (index >= 0) {
          pad_active_[index] = true;
          pad_velocity_[index] = data2;
          selected_row_ = index / MPC_MINI_GRID_COLS;
          selected_col_ = index % MPC_MINI_GRID_COLS;
        }
      } else if (message == HEM_MIDI_NOTE_OFF || (message == HEM_MIDI_NOTE_ON && data2 == 0)) {
        const int index = pad_index_from_note(data1);
        if (index >= 0)
          pad_active_[index] = false;
      }
    }
  }
};

} // namespace OC
