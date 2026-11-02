#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
namespace worlddiff::notes {
// A small, inert Markdown subset: headings, list items, and plain paragraphs.
// File contents are displayed as text, never used as format strings or commands.
enum class Kind {Heading,Item,Text};
struct Line {Kind kind=Kind::Text;std::string text;};
struct Document {std::vector<Line> lines;std::string error;};
constexpr size_t kMaxBytes=256*1024;
inline bool ValidUtf8(const std::string& text){
 for(size_t i=0;i<text.size();){
  unsigned char c=(unsigned char)text[i++];if(c<0x80){if(!c)return false;continue;}
  unsigned count=0;uint32_t point=0,min=0;
  if(c>=0xc2&&c<=0xdf){count=1;point=c&0x1f;min=0x80;}
  else if(c>=0xe0&&c<=0xef){count=2;point=c&0xf;min=0x800;}
  else if(c>=0xf0&&c<=0xf4){count=3;point=c&7;min=0x10000;}
  else return false;
  if(i+count>text.size())return false;
  while(count--){c=(unsigned char)text[i++];if((c&0xc0)!=0x80)return false;point=(point<<6)|(c&0x3f);}
  if(point<min||point>0x10ffff||(point>=0xd800&&point<=0xdfff))return false;
 }return true;
}
inline Document Parse(std::string text){
 Document result;
 if(text.size()>kMaxBytes){result.error="更新日志过大，请保持在 256 KB 以内。";return result;}
 if(text.starts_with("\xef\xbb\xbf"))text.erase(0,3);
 if(!ValidUtf8(text)){result.error="更新日志编码无效，请在记事本中另存为 UTF-8。";return result;}
 std::istringstream in(text);std::string line;
 while(std::getline(in,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  auto first=line.find_first_not_of(" \t"),last=line.find_last_not_of(" \t");if(first==std::string::npos)continue;line=line.substr(first,last-first+1);
  size_t hashes=line.find_first_not_of('#');
  if(hashes>0&&hashes<=6&&hashes<line.size()&&line[hashes]==' ')result.lines.push_back({Kind::Heading,line.substr(hashes+1)});
  else if(line.starts_with("- ")||line.starts_with("* "))result.lines.push_back({Kind::Item,line.substr(2)});
  else result.lines.push_back({Kind::Text,std::move(line)});
 }
 return result;
}
inline Document Read(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);if(!in)return {{},"未能读取更新日志，请确认文件存在且可以访问。"};
 std::string text(kMaxBytes+1,'\0');in.read(text.data(),(std::streamsize)text.size());auto count=in.gcount();
 if(in.bad())return {{},"读取更新日志失败，请保存文件后重试。"};text.resize((size_t)count);return Parse(std::move(text));
}
// Entering the tab and pressing Reload are the only read points. No disk I/O
// is performed on each rendered frame, and a failed reload drops stale text.
struct Cache {
 std::filesystem::path path;Document document;bool loaded=false;
 void Load(const std::filesystem::path& file){path=file;document=Read(file);loaded=true;}
};
inline bool ShowStartupNotice(bool enabled,bool panelOpen,uint32_t elapsed){return enabled&&!panelOpen&&elapsed<9000;}
}
