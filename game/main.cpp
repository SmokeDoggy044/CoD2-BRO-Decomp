// The final, standalone game (route C). Until the decompiled code moves in here it
// only checks that the portable layers build and find the assets on every platform.

#include <cstdio>
#include <optional>
#include <string>

#include "log.h"
#include "paths.h"

int main(int argc, char** argv) {
    std::optional<std::filesystem::path> assets_override;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--assets") assets_override = argv[i + 1];

    const auto assets = platform::resolve_asset_root(assets_override);
    if (!assets) return 1;
    LOG_INFO("game", "assets: %s", assets->string().c_str());
    LOG_INFO("game", "no decompiled game code yet - run cod_bro_loader for now");
    return 0;
}
