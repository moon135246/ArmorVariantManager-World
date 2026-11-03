// Shared insert preview for groups, presets and meshes. Preview never mutates
// application state; only a valid mouse release returns a move to the caller.
struct ReorderMove {uint64_t source=0,before=UINT64_MAX;};
struct ReorderPayload {ImGuiID list=0;uint64_t key=0;};
struct ReorderDrag {
 ImGuiID list=0;uint64_t source=0;std::vector<uint64_t> original;
 size_t slot=0;std::string label;bool cancelled=false;int frame=-1;
 ImVec2 placeholderMin{},placeholderMax{},cardMin{},cardMax{};
};
ReorderDrag reorderDrag;
struct ReorderAnimation {float pitch=0;std::map<uint64_t,float> y;std::vector<uint64_t> keys;};
std::map<ImGuiID,ReorderAnimation> reorderAnimations;

void DashedRoundedRect(ImDrawList* draw,ImVec2 min,ImVec2 max,float radius,ImU32 color){
 std::vector<ImVec2> points;auto arc=[&](float x,float y,float start){for(int i=0;i<=8;i++){float a=start+(float)i*1.5707963f/8;points.emplace_back(x+std::cos(a)*radius,y+std::sin(a)*radius);}};
 arc(max.x-radius,min.y+radius,-1.5707963f);arc(max.x-radius,max.y-radius,0);arc(min.x+radius,max.y-radius,1.5707963f);arc(min.x+radius,min.y+radius,3.1415927f);points.push_back(points.front());
 float phase=0,dash=S(8),period=S(13);
 for(size_t i=1;i<points.size();i++){
  auto a=points[i-1],b=points[i];float dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy),at=0;if(length<.001f)continue;
  while(at<length){float dashPhase=std::fmod(phase,period),step=std::min(length-at,(dashPhase<dash?dash:period)-dashPhase);if(step<.001f)step=std::min(length-at,.01f);
   if(dashPhase<dash)draw->AddLine(ImVec2(a.x+dx*at/length,a.y+dy*at/length),ImVec2(a.x+dx*(at+step)/length,a.y+dy*(at+step)/length),color,S(2));at+=step;phase+=step;
  }
 }
}

class ReorderList {
 ImGuiID id;std::vector<uint64_t> keys;ImVec2 origin;float width,height,gap,pitch;
 ReorderAnimation& animation;uint64_t current=0;bool gesture=false,visual=false,inside=false,ghostStyle=false;
 ImDrawListSplitter layers;
 size_t sourceIndex=0;float blend;
public:
 ReorderList(const std::string& scope,std::vector<uint64_t> values,float rowHeight,float rowGap)
  :id(ImGui::GetID(scope.c_str())),keys(std::move(values)),origin(ImGui::GetCursorScreenPos()),width(std::max(1.f,ImGui::GetContentRegionAvail().x)),height(rowHeight),gap(rowGap),pitch(rowHeight+rowGap),animation(reorderAnimations[id]),blend(1.f-std::exp(-18.f*ImGui::GetIO().DeltaTime)){
  auto* data=ImGui::GetDragDropPayload();
  if(!data&&!ImGui::IsMouseDown(0))reorderDrag={};
  if(data&&data->IsDataType("WD_INSERT")&&data->DataSize==sizeof(ReorderPayload)){
   auto& payload=*(const ReorderPayload*)data->Data;gesture=payload.list==id&&reorderDrag.list==id&&payload.key==reorderDrag.source;
  }
  if(gesture){
   auto from=std::find(keys.begin(),keys.end(),reorderDrag.source);if(from==keys.end()||keys!=reorderDrag.original)reorderDrag.cancelled=true;
   else sourceIndex=(size_t)(from-keys.begin());if(ImGui::IsKeyPressed(ImGuiKey_Escape))reorderDrag.cancelled=true;
   visual=!reorderDrag.cancelled&&!keys.empty();
   if(visual){
    reorderDrag.slot=std::min(reorderDrag.slot,keys.size()-1);
    inside=ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)&&ImGui::IsMouseHoveringRect(origin,ImVec2(origin.x+width,origin.y+pitch*(float)keys.size()),true);
    if(inside){float relative=ImGui::GetMousePos().y-origin.y,top=(float)reorderDrag.slot*pitch;
     if(relative<top||relative>top+pitch){if(relative>top+pitch)relative-=pitch;int index=(int)std::floor((relative+pitch*.5f)/pitch);reorderDrag.slot=(size_t)std::clamp(index,0,(int)keys.size()-1);}
    }
   }
  }
  bool sameSet=animation.keys.size()==keys.size();if(sameSet&&animation.keys!=keys){auto a=animation.keys,b=keys;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());sameSet=a==b;}
  if(std::abs(animation.pitch-pitch)>.01f||!sameSet){animation.y.clear();animation.pitch=pitch;}animation.keys=keys;
  size_t rest=0;for(size_t i=0;i<keys.size();i++){
   size_t slot=i;if(visual){if(keys[i]==reorderDrag.source)slot=reorderDrag.slot;else{slot=rest++;if(slot>=reorderDrag.slot)slot++;}}
   float target=(float)slot*pitch;auto [it,added]=animation.y.try_emplace(keys[i],(float)i*pitch);if(!added){it->second+=(target-it->second)*blend;if(std::abs(target-it->second)<.25f)it->second=target;}
  }
  layers.Split(ImGui::GetWindowDrawList(),2);layers.SetCurrentChannel(ImGui::GetWindowDrawList(),1);
 }
 bool Dragging()const{return gesture;}
 bool BeginRow(uint64_t key){
  current=key;bool ghost=visual&&key==reorderDrag.source;float y=ghost?(float)reorderDrag.slot*pitch:animation.y.at(key);
  ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+y));if(ghost){ImGui::PushStyleVar(ImGuiStyleVar_Alpha,0.f);ghostStyle=true;}return ghost;
 }
 void Source(const std::string& label){
  auto* existing=ImGui::GetDragDropPayload();if(existing&&existing->IsDataType("WD_INSERT")&&existing->DataSize==sizeof(ReorderPayload)&&((const ReorderPayload*)existing->Data)->list!=id)return;
  if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip|ImGuiDragDropFlags_SourceNoDisableHover)){
   ReorderPayload payload{id,current};ImGui::SetDragDropPayload("WD_INSERT",&payload,sizeof(payload));
   if(reorderDrag.list!=id||reorderDrag.source!=current||reorderDrag.frame<ImGui::GetFrameCount()-1){
    reorderDrag={};reorderDrag.list=id;reorderDrag.source=current;reorderDrag.original=keys;auto from=std::find(keys.begin(),keys.end(),current);sourceIndex=(size_t)(from-keys.begin());reorderDrag.slot=sourceIndex;
   }
   reorderDrag.label=label;reorderDrag.frame=ImGui::GetFrameCount();gesture=true;visual=!reorderDrag.cancelled;ImGui::EndDragDropSource();
  }
 }
 void EndRow(){if(ghostStyle){ImGui::PopStyleVar();ghostStyle=false;}}
 std::optional<ReorderMove> End(){
  std::optional<ReorderMove> move;
  if(gesture){reorderDrag.frame=ImGui::GetFrameCount();
   if(visual){
    auto* draw=ImGui::GetWindowDrawList();layers.SetCurrentChannel(draw,0);ImVec2 min(origin.x+S(2),origin.y+(float)reorderDrag.slot*pitch+S(2)),max(origin.x+width-S(2),min.y+height);
    reorderDrag.placeholderMin=min;reorderDrag.placeholderMax=max;
    if(paper::active){draw->AddRectFilled(min,max,paper::colors->window);DashedRoundedRect(draw,min,max,0,inside?paper::colors->text:paper::colors->muted);}else{
    draw->AddRectFilled(min,max,IM_COL32(19,124,146,90),S(8));draw->AddRect(min,max,IM_COL32(32,222,238,25),S(8),S(5));DashedRoundedRect(draw,min,max,S(8),IM_COL32(43,225,239,inside?245:140));}
    auto text=ImGui::CalcTextSize("放置到此处");draw->AddText(ImVec2(min.x+(max.x-min.x-text.x)*.5f,min.y+(max.y-min.y-text.y)*.5f),paper::active?paper::colors->text:IM_COL32(155,220,230,230),"放置到此处");
    layers.SetCurrentChannel(draw,1);
    auto* front=ImGui::GetForegroundDrawList();auto mouse=ImGui::GetMousePos();float cardWidth=std::clamp(ImGui::CalcTextSize(reorderDrag.label.c_str()).x+S(32),S(180),std::max(S(180),std::min(S(340),width))),cardHeight=std::max(S(56),height+S(10));
    ImVec2 card(mouse.x-std::min(S(72),cardWidth*.45f),std::min(mouse.y-cardHeight*.75f,min.y-cardHeight+S(8)));auto* viewport=ImGui::GetMainViewport();
    card.x=std::clamp(card.x,viewport->Pos.x+S(4),std::max(viewport->Pos.x+S(4),viewport->Pos.x+viewport->Size.x-cardWidth-S(4)));card.y=std::clamp(card.y,viewport->Pos.y+S(4),std::max(viewport->Pos.y+S(4),viewport->Pos.y+viewport->Size.y-cardHeight-S(4)));
    ImVec2 end(card.x+cardWidth,card.y+cardHeight);reorderDrag.cardMin=card;reorderDrag.cardMax=end;
    if(paper::active){paper::CutRect(front,card,end,paper::colors->selection,S(7));front->AddRectFilled(card,{card.x+S(6),end.y},paper::colors->signal);}else{
    front->AddRectFilled(ImVec2(card.x+S(3),card.y+S(6)),ImVec2(end.x+S(3),end.y+S(6)),IM_COL32(0,0,0,85),S(10));front->AddRectFilled(card,end,IM_COL32(26,55,70,248),S(10));front->AddRect(card,end,IM_COL32(72,218,234,35),S(10),S(5));front->AddRect(card,end,IM_COL32(109,224,236,235),S(10),S(1.5f));}
    front->PushClipRect(ImVec2(card.x+S(14),card.y),ImVec2(end.x-S(10),end.y),true);front->AddText(ImVec2(card.x+S(16),card.y+(cardHeight-ImGui::GetFontSize())*.5f),IM_COL32(234,250,255,255),reorderDrag.label.c_str());front->PopClipRect();
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)&&ImGui::IsMouseDown(0)){
     auto clipMin=draw->GetClipRectMin(),clipMax=draw->GetClipRectMax();float edge=S(30),speed=0;
     if(mouse.y<clipMin.y+edge)speed=-std::clamp((clipMin.y+edge-mouse.y)/edge,0.f,1.f);else if(mouse.y>clipMax.y-edge)speed=std::clamp((mouse.y-(clipMax.y-edge))/edge,0.f,1.f);
     if(speed)ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()+speed*S(650)*ImGui::GetIO().DeltaTime,0.f,ImGui::GetScrollMaxY()));
    }
   }
   if(ImGui::IsMouseReleased(0)){
    if(visual&&inside&&reorderDrag.slot!=sourceIndex){std::vector<uint64_t> rest;for(auto key:keys)if(key!=reorderDrag.source)rest.push_back(key);move=ReorderMove{reorderDrag.source,reorderDrag.slot<rest.size()?rest[reorderDrag.slot]:UINT64_MAX};}
    if(visual)animation.y[reorderDrag.source]=(float)(inside?reorderDrag.slot:sourceIndex)*pitch;
    reorderDrag={};
   }
  }
  // Every cursor reposition above is followed by a real item. This final item
  // owns the full scroll extent, including the source's reserved placeholder.
  ImGui::SetCursorScreenPos(origin);ImGui::Dummy(ImVec2(width,keys.empty()?0.f:(float)keys.size()*pitch-gap));layers.Merge(ImGui::GetWindowDrawList());return move;
 }
};
