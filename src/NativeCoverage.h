#pragma once
#include "DissolveTransition.h"
#include <algorithm>
#include <array>

namespace worlddiff::dissolve {
struct IndexSpan { uint32_t first,count; };
struct CoverageSpans {
    std::array<IndexSpan,12> ranges{};
    unsigned used=0;
    unsigned indices=0;
};

// Select native triangles in several regions of the original index order.
// Works with any vertex format or UV layout. Both sides of a handoff use the
// same integer boundary for equal topology, and keep the original winding.
inline CoverageSpans NativeCoverage(uint32_t count,float hidden,bool suffix) {
    CoverageSpans result;
    if (!count || count%3 || !std::isfinite(hidden) || hidden>=1.f) return result;
    if (hidden<=0.f) { result.ranges[0]={0,count};result.used=1;result.indices=count;return result; }
    const unsigned triangles=count/3;
    const unsigned bands=(std::min)(12u,1u+(triangles-1)/96);
    const float phase=suffix?hidden:1.f-hidden;
    for(unsigned i=0;i<bands;++i) {
        const auto begin=static_cast<unsigned>(uint64_t(triangles)*i/bands);
        const auto end=static_cast<unsigned>(uint64_t(triangles)*(i+1)/bands);
        const float stagger=bands==1?0.f:((i*7)%12)/11.f;
        const float t=std::clamp(phase*(bands==1?1.f:1.2f)-.2f*stagger,0.f,1.f);
        const auto cut=begin+static_cast<unsigned>(std::llround(double(end-begin)*t));
        const unsigned first=3*(suffix?cut:begin),length=3*(suffix?end-cut:cut-begin);
        if(!length)continue;
        if(result.used && result.ranges[result.used-1].first+result.ranges[result.used-1].count==first)
            result.ranges[result.used-1].count+=length;
        else result.ranges[result.used++]={first,length};
        result.indices+=length;
    }
    return result;
}
} // namespace worlddiff::dissolve
