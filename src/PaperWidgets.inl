// Visuals share the existing ImGui items, IDs, hit areas and navigation.
ImFont* PaperBold(){
 auto& fonts=ImGui::GetIO().Fonts->Fonts;
 return fonts.Size>1?fonts[1]:ImGui::GetFont();
}
enum class ButtonTone { Normal, Primary, Selected, Delete };
bool PanelButton(const char* label,ImVec2 size,ButtonTone tone=ButtonTone::Normal){
 if(!paper::active)return ImGui::Button(label,size);
 ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
 for(auto col:{ImGuiCol_Button,ImGuiCol_ButtonHovered,ImGuiCol_ButtonActive,ImGuiCol_Text})
  ImGui::PushStyleColor(col,ImU32(0));
 ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
 bool hit=ImGui::Button(label,size);
 ImGui::PopStyleVar();ImGui::PopStyleColor(4);
 auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
 auto* draw=ImGui::GetWindowDrawList();
 bool primary=tone==ButtonTone::Primary,selected=tone==ButtonTone::Selected;
 ImU32 fill=selected?paper::colors->signal:primary?paper::colors->primary:paper::colors->button;
 if(ImGui::IsItemHovered())fill=selected?paper::colors->signalHover:primary?paper::colors->primaryHover:paper::colors->hover;
 if(ImGui::IsItemActive())fill=selected?paper::colors->signalHeld:primary?paper::colors->primaryHeld:paper::colors->held;
 paper::CutRect(draw,lo,hi,fill,S(6));
 auto end=ImGui::FindRenderedTextEnd(label);auto text=ImGui::CalcTextSize(label,end);
 ImU32 fg=selected?paper::colors->onSignal:primary?paper::colors->onPrimary:tone==ButtonTone::Delete?paper::colors->danger:paper::colors->text;
 draw->PushClipRect(lo,hi,true);
 draw->AddText({lo.x+std::max(0.f,(hi.x-lo.x-text.x)*.5f),lo.y+(hi.y-lo.y-text.y)*.5f},ImGui::GetColorU32(fg),label,end);
 draw->PopClipRect();
 if(ImGui::IsItemFocused())draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted));
 ImGui::PopFont();return hit;
}
void InlineChip(const char* label){
 auto* font=PaperBold();float size=S(17),pad=S(6);
 float width=font->CalcTextSizeA(size,FLT_MAX,0,label).x+2*pad;
 ImGui::Dummy({width,ImGui::GetFontSize()});auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
 auto* d=ImGui::GetWindowDrawList();d->AddRectFilled(lo,hi,paper::colors->keycap);
 d->AddText(font,size,{lo.x+pad,lo.y+(hi.y-lo.y-size)*.5f},paper::colors->text,label);
}
bool PanelCheckbox(const char* label,bool* value){
 if(!paper::active)return ImGui::Checkbox(label,value);
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(ImGui::GetStyle().FramePadding.x,S(3)));
 if(paper::IsNight()){
  // Keep the native checkbox hit target, label and keyboard navigation.
  for(auto col:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_CheckboxSelectedBg,ImGuiCol_CheckMark})
   ImGui::PushStyleColor(col,ImU32(0));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
  bool hit=ImGui::Checkbox(label,value);
  ImGui::PopStyleVar();ImGui::PopStyleColor(5);
  if(ImGui::IsItemVisible()){
   auto* draw=ImGui::GetWindowDrawList();auto lo=ImGui::GetItemRectMin();
   float side=ImGui::GetFrameHeight();ImVec2 hi(lo.x+side,lo.y+side);
   if(*value){
    paper::CutRect(draw,lo,hi,ImGui::IsItemHovered()?paper::colors->primaryHover:paper::colors->primary,S(5));
    ImGui::RenderCheckMark(draw,{lo.x+side*.22f,lo.y+side*.22f},ImGui::GetColorU32(paper::colors->onPrimary),side*.56f);
   }else{
    draw->AddRectFilled(lo,hi,ImGui::GetColorU32(ImGui::IsItemHovered()?paper::colors->hover:paper::colors->field));
    draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted));
   }
  }
  ImGui::PopStyleVar();return hit;
 }
 auto fill=*value?paper::colors->primary:paper::colors->input;
 ImGui::PushStyleColor(ImGuiCol_FrameBg,fill);
 ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,*value?paper::colors->primaryHeld:paper::colors->button);
 ImGui::PushStyleColor(ImGuiCol_FrameBgActive,paper::colors->muted);
 ImGui::PushStyleColor(ImGuiCol_CheckMark,paper::colors->onPrimary);
 bool hit=ImGui::Checkbox(label,value);ImGui::PopStyleColor(4);ImGui::PopStyleVar();return hit;
}
bool PanelHeader(const char* label,ImGuiTreeNodeFlags flags,bool current=false){
 if(!paper::active)return ImGui::CollapsingHeader(label,flags);
 ImGui::PushStyleColor(ImGuiCol_Header,current?paper::colors->signal:paper::colors->header);
 ImGui::PushStyleColor(ImGuiCol_HeaderHovered,current?paper::colors->signal:(paper::IsNight()?paper::colors->hover:paper::colors->muted));
 ImGui::PushStyleColor(ImGuiCol_HeaderActive,current?paper::colors->signal:(paper::IsNight()?paper::colors->hover:paper::colors->muted));
 ImGui::PushStyleColor(ImGuiCol_Text,current?paper::colors->onSignal:paper::colors->onSelection);
 ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
 bool open=ImGui::CollapsingHeader(label,flags);
 ImGui::PopFont();ImGui::PopStyleColor(4);return open;
}
bool PanelModal(const char* label,bool* open,ImGuiWindowFlags flags){
 if(paper::active)ImGui::PushStyleColor(ImGuiCol_Text,paper::colors->onTitle);
 bool shown=ImGui::BeginPopupModal(label,open,flags);
 if(paper::active)ImGui::PopStyleColor();
 return shown;
}
bool PresetFileModal(const char* title,const char* caption,float width){
 constexpr auto flags=ImGuiWindowFlags_AlwaysAutoResize;
 if(!paper::active)return PanelModal(title,nullptr,flags);
 ImGui::SetNextWindowSize({std::min(S(width),ImGui::GetIO().DisplaySize.x-S(24)),0});
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{S(18),S(12)});
 ImGui::PushStyleColor(ImGuiCol_PopupBg,paper::colors->dialog);
 ImGui::PushStyleColor(ImGuiCol_Text,ImU32(0));
 bool open=ImGui::BeginPopupModal(title,nullptr,flags);
 ImGui::PopStyleColor(2);ImGui::PopStyleVar();
 if(open){
  auto* w=ImGui::GetCurrentWindow();auto bar=w->TitleBarRect();
  auto* d=w->DrawList;float y=bar.GetCenter().y;
  d->PushClipRect(bar.Min,bar.Max,false);
  d->AddRectFilled({bar.Min.x+S(10),y-S(8)},{bar.Min.x+S(15),y+S(8)},paper::colors->signal);
  d->AddText(PaperBold(),S(20),{bar.Min.x+S(25),y-S(10)},paper::colors->onTitle,title);
  float captionSize=S(13),cw=PaperBold()->CalcTextSizeA(captionSize,FLT_MAX,0,caption).x;
  if(bar.GetWidth()>S(40)+ImGui::CalcTextSize(title).x+cw)
   d->AddText(PaperBold(),captionSize,{bar.Max.x-S(12)-cw,y-captionSize*.5f},paper::colors->caption,caption);
  d->PopClipRect();
 }
 return open;
}
bool BeginManager(bool* open){
 const char* label="差分管理器 · 世界版 1.2.4###WorldDiffInternal";
 if(!paper::active)return ImGui::Begin(label,open,ImGuiWindowFlags_NoCollapse);
 ImGui::PushStyleColor(ImGuiCol_Text,paper::colors->onTitle);ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
 bool shown=ImGui::Begin("    差分管理器  |  世界版  1.2.4###WorldDiffInternal",open,ImGuiWindowFlags_NoCollapse);
 ImGui::PopFont();ImGui::PopStyleColor();
 if(shown){
  auto title=ImGui::GetCurrentWindow()->TitleBarRect();
  ImVec2 p(title.Min.x+S(9),title.Min.y+title.GetHeight()*.25f);
  auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(title.Min,title.Max,false);
  draw->AddRectFilled(p,{p.x+S(6),p.y+title.GetHeight()*.5f},paper::colors->signal);draw->PopClipRect();
 }
 return shown;
}
void PaperRule(){
 if(!paper::active)return;
 auto* w=ImGui::GetCurrentWindow();auto* draw=w->DrawList;
 // Clip to the visible part of the parent when a child scrolls out of view.
 draw->PushClipRect(w->OuterRectClipped.Min,w->OuterRectClipped.Max,false);
 draw->AddRectFilled(w->Pos,{w->Pos.x+w->Size.x,w->Pos.y+S(2)},paper::colors->topRule);
 draw->PopClipRect();
}
void PaperKicker(const char* label){
 if(!paper::active)return;
 ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,0});
 ImGui::PushFont(PaperBold(),S(11));ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(paper::colors->muted),"%s",label);
 ImGui::PopFont();ImGui::PopStyleVar();
}
float PaperBadge(ImDrawList* draw,ImVec2 right,float height,const char* text,ImU32 color,float fontSize=12){
 float size=S(fontSize),pad=S(7);auto font=PaperBold();
 float width=font->CalcTextSizeA(size,FLT_MAX,0,text).x+2*pad;
 ImVec2 lo(right.x-width,right.y-height*.5f),hi(right.x,right.y+height*.5f);
 draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted));
 draw->AddText(font,size,{lo.x+pad,right.y-size*.5f},ImGui::GetColorU32(color),text);
 return width;
}
void SectionTitle(const char* label){
 if(!paper::active){ImGui::TextColored(accent,"%s",label);return;}
 auto lo=ImGui::GetCursorScreenPos();
 ImGui::GetWindowDrawList()->AddRectFilled(lo,{lo.x+S(5),lo.y+ImGui::GetFontSize()},paper::colors->signal);
 ImGui::Indent(S(12));ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
 ImGui::TextUnformatted(label);ImGui::PopFont();ImGui::Unindent(S(12));
}
void PaperSelection(ImDrawList* draw,ImVec2 lo,ImVec2 hi,ImU32 fill){
  paper::CutRect(draw,lo,hi,fill,S(7));float flag=S(paper::IsNight()?22.f:29.f);
  paper::CutRect(draw,lo,{lo.x+flag,hi.y},paper::colors->signal,S(7));
  float x=lo.x+flag*.42f,y=(lo.y+hi.y)*.5f;
  draw->AddTriangleFilled({x,y-S(3)},{x+S(6),y},{x,y+S(3)},ImGui::GetColorU32(paper::colors->onSignal));
}
bool PaperTemplateRow(const app::ProfileCandidate& file,const std::string& rate){
 // The caller's filename ID stays stable even when a long name is clipped.
 auto flags=ImGuiSelectableFlags(ImGuiSelectableFlags_NoAutoClosePopups)|ImGuiSelectableFlags_NoPadWithHalfSpacing;
 ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImU32(0));ImGui::PushStyleColor(ImGuiCol_HeaderActive,ImU32(0));
 bool hit=ImGui::Selectable("##paperTemplate",false,flags,{0,S(42)});
 ImGui::PopStyleColor(2);
 if(!ImGui::IsItemVisible())return hit;
 auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();auto* d=ImGui::GetWindowDrawList();
 bool highlighted=ImGui::IsItemHovered()||(ImGui::IsItemFocused()&&ImGui::GetCurrentContext()->NavCursorVisible);
 if(highlighted)PaperSelection(d,lo,hi,paper::colors->fileActive);
 else paper::CutRect(d,lo,hi,paper::colors->fileIdle,S(7));
 ImU32 fg=highlighted?paper::colors->onSelection:paper::colors->text;
 float y=(lo.y+hi.y)*.5f,right=hi.x-S(12);
 if(file.strong)right-=PaperBadge(d,{right,y},S(22),"完整匹配",fg,15)+S(12);
 float x=lo.x+S(39),size=S(22),rateSize=S(17);
 float nameW=PaperBold()->CalcTextSizeA(size,FLT_MAX,0,file.name.c_str()).x;
 float rateW=PaperBold()->CalcTextSizeA(rateSize,FLT_MAX,0,rate.c_str()).x;
 float end=std::min(x+nameW,right-rateW-S(24));
 if(end>x){
  d->PushClipRect({x,lo.y},{end,hi.y},true);
  d->AddText(PaperBold(),size,{x,y-size*.5f},fg,file.name.c_str());d->PopClipRect();
  d->AddLine({end+S(11),y-S(8)},{end+S(11),y+S(8)},highlighted?paper::colors->muted:paper::colors->rule);
 }
 d->PushClipRect(lo,{right,hi.y},true);
 d->AddText(PaperBold(),rateSize,{std::max(x,end+S(23)),y-rateSize*.5f},fg,rate.c_str());d->PopClipRect();
 if(ImGui::IsItemHovered()&&end<x+nameW)ImGui::SetTooltip("%s",file.name.c_str());
 return hit;
}
void PaperRow(ImDrawList* draw,ImVec2 lo,ImVec2 hi,const char* label,bool selected,const char* badge){
 bool hovered=ImGui::IsItemHovered();
 float left=S(12);
 if(selected){
  PaperSelection(draw,lo,hi,paper::colors->selection);left=S(paper::IsNight()?33.f:41.f);
 }else{
  if(hovered)draw->AddRectFilled(lo,hi,ImGui::GetColorU32(paper::colors->window));
  draw->AddLine({lo.x+left,hi.y},{hi.x-S(9),hi.y},ImGui::GetColorU32(paper::colors->rule));
 }
 if(ImGui::IsItemFocused())draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted));
 if(badge)hi.x-=PaperBadge(draw,{hi.x-S(12),(hi.y+lo.y)*.5f},S(22),badge,selected?paper::colors->onSelection:paper::colors->text)+S(10);
 auto fg=ImGui::GetColorU32(selected?paper::colors->onSelection:paper::colors->text);float size=ImGui::GetFontSize();
 auto font=selected?PaperBold():ImGui::GetFont();
 ImVec2 start(lo.x+left,lo.y+(hi.y-lo.y-size)*.5f);
 draw->PushClipRect({start.x,lo.y},{hi.x-S(8),hi.y},true);draw->AddText(font,size,start,fg,label);draw->PopClipRect();
 if(hovered&&font->CalcTextSizeA(size,FLT_MAX,0,label).x>hi.x-S(8)-start.x)ImGui::SetTooltip("%s",label);
}
// Faint open contour lines occupy only the free space below the preset rows.
void PaperContours(size_t rows){
 if(!paper::active||paper::IsNight())return;
 auto* draw=ImGui::GetWindowDrawList();auto pos=ImGui::GetCursorScreenPos();auto avail=ImGui::GetContentRegionAvail();
 float top=pos.y+S(48)*static_cast<float>(rows)+S(16),bottom=pos.y+avail.y-S(12);
 if(bottom-top<S(95))return;
 float width=std::min(S(300),avail.x),height=std::min(S(240),bottom-top);
 float x=pos.x+avail.x-width,y=top+(bottom-top-height)*.6f;
 draw->PushClipRect({x,y},{x+width,y+height},true);
 for(int line=-5;line<15;++line){
  ImVec2 pts[33];
  for(int j=0;j<33;++j){
   float t=j/32.f,k=line*.075f;
   pts[j]={x+t*width,y+height*(k+.38f-.58f*t+.075f*std::sin(9*t+3*k)+.045f*std::sin(16*t-5*k))};
  }
  for(int j=1;j<33;++j){
   float u=(j-.5f)/32.f,v=((pts[j-1].y+pts[j].y)*.5f-y)/height;
   float fade=std::clamp(6.f*std::min({u,1.f-u,v,1.f-v}),0.f,1.f);
   draw->AddLine(pts[j-1],pts[j],IM_COL32(215,215,207,(int)(100*fade)));
  }
 }
 draw->PopClipRect();
}

