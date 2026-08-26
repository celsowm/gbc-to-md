#include "recompiler/analyzer.h"
#include "recompiler/codegen/c_emitter.h"
#include "recompiler/ir/ir_builder.h"
#include "recompiler/rom.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define GBMD_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define GBMD_WASM_EXPORT
#endif

namespace {
struct GeneratedFile {
    std::string name;
    std::string content;
};

std::vector<GeneratedFile> g_files;
std::string g_last_error;

void add_file(const std::string &name, const std::string &content) {
    if (!name.empty() && !content.empty()) g_files.push_back({name, content});
}
}

extern "C" GBMD_WASM_EXPORT int32_t gbrecomp_wasm_compile(const uint8_t *data, size_t size) {
    g_files.clear();
    g_last_error.clear();

    if (data == nullptr || size < 0x150u) {
        g_last_error = "ROM buffer is too small";
        return -1;
    }

    std::vector<uint8_t> bytes(data, data + size);
    auto rom = gbrecomp::ROM::load_from_buffer(std::move(bytes), "browser.gb");
    if (!rom || !rom->is_valid()) {
        g_last_error = rom ? rom->error() : "ROM parser rejected buffer";
        return -2;
    }

    gbrecomp::AnalyzerOptions analysis_options;
    analysis_options.aggressive_scan = false;
    analysis_options.verbose = false;
    analysis_options.trace_log = false;

    auto analysis = gbrecomp::analyze(*rom, analysis_options);

    gbrecomp::ir::BuilderOptions builder_options;
    builder_options.emit_source_locations = true;
    builder_options.emit_comments = false;
    gbrecomp::ir::IRBuilder builder(builder_options);
    auto program = builder.build(analysis, "browserrom");

    gbrecomp::codegen::GeneratorOptions generator_options;
    generator_options.output_prefix = "browserrom";
    generator_options.emit_comments = false;
    generator_options.emit_address_comments = true;
    generator_options.emit_cmake = false;
    generator_options.parallel_codegen_jobs = 1;

    auto output = gbrecomp::codegen::generate_output(
        program, rom->data(), rom->size(), generator_options);

    add_file(output.header_file, output.header_content);
    add_file(output.source_file, output.source_content);
    add_file(output.rom_data_file, output.rom_data_content);
    add_file(output.main_file, output.main_content);
    add_file(output.cmake_file, output.cmake_content);
    for (const auto &extra : output.extra_files) add_file(extra.filename, extra.content);

    if (g_files.empty()) {
        g_last_error = "code generator returned no files";
        return -3;
    }
    return static_cast<int32_t>(g_files.size());
}

extern "C" GBMD_WASM_EXPORT int32_t gbrecomp_wasm_file_count(void) {
    return static_cast<int32_t>(g_files.size());
}

extern "C" GBMD_WASM_EXPORT const char *gbrecomp_wasm_file_name_ptr(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= g_files.size()) return nullptr;
    return g_files[static_cast<size_t>(index)].name.c_str();
}

extern "C" GBMD_WASM_EXPORT size_t gbrecomp_wasm_file_name_size(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= g_files.size()) return 0;
    return g_files[static_cast<size_t>(index)].name.size();
}

extern "C" GBMD_WASM_EXPORT const uint8_t *gbrecomp_wasm_file_data_ptr(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= g_files.size()) return nullptr;
    return reinterpret_cast<const uint8_t *>(g_files[static_cast<size_t>(index)].content.data());
}

extern "C" GBMD_WASM_EXPORT size_t gbrecomp_wasm_file_data_size(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= g_files.size()) return 0;
    return g_files[static_cast<size_t>(index)].content.size();
}

extern "C" GBMD_WASM_EXPORT const char *gbrecomp_wasm_error_ptr(void) {
    return g_last_error.c_str();
}

extern "C" GBMD_WASM_EXPORT size_t gbrecomp_wasm_error_size(void) {
    return g_last_error.size();
}
