// Local positions for each indexed triangle in this draw, in index order.
// Preserve the material's vertex outputs in signature order, including integer
// interpolants. The native shader-link test supplies the fields of each pass.
#ifndef MATERIAL_VERTEX_FIELDS
#define MATERIAL_VERTEX_FIELDS(F) F(float4, clipPosition, SV_Position) F(float3, materialData, TEXCOORD0)
#endif
#ifndef DISSOLVE_POSITIONS_REGISTER
#define DISSOLVE_POSITIONS_REGISTER register(t0)
#define DISSOLVE_SETTINGS_REGISTER register(b0)
#endif
#ifndef DISSOLVE_MASK_REGISTER
#define DISSOLVE_MASK_REGISTER register(t1)
#define DISSOLVE_MASK_LAYOUT_REGISTER register(b1)
#endif
#define DECLARE_FIELD(type, field, semantic) type field : semantic;
#ifdef SILVER_GAME_FIELD
struct MeshCorner { float2 silver; uint crystal; };
StructuredBuffer<MeshCorner> trianglePositions : DISSOLVE_POSITIONS_REGISTER;
#else
StructuredBuffer<float3> trianglePositions : DISSOLVE_POSITIONS_REGISTER;
#endif
cbuffer DissolveSettings : DISSOLVE_SETTINGS_REGISTER {
    float progression;
    float lowerHeight;
    float inverseHeight;
    float edgeWidth;
    float boundarySign;
};

struct Surface {
    MATERIAL_VERTEX_FIELDS(DECLARE_FIELD)
};

#ifndef SILVER_GAME_FIELD
float boundaryDistance(float3 p) {
    float height = (p.y - lowerHeight) * inverseHeight;
    // Bind-pose coordinates keep the ripple attached while the hunter moves.
    // Taper displacement at both ends so the sweep never leaves a final patch.
    float envelope = 4.0 * saturate(height) * saturate(1.0 - height);
    float slope = 0.064 * sin(p.x * 0.075 + p.z * 0.038);
    float ripple = 0.024 * sin(dot(p, float3(0.33, 0.09, 0.23)));
    float field = saturate(height + envelope * (slope + ripple));
    return boundarySign * (field - progression);
}
#endif

struct ClippedSurface {
    MATERIAL_VERTEX_FIELDS(DECLARE_FIELD)
    float distance : SV_ClipDistance0;
#ifdef SILVER_GAME_FIELD
    // Distance follows the same clipped triangle plane as the native material.
    float4 moon : WORLDDIFF_EDGE0; // signed distance, bind-pose u, height, phase
#else
    float edgeDistance : WORLDDIFF_EDGE0;
#endif
};

[maxvertexcount(3)]
void CoverageGS(triangle Surface input[3], uint primitive : SV_PrimitiveID,
                inout TriangleStream<ClippedSurface> outputStream) {
    [unroll] for (uint i = 0; i < 3; ++i) {
        ClippedSurface vertex;
        #define COPY_FIELD(type, field, semantic) vertex.field = input[i].field;
        MATERIAL_VERTEX_FIELDS(COPY_FIELD)
        #undef COPY_FIELD
        #ifdef SILVER_GAME_FIELD
        #ifdef MATERIAL_UV
        float2 uv = MATERIAL_UV(input[i]);
        float h = 1.0 - uv.y;
        float2 xz = float2(uv.x - 0.5, 0.0);
        #else
        MeshCorner corner = trianglePositions[primitive * 3 + i];
        float h = saturate((corner.silver.x - lowerHeight) * inverseHeight);
        float2 xz = float2(corner.crystal & 1023u, (corner.crystal >> 20u) & 1023u) / 1023.0 - 0.5;
        #endif
        float u = dot(xz, float2(1.32, 0.78)) + 0.11 * h;
        float bend = 0.145*sin(5.6*u + 2.4*h - 0.6) + 0.055*sin(9.3*u - 3.8*h + 0.5) - 0.14*u;
        vertex.distance = progression - saturate(h + 4.0*h*(1.0-h)*bend);
        vertex.moon = float4(vertex.distance, u, h, progression);
        #else
        vertex.distance = boundaryDistance(trianglePositions[primitive * 3 + i]);
        vertex.edgeDistance = vertex.distance;
        #endif
        outputStream.Append(vertex);
    }
}

// White is the clear value. The original material PS clears surviving samples
// to black through fixed-function blending, including its own alpha discard.
// The R8 target is copied into a GPU buffer with the device's row pitch.
// A root SRV can address this buffer while the original VS keeps its heaps.
ByteAddressBuffer materialCoverage : DISSOLVE_MASK_REGISTER;
cbuffer CoverageLayout : DISSOLVE_MASK_LAYOUT_REGISTER {
    uint maskRowPitch;
    float maskEdgeWidth;
};
#ifdef SILVER_GAME_FIELD
float stripe(float x,float center,float halfWidth,float pixel) {
    return 1-smoothstep(max(0,halfWidth-pixel*.5),halfWidth+pixel*.5,abs(x-center));
}
float4 silverBand(ClippedSurface s) {
    float d=s.moon.x, u=s.moon.y, h=s.moon.z, t=s.moon.w;
    float px=max(fwidth(d),.0001);
    // Two ribbons peel away over different spans and meet again at the hem.
    float lobe=.5+.5*cos(6.8*u+2.8*h-4.6*t+.8);
    float other=.5+.5*cos(7.6*u-2.1*h+3.4*t+1.7);
    float lead=min(7.4*px,.086);
    float bowA=lead*.26+.165*lobe*lobe;
    float bowB=lead*.18+.110*other*other;
    float pulse=pow(saturate(.5+.5*cos(13.6*u-10.0*t+2.2*h)),9);
    float a=stripe(d,bowA,min((1.0+1.0*lobe+.9*pulse)*px,.021),px)*(.25+.75*lobe);
    float b=stripe(d,bowB,min((.65+.8*other)*px,.014),px)*(.15+.85*other);
    float body=(1-smoothstep(.73,1.0,d/lead));
    float core=(1-smoothstep(.19,.46,d/lead));
    float between=saturate(d/max(bowA,.001));
    float veil=sin(3.141593*between)*lobe*(.41+.35*lobe);
    float3 violet=float3(.47,.095,.99), silver=float3(1.0,.97,1.0);
    float3 lightA=lerp(float3(.70,.48,1),silver,pulse);
    float3 lightB=float3(.43,.23,.91);
    // Compose fine threads over cloth with straight alpha, preserving material at rest.
    float opacity=veil;
    float3 satin=lerp(float3(.20,.055,.48),float3(.64,.29,.91),between*between);
    float3 color=satin*veil;
    color=lerp(color,lerp(violet,silver,core),body);opacity=lerp(opacity,1,body);
    color=lerp(color,lightB,b);opacity=lerp(opacity,1,b);
    color=lerp(color,lightA,a);opacity=lerp(opacity,1,a);
    float join=saturate(1-abs(bowA-bowB)/max(3*px,.006))*pulse*max(a,b);
    color=lerp(color,silver,join);opacity=max(opacity,join);
    // A single traveling highlight stays on the outer silk thread. Its broad
    // reflection and short crossing ray read at different viewing distances.
    float along=(u-(-.67+1.31*t+.07*sin(5*t)))/max(fwidth(u),.0004);
    float across=(d-bowA)/max(fwidth(d-bowA),.0003);
    float halo=.55*exp2(-.009*along*along-.085*across*across);
    float flare=max(exp2(-.011*along*along-1.4*across*across),
                    .90*exp2(-1.3*along*along-.10*across*across));
    color=lerp(color,float3(.83,.57,1),halo);opacity=lerp(opacity,1,halo);
    color=lerp(color,silver,flare);opacity=lerp(opacity,1,flare);
    float envelope=smoothstep(0,.035,t)*(1-smoothstep(.955,1,t));
    clip(d);clip(opacity*envelope-.004);
    return float4(color/max(opacity,.001),opacity*envelope);
}
#else
float4 silverBand(ClippedSurface s) {
    float distance = s.edgeDistance;
    const float referenceWidth = .035;
    float pixelWidth = max(fwidth(distance), 1e-6);
    // Keep a readable core and a separate violet shoulder on pale cloth.
    // The field-space cap prevents a small accessory filling entirely with light.
    float width = min(7.4 * pixelWidth, 2.5 * referenceWidth);
    clip(width - distance);
    float band = saturate(distance / width);
    float core = 1.0 - smoothstep(0.19, 0.43, band);
    float3 color = lerp(float3(0.52, 0.13, 1.0), float3(1.0, 0.98, 1.0), core);
    return float4(color, 1.0 - smoothstep(0.85, 1.0, band));
}
#endif
float4 SilverEdgePS(ClippedSurface s) : SV_Target {
    float4 band = silverBand(s);
    uint2 pixel = uint2(s.clipPosition.xy);
    uint address = pixel.y * maskRowPitch + pixel.x;
    uint packed = materialCoverage.Load(address & ~3u);
    if (((packed >> ((address & 3u) * 8u)) & 255u) >= 128u) discard;
    return band;
}
// The six-target material pass has already populated depth and the G-buffer.
// Blend only the albedo RGB channels, keeping material classification and normals.
float4 SilverAlbedoPS(ClippedSurface s) : SV_Target {
    return silverBand(s);
}
