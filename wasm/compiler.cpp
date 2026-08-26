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

GeneratedFile *find_file(const std::string &name) {
    for (auto &file : g_files) {
        if (file.name == name) return &file;
    }
    return nullptr;
}

void erase_file(const std::string &name) {
    for (auto it = g_files.begin(); it != g_files.end(); ++it) {
        if (it->name == name) {
            g_files.erase(it);
            return;
        }
    }
}

void replace_all(std::string &text, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool erase_if_block(std::string &text, const std::string &marker) {
    size_t begin = text.find(marker);
    if (begin == std::string::npos) return false;

    size_t open = text.find('{', begin);
    if (open == std::string::npos) return false;

    unsigned depth = 0;
    for (size_t i = open; i < text.size(); ++i) {
        if (text[i] == '{') {
            ++depth;
        } else if (text[i] == '}') {
            if (--depth == 0) {
                size_t end = i + 1;
                while (end < text.size() && (text[end] == '\r' || text[end] == '\n')) ++end;
                text.erase(begin, end - begin);
                return true;
            }
        }
    }
    return false;
}

bool strip_line_containing(std::string &text, const std::string &marker) {
    size_t pos = text.find(marker);
    if (pos == std::string::npos) return false;
    size_t begin = text.rfind('\n', pos);
    begin = (begin == std::string::npos) ? 0 : begin + 1;
    size_t end = text.find('\n', pos);
    end = (end == std::string::npos) ? text.size() : end + 1;
    text.erase(begin, end - begin);
    return true;
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

extern "C" GBMD_WASM_EXPORT int32_t gbrecomp_wasm_prepare_sgdk(size_t rom_size) {
    g_last_error.clear();

    GeneratedFile *main_file = find_file("browserrom.c");
    if (main_file == nullptr) {
        g_last_error = "compile must run before SGDK preparation";
        return -1;
    }

    std::string &text = main_file->content;
    replace_all(text, "#include <stdio.h>\n", "");
    replace_all(text, "#include <stdlib.h>\n", "");
    replace_all(text,
                "ctx->mbc_type = rom_data[0x147];",
                "ctx->mbc_type = gbrt_sgdk_rom_read8(ctx, 0x147u);");

    erase_if_block(text, "if (gbrt_instruction_limit > 0)");
    strip_line_containing(text,
                          "if (ctx->trace_entries_enabled) gbrt_log_trace(ctx, bank, addr);");
    erase_if_block(text, "if (gbrt_trace_enabled)");

    if (text.find("rom_data[0x147]") != std::string::npos) {
        g_last_error = "failed to rewrite direct ROM header access";
        return -2;
    }

    erase_file("browserrom_rom.c");
    erase_file("browserrom_main.c");

    std::string asm_text =
        "/* Browser-generated target ROM blob. */\n"
        ".section .rodata_binf,\"a\"\n"
        ".balign 4\n"
        ".global rom_size\n"
        ".type rom_size,@object\n"
        "rom_size:\n"
        "    .long " + std::to_string(rom_size) + "\n"
        ".size rom_size, 4\n\n"
        ".balign 2\n"
        ".global rom_data\n"
        ".type rom_data,@object\n"
        "rom_data:\n"
        "    .incbin \"browserrom.gb\"\n"
        ".size rom_data, .-rom_data\n";
    add_file("browserrom_rom.s", asm_text);

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
