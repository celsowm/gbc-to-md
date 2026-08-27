#include "recompiler/analyzer.h"
#include "recompiler/codegen/c_emitter.h"
#include "recompiler/ir/ir_builder.h"
#include "recompiler/rom.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
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

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool parse_hex_u32(const std::string &text, uint32_t &value) {
    if (text.empty()) return false;
    try {
        size_t used = 0;
        unsigned long parsed = std::stoul(text, &used, 16);
        if (used != text.size() || parsed > 0xFFFFFFFFul) return false;
        value = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_address(const std::string &token, uint32_t &combined) {
    size_t colon = token.find(':');
    uint32_t bank = 0;
    uint32_t addr = 0;
    if (colon == std::string::npos) {
        if (!parse_hex_u32(token, addr) || addr > 0xFFFFu) return false;
    } else {
        if (!parse_hex_u32(token.substr(0, colon), bank) || bank > 0xFFFFu) return false;
        if (!parse_hex_u32(token.substr(colon + 1), addr) || addr > 0xFFFFu) return false;
    }
    combined = (bank << 16) | addr;
    return true;
}

bool parse_annotations(const char *text,
                       size_t size,
                       std::vector<gbrecomp::AnalysisAnnotation> &out,
                       std::vector<uint32_t> &entry_points,
                       std::string &error) {
    if (text == nullptr || size == 0) return true;

    std::istringstream input(std::string(text, size));
    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        line = trim_copy(line);
        if (line.empty()) continue;

        std::istringstream fields(line);
        std::string kind_text;
        std::string address_text;
        fields >> kind_text >> address_text;
        if (kind_text.empty() || address_text.empty()) {
            error = "invalid annotation at line " + std::to_string(line_number);
            return false;
        }

        std::transform(kind_text.begin(), kind_text.end(), kind_text.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        gbrecomp::AnalysisAnnotation annotation;
        if (!parse_address(address_text, annotation.addr)) {
            error = "invalid annotation address at line " + std::to_string(line_number) + ": " + address_text;
            return false;
        }

        if (kind_text == "function") {
            annotation.kind = gbrecomp::AnalysisAnnotationKind::FUNCTION;
            annotation.size = 1;
            entry_points.push_back(annotation.addr);
        } else if (kind_text == "label") {
            annotation.kind = gbrecomp::AnalysisAnnotationKind::LABEL;
            annotation.size = 1;
        } else if (kind_text == "data") {
            annotation.kind = gbrecomp::AnalysisAnnotationKind::DATA;
            annotation.size = 1;
            std::string size_text;
            if (fields >> size_text) {
                uint32_t parsed_size = 0;
                if (!parse_hex_u32(size_text, parsed_size) || parsed_size == 0) {
                    error = "invalid data annotation size at line " + std::to_string(line_number);
                    return false;
                }
                annotation.size = parsed_size;
            }
        } else {
            error = "unsupported annotation kind at line " + std::to_string(line_number) + ": " + kind_text;
            return false;
        }

        out.push_back(annotation);
    }
    return true;
}

int32_t compile_impl(const uint8_t *data,
                     size_t size,
                     const char *annotations_text,
                     size_t annotations_size) {
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

    if (annotations_text != nullptr && annotations_size != 0) {
        if (!parse_annotations(annotations_text,
                               annotations_size,
                               analysis_options.annotations,
                               analysis_options.entry_points,
                               g_last_error)) {
            return -4;
        }
        // Match the native --reachable-only + --no-scan workflow used for
        // large banked ROMs. Explicit FUNCTION annotations become roots while
        // DATA annotations suppress false code discovery.
        analysis_options.analyze_all_banks = false;
    }

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
}

extern "C" GBMD_WASM_EXPORT int32_t gbrecomp_wasm_compile(const uint8_t *data, size_t size) {
    return compile_impl(data, size, nullptr, 0);
}

extern "C" GBMD_WASM_EXPORT int32_t gbrecomp_wasm_compile_annotated(
    const uint8_t *data,
    size_t size,
    const char *annotations_text,
    size_t annotations_size) {
    return compile_impl(data, size, annotations_text, annotations_size);
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
