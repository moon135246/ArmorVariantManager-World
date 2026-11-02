#include <cstdio>
#include <cstdlib>
#define IM_ASSERT(expr) do { if(!(expr)) { std::fprintf(stderr,"ImGui assertion: %s at %s:%d\n",#expr,__FILE__,__LINE__); std::exit(3); } } while(0)
