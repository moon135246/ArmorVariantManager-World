#include <fstream>
#include <cstdlib>
static void SaveCurrentFrame(){
 char destination[32768]{};auto length=GetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",destination,(DWORD)sizeof(destination));if(!length||length>=sizeof(destination))return;
 auto folder=worlddiff::app::PathUtf8(destination);std::filesystem::create_directories(folder);unsigned char* atlas;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas,&w,&h);
 {std::ofstream rgba(folder/L"atlas.rgba",std::ios::binary);rgba.write((const char*)atlas,(std::streamsize)w*h*4);}
 std::ofstream output(folder/L"drag_preview.json");auto* data=ImGui::GetDrawData();output<<"{\"width\":"<<data->DisplaySize.x<<",\"height\":"<<data->DisplaySize.y<<",\"atlas_width\":"<<w<<",\"atlas_height\":"<<h<<",\"lists\":[";
 for(int i=0;i<data->CmdListsCount;i++){if(i)output<<',';auto* list=data->CmdLists[i];output<<"{\"vertices\":[";for(int n=0;n<list->VtxBuffer.Size;n++){if(n)output<<',';auto& v=list->VtxBuffer[n];output<<'['<<v.pos.x<<','<<v.pos.y<<','<<v.uv.x<<','<<v.uv.y<<','<<v.col<<']';}output<<"],\"indices\":[";for(int n=0;n<list->IdxBuffer.Size;n++){if(n)output<<',';output<<list->IdxBuffer[n];}output<<"],\"commands\":[";
  for(int n=0;n<list->CmdBuffer.Size;n++){if(n)output<<',';auto& c=list->CmdBuffer[n];output<<'['<<c.ElemCount<<','<<c.IdxOffset<<','<<c.VtxOffset<<','<<c.ClipRect.x<<','<<c.ClipRect.y<<','<<c.ClipRect.z<<','<<c.ClipRect.w<<']';}output<<"]}";}output<<"]}";
}
