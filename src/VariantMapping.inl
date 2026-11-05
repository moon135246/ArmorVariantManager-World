// Material names identify candidates. Geometry only helps distinguish siblings.
struct VariantMapping {
 struct Row {Target target;uint32_t vertices=0,indices=0,lod=0;};
 struct Bucket {std::vector<Row> oldRows,newRows;};
 std::map<Target,Target> matches;
 std::map<Target,std::vector<Target>> sourceMaterials,destMaterials;
 std::map<Target,Target> materialOf;
 std::set<uint32_t> compatibleNames;
 std::set<std::string> parts;
 std::set<Target> conflictingMembers;
 std::vector<std::string> missingNames,addedNames;
 size_t oldNames=0,newNames=0,sharedNames=0,unknownNames=0,controlled=0,resolved=0;
 bool malformed=false,catalogFallback=false;

 VariantMapping(const File& file,const std::vector<const Model*>& models){
  catalogFallback=file.catalogFallback;
  std::map<uint32_t,Bucket> buckets;
  std::map<uint32_t,std::string> currentNames;
  std::set<uint32_t> collisions,oldHashes;
  std::set<Target> seen,claimed;
  std::set<std::string> identical;
  for(auto* model:models){
   auto path=Normalize(model->relativePath);
   auto stamp=file.stamps.find(path);
   if(stamp!=file.stamps.end()&&stamp->second==ModelStamp(*model))identical.insert(path);
   for(auto& mesh:model->meshes){
    if(mesh.materialId>=model->materials.size()){malformed=true;continue;}
    const auto& mat=model->materials[mesh.materialId];
    if(currentNames.count(mat.hash)&&currentNames.at(mat.hash)!=mat.name)collisions.insert(mat.hash);
    currentNames[mat.hash]=mat.name;
    Target row{path,mesh.index,true},material{path,mat.hash,false};
    buckets[mat.hash].newRows.push_back({row,mesh.vertexCount,mesh.indexCount,mesh.lod});
    destMaterials[material].push_back(row);materialOf[row]=material;
   }
  }
  for(auto& b:file.meshes){
   Target row{b.model,b.index,true};
   if(!seen.insert(row).second){malformed=true;continue;}
   buckets[b.material].oldRows.push_back({row,b.vertices,b.indices,b.lod});
   sourceMaterials[{b.model,b.material,false}].push_back(row);oldHashes.insert(b.material);
  }
  std::set<Target> targets;
  for(auto& g:file.document.groups){
   targets.insert(g.members.begin(),g.members.end());
   auto collect=[&](const Rules& r){targets.insert(r.hidden.begin(),r.hidden.end());targets.insert(r.shown.begin(),r.shown.end());};
   collect(g.working);for(auto& preset:g.presets)collect(preset.rules);
  }
  for(auto& [target,view]:file.document.meshDisplay)targets.insert(target);
  for(auto& t:targets)if(!t.mesh)oldHashes.insert(t.value);
  oldNames=oldHashes.size();newNames=currentNames.size();
  for(auto hash:oldHashes){
   auto old=file.materialNames.find(hash);auto now=currentNames.find(hash);
   if(old==file.materialNames.end()){unknownNames++;continue;}
   if(now!=currentNames.end()&&old->second==now->second&&!collisions.count(hash)){
    compatibleNames.insert(hash);sharedNames++;
   }else missingNames.push_back(old->second);
  }
  for(auto& [hash,name]:currentNames)if(!compatibleNames.count(hash))addedNames.push_back(name);
  std::sort(missingNames.begin(),missingNames.end());std::sort(addedNames.begin(),addedNames.end());
  auto link=[&](const Target& old,const Target& dest){
   matches.emplace(old,dest);claimed.insert(dest);parts.insert(ProfilePart(dest.model));
  };
  for(auto& [hash,bucket]:buckets){
   if(collisions.count(hash)||bucket.oldRows.empty()||bucket.newRows.empty())continue;
   bool named=compatibleNames.count(hash)!=0;
   // A missing historical name may use an exact geometric match in manual import.
   if(!named&&file.materialNames.count(hash))continue;
   if(named&&bucket.oldRows.size()==1&&bucket.newRows.size()==1){
    link(bucket.oldRows.front().target,bucket.newRows.front().target);continue;
   }
   for(auto& old:bucket.oldRows)if(identical.count(old.target.model)){
    for(auto& now:bucket.newRows)if(old.target==now.target){link(old.target,now.target);break;}
   }
   // Commit only mutually unique, non-dominated candidates. Conflicting evidence
   // (same part versus matching geometry elsewhere) leaves both candidates pending.
   bool progress=true;
   while(progress){
    progress=false;
    struct Edge {Target old,dest;unsigned evidence;};
    std::vector<Edge> edges;
    for(auto& old:bucket.oldRows)if(!matches.count(old.target))
     for(auto& now:bucket.newRows)if(!claimed.count(now.target)){
      bool geometry=old.vertices&&old.indices&&old.vertices==now.vertices&&old.indices==now.indices;
      bool lod=old.lod==now.lod;
      if(!named&&!(geometry&&lod))continue;
      unsigned evidence=(ProfilePart(old.target.model)==ProfilePart(now.target.model)?1u:0u)|(lod?2u:0u)|(geometry?4u:0u);
      if(evidence)edges.push_back({old.target,now.target,evidence});
     }
    std::map<Target,unsigned> oldEvidence,newEvidence;
    for(auto& edge:edges){
     oldEvidence[edge.old]|=1u<<edge.evidence;
     newEvidence[edge.dest]|=1u<<edge.evidence;
    }
    std::vector<Edge> best;
    std::map<Target,size_t> oldCounts,newCounts;
    for(auto& edge:edges){
     unsigned alternatives=oldEvidence.at(edge.old)|newEvidence.at(edge.dest);
     bool dominated=false;
     for(unsigned mask=1;mask<=7;mask++)
      if(mask!=edge.evidence&&(alternatives&(1u<<mask))&&(edge.evidence|mask)==mask){dominated=true;break;}
     if(!dominated){best.push_back(edge);oldCounts[edge.old]++;newCounts[edge.dest]++;}
    }
    for(auto& edge:best)if(oldCounts.at(edge.old)==1&&newCounts.at(edge.dest)==1){
     link(edge.old,edge.dest);progress=true;
    }
   }
  }
  std::map<Target,std::map<uint64_t,std::set<Target>>> owners;
  for(auto& g:file.document.groups)if(!g.main&&!g.global)
   for(auto& member:g.members)if(auto dest=resolve(member))owners[*dest][g.id].insert(member);
  for(auto& [dest,groups]:owners)if(groups.size()>1)
   for(auto& [id,members]:groups)conflictingMembers.insert(members.begin(),members.end());
  controlled=targets.size();
  for(auto& target:targets)if(resolve(target))resolved++;
 }
 double similarity()const{
  size_t combined=oldNames+newNames-sharedNames;
  return combined?double(sharedNames)/combined:0;
 }
 bool sameFamily()const{
  // This threshold identifies the template family only. Each rule still needs
  // its own unambiguous mapping, and unresolved rules remain in the document.
  return !malformed&&!catalogFallback&&!unknownNames&&similarity()>=.95;
 }
 bool strong()const{return sameFamily()&&(!controlled||resolved>0);}
 std::optional<Target> resolve(const Target& target)const{
  if(conflictingMembers.count(target))return {};
  if(target.mesh){
   auto hit=matches.find(target);
   if(hit!=matches.end())return hit->second;
   return {};
  }
  if(!compatibleNames.count(target.value))return {};
  std::vector<Target> any,samePart;
  for(auto& [dest,rows]:destMaterials)if(dest.value==target.value){
   any.push_back(dest);if(ProfilePart(dest.model)==ProfilePart(target.model))samePart.push_back(dest);
  }
  if(any.size()==1)return any.front();
  // A whole-material rule intentionally includes newly added meshes. Use the
  // resolved original mesh locations to disambiguate repeated material names.
  auto original=sourceMaterials.find(target);
  if(original!=sourceMaterials.end()){
   std::set<Target> locations;
   for(auto& row:original->second){
    auto hit=matches.find(row);
    if(hit!=matches.end())locations.insert(materialOf.at(hit->second));
   }
   if(locations.size()==1)return *locations.begin();
   if(locations.size()>1)return {};
  }
  if(samePart.size()==1)return samePart.front();
  return {};
 }
};

// An impossible game-resource prefix keeps unresolved targets inert even if their
// old mesh index is occupied. Their original part and geometry travel in the WDP.
struct PendingTargets {
 const File& file;std::string scope;
 std::map<std::string,std::string> aliases;
 std::set<Target> recorded,parked;
 std::vector<MeshBinding> bindings;
 PendingTargets(const File& f,const std::string& equip):file(f),scope(equip){}
 Target park(const Target& target){
  auto& alias=aliases[target.model];
  if(alias.empty())alias="?variant\\"+std::to_string(aliases.size())+"\\f_equip\\"+scope+"\\"+ProfilePart(target.model)+"\\pending";
  Target inert{alias,target.value,target.mesh};
  if(!parked.insert(inert).second)return inert;
  for(const auto& b:file.meshes){
   if(b.model!=target.model||(target.mesh?b.index!=target.value:b.material!=target.value))continue;
   if(recorded.insert({alias,b.index,true}).second){bindings.push_back(b);bindings.back().model=alias;}
  }
  return inert;
 }
};
