// Route-B loader: runs the original default.xbe natively on 32-bit x86 Linux, with
// the Xbox kernel (and, later, the SDK libraries) replaced by our own code.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <unistd.h>
#include <string>

#include "crash.h"
#include "d3d.h"
#include "xinput.h"
#include "dsound.h"
#include "kernel.h"
#include "kernel_thunks.h"
#include "log.h"
#include "memory.h"
#include "patches.h"
#include "paths.h"
#include "script.h"
#include "symbols.h"
#include "thread_env.h"
#include "xbe.h"

namespace fs = std::filesystem;

namespace {

struct Options {
    std::optional<fs::path> assets;
    std::optional<fs::path> xbe;
    std::optional<fs::path> symbols;
    std::optional<fs::path> data;
    platform::LogLevel log_level = platform::LogLevel::Info;
    bool trace_kernel = false;
    uint32_t memory_mb = 64;
    uint32_t scale = 2;
    uint32_t screenshot_interval = 0;
    std::optional<fs::path> screenshot_dir;
    bool silent = false;
    std::string press_script;
    float mouse_sensitivity = 0.11f;
    std::optional<fs::path> diag_dir;
    uint32_t diag_seconds = 10;
};

void print_usage(const char* program) {
    std::printf(
        "usage: %s [options]\n"
        "  --assets DIR     game files (default: COD_BRO_ASSETS, ./assets, or the dev folder)\n"
        "  --xbe FILE       executable to load (default: <assets>/default.xbe)\n"
        "  --symbols FILE   Ghidra symbol export for readable backtraces\n"
        "  --data DIR       where saves and the cache drive go (default: the user data folder)\n"
        "  --debug          debug logging\n"
        "  --trace          log every kernel call (implies --debug)\n"
        "  --silent         no sound output (audio still runs, into a silent device)\n"
        "  --memory MB      Xbox memory budget (default 64, like a retail Xbox; devkit: 128)\n"
        "  --scale N        window size as a multiple of the game's resolution (default 2)\n"
        "  --screenshots N DIR  save every Nth frame as DIR/frame_<n>.bmp (debugging)\n"
        "  --press LIST     scripted presses on port 0, e.g. start@20,a@25.5 (seconds after start;\n"
        "                   buttons: a b x y black white lt rt start back up down left right ls rs)\n"
        "  --diag DIR [S]   diagnostics while playing: DIR/log.txt, a screenshot every S seconds\n"
        "                   (default 10); F12 marks a problem: screenshot, a note typed while the\n"
        "                   game is paused (DIR/marks.txt), draw dump in the log\n"
        "  --mouse-sensitivity DEG  degrees the view turns per mouse count (default 0.11)\n",
        program);
}

std::optional<Options> parse_args(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.size() > 2 && arg[0] == '-' && arg[1] != '-') arg.insert(0, "-");  // -diag works like --diag
        auto value = [&]() -> std::optional<fs::path> {
            if (i + 1 < argc) return fs::path(argv[++i]);
            std::fprintf(stderr, "%s needs a value\n", arg.c_str());
            return std::nullopt;
        };
        if (arg == "--assets") {
            if (!(options.assets = value())) return std::nullopt;
        } else if (arg == "--xbe") {
            if (!(options.xbe = value())) return std::nullopt;
        } else if (arg == "--data") {
            if (!(options.data = value())) return std::nullopt;
        } else if (arg == "--symbols") {
            if (!(options.symbols = value())) return std::nullopt;
        } else if (arg == "--memory") {
            if (i + 1 >= argc || (options.memory_mb = static_cast<uint32_t>(std::atoi(argv[++i]))) < 16 ||
                options.memory_mb > 1024) {
                std::fprintf(stderr, "--memory needs a size in MB (16-1024)\n");
                return std::nullopt;
            }
        } else if (arg == "--scale") {
            if (i + 1 >= argc || (options.scale = static_cast<uint32_t>(std::atoi(argv[++i]))) < 1 ||
                options.scale > 8) {
                std::fprintf(stderr, "--scale needs a number (1-8)\n");
                return std::nullopt;
            }
        } else if (arg == "--screenshots") {
            if (i + 2 >= argc || (options.screenshot_interval = static_cast<uint32_t>(std::atoi(argv[++i]))) < 1) {
                std::fprintf(stderr, "--screenshots needs a frame interval and a folder\n");
                return std::nullopt;
            }
            options.screenshot_dir = fs::path(argv[++i]);
        } else if (arg == "--press") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--press needs a list of button@seconds\n");
                return std::nullopt;
            }
            options.press_script = argv[++i];
        } else if (arg == "--mouse-sensitivity") {
            if (i + 1 >= argc || (options.mouse_sensitivity = std::strtof(argv[++i], nullptr)) <= 0.0f ||
                options.mouse_sensitivity > 10.0f) {
                std::fprintf(stderr, "--mouse-sensitivity needs degrees per count (0-10)\n");
                return std::nullopt;
            }
        } else if (arg == "--diag") {
            if (!(options.diag_dir = value())) return std::nullopt;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                options.diag_seconds = static_cast<uint32_t>(std::atoi(argv[++i]));
                if (options.diag_seconds < 1) {
                    std::fprintf(stderr, "--diag needs seconds >= 1\n");
                    return std::nullopt;
                }
            }
        } else if (arg == "--silent") {
            options.silent = true;
        } else if (arg == "--debug") {
            options.log_level = platform::LogLevel::Debug;
        } else if (arg == "--trace") {
            options.log_level = platform::LogLevel::Trace;
            options.trace_kernel = true;
        } else {
            print_usage(argv[0]);
            return std::nullopt;
        }
    }
    return options;
}

uint32_t current_kthread() {
    const loader::ThreadEnv* env = loader::current_thread_env();
    return env ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(env->kthread)) : 0;
}

uint8_t* current_irql() {
    loader::ThreadEnv* env = loader::current_thread_env();
    return env ? &env->kpcr->irql : nullptr;
}

void attach_kernel_thread() {
    loader::ThreadEnv* env = loader::create_thread_env(0, 0);
    if (!env || !loader::activate_thread_env(env)) LOG_ERROR("loader", "cannot prepare a kernel thread");
}

void call_guest(uint32_t function, const uint32_t* args, uint32_t count) {
    loader::ThreadEnv* env = loader::current_thread_env();
    if (!env) {
        LOG_ERROR("loader", "call to %08x from a thread without an Xbox environment", function);
        return;
    }
    loader::run_on_xbox_stack(function, *env, std::span(args, count));
}

xbox::KernelConfig kernel_config(const Options& options, const fs::path& assets) {
    xbox::KernelConfig config{};
    config.image_file_name = "\\Device\\CdRom0\\default.xbe";
    config.trace_calls = options.trace_kernel;
    config.current_thread = current_kthread;
    config.current_irql = current_irql;
    config.attach_kernel_thread = attach_kernel_thread;
    config.call_guest = call_guest;
    config.memory.physical_bytes = options.memory_mb * 1024 * 1024;
    config.cdrom_root = assets;
    config.data_root = options.data.value_or(platform::user_data_dir());
    return config;
}

}  // namespace

int main(int argc, char** argv) {
    const std::optional<Options> options = parse_args(argc, argv);
    if (!options) return 2;
    platform::log_set_level(options->log_level);
    if (options->diag_dir) {
        fs::create_directories(*options->diag_dir);
        const fs::path log = *options->diag_dir / "log.txt";
        if (!platform::log_set_file(log.string().c_str())) {
            std::fprintf(stderr, "cannot write %s\n", log.string().c_str());
            return 1;
        }
        LOG_INFO("diag", "diagnostics in %s: a screenshot every %u s, F12 marks a problem",
                 options->diag_dir->string().c_str(), options->diag_seconds);
    }

    const auto assets = platform::resolve_asset_root(options->assets);
    if (!assets) return 1;
    LOG_INFO("loader", "assets: %s", assets->string().c_str());

    static loader::XbeImage image;
    if (!image.load(options->xbe.value_or(*assets / "default.xbe"))) return 1;

    static loader::SymbolTable symbols;
    const fs::path symbol_path = options->symbols.value_or(COD_BRO_DEFAULT_SYMBOLS);
    if (symbols.load(symbol_path))
        LOG_INFO("loader", "%zu symbols from %s", symbols.size(), symbol_path.string().c_str());
    else
        LOG_WARN("loader", "no symbols at %s; backtraces will show addresses only", symbol_path.string().c_str());

    loader::install_crash_handler(image, symbols);
    if (!xbox::kernel_init(kernel_config(*options, *assets))) return 1;
    xbox::charge_image_pages(loader::initial_image_pages(image));
    loader::install_kernel_thunks(image);
    loader::install_title_patches({options->mouse_sensitivity});
    audio::configure({options->silent ? audio::OutputConfig::Device::Loopback : audio::OutputConfig::Device::Default});
    render::D3DConfig d3d_config;
    d3d_config.backend.window_scale = options->scale;
    d3d_config.screenshot_interval = options->screenshot_interval;
    if (options->diag_dir) {
        d3d_config.diag_dir = options->diag_dir->string();
        d3d_config.diag_seconds = options->diag_seconds;
        d3d_config.pause_audio = &audio::set_paused;
    }
    if (options->screenshot_dir) {
        fs::create_directories(*options->screenshot_dir);
        d3d_config.screenshot_dir = options->screenshot_dir->string();
    }
    d3d_config.device_pointer = loader::kTitleD3DDevicePointer;
    d3d_config.render_state = loader::kTitleD3DRenderState;
    d3d_config.texture_state = loader::kTitleD3DTextureState;
    render::configure(d3d_config);
    input::configure({loader::kTitleGamepadType});
    if (!options->press_script.empty() && !input::set_script(options->press_script)) return 1;
    if (!loader::install_sdk_replacements(image, symbols)) {
        LOG_ERROR("loader", "SDK replacements need the symbol file (%s)", symbol_path.string().c_str());
        return 1;
    }

    // The title's initial thread. Like the Xbox's, it has no TLS block (TlsData is
    // null); XAPI only creates TLS for the threads it starts itself.
    loader::ThreadEnv* env = loader::create_thread_env(image.header().pe_stack_commit, 0);
    if (!env || !loader::activate_thread_env(env)) return 1;

    LOG_INFO("loader", "starting title at %08x", image.entry_point());
    // Four zero dwords stand in for arguments in case the entry point reads any.
    const uint32_t args[4] = {};
    loader::run_on_xbox_stack(image.entry_point(), *env, args);
    // The entry point only starts the title's main thread and returns. Park this
    // thread; the process lives on until the title quits. It must not exit: once the
    // initial thread is gone, Linux no longer resolves /proc/self/exe, which DXVK
    // and SDL read.
    LOG_INFO("loader", "title entry point returned");
    for (;;) pause();
}
