// Visuals share the existing ImGui items, IDs, hit areas and navigation.
ImFont* PaperBold(){
 auto& fonts=ImGui::GetIO().Fonts->Fonts;
 return fonts.Size>1?fonts[1]:ImGui::GetFont();
}
void GlassShadow(ImDrawList* draw,ImVec2 lo,ImVec2 hi,float radius,float scale){
 // Concentric outlines stay outside the surface, including its rounded corners.
 for(int i=1;i<=7;++i){
  float spread=i*scale,drop=spread*.65f;
  ImVec2 start(lo.x-spread,lo.y-spread+drop),end(hi.x+spread,hi.y+spread+drop);
  draw->AddRect(start,end,IM_COL32(51,78,112,18-i*2),radius+spread,scale+1.f);
 }
}
// Shade the native window decorations before submitting content. Title text,
// the close button, the resize grip and the scrollbar retain their native hit areas.
void GlassWindowTint(bool modal,ImU32 body){
 auto* w=ImGui::GetCurrentWindow();
 if(w->BeginCount>1)return;
 for(auto& v:w->DrawList->VtxBuffer){
  auto rgb=v.col&~IM_COL32_A_MASK;
  bool title=rgb==(paper::glass.title&~IM_COL32_A_MASK);
  if(!title&&rgb!=(body&~IM_COL32_A_MASK))continue;
  float x=std::clamp((v.pos.x-w->Pos.x)/w->Size.x,0.f,1.f);
  float y=std::clamp((v.pos.y-w->Pos.y)/w->Size.y,0.f,1.f);
  ImU32 tint;
  if(title)tint=paper::Blend(paper::rgb(0xe5efff),modal?paper::rgb(0xdfefed):paper::rgb(0xf8f2e9),x);
  else{
   ImU32 top=paper::Blend(modal?paper::rgb(0xecf4ff):paper::rgb(0xc0d6f8),modal?paper::rgb(0xf5f9fc):paper::rgb(0xf6e4cf),x);
   ImU32 bottom=paper::Blend(modal?paper::rgb(0xe7eff7):paper::rgb(0xd5e8f3),modal?paper::rgb(0xdbece7):paper::rgb(0xb8cfec),x);
   tint=paper::Blend(top,bottom,y);
  }
  v.col=(v.col&IM_COL32_A_MASK)|(tint&~IM_COL32_A_MASK);
 }
 if(modal){
  auto* vp=ImGui::GetMainViewport();auto* d=w->DrawList;
  d->PushClipRect(vp->Pos,{vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y},false);
  GlassShadow(d,w->Pos,{w->Pos.x+w->Size.x,w->Pos.y+w->Size.y},w->WindowRounding,S(1.7f));
  d->PopClipRect();
 }
}
void GlassButtonFace(ImDrawList* d,ImVec2 lo,ImVec2 hi,bool primary,bool hovered,bool held,float radius){
 auto top=primary?paper::rgb(0x488ef7):paper::rgb(0xffffff);
 auto bottom=primary?paper::rgb(0x245fc0):paper::rgb(0xe2edf6);
 if(hovered||held){
  auto tint=held?paper::glass.primaryHeld:paper::glass.primaryHover;
  top=paper::Blend(top,tint,primary?.38f:.14f);bottom=paper::Blend(bottom,tint,primary?.45f:.18f);
 }
 paper::GlassSurface(d,lo,hi,radius,top,bottom,primary?paper::rgb(0x2e6bc7):paper::glass.edge);
 d->AddLine({lo.x+radius,lo.y+S(1.5f)},{hi.x-radius,lo.y+S(1.5f)},ImGui::GetColorU32(IM_COL32(255,255,255,135)));
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
 if(paper::IsGlass())GlassButtonFace(draw,lo,hi,primary||selected,ImGui::IsItemHovered(),ImGui::IsItemActive(),S(10));
 else paper::CutRect(draw,lo,hi,fill,S(6));
 auto end=ImGui::FindRenderedTextEnd(label);auto text=ImGui::CalcTextSize(label,end);
 ImU32 fg=selected?paper::colors->onSignal:primary?paper::colors->onPrimary:tone==ButtonTone::Delete?paper::colors->danger:paper::colors->text;
 draw->PushClipRect(lo,hi,true);
 draw->AddText({lo.x+std::max(0.f,(hi.x-lo.x-text.x)*.5f),lo.y+(hi.y-lo.y-text.y)*.5f},ImGui::GetColorU32(fg),label,end);
 draw->PopClipRect();
 if(ImGui::IsItemFocused())draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted),paper::IsGlass()?S(10):0);
 ImGui::PopFont();return hit;
}
void InlineChip(const char* label){
 auto* font=PaperBold();float size=S(17),pad=S(6);
 float width=font->CalcTextSizeA(size,FLT_MAX,0,label).x+2*pad;
 ImGui::Dummy({width,ImGui::GetFontSize()});auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
 auto* d=ImGui::GetWindowDrawList();if(paper::IsGlass())paper::GlassSurface(d,lo,hi,S(6),IM_COL32_WHITE,paper::glass.keycap,paper::glass.edge);
 else d->AddRectFilled(lo,hi,paper::colors->keycap);
 d->AddText(font,size,{lo.x+pad,lo.y+(hi.y-lo.y-size)*.5f},paper::colors->text,label);
}
bool PanelCheckbox(const char* label,bool* value){
 if(!paper::active)return ImGui::Checkbox(label,value);
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(ImGui::GetStyle().FramePadding.x,S(3)));
 if(paper::IsNight()||paper::IsGlass()){
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
    if(paper::IsGlass())GlassButtonFace(draw,lo,hi,true,ImGui::IsItemHovered(),ImGui::IsItemActive(),S(7));
    else paper::CutRect(draw,lo,hi,ImGui::IsItemHovered()?paper::colors->primaryHover:paper::colors->primary,S(5));
    ImGui::RenderCheckMark(draw,{lo.x+side*.22f,lo.y+side*.22f},ImGui::GetColorU32(paper::colors->onPrimary),side*.56f);
   }else{
    draw->AddRectFilled(lo,hi,ImGui::GetColorU32(ImGui::IsItemHovered()?paper::colors->hover:paper::colors->field),paper::IsGlass()?S(7):0);
    draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted),paper::IsGlass()?S(7):0);
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
bool SidebarCombo(const char* label,const char* preview,ImGuiComboFlags flags=0){
 if(!paper::active)return ImGui::BeginCombo(label,preview,flags);
 ImGui::PushStyleColor(ImGuiCol_FrameBg,paper::IsGlass()?paper::glass.input:paper::colors->button);
 ImGui::PushStyleColor(ImGuiCol_Button,paper::IsGlass()?paper::glass.keycap:paper::colors->hover);
 ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,paper::IsGlass()?S(1):0.f);
 bool open=ImGui::BeginCombo(label,preview,flags);
 ImGui::PopStyleVar();ImGui::PopStyleColor(2);
 return open;
}
void SidebarInline(float width,float gap){
 if(!paper::active){ImGui::SameLine();return;}
 if(ImGui::GetItemRectMax().x+S(gap)+width<=ImGui::GetCurrentWindow()->WorkRect.Max.x)
  ImGui::SameLine(0,S(gap));
}
bool SidebarRadio(const char* label,bool selected){
 if(!paper::active)return ImGui::RadioButton(label,selected);
 auto* window=ImGui::GetCurrentWindow();
 if(window->SkipItems)return false;
 const ImGuiID id=window->GetID(label);
 const auto text=ImGui::CalcTextSize(label);
 const float diameter=S(18),gap=S(8),height=S(31);
 const auto lo=ImGui::GetCursorScreenPos();
 ImRect rect(lo,{lo.x+diameter+gap+text.x,lo.y+height});
 ImGui::ItemSize(rect,(height-text.y)*.5f);
 if(!ImGui::ItemAdd(rect,id))return false;
 bool hovered=false,held=false;
 bool pressed=ImGui::ButtonBehavior(rect,id,&hovered,&held);
 if(pressed)ImGui::MarkItemEdited(id);
 ImGui::RenderNavCursor(rect,id);
 auto* draw=window->DrawList;
 const ImVec2 center(lo.x+diameter*.5f,rect.GetCenter().y);
 draw->AddCircleFilled(center,diameter*.5f,ImGui::GetColorU32(hovered?paper::colors->hover:paper::colors->panel));
 draw->AddCircle(center,diameter*.5f,ImGui::GetColorU32(paper::colors->muted),0,S(1.4f));
 if(selected)draw->AddCircleFilled(center,S(5),ImGui::GetColorU32(paper::colors->radio));
 ImGui::RenderText({lo.x+diameter+gap,center.y-text.y*.5f},label);
 return pressed;
}
bool PanelHeader(const char* label,ImGuiTreeNodeFlags flags,bool current=false){
 if(!paper::active)return ImGui::CollapsingHeader(label,flags);
 ImGui::PushStyleColor(ImGuiCol_Header,current?paper::colors->signal:paper::colors->header);
 ImGui::PushStyleColor(ImGuiCol_HeaderHovered,current?paper::colors->signal:((paper::IsNight()||paper::IsGlass())?paper::colors->hover:paper::colors->muted));
 ImGui::PushStyleColor(ImGuiCol_HeaderActive,current?paper::colors->signal:((paper::IsNight()||paper::IsGlass())?paper::colors->hover:paper::colors->muted));
 ImGui::PushStyleColor(ImGuiCol_Text,current?paper::colors->onSignal:paper::colors->onSelection);
 ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
 auto* d=ImGui::GetWindowDrawList();int start=d->VtxBuffer.Size;
 bool open=ImGui::CollapsingHeader(label,flags);
 if(paper::IsGlass()&&ImGui::IsItemVisible()){
  auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
  ImU32 fill=current?paper::glass.signal:paper::glass.header;
  for(int i=start;i<d->VtxBuffer.Size;++i){
   auto& v=d->VtxBuffer[i];if((v.col&~IM_COL32_A_MASK)!=(fill&~IM_COL32_A_MASK))continue;
   float t=std::clamp((v.pos.y-lo.y)/std::max(1.f,hi.y-lo.y),0.f,1.f);
   ImU32 shade=current?paper::Blend(paper::glass.signalHover,paper::glass.signalHeld,t):
                       paper::Blend(paper::rgb(0xe7f2fd),paper::rgb(0xd2e3f1),t);
   v.col=(shade&~IM_COL32_A_MASK)|(v.col&IM_COL32_A_MASK);
  }
  d->AddRect(lo,hi,ImGui::GetColorU32(paper::glass.edge),S(9));
 }
 ImGui::PopFont();ImGui::PopStyleColor(4);return open;
}
bool PanelModal(const char* label,bool* open,ImGuiWindowFlags flags){
 if(paper::active)ImGui::PushStyleColor(ImGuiCol_Text,paper::colors->onTitle);
 bool shown=ImGui::BeginPopupModal(label,open,flags);
 if(shown&&paper::IsGlass())GlassWindowTint(true,paper::glass.panel);
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
  if(paper::IsGlass())GlassWindowTint(true,paper::glass.dialog);
  else d->AddRectFilled({bar.Min.x+S(10),y-S(8)},{bar.Min.x+S(15),y+S(8)},paper::colors->signal);
  d->AddText(PaperBold(),S(20),{bar.Min.x+S(paper::IsGlass()?18.f:25.f),y-S(10)},paper::colors->onTitle,title);
  float captionSize=S(13),cw=PaperBold()->CalcTextSizeA(captionSize,FLT_MAX,0,caption).x;
  if(!paper::IsGlass()&&bar.GetWidth()>S(40)+ImGui::CalcTextSize(title).x+cw)
   d->AddText(PaperBold(),captionSize,{bar.Max.x-S(12)-cw,y-captionSize*.5f},paper::colors->caption,caption);
  d->PopClipRect();
 }
 return open;
}
bool BeginManager(bool* open){
 const char* label="差分管理器 · 世界版 1.3.2###WorldDiffInternal";
 if(!paper::active)return ImGui::Begin(label,open,ImGuiWindowFlags_NoCollapse);
 ImGui::PushStyleColor(ImGuiCol_Text,paper::colors->onTitle);ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
 bool shown=ImGui::Begin("    差分管理器  |  世界版  1.3.2###WorldDiffInternal",open,ImGuiWindowFlags_NoCollapse);
 ImGui::PopFont();ImGui::PopStyleColor();
 if(shown&&paper::IsGlass())GlassWindowTint(false,paper::glass.window);
 else if(shown){
  auto title=ImGui::GetCurrentWindow()->TitleBarRect();
  ImVec2 p(title.Min.x+S(9),title.Min.y+title.GetHeight()*.25f);
  auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(title.Min,title.Max,false);
  draw->AddRectFilled(p,{p.x+S(6),p.y+title.GetHeight()*.5f},paper::colors->signal);draw->PopClipRect();
 }
 return shown;
}
enum class PaneSurface { List, Presets, Detail, Inset };
void PaperRule(PaneSurface surface=PaneSurface::List){
 if(!paper::active)return;
 auto* w=ImGui::GetCurrentWindow();auto* draw=w->DrawList;
 if(paper::IsGlass()&&surface!=PaneSurface::Inset){
  auto clip=w->ParentWindow->ClipRect;
  draw->PushClipRect(clip.Min,clip.Max,false);
  GlassShadow(draw,w->Pos,{w->Pos.x+w->Size.x,w->Pos.y+w->Size.y},S(18),S(.8f));
  draw->PopClipRect();
 }
 // Clip to the visible part of the parent when a child scrolls out of view.
 draw->PushClipRect(w->OuterRectClipped.Min,w->OuterRectClipped.Max,false);
 if(paper::IsGlass()){
  ImVec2 hi(w->Pos.x+w->Size.x,w->Pos.y+w->Size.y);
  if(surface==PaneSurface::List||surface==PaneSurface::Presets){
   paper::GlassSurface(draw,w->Pos,hi,S(19),paper::rgb(0xf1f6fe),
      surface==PaneSurface::Presets?paper::rgb(0xdcefeb):paper::rgb(0xe0edf5),IM_COL32_WHITE,.85f);
  }else draw->AddRect(w->Pos,hi,ImGui::GetColorU32(surface==PaneSurface::Detail?IM_COL32_WHITE:paper::glass.edge),w->WindowRounding);
 }else draw->AddRectFilled(w->Pos,{w->Pos.x+w->Size.x,w->Pos.y+S(2)},paper::colors->topRule);
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
 if(paper::IsGlass())paper::GlassSurface(draw,lo,hi,height*.5f,IM_COL32_WHITE,paper::glass.keycap,paper::glass.edge);
 else draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted));
 draw->AddText(font,size,{lo.x+pad,right.y-size*.5f},ImGui::GetColorU32(color),text);
 return width;
}
void SectionTitle(const char* label){
 if(!paper::active){ImGui::TextColored(accent,"%s",label);return;}
 if(paper::IsGlass()){ImGui::PushFont(PaperBold(),ImGui::GetFontSize());ImGui::TextUnformatted(label);ImGui::PopFont();return;}
 auto lo=ImGui::GetCursorScreenPos();
 ImGui::GetWindowDrawList()->AddRectFilled(lo,{lo.x+S(5),lo.y+ImGui::GetFontSize()},paper::colors->signal);
 ImGui::Indent(S(12));ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
 ImGui::TextUnformatted(label);ImGui::PopFont();ImGui::Unindent(S(12));
}
void PaperSelection(ImDrawList* draw,ImVec2 lo,ImVec2 hi,ImU32 fill){
  if(paper::IsGlass()){
   float r=(hi.y-lo.y)*.5f;
   draw->AddRectFilled({lo.x,lo.y+S(3)},{hi.x,hi.y+S(3)},ImGui::GetColorU32(IM_COL32(81,107,154,32)),r);
   int begin=draw->VtxBuffer.Size;
   paper::GlassSurface(draw,lo,hi,r,paper::rgb(0xe5f6ff),paper::rgb(0xb6d3f2),paper::rgb(0x80a4ce));
   // Like a tilted lens, the near rim catches more light at the lower right.
   for(int i=begin;i<draw->VtxBuffer.Size;++i){
    auto& v=draw->VtxBuffer[i];
    float x=std::clamp((v.pos.x-lo.x)/(hi.x-lo.x),0.f,1.f);
    float y=std::clamp((v.pos.y-lo.y)/(hi.y-lo.y),0.f,1.f);
    float sheen=std::max(0.f,(x*.48f+y*.52f-.68f)*2.f);
    v.col=(paper::Blend(v.col,paper::rgb(0xf7fcff),sheen)&~IM_COL32_A_MASK)|(v.col&IM_COL32_A_MASK);
   }
   draw->AddRect({lo.x+S(2),lo.y+S(2)},{hi.x-S(2),hi.y-S(2)},ImGui::GetColorU32(IM_COL32(255,255,255,175)),r-S(2));
   ImVec2 dot(lo.x+S(19),lo.y+r);draw->AddCircleFilled(dot,S(11),ImGui::GetColorU32(paper::rgb(0xf8fcff)));
   draw->AddTriangleFilled({dot.x-S(1),dot.y-S(2.5f)},{dot.x+S(3),dot.y},{dot.x-S(1),dot.y+S(2.5f)},ImGui::GetColorU32(paper::glass.signal));
   return;
  }
  paper::CutRect(draw,lo,hi,fill,S(7));float flag=S(paper::IsNight()?22.f:29.f);
  paper::CutRect(draw,lo,{lo.x+flag,hi.y},paper::colors->signal,S(7));
  float x=lo.x+flag*.42f,y=(lo.y+hi.y)*.5f;
  draw->AddTriangleFilled({x,y-S(3)},{x+S(6),y},{x,y+S(3)},ImGui::GetColorU32(paper::colors->onSignal));
}
bool PaperTemplateRow(const app::ProfileCandidate& file,const std::string& rate,bool selected){
 // The caller's filename ID stays stable even when a long name is clipped.
 auto flags=ImGuiSelectableFlags(ImGuiSelectableFlags_NoAutoClosePopups)|ImGuiSelectableFlags_NoPadWithHalfSpacing;
 ImGui::PushStyleColor(ImGuiCol_Header,ImU32(0));
 ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImU32(0));ImGui::PushStyleColor(ImGuiCol_HeaderActive,ImU32(0));
 bool hit=ImGui::Selectable("##paperTemplate",selected,flags,{0,S(42)});
 ImGui::PopStyleColor(3);
 if(!ImGui::IsItemVisible())return hit;
 auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();auto* d=ImGui::GetWindowDrawList();
 bool highlighted=selected||hit;
 bool hovered=ImGui::IsItemHovered();
 if(highlighted)PaperSelection(d,lo,hi,paper::colors->fileActive);
 else if(paper::IsGlass())paper::GlassSurface(d,lo,hi,S(21),hovered?paper::glass.hover:paper::glass.fileIdle,paper::glass.fileIdle,paper::glass.rule);
 else paper::CutRect(d,lo,hi,hovered?paper::colors->hover:paper::colors->fileIdle,S(7));
 ImU32 fg=highlighted?paper::colors->onSelection:paper::colors->text;
 float y=(lo.y+hi.y)*.5f,right=hi.x-S(12);
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
 if(ImGui::IsItemFocused())draw->AddRect(lo,hi,ImGui::GetColorU32(paper::colors->muted),paper::IsGlass()?S(10):0);
 if(badge)hi.x-=PaperBadge(draw,{hi.x-S(12),(hi.y+lo.y)*.5f},S(22),badge,selected?paper::colors->onSelection:paper::colors->text)+S(10);
 auto fg=ImGui::GetColorU32(selected?paper::colors->onSelection:paper::colors->text);float size=ImGui::GetFontSize();
 auto font=selected?PaperBold():ImGui::GetFont();
 ImVec2 start(lo.x+left,lo.y+(hi.y-lo.y-size)*.5f);
 draw->PushClipRect({start.x,lo.y},{hi.x-S(8),hi.y},true);draw->AddText(font,size,start,fg,label);draw->PopClipRect();
 if(hovered&&font->CalcTextSizeA(size,FLT_MAX,0,label).x>hi.x-S(8)-start.x)ImGui::SetTooltip("%s",label);
}
// Faint open contour lines occupy only the free space below the preset rows.
void PaperContours(size_t rows){
 if(!paper::active||paper::IsNight()||paper::IsGlass())return;
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

