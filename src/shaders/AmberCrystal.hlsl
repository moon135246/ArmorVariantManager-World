// Original VS and PS stay in the material pipeline. Tessellate in bind space
// and pass a clip distance to the rasterizer for both the depth and color draws.
#ifndef MATERIAL_VERTEX_FIELDS
#define MATERIAL_VERTEX_FIELDS(F) F(float4,clipPosition,SV_Position) F(float3,materialData,TEXCOORD0)
#define MATERIAL_INTERPOLATE(o,p,w) o.clipPosition=p[0].clipPosition*w.x+p[1].clipPosition*w.y+p[2].clipPosition*w.z; o.materialData=p[0].materialData*w.x+p[1].materialData*w.y+p[2].materialData*w.z;
#define DISSOLVE_SETTINGS_REGISTER register(b0)
#define DISSOLVE_POSITIONS_REGISTER register(t0)
#endif
struct MeshCorner { float2 silver; uint crystal; };
StructuredBuffer<MeshCorner> corners : DISSOLVE_POSITIONS_REGISTER;
cbuffer PatchSettings : DISSOLVE_SETTINGS_REGISTER {
    float visible;
    float3 reservedFieldSettings;
    float inkMode;
}
#define DECLARE_FIELD(type,field,semantic) type field : semantic;
struct Surface { MATERIAL_VERTEX_FIELDS(DECLARE_FIELD) };
struct CrystalSurface {
    MATERIAL_VERTEX_FIELDS(DECLARE_FIELD)
    float distance : SV_ClipDistance0;
    float4 crystalData : WORLDDIFF_CRYSTAL0;
};
struct PatchData {
    float edge[3] : SV_TessFactor;
    float inner : SV_InsideTessFactor;
    float3 a : BINDPOS0;
    float3 b : BINDPOS1;
    float3 c : BINDPOS2;
};
float3 unpackCorner(uint index) {
    uint bits=corners[index].crystal;
    return float3(bits&1023u,(bits>>10)&1023u,(bits>>20)&1023u)/1023.0-.5;
}
float3 gridPosition(float3 local) {
    return float3(local.x+.23*local.y,local.y-.15*local.x,local.z)*float3(9.0,6.2,7.6);
}
float sweepOrder(float3 p) {
    return .5+dot(float3(p.x+.23*p.y,p.y-.15*p.x,p.z),float3(.64,.87,.33));
}
// The nearest site is at most sqrt(3)*.78 grid units away (the home cell
// always contains a site). Together with the jitter this bounds its start time.
float density(float3 a,float3 b) {
    if(inkMode>0) return clamp(ceil(120.0*length(a-b)),1.0,40.0);
    float hi=.72*saturate(max(sweepOrder(a),sweepOrder(b))+.28);
    return visible>hi+.34?1.0:clamp(ceil(16.0*length(gridPosition(a-b))),1.0,40.0);
}
PatchData patchFactors(InputPatch<Surface,3> patch,uint primitive:SV_PrimitiveID) {
    PatchData o;
    #ifdef MATERIAL_UV
    o.a=float3(MATERIAL_UV(patch[0])-0.5,0);
    o.b=float3(MATERIAL_UV(patch[1])-0.5,0);
    o.c=float3(MATERIAL_UV(patch[2])-0.5,0);
    #else
    o.a=unpackCorner(primitive*3);o.b=unpackCorner(primitive*3+1);o.c=unpackCorner(primitive*3+2);
    #endif
    o.edge[0]=density(o.b,o.c);o.edge[1]=density(o.c,o.a);o.edge[2]=density(o.a,o.b);
    o.inner=max(o.edge[0],max(o.edge[1],o.edge[2]));
    float earliest=.72*saturate(min(sweepOrder(o.a),min(sweepOrder(o.b),sweepOrder(o.c)))-.28);
    if(inkMode<=0 && visible<earliest) {o.edge[0]=o.edge[1]=o.edge[2]=0;o.inner=0;}
    return o;
}
[domain("tri")][partitioning("integer")][outputtopology("triangle_cw")]
[outputcontrolpoints(3)][patchconstantfunc("patchFactors")][maxtessfactor(40.0)]
Surface AmberHS(InputPatch<Surface,3> patch,uint i:SV_OutputControlPointID) {return patch[i];}

float3 cellJitter(int3 cell) {
    uint bitPattern = (uint)cell.x*1597334677u ^ (uint)cell.y*3812015801u ^ (uint)cell.z*958282793u;
    bitPattern ^= bitPattern >> 16;
    bitPattern *= 2246822519u;
    bitPattern ^= bitPattern >> 13;
    return float3(bitPattern&1023u,(bitPattern>>10)&1023u,(bitPattern>>20)&1023u)/1023.0;
}

struct Fracture { float remaining; float age; float seam; float tint; };
Fracture crystal(float3 local,float phase) {
    // Anisotropic cells have longer facets along the cloth. Both the sites
    // and the activation order are stationary when the transition reverses.
    float3 q=float3(local.x+0.23*local.y,local.y-0.15*local.x,local.z);
    float3 grid=q*float3(9.0,6.2,7.6);
    int3 base=(int3)floor(grid);
    float first=100.0,second=100.0;
    float3 closest=0,neighbor=0;
    float tint=0;
    [loop] for(int z=-1;z<=1;++z) [loop] for(int y=-1;y<=1;++y) [loop] for(int x=-1;x<=1;++x) {
        int3 cell=base+int3(x,y,z);
        float3 jitter=cellJitter(cell);
        float3 site=(float3)cell+0.22+0.56*jitter;
        float3 offset=grid-site; float d=dot(offset,offset);
        if(d<first) {second=first;neighbor=closest;first=d;closest=site;tint=jitter.x;}
        else if(d<second) {second=d;neighbor=site;}
    }
    float seam=(second-first)/max(2.0*length(neighbor-closest),0.001);
    float3 center=closest/float3(9.0,6.2,7.6);
    float order=saturate(0.5+dot(center,float3(0.64,0.87,0.33))+(tint-0.5)*0.085);
    float start=order*0.72;
    float age=phase-start;
    float inward=saturate(seam/0.42);
    Fracture c;
    c.remaining=age-0.245*inward;
    c.age=age; c.seam=seam; c.tint=tint;
    return c;
}

// Static in bind space: overlapping drops follow the same capillaries on reversal.
float inkThreshold(float3 p) {
    float3 q=p+float3(.38,-.21,.19);
    q+=.075*sin(q.yzx*13.0+q.zxy*4.1+float3(.1,2.3,4.7));
    float3 wet=cos(q*float3(17.8,14.5,16.4)+q.yzx*4.2);
    float droplets=.5-dot(wet,float3(.23,.18,.09));
    float grain=sin(dot(q,float3(142,37,53))+2.2*sin(q.y*20));
    float field=(droplets-.5)*1.23+.5+dot(p,float3(.14,.23,-.12))+.018*grain;
    // Soft tails preserve the order of the first and last droplets instead of
    // clipping an entire patch to one activation time.
    if(field<.14)return .14*exp((field-.14)/.14);
    if(field>.86)return 1.0-.14*exp((.86-field)/.14);
    return field;
}

float4 inkWash(float3 p,float phase) {
    float distance=phase-inkThreshold(p);
    float pixel=max(fwidth(distance),.0001);
    clip(distance);
    float u=distance/clamp(17.0*pixel,.065,.22);
    clip(1.0-u);
    // A broken sea-green meniscus, a dark wet deposit, then a translucent wash.
    float fibre=.5+.5*sin(dot(p,float3(158,43,61))+1.8*sin(p.y*23));
    float lip=(1.0-smoothstep(.13,.36,u))*(.80+.20*fibre);
    float tide=(1.0-smoothstep(.025,.11,abs(u-(.67+.035*fibre))))*(.48+.46*fibre);
    float3 pigment=lerp(float3(.012,.037,.077),float3(.035,.21,.23),u);
    float3 color=lerp(pigment,float3(.32,.96,.74),lip);
    color=lerp(color,float3(.19,.67,.60),tide);
    float fade=smoothstep(0,.022,phase)*(1.0-smoothstep(.965,1.0,phase));
    return float4(color,max(1.0-smoothstep(.45,1.0,u),tide)*.96*fade);
}


[domain("tri")]
CrystalSurface AmberDS(PatchData data,float3 w:SV_DomainLocation,const OutputPatch<Surface,3> patch) {
    CrystalSurface o;
    MATERIAL_INTERPOLATE(o,patch,w)
    float3 local=data.a*w.x+data.b*w.y+data.c*w.z;
    // Beyond this bound every fragment is visible.
    float latest=.72*saturate(sweepOrder(local)+.28);
    if(inkMode>0)o.distance=visible-inkThreshold(local);
    else if(visible>latest+.245)o.distance=visible-latest-.245;
    else o.distance=crystal(local,visible).remaining;
    o.crystalData=float4(local,visible);
    return o;
}
float4 AmberAlbedoPS(CrystalSurface input):SV_Target {
    if(inkMode>0)return inkWash(input.crystalData.xyz,input.crystalData.w);
    if(input.crystalData.w>.72*saturate(sweepOrder(input.crystalData.xyz)+.28)+.34)discard;
    Fracture c=crystal(input.crystalData.xyz,input.crystalData.w);
    clip(c.remaining);
    float seamWidth=clamp(2.2*fwidth(c.seam),.023,.10);
    float vein=(1.0-smoothstep(.18*seamWidth,seamWidth,c.seam))*
               (1.0-smoothstep(.22,.34,c.age));
    float front=1.0-smoothstep(.009,.06,c.remaining);
    float facet=.32*(1.0-smoothstep(.0,.13,c.remaining))*(.65+.35*c.tint);
    float alpha=max(vein,max(front,facet));clip(alpha-.004);
    float3 ember=float3(.81,.28,.035);
    float3 honey=float3(1.0,.62,.13);
    float3 light=float3(1.0,.91,.56);
    return float4(lerp(lerp(ember,honey,front),light,vein),alpha);
}
