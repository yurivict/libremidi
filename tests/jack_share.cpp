#include <libremidi/configurations.hpp>
#include <libremidi/libremidi.hpp>

#include <jack/jack.h>

#include <chrono>
#include <thread>

struct my_app
{
  libremidi::unique_handle<jack_client_t, jack_client_close> handle;

  std::vector<libremidi::midi_in> midiin;
  std::vector<libremidi::midi_out> midiout;

  std::vector<libremidi::jack_callback_function> midiin_callbacks;
  std::vector<libremidi::jack_callback_function> midiout_callbacks;

  my_app()
  {
    auto callback = [&](int port, const libremidi::message& message) {
      auto sz = message.size();
      std::cout << "Message from port " << port << " : ";
      for (auto i = 0U; i < sz; i++)
        std::cout << "Byte " << i << " = " << (int)message[i] << ", ";
      if (sz > 0)
        std::cout << "stamp = " << message.timestamp << std::endl;

      midiout[port].send_message(message);
    };

    // Create a JACK client which will be shared across objects
    jack_status_t status{};
    handle.reset(jack_client_open("My MIDI app", JackNoStartServer, &status));

    if (!handle)
      throw std::runtime_error("Could not start JACK client");

    jack_set_process_callback(handle.get(), jack_callback, this);
    jack_activate(handle.get());

    // Create our configuration
    auto api_input_config = libremidi::jack_input_configuration{
        .context = handle.get(), .set_process_func = [this](libremidi::jack_callback_function cb) {
          midiin_callbacks.push_back(std::move(cb));
        }};
    auto api_output_config = libremidi::jack_output_configuration{
        .context = handle.get(), .set_process_func = [this](libremidi::jack_callback_function cb) {
          midiout_callbacks.push_back(std::move(cb));
        }};

    // Create 16 inputs and 16 outputs
    for (int i = 0; i < 16; i++)
    {
      midiin.emplace_back(
          libremidi::input_configuration{
              .on_message = [=](const libremidi::message& msg) { callback(i, msg); }},
          api_input_config);
      midiin[i].open_virtual_port("Input: " + std::to_string(i));

      midiout.emplace_back(libremidi::output_configuration{}, api_output_config);
      midiout[i].open_virtual_port("Output: " + std::to_string(i));
    }
  }

  static int jack_callback(jack_nframes_t cnt, void* ctx)
  {
    auto& self = *(my_app*)ctx;

    // Process the midi inputs
    for (auto& cb : self.midiin_callbacks)
      cb(cnt);

    // Do some other things

    // Process the midi outputs
    for (auto& cb : self.midiout_callbacks)
      cb(cnt);

    return 0;
  }
};

int main()
{
  my_app app{};

  for (;;)
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
