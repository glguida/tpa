#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "elfio/elfio.hpp"
#include "elfio/elfio_symbols.hpp"
#include "sys_emu.h"
#include "testLog.h"

#include "yolov5n_demo.h"

namespace {

constexpr uint64_t kDefaultMinions = 0xffu;
constexpr uint64_t kDefaultMaxCycles = 1200000000ull;
constexpr int kThread = 0;
constexpr float kDefaultConfThresh = 0.25f;
constexpr float kDefaultIouThresh = 0.45f;
constexpr size_t kDefaultTopK = 20u;
constexpr float kImageExtent = 640.0f;
constexpr size_t kRegMax = 16u;
constexpr float kInputScale = 7.87401152e-03f;

constexpr std::array<const char*, 80> kCocoClassNames = {{
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train",
    "truck", "boat", "traffic light", "fire hydrant", "stop sign",
    "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep",
    "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella",
    "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard",
    "sports ball", "kite", "baseball bat", "baseball glove", "skateboard",
    "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork",
    "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
    "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
    "couch", "potted plant", "bed", "dining table", "toilet", "tv",
    "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave",
    "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase",
    "scissors", "teddy bear", "hair drier", "toothbrush",
}};

struct detection_t {
    float x1;
    float y1;
    float x2;
    float y2;
    float score;
    int cls;
    uint16_t stride;
};

struct head_debug_entry_t {
    size_t pix;
    int cls;
    int8_t raw;
    float logit;
    float score;
};

std::optional<uint64_t> find_symbol_addr(const std::string& elf_path,
                                         const std::string& name)
{
    ELFIO::elfio reader;

    if (!reader.load(elf_path))
        return std::nullopt;

    for (const auto* sec : reader.sections) {
        if (!sec)
            continue;
        if (sec->get_type() != SHT_SYMTAB &&
            sec->get_type() != SHT_DYNSYM)
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

bool load_exact_file(const std::string& path, std::vector<uint8_t>& out)
{
    std::ifstream in(path, std::ios::binary);

    if (!in)
        return false;

    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool load_ppm_p6(const std::string& path, std::vector<uint8_t>& out)
{
    std::ifstream in(path, std::ios::binary);
    std::string magic;
    int width = 0;
    int height = 0;
    int maxval = 0;

    if (!in)
        return false;

    in >> magic;
    if (magic != "P6")
        return false;

    auto skip_ws_comments = [&]() {
        in >> std::ws;
        while (in.peek() == '#') {
            std::string line;
            std::getline(in, line);
            in >> std::ws;
        }
    };

    skip_ws_comments();
    in >> width;
    skip_ws_comments();
    in >> height;
    skip_ws_comments();
    in >> maxval;
    in.get();

    if (!in || width <= 0 || height <= 0 || maxval != 255)
        return false;

    out.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);
    in.read(reinterpret_cast<char*>(out.data()),
            static_cast<std::streamsize>(out.size()));
    return in.good() || in.eof();
}

std::vector<int8_t> quantize_input_bytes(const std::vector<uint8_t>& src)
{
    std::vector<int8_t> out(src.size(), 0);

    for (size_t i = 0; i < src.size(); ++i) {
        float real = static_cast<float>(src[i]) / 255.0f;
        long q = std::lround(real / kInputScale);
        q = std::clamp(q, 0l, 127l);
        out[i] = static_cast<int8_t>(q);
    }

    return out;
}

std::vector<int8_t> load_image_bytes(const std::string& path)
{
    std::vector<uint8_t> bytes;

    if (path.empty()) {
        return std::vector<int8_t>(YV5N_DEMO_INPUT_BYTES, 0);
    }

    if (load_ppm_p6(path, bytes) && bytes.size() == YV5N_DEMO_INPUT_BYTES)
        return quantize_input_bytes(bytes);

    bytes.clear();
    if (load_exact_file(path, bytes) && bytes.size() == YV5N_DEMO_INPUT_BYTES)
        return quantize_input_bytes(bytes);

    throw std::runtime_error("input image must be a P6 PPM or raw 640x640x3 byte file");
}

float sigmoid(float x)
{
    if (x >= 0.0f) {
        float z = std::exp(-x);
        return 1.0f / (1.0f + z);
    }

    float z = std::exp(x);
    return z / (1.0f + z);
}

std::vector<int8_t> read_tensor(sys_emu& emu, const yv5n_demo_tensor_ref_t& ref)
{
    std::vector<int8_t> out(ref.len);

    if (ref.len)
        emu.thread_read_memory(kThread, ref.addr, ref.len,
                               reinterpret_cast<uint8_t*>(out.data()));

    return out;
}

std::vector<head_debug_entry_t> collect_head_debug(const yv5n_demo_tensor_ref_t& cls_ref,
                                                   const std::vector<int8_t>& cls,
                                                   size_t topn)
{
    std::vector<head_debug_entry_t> top;
    const size_t nr_pix = static_cast<size_t>(cls_ref.h) * static_cast<size_t>(cls_ref.w);

    if (cls.size() != cls_ref.len)
        throw std::runtime_error("demo cls tensor size mismatch");

    top.reserve(topn);
    for (size_t pix = 0; pix < nr_pix; ++pix) {
        const int8_t* cls_ptr = cls.data() + pix * cls_ref.c;
        int best_cls = -1;
        int8_t best_raw = INT8_MIN;
        float best_logit = -INFINITY;
        float best_score = 0.0f;

        for (size_t c = 0; c < cls_ref.c; ++c) {
            int8_t raw = cls_ptr[c];
            float logit = static_cast<float>(raw) * cls_ref.scale;
            float score = sigmoid(logit);
            if (score > best_score) {
                best_cls = static_cast<int>(c);
                best_raw = raw;
                best_logit = logit;
                best_score = score;
            }
        }

        if (best_cls < 0)
            continue;

        head_debug_entry_t entry = {
            .pix = pix,
            .cls = best_cls,
            .raw = best_raw,
            .logit = best_logit,
            .score = best_score,
        };

        auto it = std::upper_bound(
            top.begin(), top.end(), entry.score,
            [](float score, const head_debug_entry_t& rhs) {
                return score > rhs.score;
            });
        if (it != top.end() || top.size() < topn)
            top.insert(it, entry);
        if (top.size() > topn)
            top.pop_back();
    }

    return top;
}

void print_head_debug(const char* name,
                      const yv5n_demo_tensor_ref_t& cls_ref,
                      const std::vector<int8_t>& cls)
{
    auto top = collect_head_debug(cls_ref, cls, 5u);

    std::cout << "head=" << name
              << " cls_scale=" << cls_ref.scale
              << " stride=" << cls_ref.stride
              << " top_scores=" << top.size()
              << "\n";

    for (size_t i = 0; i < top.size(); ++i) {
        size_t gy = top[i].pix / cls_ref.w;
        size_t gx = top[i].pix % cls_ref.w;
        const char* cls_name = "unknown";
        if (top[i].cls >= 0 && static_cast<size_t>(top[i].cls) < kCocoClassNames.size())
            cls_name = kCocoClassNames[static_cast<size_t>(top[i].cls)];

        std::cout << "  "
                  << i
                  << ": pix=(" << gx << "," << gy << ")"
                  << " cls=" << top[i].cls
                  << " name=" << cls_name
                  << " raw=" << static_cast<int>(top[i].raw)
                  << " logit=" << top[i].logit
                  << " score=" << top[i].score
                  << "\n";
    }
}

std::vector<detection_t> decode_head(const yv5n_demo_tensor_ref_t& box_ref,
                                     const std::vector<int8_t>& box,
                                     const yv5n_demo_tensor_ref_t& cls_ref,
                                     const std::vector<int8_t>& cls,
                                     float conf_thresh)
{
    std::vector<detection_t> out;
    const size_t nr_pix = static_cast<size_t>(box_ref.h) * static_cast<size_t>(box_ref.w);

    if (box_ref.c != YV5N_DEMO_BOX_C || cls_ref.c != YV5N_DEMO_CLS_C)
        throw std::runtime_error("unexpected demo tensor channel count");
    if (box.size() != box_ref.len || cls.size() != cls_ref.len)
        throw std::runtime_error("demo tensor size mismatch");

    out.reserve(nr_pix / 4u);
    for (size_t pix = 0; pix < nr_pix; ++pix) {
        const int8_t* cls_ptr = cls.data() + pix * cls_ref.c;
        int best_cls = -1;
        float best_score = 0.0f;

        for (size_t c = 0; c < cls_ref.c; ++c) {
            float logit = static_cast<float>(cls_ptr[c]) * cls_ref.scale;
            float score = sigmoid(logit);
            if (score > best_score) {
                best_score = score;
                best_cls = static_cast<int>(c);
            }
        }

        if (best_cls < 0 || best_score < conf_thresh)
            continue;

        const int8_t* box_ptr = box.data() + pix * box_ref.c;
        float dist[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        for (size_t side = 0; side < 4u; ++side) {
            std::array<float, kRegMax> logits{};
            float max_logit = -INFINITY;

            for (size_t i = 0; i < kRegMax; ++i) {
                float v = static_cast<float>(box_ptr[side * kRegMax + i]) * box_ref.scale;
                logits[i] = v;
                max_logit = std::max(max_logit, v);
            }

            float denom = 0.0f;
            for (size_t i = 0; i < kRegMax; ++i) {
                logits[i] = std::exp(logits[i] - max_logit);
                denom += logits[i];
            }

            if (denom <= 0.0f)
                continue;

            float expv = 0.0f;
            for (size_t i = 0; i < kRegMax; ++i)
                expv += static_cast<float>(i) * (logits[i] / denom);
            dist[side] = expv * static_cast<float>(box_ref.stride);
        }

        size_t gy = pix / box_ref.w;
        size_t gx = pix % box_ref.w;
        float cx = (static_cast<float>(gx) + 0.5f) * static_cast<float>(box_ref.stride);
        float cy = (static_cast<float>(gy) + 0.5f) * static_cast<float>(box_ref.stride);

        detection_t det = {
            .x1 = std::clamp(cx - dist[0], 0.0f, kImageExtent),
            .y1 = std::clamp(cy - dist[1], 0.0f, kImageExtent),
            .x2 = std::clamp(cx + dist[2], 0.0f, kImageExtent),
            .y2 = std::clamp(cy + dist[3], 0.0f, kImageExtent),
            .score = best_score,
            .cls = best_cls,
            .stride = box_ref.stride,
        };

        if (det.x2 > det.x1 && det.y2 > det.y1)
            out.push_back(det);
    }

    return out;
}

float iou(const detection_t& a, const detection_t& b)
{
    float ix1 = std::max(a.x1, b.x1);
    float iy1 = std::max(a.y1, b.y1);
    float ix2 = std::min(a.x2, b.x2);
    float iy2 = std::min(a.y2, b.y2);
    float iw = std::max(0.0f, ix2 - ix1);
    float ih = std::max(0.0f, iy2 - iy1);
    float inter = iw * ih;
    float area_a = std::max(0.0f, a.x2 - a.x1) * std::max(0.0f, a.y2 - a.y1);
    float area_b = std::max(0.0f, b.x2 - b.x1) * std::max(0.0f, b.y2 - b.y1);
    float denom = area_a + area_b - inter;

    if (denom <= 0.0f)
        return 0.0f;

    return inter / denom;
}

std::vector<detection_t> nms(std::vector<detection_t> dets, float iou_thresh, size_t topk)
{
    std::sort(dets.begin(), dets.end(),
              [](const detection_t& a, const detection_t& b) {
                  return a.score > b.score;
              });

    std::vector<detection_t> keep;
    std::vector<uint8_t> dead(dets.size(), 0u);

    for (size_t i = 0; i < dets.size(); ++i) {
        if (dead[i])
            continue;

        keep.push_back(dets[i]);
        if (keep.size() >= topk)
            break;

        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (dead[j] || dets[i].cls != dets[j].cls)
                continue;
            if (iou(dets[i], dets[j]) > iou_thresh)
                dead[j] = 1u;
        }
    }

    return keep;
}

void print_detection(size_t idx, const detection_t& det)
{
    const char* cls_name = "unknown";

    if (det.cls >= 0 && static_cast<size_t>(det.cls) < kCocoClassNames.size())
        cls_name = kCocoClassNames[static_cast<size_t>(det.cls)];

    std::cout << idx
              << ": cls=" << det.cls
              << " name=" << cls_name
              << " score=" << det.score
              << " box=[" << det.x1 << ", " << det.y1
              << ", " << det.x2 << ", " << det.y2 << "]"
              << " stride=" << det.stride
              << "\n";
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path exe_path = std::filesystem::absolute(argv[0]);
    std::string elf_path = (exe_path.parent_path() / "tpa_yolov5n_demo.elf").string();
    std::string image_path;
    float conf_thresh = kDefaultConfThresh;
    float iou_thresh = kDefaultIouThresh;
    size_t topk = kDefaultTopK;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--elf" && i + 1 < argc) {
            elf_path = argv[++i];
        } else if (arg == "--image" && i + 1 < argc) {
            image_path = argv[++i];
        } else if (arg == "--conf" && i + 1 < argc) {
            conf_thresh = std::stof(argv[++i]);
        } else if (arg == "--iou" && i + 1 < argc) {
            iou_thresh = std::stof(argv[++i]);
        } else if (arg == "--topk" && i + 1 < argc) {
            topk = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--help") {
            std::cout
                << "Usage: yolo_demo [--elf <path>] [--image <ppm-or-raw>] "
                << "[--conf <thr>] [--iou <thr>] [--topk <n>]\n"
                << "If --image is omitted, a zero 640x640x3 input is used.\n";
            return EXIT_SUCCESS;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            return EXIT_FAILURE;
        }
    }

    auto input_addr = find_symbol_addr(elf_path, "__tpa_edge_pool_cluster0");
    auto result_addr = find_symbol_addr(elf_path, "yolov5n_demo_result");
    if (!input_addr || !result_addr) {
        std::cerr << "failed to resolve demo symbols in " << elf_path << "\n";
        return EXIT_FAILURE;
    }

    auto image = load_image_bytes(image_path);

    sys_emu_cmd_options opts{};
    opts.elf_files.push_back(elf_path);
    opts.minions_en = kDefaultMinions;
    opts.mins_dis = true;
    opts.max_cycles = kDefaultMaxCycles;
    opts.display_trap_info = true;

    testLog::maxErrors_ = 16u;
    sys_emu emu(opts);
    emu.thread_write_memory(kThread, *input_addr, image.size(),
                            reinterpret_cast<uint8_t*>(image.data()));

    int rc = emu.main_internal();
    yv5n_demo_result_t result{};
    emu.thread_read_memory(kThread, *result_addr, sizeof(result),
                           reinterpret_cast<uint8_t*>(&result));

    if (result.magic != YV5N_DEMO_RESULT_MAGIC || result.ready != 1u) {
        if (rc != EXIT_SUCCESS)
            std::cerr << "emulation failed with code " << rc << "\n";
        std::cerr << "demo result was not produced\n";
        return EXIT_FAILURE;
    }

    auto p6_box = read_tensor(emu, result.p6_box);
    auto p6_cls = read_tensor(emu, result.p6_cls);
    auto p8_box = read_tensor(emu, result.p8_box);
    auto p8_cls = read_tensor(emu, result.p8_cls);
    auto p10_box = read_tensor(emu, result.p10_box);
    auto p10_cls = read_tensor(emu, result.p10_cls);

    print_head_debug("p6", result.p6_cls, p6_cls);
    print_head_debug("p8", result.p8_cls, p8_cls);
    print_head_debug("p10", result.p10_cls, p10_cls);

    auto dets_p6 = decode_head(result.p6_box, p6_box, result.p6_cls, p6_cls, conf_thresh);
    auto dets_p8 = decode_head(result.p8_box, p8_box, result.p8_cls, p8_cls, conf_thresh);
    auto dets_p10 = decode_head(result.p10_box, p10_box, result.p10_cls, p10_cls, conf_thresh);

    std::vector<detection_t> dets;
    dets.reserve(dets_p6.size() + dets_p8.size() + dets_p10.size());
    dets.insert(dets.end(), dets_p6.begin(), dets_p6.end());
    dets.insert(dets.end(), dets_p8.begin(), dets_p8.end());
    dets.insert(dets.end(), dets_p10.begin(), dets_p10.end());

    auto keep = nms(std::move(dets), iou_thresh, topk);

    std::cout << "cycles=" << emu.get_emu_cycle() << "\n";
    std::cout << "candidates=" << (dets_p6.size() + dets_p8.size() + dets_p10.size()) << "\n";
    std::cout << "detections=" << keep.size() << "\n";
    for (size_t i = 0; i < keep.size(); ++i)
        print_detection(i, keep[i]);

    return EXIT_SUCCESS;
}
