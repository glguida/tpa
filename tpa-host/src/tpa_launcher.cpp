#include <device-layer/IDeviceLayer.h>
#include <hostUtils/logging/Logger.h>
#include <runtime/DeviceLayerFake.h>
#include <runtime/IRuntime.h>
#include <sw-sysemu/SysEmuOptions.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

enum class Mode {
  Pcie,
  Sysemu,
  Fake,
};

struct Options {
  std::filesystem::path kernel_path;
  Mode mode = Mode::Sysemu;
  uint64_t shire_mask = 0xffffffffULL;
  uint64_t sysemu_shires_mask = 0x1FFFFFFFFULL;
  int timeout_seconds = 0;
  int launches = 1;
  std::filesystem::path sysemu_log;
  std::vector<std::string> sysemu_options;
};

void usage(const char* argv0) {
  std::cerr
      << "Usage: " << argv0 << " --kernel <elf> [options]\n\n"
      << "Options:\n"
      << "  -k, --kernel, --kernel_path <elf>    TPA ET-SoC-1 ELF to load\n"
      << "      --mode=sysemu|pcie|fake          Runtime device layer, default sysemu\n"
      << "      --shire-mask <mask>              Kernel launch shire mask, default 0xffffffff\n"
      << "      --sysemu-shires-mask <mask>      Enabled sysemu shires, default 0x1FFFFFFFF\n"
      << "      --sysemu-log <path>              Sysemu log path, default ./sysemu.log\n"
      << "      --simulator_params <args>        Extra sysemu args, e.g. \"-l -lt 0\"\n"
      << "      --sysemu-option <arg>            One raw sysemu arg; may be repeated\n"
      << "      --timeout <seconds>              Per-launch timeout, default 300 for sysemu, 30 otherwise\n"
      << "      --launches <count>               Number of launches, default 1\n"
      << "  -h, --help                           Show this help\n";
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string_view option_value(std::string_view arg, std::string_view name) {
  if (arg.size() <= name.size() + 1) {
    return {};
  }
  if (!starts_with(arg, name) || arg[name.size()] != '=') {
    return {};
  }
  return arg.substr(name.size() + 1);
}

std::string consume_value(int& index, int argc, char** argv, std::string_view option) {
  auto arg = std::string_view(argv[index]);
  if (auto value = option_value(arg, option); !value.empty()) {
    return std::string(value);
  }
  if (++index >= argc) {
    throw std::runtime_error(std::string(option) + " needs a value");
  }
  return argv[index];
}

uint64_t parse_u64(const std::string& value, std::string_view option) {
  size_t parsed = 0;
  auto result = std::stoull(value, &parsed, 0);
  if (parsed != value.size()) {
    throw std::runtime_error(std::string(option) + " has invalid numeric value: " + value);
  }
  return result;
}

int parse_positive_int(const std::string& value, std::string_view option) {
  auto parsed = parse_u64(value, option);
  if (parsed == 0 || parsed > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error(std::string(option) + " must be a positive integer");
  }
  return static_cast<int>(parsed);
}

Mode parse_mode(std::string_view value) {
  if (value == "pcie" || value == "silicon") {
    return Mode::Pcie;
  }
  if (value == "sysemu") {
    return Mode::Sysemu;
  }
  if (value == "fake") {
    return Mode::Fake;
  }
  throw std::runtime_error("unsupported --mode: " + std::string(value));
}

void append_split_options(std::vector<std::string>& out, const std::string& args) {
  std::istringstream stream(args);
  std::string arg;

  while (stream >> arg) {
    out.push_back(arg);
  }
}

const char* mode_name(Mode mode) {
  switch (mode) {
  case Mode::Pcie:
    return "pcie";
  case Mode::Sysemu:
    return "sysemu";
  case Mode::Fake:
    return "fake";
  }
  return "unknown";
}

void print_stream_errors(const std::vector<rt::StreamError>& errors) {
  for (const auto& error : errors) {
    std::cerr << error.getString() << "\n";
  }
}

void abort_stream(rt::IRuntime& runtime, rt::StreamId stream, std::chrono::seconds timeout,
                  std::string_view reason) {
  auto abort_event = runtime.abortStream(stream);
  if (!runtime.waitForEvent(abort_event, timeout)) {
    throw std::runtime_error(std::string(reason) + " and abort did not complete");
  }
  if (!runtime.waitForStream(stream, timeout)) {
    throw std::runtime_error(std::string(reason) + " and stream did not drain after abort");
  }
}

Options parse_args(int argc, char** argv) {
  Options opts;

  for (int i = 1; i < argc; ++i) {
    auto arg = std::string_view(argv[i]);

    if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      std::exit(0);
    }
    if (arg == "-k" || arg == "--kernel" || arg == "--kernel_path" || starts_with(arg, "--kernel=") ||
        starts_with(arg, "--kernel_path=")) {
      auto opt_name = starts_with(arg, "--kernel_path") ? "--kernel_path" : (starts_with(arg, "--kernel") ? "--kernel" : "-k");
      opts.kernel_path = consume_value(i, argc, argv, opt_name);
      continue;
    }
    if (arg == "--mode" || starts_with(arg, "--mode=")) {
      opts.mode = parse_mode(consume_value(i, argc, argv, "--mode"));
      continue;
    }
    if (arg == "--shire-mask" || arg == "--shire_mask" || starts_with(arg, "--shire-mask=") ||
        starts_with(arg, "--shire_mask=")) {
      auto opt_name = starts_with(arg, "--shire_mask") ? "--shire_mask" : "--shire-mask";
      opts.shire_mask = parse_u64(consume_value(i, argc, argv, opt_name), opt_name);
      continue;
    }
    if (arg == "--sysemu-shires-mask" || starts_with(arg, "--sysemu-shires-mask=")) {
      opts.sysemu_shires_mask =
          parse_u64(consume_value(i, argc, argv, "--sysemu-shires-mask"), "--sysemu-shires-mask");
      continue;
    }
    if (arg == "--sysemu-log" || starts_with(arg, "--sysemu-log=")) {
      opts.sysemu_log = consume_value(i, argc, argv, "--sysemu-log");
      continue;
    }
    if (arg == "--simulator_params" || arg == "--simulator-params" || starts_with(arg, "--simulator_params=") ||
        starts_with(arg, "--simulator-params=")) {
      auto opt_name = starts_with(arg, "--simulator-params") ? "--simulator-params" : "--simulator_params";
      append_split_options(opts.sysemu_options, consume_value(i, argc, argv, opt_name));
      continue;
    }
    if (arg == "--sysemu-option" || starts_with(arg, "--sysemu-option=")) {
      opts.sysemu_options.push_back(consume_value(i, argc, argv, "--sysemu-option"));
      continue;
    }
    if (arg == "--timeout" || starts_with(arg, "--timeout=")) {
      opts.timeout_seconds = parse_positive_int(consume_value(i, argc, argv, "--timeout"), "--timeout");
      continue;
    }
    if (arg == "--launches" || starts_with(arg, "--launches=")) {
      opts.launches = parse_positive_int(consume_value(i, argc, argv, "--launches"), "--launches");
      continue;
    }

    throw std::runtime_error("unknown option: " + std::string(arg));
  }

  if (opts.kernel_path.empty()) {
    throw std::runtime_error("missing required --kernel <elf>");
  }
  if (opts.timeout_seconds == 0) {
    opts.timeout_seconds = (opts.mode == Mode::Sysemu) ? 300 : 30;
  }

  return opts;
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  auto file = std::ifstream(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("cannot open kernel ELF: " + path.string());
  }

  file.seekg(0, std::ios::end);
  const auto size = file.tellg();
  if (size < 0) {
    throw std::runtime_error("cannot determine kernel ELF size: " + path.string());
  }
  file.seekg(0, std::ios::beg);

  std::vector<std::byte> bytes(static_cast<size_t>(size));
  file.read(reinterpret_cast<char*>(bytes.data()), size);
  if (!file) {
    throw std::runtime_error("cannot read complete kernel ELF: " + path.string());
  }

  return bytes;
}

void set_if_exists(std::string& dest, const char* path) {
  if (path != nullptr && path[0] != '\0' && std::filesystem::exists(path)) {
    dest = path;
  }
}

emu::SysEmuOptions make_sysemu_options(const Options& opts) {
  emu::SysEmuOptions sysemu;

  set_if_exists(sysemu.bootromTrampolineToBL2ElfPath, TPA_BOOTROM_TRAMPOLINE_TO_BL2_ELF);
  set_if_exists(sysemu.spBL2ElfPath, TPA_BL2_ELF);
  set_if_exists(sysemu.machineMinionElfPath, TPA_MACHINE_MINION_ELF);
  set_if_exists(sysemu.masterMinionElfPath, TPA_MASTER_MINION_ELF);
  set_if_exists(sysemu.workerMinionElfPath, TPA_WORKER_MINION_ELF);

  if (std::filesystem::exists(TPA_SYSEMU_EXECUTABLE)) {
    sysemu.executablePath = TPA_SYSEMU_EXECUTABLE;
  } else {
    sysemu.executablePath = "sys_emu";
  }

  sysemu.runDir = std::filesystem::current_path();
  sysemu.maxCycles = std::numeric_limits<uint64_t>::max();
  sysemu.minionShiresMask = opts.sysemu_shires_mask;
  if (!opts.sysemu_log.empty()) {
    sysemu.logFile = opts.sysemu_log.string();
  }
  sysemu.additionalOptions = opts.sysemu_options;
  sysemu.puUart0Path = sysemu.runDir + "/pu_uart0_tx.log";
  sysemu.puUart1Path = sysemu.runDir + "/pu_uart1_tx.log";
  sysemu.spUart0Path = sysemu.runDir + "/spio_uart0_tx.log";
  sysemu.spUart1Path = sysemu.runDir + "/spio_uart1_tx.log";
  sysemu.startGdb = false;

  return sysemu;
}

std::shared_ptr<dev::IDeviceLayer> make_device_layer(const Options& opts, rt::Options& rt_opts) {
  switch (opts.mode) {
  case Mode::Pcie:
    return std::shared_ptr<dev::IDeviceLayer>(dev::IDeviceLayer::createPcieDeviceLayer());
  case Mode::Sysemu:
    return std::shared_ptr<dev::IDeviceLayer>(dev::IDeviceLayer::createSysEmuDeviceLayer(make_sysemu_options(opts)));
  case Mode::Fake:
    rt_opts.checkDeviceApiVersion_ = false;
    return std::make_shared<dev::DeviceLayerFake>();
  }
  throw std::runtime_error("unsupported device mode");
}

int run(const Options& opts) {
  logging::LoggerDefault logger;
  auto kernel = read_file(opts.kernel_path);

  std::cout << "TPA launcher: mode=" << mode_name(opts.mode) << " kernel=" << opts.kernel_path
            << " shire_mask=0x" << std::hex << opts.shire_mask << std::dec << "\n";
  if (opts.mode == Mode::Sysemu) {
    std::cout << "TPA launcher: sysemu firmware"
              << " bootrom=" << TPA_BOOTROM_TRAMPOLINE_TO_BL2_ELF
              << " bl2=" << TPA_BL2_ELF
              << " master=" << TPA_MASTER_MINION_ELF
              << " machine=" << TPA_MACHINE_MINION_ELF
              << " worker=" << TPA_WORKER_MINION_ELF << "\n";
    std::cout << "TPA launcher: sysemu log="
              << (opts.sysemu_log.empty() ? std::filesystem::path("sysemu.log") : opts.sysemu_log);
    if (!opts.sysemu_options.empty()) {
      std::cout << " options=";
      for (const auto& opt : opts.sysemu_options) {
        std::cout << opt << " ";
      }
    }
    std::cout << "\n";
  }

  auto rt_opts = rt::getDefaultOptions();
  auto device_layer = make_device_layer(opts, rt_opts);
  auto runtime = rt::IRuntime::create(device_layer, rt_opts);
  auto devices = runtime->getDevices();
  if (devices.empty()) {
    throw std::runtime_error("runtime returned no devices");
  }

  runtime->setOnStreamErrorsCallback(nullptr);

  auto stream = runtime->createStream(devices.front());
  auto load = runtime->loadCode(stream, kernel.data(), kernel.size());
  const auto timeout = std::chrono::seconds(opts.timeout_seconds);
  const auto abort_timeout = std::chrono::seconds(10);
  runtime->waitForEvent(load.event_, std::chrono::hours(24));

  auto load_errors = runtime->retrieveStreamErrors(stream);
  if (!load_errors.empty()) {
    print_stream_errors(load_errors);
    runtime->unloadCode(load.kernel_);
    runtime->destroyStream(stream);
    return 1;
  }

  std::array<std::byte, 1> no_args{};
  for (int launch = 0; launch < opts.launches; ++launch) {
    runtime->kernelLaunch(stream, load.kernel_, no_args.data(), 0, opts.shire_mask);
    if (!runtime->waitForStream(stream, timeout)) {
      auto errors = runtime->retrieveStreamErrors(stream);
      if (!errors.empty()) {
        print_stream_errors(errors);
        runtime->unloadCode(load.kernel_);
        runtime->destroyStream(stream);
        return 1;
      }

      abort_stream(*runtime, stream, abort_timeout, "timed out waiting for kernel launch completion");
      runtime->destroyStream(stream);
      throw std::runtime_error("timed out waiting for kernel launch completion; stream was aborted");
    }

    auto errors = runtime->retrieveStreamErrors(stream);
    if (!errors.empty()) {
      print_stream_errors(errors);
      runtime->unloadCode(load.kernel_);
      runtime->destroyStream(stream);
      return 1;
    }
  }

  runtime->waitForStream(stream, timeout);
  auto errors = runtime->retrieveStreamErrors(stream);
  runtime->unloadCode(load.kernel_);
  runtime->destroyStream(stream);

  if (!errors.empty()) {
    print_stream_errors(errors);
    return 1;
  }

  std::cout << "TPA launcher: completed " << opts.launches << " launch(es)\n";
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  try {
    return run(parse_args(argc, argv));
  } catch (const std::exception& e) {
    std::cerr << "tpa_launcher: " << e.what() << "\n";
    usage(argv[0]);
    return 2;
  }
}
