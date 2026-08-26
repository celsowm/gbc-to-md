#include "recompiler/rom.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define GBMD_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define GBMD_WASM_EXPORT
#endif

extern "C" GBMD_WASM_EXPORT int32_t gbrecomp_wasm_probe(const uint8_t *data, size_t size) {
    if (data == nullptr || size < 0x150u) return -1;

    std::vector<uint8_t> bytes(data, data + size);
    auto rom = gbrecomp::ROM::load_from_buffer(std::move(bytes), "browser.gb");
    if (!rom || !rom->is_valid()) return -2;

    const auto &header = rom->header();
    return (static_cast<int32_t>(header.rom_banks) << 8) |
           static_cast<int32_t>(static_cast<uint8_t>(header.mbc_type));
}
