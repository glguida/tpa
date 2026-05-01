#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "elfio/elfio.hpp"
#include "elfio/elfio_symbols.hpp"
#include "sys_emu.h"
#include "testLog.h"

#include "ltfarm_litecoin_core_vectors.h"
#include "ltfarm_worker_core.h"

namespace {

constexpr uint64_t kDefaultMinions = 0xffu;
constexpr uint64_t kDefaultMaxCycles = 5000000000ull;
constexpr int kThread = 0;

std::optional<uint64_t> find_symbol_addr(const std::string& elf_path,
                                         const std::string& name)
{
    ELFIO::elfio reader;

    if (!reader.load(elf_path))
        return std::nullopt;

    for (const auto* sec : reader.sections) {
        if (!sec)
            continue;
        if (sec->get_type() != SHT_SYMTAB && sec->get_type() != SHT_DYNSYM)
            continue;

        ELFIO::symbol_section_accessor symbols(
            reader, const_cast<ELFIO::section*>(sec));
        for (ELFIO::Elf_Xword i = 0; i < symbols.get_symbols_num(); ++i) {
            std::string sym_name;
            ELFIO::Elf64_Addr value = 0;
            ELFIO::Elf_Xword size = 0;
            unsigned char bind = 0;
            unsigned char type = 0;
            ELFIO::Elf_Half section = 0;
            unsigned char other = 0;

            symbols.get_symbol(i, sym_name, value, size, bind, type, section, other);
            if (sym_name == name)
                return value;
        }
    }

    return std::nullopt;
}

bool hex_to_bytes(const std::string& hex, std::vector<uint8_t>& out)
{
    auto hex_nibble = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        return -1;
    };

    if ((hex.size() & 1u) != 0u)
        return false;

    out.resize(hex.size() / 2u);
    for (size_t i = 0; i < out.size(); ++i) {
        int hi = hex_nibble(hex[i * 2u]);
        int lo = hex_nibble(hex[i * 2u + 1u]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return true;
}

std::string hash_to_canonical_hex(const uint8_t hash[LTFARM_SCRYPT_HASH_BYTES])
{
    std::ostringstream oss;

    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < LTFARM_SCRYPT_HASH_BYTES; ++i) {
        size_t idx = LTFARM_SCRYPT_HASH_BYTES - 1u - i;
        oss << std::setw(2) << static_cast<unsigned>(hash[idx]);
    }

    return oss.str();
}

int run_case(const std::string& elf_path,
             uint64_t input_addr,
             uint64_t result_addr,
             size_t case_idx)
{
    if (case_idx >= kLTFarmLitecoinCoreVectorCount) {
        std::cerr << "case index out of range: " << case_idx << "\n";
        return EXIT_FAILURE;
    }

    const auto& vec = kLTFarmLitecoinCoreVectors[case_idx];
    std::vector<uint8_t> input_bytes;
    ltfarm_job_input_t input{};
    ltfarm_hash_result_t result{};
    size_t lane = 0u;

    if (!hex_to_bytes(vec.input_hex, input_bytes) ||
        input_bytes.size() != LTFARM_SCRYPT_INPUT_BYTES) {
        std::cerr << "invalid input vector at case " << case_idx << "\n";
        return EXIT_FAILURE;
    }

    input.count = LTFARM_SCRYPT_LANES;
    input.reserved = 0u;
    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane)
        std::copy(input_bytes.begin(), input_bytes.end(), input.header[lane]);

    sys_emu_cmd_options opts{};
    opts.elf_files.push_back(elf_path);
    opts.minions_en = kDefaultMinions;
    opts.mins_dis = true;
    opts.max_cycles = kDefaultMaxCycles;
    opts.display_trap_info = true;

    testLog::maxErrors_ = 16u;
    sys_emu emu(opts);
    emu.thread_write_memory(kThread, input_addr, sizeof(input),
                            reinterpret_cast<const uint8_t*>(&input));

    int rc = emu.main_internal();
    emu.thread_read_memory(kThread, result_addr, sizeof(result),
                           reinterpret_cast<uint8_t*>(&result));

    if (result.magic != LTFARM_SCRYPT_RESULT_MAGIC ||
        result.ready != 1u ||
        result.status != LTFARM_WORKER_STATUS_DONE) {
        if (rc != EXIT_SUCCESS)
            std::cerr << "emulation failed with code " << rc << "\n";
        std::cerr << "ltfarm scrypt result was not produced for case "
                  << case_idx << "\n";
        return EXIT_FAILURE;
    }

    if (result.count != LTFARM_SCRYPT_LANES) {
        std::cerr << "case=" << case_idx << " unexpected batch count="
                  << result.count << "\n";
        return EXIT_FAILURE;
    }

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        std::string actual = hash_to_canonical_hex(result.hash[lane]);
        if (actual != vec.expected_hex) {
            std::cerr << "case=" << case_idx << " lane=" << lane << " mismatch\n"
                      << "  expected=" << vec.expected_hex << "\n"
                      << "  actual  =" << actual << "\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "case=" << case_idx
              << " cycles=" << emu.get_emu_cycle()
              << " cycles_per_hash="
              << (static_cast<double>(emu.get_emu_cycle()) /
                  static_cast<double>(LTFARM_SCRYPT_LANES))
              << " hash=" << hash_to_canonical_hex(result.hash[0])
              << " ok\n";
    return EXIT_SUCCESS;
}

int run_packed_cases(const std::string& elf_path,
                     uint64_t input_addr,
                     uint64_t result_addr)
{
    ltfarm_job_input_t input{};
    ltfarm_hash_result_t result{};
    std::vector<uint8_t> input_bytes;
    size_t lane = 0u;

    input.count = LTFARM_SCRYPT_LANES;
    input.reserved = 0u;
    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        const auto& vec =
            kLTFarmLitecoinCoreVectors[lane % kLTFarmLitecoinCoreVectorCount];
        if (!hex_to_bytes(vec.input_hex, input_bytes) ||
            input_bytes.size() != LTFARM_SCRYPT_INPUT_BYTES) {
            std::cerr << "invalid packed input vector at lane " << lane << "\n";
            return EXIT_FAILURE;
        }
        std::copy(input_bytes.begin(), input_bytes.end(), input.header[lane]);
    }

    sys_emu_cmd_options opts{};
    opts.elf_files.push_back(elf_path);
    opts.minions_en = kDefaultMinions;
    opts.mins_dis = true;
    opts.max_cycles = kDefaultMaxCycles;
    opts.display_trap_info = true;

    testLog::maxErrors_ = 16u;
    sys_emu emu(opts);
    emu.thread_write_memory(kThread, input_addr, sizeof(input),
                            reinterpret_cast<const uint8_t*>(&input));

    int rc = emu.main_internal();
    emu.thread_read_memory(kThread, result_addr, sizeof(result),
                           reinterpret_cast<uint8_t*>(&result));

    if (result.magic != LTFARM_SCRYPT_RESULT_MAGIC ||
        result.ready != 1u ||
        result.status != LTFARM_WORKER_STATUS_DONE ||
        result.count != LTFARM_SCRYPT_LANES) {
        if (rc != EXIT_SUCCESS)
            std::cerr << "emulation failed with code " << rc << "\n";
        std::cerr << "ltfarm packed scrypt result was not produced\n";
        return EXIT_FAILURE;
    }

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        const auto& vec =
            kLTFarmLitecoinCoreVectors[lane % kLTFarmLitecoinCoreVectorCount];
        std::string actual = hash_to_canonical_hex(result.hash[lane]);
        if (actual != vec.expected_hex) {
            std::cerr << "packed lane=" << lane << " mismatch\n"
                      << "  expected=" << vec.expected_hex << "\n"
                      << "  actual  =" << actual << "\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "packed"
              << " cycles=" << emu.get_emu_cycle()
              << " hashes=" << LTFARM_SCRYPT_LANES
              << " cycles_per_hash="
              << (static_cast<double>(emu.get_emu_cycle()) /
                  static_cast<double>(LTFARM_SCRYPT_LANES))
              << " ok\n";
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path exe_path = std::filesystem::absolute(argv[0]);
    std::string elf_path = (exe_path.parent_path() / "tpa_ltfarm_scrypt_core.elf").string();
    bool run_all = false;
    bool run_packed = false;
    size_t case_idx = 0u;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--elf" && i + 1 < argc) {
            elf_path = argv[++i];
        } else if (arg == "--case" && i + 1 < argc) {
            case_idx = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--all") {
            run_all = true;
        } else if (arg == "--packed") {
            run_packed = true;
        } else if (arg == "--help") {
            std::cout
                << "Usage: ltfarm_scrypt_core_host [--elf <path>] [--case <n>] [--all] [--packed]\n"
                << "Default: run official Litecoin vector 0 only.\n";
            return EXIT_SUCCESS;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            return EXIT_FAILURE;
        }
    }

    auto input_addr = find_symbol_addr(elf_path, "ltfarm_job_input");
    auto result_addr = find_symbol_addr(elf_path, "ltfarm_hash_result");
    if (!input_addr || !result_addr) {
        std::cerr << "failed to resolve ltfarm graph symbols in "
                  << elf_path << "\n";
        return EXIT_FAILURE;
    }

    if (run_packed)
        return run_packed_cases(elf_path, *input_addr, *result_addr);

    if (run_all) {
        for (size_t i = 0; i < kLTFarmLitecoinCoreVectorCount; ++i) {
            int rc = run_case(elf_path, *input_addr, *result_addr, i);
            if (rc != EXIT_SUCCESS)
                return rc;
        }
        std::cout << "all " << kLTFarmLitecoinCoreVectorCount
                  << " ltfarm scrypt vectors passed\n";
        return EXIT_SUCCESS;
    }

    return run_case(elf_path, *input_addr, *result_addr, case_idx);
}
