// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic records only; exercise malformed input before allocating or building a grid.
#include "../src/web_assets.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while(0)
template<class T> void add(std::vector<uint8_t>& b,T x){const auto* p=reinterpret_cast<const uint8_t*>(&x);b.insert(b.end(),p,p+sizeof x);}
std::vector<uint8_t> triangle(){
    std::vector<uint8_t> b{'D','H','M','2'};add(b,0u);add(b,3u);add(b,3u);
    for(int i=0;i<3;i++){float v[10]={float(i),float(i+1),float(i+2),0,0,1,1,1,1,-1};for(float f:v)add(b,f);}add(b,0u);add(b,1u);add(b,2u);return b;
}
int main(){
    std::string error;webassets::Model m;auto b=triangle();CHECK(webassets::model(b,m,true,error));CHECK(m.vertices[1]==2);CHECK(m.vertices[2]==-1);
    for(size_t n=0;n<b.size();n++){std::vector<uint8_t> truncated(b.begin(),b.begin()+n);CHECK(!webassets::model(truncated,m,false,error));}
    auto bad=b;uint32_t index=3;std::memcpy(bad.data()+bad.size()-4,&index,4);CHECK(!webassets::model(bad,m,false,error));
    bad=b;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+16,&nan,4);CHECK(!webassets::model(bad,m,false,error));
    bad=b;uint32_t huge=0xffffffffu;std::memcpy(bad.data()+8,&huge,4);CHECK(!webassets::model(bad,m,false,error));
    bad=b;float tex=0;std::memcpy(bad.data()+16+36,&tex,4);CHECK(!webassets::model(bad,m,false,error));
    bad=b;bad.push_back(0);CHECK(!webassets::model(bad,m,false,error));
    auto v3=b;v3[3]='3';add(v3,1u);add(v3,0u);add(v3,3u);add(v3,1u);for(float f: {0.f,1.f,2.f,3.f,100.f})add(v3,f);CHECK(webassets::model(v3,m,true,error));CHECK(m.chunks[0].selectors[0][1]==2);
    bad=v3;float neg=-1;std::memcpy(bad.data()+bad.size()-4,&neg,4);CHECK(!webassets::model(bad,m,true,error));
    std::vector<uint8_t> col;add(col,1u);for(float f:{0.f,0.f,0.f,10.f,0.f,0.f,0.f,10.f,0.f})add(col,f);add(col,0u);CHECK(webassets::collision(col,error));
    bad=col;std::memcpy(bad.data()+4,&nan,4);CHECK(!webassets::collision(bad,error));
    bad=col;float big=200000,small=-big;std::memcpy(bad.data()+4,&small,4);std::memcpy(bad.data()+8,&small,4);std::memcpy(bad.data()+16,&big,4);std::memcpy(bad.data()+32,&big,4);CHECK(!webassets::collision(bad,error));
    std::vector<uint8_t> g;add(g,1u);add(g,8050u);add(g,0u);for(float f:{0.f,1.f,0.f,50.f})add(g,f);CHECK(webassets::gates(g,error));bad=g;std::memcpy(bad.data()+12,&nan,4);CHECK(!webassets::gates(bad,error));
    std::vector<uint8_t> start;for(float f:{1.f,2.f,3.f})add(start,f);std::array<float,3> p;CHECK(webassets::start(start,p,error));start.pop_back();CHECK(!webassets::start(start,p,error));
    std::puts("web_assets_test OK (synthetic data only)");
}
