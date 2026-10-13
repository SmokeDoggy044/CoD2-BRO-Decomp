// Route-B loader: runs the original default.xbe natively on 32-bit x86 Linux, with
// the Xbox kernel (and, later, the SDK libraries) replaced by our own code.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <pthread.h>
#include <string>

#include "crash.h"
#include "kernel.h"
#include "kernel_thunks.h"
#include "log.h"
#include "memory.h"
#include "patches.h"
#include "paths.h"
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
        "  --memory MB      Xbox memory budget (default 64, like a retail Xbox; devkit: 128)\n",
        program);
}

std::optional<Options> parse_args(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
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
    loader::install_title_patches();

    // The title's initial thread. Like the Xbox's, it has no TLS block (TlsData is
    // null); XAPI only creates TLS for the threads it starts itself.
    loader::ThreadEnv* env = loader::create_thread_env(image.header().pe_stack_commit, 0);
    if (!env || !loader::activate_thread_env(env)) return 1;

    LOG_INFO("loader", "starting title at %08x", image.entry_point());
    // Four zero dwords stand in for arguments in case the entry point reads any.
    const uint32_t args[4] = {};
    loader::run_on_xbox_stack(image.entry_point(), *env, args);
    // The entry point only starts the title's main thread and returns. End this
    // thread alone; the process lives on until the title quits.
    LOG_INFO("loader", "title entry point returned");
    pthread_exit(nullptr);
}
