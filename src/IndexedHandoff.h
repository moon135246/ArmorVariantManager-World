#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace worlddiff::dissolve {
// The paired meshes use their original vertex streams. Only the order of the
// index triplets changes. Both use the field on the first row's bind pose.
struct IndexedHandoff {
    std::array<unsigned,2> rows{}, first{};
    std::array<int,2> base{};
    std::array<std::array<uint8_t,80>,2> headers{};
    std::vector<uint16_t> indices;
    std::vector<float> thresholds;
    std::vector<double> displacementPrefix;
    std::vector<uint8_t> insideQuad;

    unsigned Cut(float boundary) const {
        if (boundary <= 0.f) return 0;
        if (boundary >= 1.f) return static_cast<unsigned>(thresholds.size()) * 3;
        return static_cast<unsigned>(std::lower_bound(thresholds.begin(), thresholds.end(), boundary) -
                                     thresholds.begin()) * 3;
    }

    unsigned ShapeCut(float progress) const {
        if (displacementPrefix.empty() || progress<=0.f || progress>=1.f) return Cut(progress);
        const auto end=std::lower_bound(displacementPrefix.begin(),displacementPrefix.end(),
                                        progress*displacementPrefix.back());
        const auto n=static_cast<unsigned>(end-displacementPrefix.begin());
        return (n+insideQuad[n])*3;
    }
};

// Steam 421810 maps the MOD3 block-type hash at 0x18 to a layout ID.
// This pair's POSITION is float3 at byte 0 in the verified 36-byte stream.
inline bool MatchesHandoffRow(const unsigned char* live,const unsigned char* file) {
    uint32_t hash{},layout{};
    std::memcpy(&hash,file+24,4);
    std::memcpy(&layout,live+24,4);
    const bool supported=(hash==0x3c730760 && layout==6 && file[14]==36) ||
                         (hash==0x81f58067 && layout==8 && file[14]==40) ||
                         (hash==0xf637401c && layout==4 && file[14]==32) ||
                         (hash==0x366995a7 && layout==10 && file[14]==44);
    return supported &&
           !std::memcmp(live+2,file+2,8) && !std::memcmp(live+14,file+14,10) &&
           !std::memcmp(live+28,file+28,12);
}

inline bool DecodeHandoff(std::span<const uint8_t> bytes, std::array<unsigned,2> rows, IndexedHandoff& out) {
    if (bytes.size() < 320 || std::memcmp(bytes.data(), "MOD\0\xed\0", 6) || rows[0] == rows[1]) return false;
    auto read = [&]<class T>(size_t offset, T) {
        T value;
        std::memcpy(&value, bytes.data()+offset, sizeof(T));
        return value;
    };
    const auto rowCount = read(8, uint16_t{});
    const auto table = read(0x48, uint64_t{}), vertices = read(0x50, uint64_t{}), indices = read(0x58, uint64_t{});
    if (table < 320 || table > vertices || vertices > indices || indices > bytes.size() ||
        rowCount > (vertices-table)/80 || rows[0]>=rowCount || rows[1]>=rowCount) return false;
    IndexedHandoff candidate;
    candidate.rows = rows;
    std::array<unsigned,2> sub{};
    std::vector<std::array<float,3>> points;
    std::vector<double> displacement;
    std::array<size_t,2> vertexStarts{};
    unsigned indexCount = 0;
    for (unsigned side=0; side<2; ++side) {
        const auto row = table+80ull*rows[side];
        std::memcpy(candidate.headers[side].data(), bytes.data()+row, 80);
        const auto count = read(row+2,uint16_t{});
        const auto stride = read(row+14,uint8_t{});
        sub[side] = read(row+16,uint32_t{});
        candidate.first[side] = read(row+28,uint32_t{});
        candidate.base[side] = read(row+36,int32_t{});
        const int64_t startVertex = int64_t(sub[side]) + candidate.base[side];
        const auto length = read(row+32,uint32_t{});
        if (stride<12 || count<3 || startVertex<0 || !length || length%3 ||
            length>2*1024*1024 || uint64_t(candidate.first[side])+length>(bytes.size()-indices)/2) return false;
        const auto offset = vertices+read(row+20,uint32_t{})+uint64_t(startVertex)*stride;
        if (offset+uint64_t(count-1)*stride+12>indices) return false;
        vertexStarts[side]=static_cast<size_t>(offset);
        if (!side) {
            indexCount=length;
            points.resize(count);
            for (unsigned i=0;i<count;++i) {
                std::memcpy(points[i].data(),bytes.data()+offset+size_t(i)*stride,12);
                for (float v:points[i]) if (!std::isfinite(v)) return false;
            }
        } else {
            if (length!=indexCount || count!=points.size() ||
                std::memcmp(candidate.headers[0].data()+8,candidate.headers[1].data()+8,2)) return false;
            displacement.resize(count);
            for (unsigned i=0;i<count;++i) {
                for(unsigned axis=0;axis<3;++axis) {
                    float position=read(offset+size_t(i)*stride+axis*4,float{});
                    if(!std::isfinite(position))return false;
                    const double delta=double(position)-points[i][axis];
                    displacement[i]+=delta*delta;
                }
            }
        }
    }
    if (candidate.first[0]==candidate.first[1] && candidate.base[0]==candidate.base[1]) return false;
    auto vertexIndex=[&](unsigned side,unsigned i) {
        return read(indices+(uint64_t(candidate.first[side])+i)*2,uint16_t{});
    };
    auto minMax=std::minmax_element(points.begin(),points.end(),[](const auto& a,const auto& b){return a[1]<b[1];});
    const float low=(*minMax.first)[1],height=(*minMax.second)[1]-low;
    if (!(height>0.f) || !std::isfinite(height)) return false;
    std::vector<float> field(points.size());
    for (unsigned i=0;i<points.size();++i) {
        const auto [x,y,z]=points[i];
        const float f=(y-low)/height+.085f*std::sin(.37f*x+.20f*y+.31f*z)+.025f*std::sin(-1.2f*x+.74f*y+.46f*z);
        if (!std::isfinite(f)) return false;
        field[i]=std::clamp(f,0.f,1.f);
    }
    constexpr unsigned noTriangle=(std::numeric_limits<unsigned>::max)();
    struct Unit { unsigned first, second; float level; double weight; };
    std::vector<Unit> units;
    std::map<std::array<unsigned,4>,size_t> quads;
    auto face=[&](unsigned s,unsigned tri) {
        std::array<unsigned,3> v{};
        for(unsigned j=0;j<3;++j)v[j]=vertexIndex(s,tri*3+j)-sub[s];
        return v;
    };
    auto perimeter=[&](unsigned s,unsigned first,unsigned second) {
        std::vector<std::pair<unsigned,unsigned>> edges;
        for(unsigned tri:{first,second}) {
            auto v=face(s,tri);
            for(unsigned j=0;j<3;++j) {
                auto a=v[j],b=v[(j+1)%3];
                const auto reverse=std::find(edges.begin(),edges.end(),std::pair{b,a});
                if(reverse==edges.end())edges.emplace_back(a,b);
                else edges.erase(reverse);
            }
        }
        std::sort(edges.begin(),edges.end());
        return edges;
    };
    for(unsigned tri=0;tri<indexCount/3;++tri) {
        const auto a=face(0,tri),b=face(1,tri);
        if(*std::max_element(a.begin(),a.end())>=field.size() ||
           *std::max_element(b.begin(),b.end())>=field.size())return false;
        if(a==b || (a[0]==b[1] && a[1]==b[2] && a[2]==b[0]) ||
                   (a[0]==b[2] && a[1]==b[0] && a[2]==b[1])) {
            units.push_back({tri,noTriangle,(field[a[0]]+field[a[1]]+field[a[2]])/3.f,
                             displacement[a[0]]+displacement[a[1]]+displacement[a[2]]});
            continue;
        }
        std::array<unsigned,6> joined{a[0],a[1],a[2],b[0],b[1],b[2]};
        std::sort(joined.begin(),joined.end());
        if(std::unique(joined.begin(),joined.end())-joined.begin()!=4)return false;
        std::array<unsigned,4> key{joined[0],joined[1],joined[2],joined[3]};
        auto [it,inserted]=quads.try_emplace(key,units.size());
        if(inserted) {
            float level=0;double weight=0;
            for(unsigned v:key){level+=field[v]*.25f;weight+=displacement[v]*1.5;}
            units.push_back({tri,noTriangle,level,weight});
        } else {
            auto& u=units[it->second];
            auto boundary=perimeter(0,u.first,tri);
            if(u.second!=noTriangle || boundary.size()!=4 || boundary!=perimeter(1,u.first,tri))return false;
            u.second=tri;
        }
    }
    if(!quads.empty()) {
        for(const auto& item:quads)if(units[item.second].second==noTriangle)return false;
        for(unsigned s=0;s<2;++s)if(candidate.headers[s][14]<24)return false;
        for(size_t v=0;v<points.size();++v) {
            if(std::memcmp(bytes.data()+vertexStarts[0]+v*candidate.headers[0][14]+20,
                           bytes.data()+vertexStarts[1]+v*candidate.headers[1][14]+20,4))return false;
        }
    }
    std::stable_sort(units.begin(),units.end(),[](const Unit& a,const Unit& b){return a.level<b.level;});
    candidate.indices.resize(size_t(indexCount)*2);
    candidate.thresholds.reserve(indexCount/3);
    candidate.insideQuad.resize(indexCount/3+1);
    const double sum=std::accumulate(units.begin(),units.end(),0.,[](double total,const Unit& u){return total+u.weight;});
    // Keep the spatial ordering, but spend time where the two shapes differ.
    // A small uniform term keeps unchanged regions continuous in the sweep.
    const double floor=sum*.02/(indexCount/3);
    if(sum>0.)candidate.displacementPrefix.push_back(0.);
    for (const auto& u:units) {
        for(unsigned tri:{u.first,u.second}) {
            if(tri==noTriangle)continue;
            const auto i=candidate.thresholds.size();
            candidate.insideQuad[i]=tri==u.second;
            candidate.thresholds.push_back(u.level);
            if(sum>0.)candidate.displacementPrefix.push_back(candidate.displacementPrefix.back()+floor+
                                                             u.weight/(u.second==noTriangle?1.:2.));
            for (unsigned s=0;s<2;++s) for(unsigned j=0;j<3;++j)
                candidate.indices[s*indexCount+3*i+j]=vertexIndex(s,3*tri+j);
        }
    }
    out=std::move(candidate);
    return true;
}

inline bool LoadHandoff(const std::filesystem::path& path,std::array<unsigned,2> rows,IndexedHandoff& out) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    const auto size=input.tellg();
    if (size<320 || size>512ll*1024*1024) return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(0);
    return input.read(reinterpret_cast<char*>(bytes.data()),size) && DecodeHandoff(bytes,rows,out);
}
} // namespace worlddiff::dissolve
