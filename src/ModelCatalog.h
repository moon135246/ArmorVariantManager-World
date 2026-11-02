#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace worlddiff {

struct Material {
    std::string name; // Original UTF-8 bytes from the MOD3 material-name table.
    std::uint32_t hash = 0;
    int mrlIndex = -1; // Matching MRL3 record index, or -1 if absent/unavailable.
};

struct Mesh {
    std::uint32_t index = 0; // Index in this MOD3's mesh table, not meshIndex at +0x2A.
    std::uint16_t groupId = 0;
    std::uint16_t materialId = 0;
    std::uint16_t vertexCount = 0;
    std::uint32_t indexCount = 0; // Number of face-buffer indices, not triangles.
    std::uint16_t lod = 0; // Original LOD bit mask (0xFFFF means all LODs).
    std::uint32_t blockType = 0;
    std::array<std::uint8_t, 80> raw{};
};

struct Model {
    // Relative to nativePC, backslash-separated, without the .mod3 extension.
    // UTF-8, including when a custom MOD uses non-ASCII path components.
    std::string relativePath;
    std::string display;
    std::string equip;
    std::string part;
    std::vector<Material> materials; // Includes unused/compatibility material 0.
    std::vector<Mesh> meshes;
    std::uint64_t signature = 0;
    std::vector<std::uint32_t> materialOrder; // MRL3 record hashes, in file order.
};

// Identity of the MOD3 material names and mesh table for saved layout metadata.
std::uint64_t MeshOrderSignature(const Model& model) noexcept;
std::uint64_t MaterialListOrder(const Material& material, std::uint32_t materialId) noexcept;
void LoadMaterialOrder(Model& model, const std::filesystem::path& gameRoot);

// Same as zlib.crc32(utf8Name) ^ 0xFFFFFFFF.
std::uint32_t MaterialNameHash(std::string_view utf8Name) noexcept;

// Read-only: recursively scans nativePC/pl/{f_equip,m_equip} for ordinary
// <equip>/<part>/mod/*.mod3 files. Missing MRL3 files are allowed. Optional
// diagnostics describe unreadable/malformed files and unmatched material hashes.
// Per-file failures are collected and do not prevent loading other models.
std::vector<Model> LoadCatalog(
    const std::filesystem::path& gameRoot,
    std::vector<std::string>* diagnostics = nullptr);

} // namespace worlddiff
