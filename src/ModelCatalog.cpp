#include "ModelCatalog.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace worlddiff {
namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kModHeaderSize = 320;
constexpr std::size_t kMaterialNameSize = 128;
constexpr std::size_t kMeshSize = 80;
constexpr std::uint64_t kMaxFileSize = 512ull * 1024 * 1024;

std::string PathUtf8(const fs::path& path, bool generic = false) {
    // C++20 changes u8string() to basic_string<char8_t>; keep this translation
    // unit compatible with both C++17 and newer project-wide language modes.
    const auto bytes = generic ? path.generic_u8string() : path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::uint16_t U16(const Bytes& bytes, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

std::uint32_t U32(const Bytes& bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint64_t U64(const Bytes& bytes, std::size_t offset) noexcept {
    return U32(bytes, offset) |
           (static_cast<std::uint64_t>(U32(bytes, offset + 4)) << 32);
}

bool Fits(const Bytes& bytes, std::uint64_t offset, std::uint64_t count,
          std::uint64_t stride) noexcept {
    const auto length = static_cast<std::uint64_t>(bytes.size());
    return offset <= length && stride != 0 && count <= (length - offset) / stride;
}

std::string LowerAscii(std::string value) {
    for (auto& ch : value) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
    }
    return value;
}

void Diagnostic(std::vector<std::string>* diagnostics, const fs::path& path,
                const std::string& message) {
    if (diagnostics) diagnostics->push_back(PathUtf8(path) + ": " + message);
}

bool ReadFile(const fs::path& path, Bytes& bytes, std::string& error) {
    // std::ifstream(path) uses the native wide-character path on Windows.
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "cannot open file";
        return false;
    }
    const auto end = file.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kMaxFileSize) {
        error = "invalid file size or file exceeds 512 MiB";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!bytes.empty() &&
        !file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()))) {
        error = "incomplete file read";
        return false;
    }
    return true;
}

bool ParseMod3(const fs::path& path, Model& model, std::string& error) {
    Bytes bytes;
    if (!ReadFile(path, bytes, error)) return false;
    if (bytes.size() < kModHeaderSize || std::memcmp(bytes.data(), "MOD\0", 4) != 0) {
        error = "invalid MOD3 header";
        return false;
    }
    if (U16(bytes, 4) != 237) {
        error = "unsupported MOD3 version " + std::to_string(U16(bytes, 4));
        return false;
    }

    const auto meshCount = U16(bytes, 0x08);
    const auto materialCount = U16(bytes, 0x0A);
    const auto materialOffset = U64(bytes, 0x40);
    const auto meshOffset = U64(bytes, 0x48);
    if (!meshCount || !materialCount) {
        error = "MOD3 has no meshes or materials";
        return false;
    }
    if (materialOffset < kModHeaderSize || meshOffset < kModHeaderSize ||
        !Fits(bytes, materialOffset, materialCount, kMaterialNameSize) ||
        !Fits(bytes, meshOffset, meshCount, kMeshSize)) {
        error = "MOD3 material/mesh table is outside the file";
        return false;
    }
    const auto materialEnd = materialOffset + materialCount * kMaterialNameSize;
    const auto meshEnd = meshOffset + meshCount * kMeshSize;
    if (materialOffset < meshEnd && meshOffset < materialEnd) {
        error = "MOD3 material and mesh tables overlap";
        return false;
    }

    model.materials.reserve(materialCount);
    for (std::uint32_t i = 0; i < materialCount; ++i) {
        const auto offset = static_cast<std::size_t>(materialOffset + i * kMaterialNameSize);
        const auto* begin = reinterpret_cast<const char*>(bytes.data() + offset);
        const auto* end = static_cast<const char*>(std::memchr(begin, 0, kMaterialNameSize));
        if (!end) {
            error = "unterminated MOD3 material name at index " + std::to_string(i);
            return false;
        }
        Material material;
        material.name.assign(begin, end);
        material.hash = MaterialNameHash(material.name);
        model.materials.push_back(std::move(material));
    }

    model.meshes.reserve(meshCount);
    for (std::uint32_t i = 0; i < meshCount; ++i) {
        const auto offset = static_cast<std::size_t>(meshOffset + i * kMeshSize);
        Mesh mesh;
        mesh.index = i;
        mesh.vertexCount = U16(bytes, offset + 0x02);
        mesh.groupId = U16(bytes, offset + 0x04);
        mesh.materialId = U16(bytes, offset + 0x06);
        mesh.lod = U16(bytes, offset + 0x08);
        mesh.blockType = U32(bytes, offset + 0x18);
        mesh.indexCount = U32(bytes, offset + 0x20);
        if (mesh.materialId >= materialCount) {
            error = "MOD3 mesh " + std::to_string(i) + " has invalid materialID " +
                    std::to_string(mesh.materialId);
            return false;
        }
        std::copy_n(bytes.data() + offset, kMeshSize, mesh.raw.begin());
        model.meshes.push_back(std::move(mesh));
    }
    return true;
}

void MatchMrl3(const fs::path& modPath, Model& model,
               std::vector<std::string>* diagnostics) {
    auto path = modPath;
    path.replace_extension(L".mrl3");
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        // Loose MOD3 overrides often use an MRL3 from the game's packed files.
        if (ec) Diagnostic(diagnostics, path, "cannot inspect MRL3: " + ec.message());
        return;
    }
    Bytes bytes;
    std::string error;
    if (!ReadFile(path, bytes, error)) {
        Diagnostic(diagnostics, path, error);
        return;
    }
    if (bytes.size() < 40 || std::memcmp(bytes.data(), "MRL\0", 4) != 0) {
        Diagnostic(diagnostics, path, "invalid MRL3 header");
        return;
    }
    if (U32(bytes, 4) != 12) {
        Diagnostic(diagnostics, path, "unsupported MRL3 version");
        return;
    }
    const auto count = U32(bytes, 0x10);
    const auto offset = U64(bytes, 0x20);
    if (count > 65535 || (count && offset < 40) || !Fits(bytes, offset, count, 56)) {
        Diagnostic(diagnostics, path, "MRL3 material table is outside the file");
        return;
    }
    std::unordered_map<std::uint32_t, int> indices;
    indices.reserve(count);
    model.materialOrder.clear();
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = static_cast<std::size_t>(offset + static_cast<std::uint64_t>(i) * 56);
        const auto hash = U32(bytes, at + 4);
        model.materialOrder.push_back(hash);
        if (!indices.emplace(hash, static_cast<int>(i)).second) {
            Diagnostic(diagnostics, path, "duplicate material hash " + std::to_string(hash));
        }
    }
    for (auto& material : model.materials) {
        const auto it = indices.find(material.hash);
        if (it != indices.end()) {
            material.mrlIndex = it->second;
        } else if (std::any_of(model.meshes.begin(), model.meshes.end(),
                              [&](const Mesh& mesh) {
                                  return &material == &model.materials[mesh.materialId];
                              })) {
            Diagnostic(diagnostics, path, "used MOD3 material has no matching MRL3 hash: " +
                                         material.name);
        }
    }
}

bool IdentifyModel(const fs::path& path, const fs::path& nativePc, Model& model) {
    const auto relative = path.lexically_relative(nativePc);
    std::vector<std::string> components;
    for (const auto& component : relative) components.push_back(PathUtf8(component));
    // Native player equipment resources have exactly this layout. This excludes
    // NPCs, stage geometry, weapons, and backup directories beneath nativePC.
    if (components.size() != 6 || LowerAscii(components[0]) != "pl" ||
        (LowerAscii(components[1]) != "f_equip" && LowerAscii(components[1]) != "m_equip") ||
        LowerAscii(components[4]) != "mod" || LowerAscii(PathUtf8(path.extension())) != ".mod3") {
        return false;
    }
    const auto part = LowerAscii(components[3]);
    if (part != "helm" && part != "body" && part != "arm" && part != "wst" && part != "leg") {
        return false;
    }
    auto withoutExtension = relative;
    withoutExtension.replace_extension();
    model.relativePath = PathUtf8(withoutExtension, true);
    std::replace(model.relativePath.begin(), model.relativePath.end(), '/', '\\');
    model.display = PathUtf8(path.stem());
    model.equip = components[2];
    model.part = part;
    return true;
}

} // namespace

std::uint32_t MaterialNameHash(std::string_view utf8Name) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const unsigned char byte : utf8Name) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

std::uint64_t MeshOrderSignature(const Model& model) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    auto add = [&](std::uint8_t byte) { hash = (hash ^ byte) * 1099511628211ull; };
    for (const auto& material : model.materials) {
        for (const unsigned char byte : material.name) add(byte);
        add(0);
    }
    for (const auto& mesh : model.meshes)
        for (const auto byte : mesh.raw) add(byte);
    return hash;
}

std::uint64_t MaterialListOrder(const Material& material, std::uint32_t materialId) noexcept {
    return material.mrlIndex >= 0 ? static_cast<std::uint64_t>(material.mrlIndex)
                                 : 0x100000000ull + materialId;
}

void LoadMaterialOrder(Model& model, const fs::path& gameRoot) {
#if defined(__cpp_char8_t)
    const fs::path relative(std::u8string(model.relativePath.begin(), model.relativePath.end()));
#else
    const auto relative = fs::u8path(model.relativePath);
#endif
    auto path = gameRoot / L"nativePC" / relative;
    path.replace_extension(L".mod3");
    MatchMrl3(path, model, nullptr);
}

std::vector<Model> LoadCatalog(const fs::path& gameRoot,
                               std::vector<std::string>* diagnostics) {
    std::vector<Model> models;
    const auto nativePc = gameRoot / L"nativePC";
    for (const auto* directory : {L"f_equip", L"m_equip"}) {
        const auto root = nativePc / L"pl" / directory;
        std::error_code ec;
        if (!fs::exists(root, ec)) {
            if (ec) Diagnostic(diagnostics, root, "cannot inspect equipment directory: " + ec.message());
            continue;
        }
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        if (ec) Diagnostic(diagnostics, root, "cannot enumerate equipment directory: " + ec.message());
        while (it != end) {
            const auto path = it->path();
            std::error_code entryError;
            const bool regular = it->is_regular_file(entryError);
            if (entryError) {
                Diagnostic(diagnostics, path, "cannot inspect entry: " + entryError.message());
            } else if (regular) {
                Model model;
                if (IdentifyModel(path, nativePc, model)) {
                    std::string error;
                    if (ParseMod3(path, model, error)) {
                        model.signature = MeshOrderSignature(model);
                        MatchMrl3(path, model, diagnostics);
                        models.push_back(std::move(model));
                    } else {
                        Diagnostic(diagnostics, path, error);
                    }
                }
            }
            it.increment(ec);
            if (ec) {
                Diagnostic(diagnostics, path, "directory iteration failed: " + ec.message());
                ec.clear();
            }
        }
    }
    std::sort(models.begin(), models.end(), [](const Model& left, const Model& right) {
        return left.relativePath < right.relativePath;
    });
    return models;
}

} // namespace worlddiff
