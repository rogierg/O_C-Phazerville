#pragma once

static constexpr uint8_t MPC_MINI_GRID_ROWS = 8;
static constexpr uint8_t MPC_MINI_GRID_COLS = 8;
static constexpr uint8_t MPC_MINI_GRID_STEPS = MPC_MINI_GRID_ROWS * MPC_MINI_GRID_COLS;
static constexpr uint8_t MPC_MINI_NOTE_BASE = 36;
static constexpr uint8_t MPC_MINI_NOTE_JUMP = 64;

namespace OC {

OC_APP_CLASS(AppMPCMiniGrid, TWOCCS("MP"), "MPC Mini Grid", "MPC") {
public:
  OC_APP_INTERFACE_DECLARE(AppMPCMiniGrid, 0);

  void Init() {
    selected_row_ = 0;
    selected_col_ = 0;
    sequence_cursor_ = 0;
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i) {
      step_active_[i] = false;
      step_velocity_[i] = 0;
    }
  }

  void Process(OC::IOFrame *ioframe) {
    process_midi_in(usbMIDI);
#ifdef ARDUINO_TEENSY41
    process_midi_in(usbHostMIDI[0]);
    process_midi_in(usbHostMIDI[1]);
    process_midi_in(MIDI1);
#endif

    bool gate = false;
    int step = sequence_cursor_;
    if (step >= 0 && step < MPC_MINI_GRID_STEPS)
      gate = step_active_[step];

    ioframe->outputs.set_gate_value(0, gate);
    ioframe->outputs.set_pitch_value(0, step_pitch(sequence_cursor_));

    ioframe->outputs.set_gate_value(1, step_active_[selected_row_ * MPC_MINI_GRID_COLS + selected_col_]);
    ioframe->outputs.set_pitch_value(1, step_pitch(selected_row_ * MPC_MINI_GRID_COLS + selected_col_));

    // Step the 8x8 cursor forward by one after each processing pass.
    sequence_cursor_ = (sequence_cursor_ + 1) % MPC_MINI_GRID_STEPS;
  }

  void GetIOConfig(OC::IOConfig &ioconfig) const {
    ioconfig.digital_inputs[DIGITAL_INPUT_1].set("MIDI Clock");
    ioconfig.digital_inputs[DIGITAL_INPUT_2].set("MIDI Start");
    ioconfig.digital_inputs[DIGITAL_INPUT_3].set("MIDI Stop");
    ioconfig.digital_inputs[DIGITAL_INPUT_4].set("MIDI Hold");

    ioconfig.cv[0].set("Grid Row");
    ioconfig.cv[1].set("Grid Col");
    ioconfig.cv[2].set("Velocity");
    ioconfig.cv[3].set("Pattern");

    ioconfig.outputs[0].set("MPC Gate A", OUTPUT_MODE_GATE);
    ioconfig.outputs[1].set("MPC Gate B", OUTPUT_MODE_GATE);
    ioconfig.outputs[2].set("MPC Pitch A", OUTPUT_MODE_PITCH);
    ioconfig.outputs[3].set("MPC Pitch B", OUTPUT_MODE_PITCH);
  }

  void HandleButtonEvent(const UI::Event &event) {
    if (UI::EVENT_BUTTON_PRESS == event.type) {
      switch (event.control) {
        case CONTROL_BUTTON_L:
          clear_grid();
          break;
        case CONTROL_BUTTON_R:
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
    gfxHeader("MPC Grid");
    graphics.setPrintPos(6, 18);
    graphics.print("Row");
    graphics.print(selected_row_ + 1);
    graphics.print("  Col");
    graphics.print(selected_col_ + 1);

    graphics.setPrintPos(6, 34);
    graphics.print("Steps: ");
    graphics.print(active_steps_());

    graphics.setPrintPos(6, 50);
    graphics.print("L: clear   R: row");
  }

  void DrawScreensaver() const {
    for (int row = 0; row < MPC_MINI_GRID_ROWS; ++row) {
      for (int col = 0; col < MPC_MINI_GRID_COLS; ++col) {
        int step = row * MPC_MINI_GRID_COLS + col;
        int x = 4 + col * 14;
        int y = 12 + row * 8;
        if (step_active_[step])
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
        break;
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
      stream_buffer.write(step_active_[i]);
      stream_buffer.write(step_velocity_[i]);
    }
    return stream_buffer.written();
  }

  size_t RestoreAppData(util::StreamBufferReader &stream_buffer) {
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i) {
      if (stream_buffer.underflow())
        break;
      step_active_[i] = stream_buffer.read();
      if (stream_buffer.underflow())
        break;
      step_velocity_[i] = stream_buffer.read();
    }
    return stream_buffer.read();
  }

private:
  uint8_t selected_row_;
  uint8_t selected_col_;
  uint8_t sequence_cursor_;
  bool step_active_[MPC_MINI_GRID_STEPS];
  uint8_t step_velocity_[MPC_MINI_GRID_STEPS];

  int note_to_step(uint8_t note) const {
    if (note < MPC_MINI_NOTE_BASE)
      return -1;

    const uint8_t offset = note - MPC_MINI_NOTE_BASE;
    if (offset >= MPC_MINI_NOTE_JUMP)
      return -1;

    return offset;
  }

  int active_steps_() const {
    int total = 0;
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i)
      total += step_active_[i] ? 1 : 0;
    return total;
  }

  int32_t step_pitch(int step) const {
    const int row = step / MPC_MINI_GRID_COLS;
    const int col = step % MPC_MINI_GRID_COLS;
    return ((row * 12) + (col * 2)) << 7;
  }

  void clear_grid() {
    for (int i = 0; i < MPC_MINI_GRID_STEPS; ++i) {
      step_active_[i] = false;
      step_velocity_[i] = 0;
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
        const int step = note_to_step(data1);
        if (step >= 0) {
          step_active_[step] = true;
          step_velocity_[step] = data2;
          selected_row_ = step / MPC_MINI_GRID_COLS;
          selected_col_ = step % MPC_MINI_GRID_COLS;
        }
      } else if (message == HEM_MIDI_NOTE_OFF || (message == HEM_MIDI_NOTE_ON && data2 == 0)) {
        const int step = note_to_step(data1);
        if (step >= 0)
          step_active_[step] = false;
      }
    }
  }
};

} // namespace OC
