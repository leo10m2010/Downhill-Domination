// SPDX-License-Identifier: GPL-3.0-or-later
// Browser platform adapter; physics, contacts, controls and race are shared with dhview.
#include <SDL3/SDL.h>
#include <emscripten.h>
#include <emscripten/heap.h>
#include "gles_renderer.hpp"
#include "race.hpp"
#include "controls.hpp"
#include <memory>

EM_JS(void, report_metrics, (double fps, double frameMs, double cpuMs, double heap, double speed, int gates, int total, int state, int countdown, double time, int calls, int chunks, int dropped), {
    if (Module.onMetrics) Module.onMetrics({fps,frameMs,cpuMs,heap,speed,gates,total,state,countdown,time,calls,chunks,dropped});
});
namespace {
struct App {
    SDL_Window* window=nullptr; SDL_GLContext context=nullptr;
    std::unique_ptr<gfx::GLESRenderer> renderer;
    std::array<webassets::Model,4> models;
    std::array<int,4> meshes{-1,-1,-1,-1};
    Ground ground; Gates gates; Race race; V3 start{},camera{};float heading=0,camHeading=0,minBikeY=0,visualLean=0;
    std::string error; bool ready=false,playing=false,paused=true,hop=false,respawn=false,restart=false;
    uint8_t inputs=0;int width=1280,height=720,nextLoad=0,nextUpload=0,dropped=0;
    double last=0,acc=0,metricStart=0,cpuTotal=0;int frames=0;
} app;
bool read(const char* name,std::vector<uint8_t>& raw,bool optional=false){
    std::string path=std::string("/game/")+name;FILE* f=std::fopen(path.c_str(),"rb");
    if(!f){if(!optional)app.error="Falta "+std::string(name);return optional;}
    std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);
    if(size<0||size>long(webassets::kMaxFile)){std::fclose(f);app.error="Archivo demasiado grande: "+std::string(name);return false;}
    raw.resize(size);bool ok=std::fread(raw.data(),1,raw.size(),f)==raw.size();std::fclose(f);if(!ok)app.error="No se pudo leer "+std::string(name);return ok;
}
void resetRace(){
    auto& p=app.race.run.bike.P;p.engineSteer=true;p.hop=32.f;p.grip=8.f;p.reverseAccel=18.f;p.reverseMax=7.f;
    app.race.load(app.ground,&app.gates,app.start,app.heading);app.camHeading=app.heading;
    const auto& b=app.race.run.bike;app.camera=b.pos+V3{-b.fwd.x*11.f,4.5f,-b.fwd.z*11.f};app.acc=0;app.visualLean=0;
}
void frame(){
    double now=emscripten_get_now(),elapsed=app.last?(now-app.last)/1000:0;app.last=now;
    SDL_Event event;while(SDL_PollEvent(&event)){if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST){app.inputs=0;app.hop=false;}}
    if(!app.ready||!app.playing||app.paused){app.acc=0;return;}
    double begin=emscripten_get_now();
    if(app.restart){resetRace();app.restart=false;app.hop=false;}if(app.respawn){app.race.run.respawn();app.respawn=false;}
    // Same 60 Hz external step as desktop. Bound catch-up and count discarded time.
    if(elapsed>.1){++app.dropped;elapsed=.1;}app.acc+=elapsed;
    while(app.acc>=1.0/60.0){BikeInput in=unpackInput(app.inputs|(app.hop?kInHop:0));app.hop=false;app.race.update(app.ground,in,1.f/60.f);app.acc-=1.0/60.0;}
    const Bike& bike=app.race.run.bike;Axes axes=bike.axes();V3 velocity=bike.vel();
    float dt=float(elapsed),hs=std::sqrt(velocity.x*velocity.x+velocity.z*velocity.z),want=hs>4?std::atan2(velocity.x,-velocity.z):bike.heading();
    float delta=std::remainder(want-app.camHeading,6.2831853f);app.camHeading+=delta*std::min(1.f,3.f*dt);
    V3 target=bike.pos+axes.u*.5f,desired=target+V3{-std::sin(app.camHeading)*11.f,4.5f,std::cos(app.camHeading)*11.f};
    app.camera=app.camera+(desired-app.camera)*std::min(1.f,8.f*dt);
    auto floor=app.ground.groundQuery(app.camera.x,app.camera.y+60.f,app.camera.z,0.f,400.f);if(floor.hit)app.camera.y=std::max(app.camera.y,floor.height+3.f);
    auto vp=gfx::multiply(gfx::perspective(float(app.width)/app.height),gfx::lookAt(app.camera,target));
    glViewport(0,0,app.width,app.height);glClearColor(.14f,.19f,.23f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    auto& renderer=*app.renderer;renderer.begin(vp,app.camera);auto matrix=gfx::identity();
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);matrix[12]=app.camera.x;matrix[13]=app.camera.y;matrix[14]=app.camera.z;renderer.draw(app.meshes[3],matrix.data(),1.f);
    glEnable(GL_DEPTH_TEST);matrix=gfx::identity();renderer.draw(app.meshes[2],matrix.data(),1.f);glDepthMask(GL_TRUE);
    renderer.draw(app.meshes[0],matrix.data(),app.models[0].chunks.empty()?2.f:1.f);
    float steer=unpackInput(app.inputs).steer;app.visualLean+=(-steer*std::min(.45f,bike.speed()*.012f)*(bike.grounded?1.f:0.f)-app.visualLean)*std::min(1.f,6.f*dt);
    V3 r=rotateAbout(axes.r,axes.f,app.visualLean),u=rotateAbout(axes.u,axes.f,app.visualLean);float scale=.28f*3.2808f;
    V3 t=bike.pos+axes.u*(-2.05f-app.minBikeY*scale);
    matrix={r.x*scale,r.y*scale,r.z*scale,0,u.x*scale,u.y*scale,u.z*scale,0,-axes.f.x*scale,-axes.f.y*scale,-axes.f.z*scale,0,t.x,t.y,t.z,1};
    renderer.draw(app.meshes[1],matrix.data(),2.6f);SDL_GL_SwapWindow(app.window);
    app.cpuTotal+=emscripten_get_now()-begin;++app.frames;
    if(now-app.metricStart>=500){double span=now-app.metricStart;report_metrics(app.frames*1000/span,span/app.frames,app.cpuTotal/app.frames,double(emscripten_get_heap_size()),1.0973*bike.speed(),int(app.race.run.gs.counter),int(app.gates.size()),int(app.race.state),app.race.countdownDigit(),app.race.displayTime(),renderer.drawCalls,renderer.visibleGroups,app.dropped);app.metricStart=now;app.frames=0;app.cpuTotal=0;}
}
}
extern "C" {
EMSCRIPTEN_KEEPALIVE const char* dh_error(){return app.error.c_str();}
// Separate bounded loading stages let the browser paint actual progress between files.
EMSCRIPTEN_KEEPALIVE int dh_load(int stage){
    try {
        if(stage==0){app.ready=app.playing=false;app.paused=true;app.error.clear();app.inputs=0;app.hop=app.restart=app.respawn=false;app.nextLoad=app.nextUpload=0;app.renderer.reset();app.models={};app.ground=Ground{};app.gates=Gates{};app.race=Race{};}
        if(stage!=app.nextLoad||stage<0||stage>6){app.error="Orden de carga inválido";return 0;}
        const char* names[]={"ALP2.mdl","bike1.mdl","ALP2.sky.mdl","ALP2.dome.mdl","ALP2.col","ALP2.gates","ALP2.start.pts"};std::vector<uint8_t> bytes;
        if(!read(names[stage],bytes,stage==2||stage==3))return 0;
        if(stage<4){if(!bytes.empty()&&!webassets::model(bytes,app.models[stage],stage!=1,app.error))return 0;}
        if(stage==4){if(!webassets::collision(bytes,app.error)||!app.ground.load(bytes))return 0;openStartGate(app.ground);}
        if(stage==5){if(!webassets::gates(bytes,app.error)||!app.gates.load(bytes)||!app.gates.size()){app.error="Puertas del recorrido incompatibles";return 0;}const Gate* g=app.gates.courseGate(0);app.heading=std::atan2(g->n.x,-g->n.z);}
        if(stage==6){std::array<float,3> p;if(!webassets::start(bytes,p,app.error))return 0;app.start={p[0],p[2],-p[1]};resetRace();app.minBikeY=std::numeric_limits<float>::max();for(size_t i=1;i<app.models[1].vertices.size();i+=10)app.minBikeY=std::min(app.minBikeY,app.models[1].vertices[i]);}
        ++app.nextLoad;return 1;
    }catch(const std::exception& e){app.error=e.what();return 0;}
}
EMSCRIPTEN_KEEPALIVE int dh_stage(int stage){
    try{
        if(app.nextLoad!=7){app.error="Recursos aún no cargados";return 0;}
        if(stage==0){app.ready=false;app.nextUpload=0;app.renderer.reset();app.renderer=std::make_unique<gfx::GLESRenderer>();app.meshes={-1,-1,-1,-1};}
        if(stage!=app.nextUpload||stage<0||stage>4){app.error="Orden de subida GPU inválido";return 0;}
        if(stage<4&&!app.models[stage].indices.empty())app.meshes[stage]=app.renderer->upload(app.models[stage]);
        GLenum error=glGetError();if(error!=GL_NO_ERROR){app.error="Error GPU al subir recursos: "+std::to_string(error);return 0;}
        if(stage==4){app.ready=true;app.last=emscripten_get_now();}++app.nextUpload;return 1;
    }catch(const std::exception& e){app.error=e.what();return 0;}
}
EMSCRIPTEN_KEEPALIVE int dh_play(){if(!app.ready)return 0;app.playing=true;app.paused=false;app.acc=0;app.last=app.metricStart=emscripten_get_now();app.frames=0;app.cpuTotal=0;return 1;}
EMSCRIPTEN_KEEPALIVE void dh_pause(int pause){app.paused=pause!=0;if(pause==2&&app.renderer)app.renderer->abandonContext();app.inputs=0;app.hop=false;app.acc=0;app.last=app.metricStart=emscripten_get_now();app.frames=0;app.cpuTotal=0;}
EMSCRIPTEN_KEEPALIVE void dh_input(int bits,int action){app.inputs=uint8_t(bits)&~kInHop;if(bits&kInHop)app.hop=true;if(action==1)app.respawn=true;if(action==2)app.restart=true;}
EMSCRIPTEN_KEEPALIVE void dh_resize(int width,int height){app.width=std::clamp(width,320,1920);app.height=std::clamp(height,180,1080);if(app.window)SDL_SetWindowSize(app.window,app.width,app.height);}
}
int main(){
    if(!SDL_Init(SDL_INIT_VIDEO)){app.error=SDL_GetError();return 1;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    app.window=SDL_CreateWindow("Downhill Web",app.width,app.height,SDL_WINDOW_OPENGL);
    if(app.window)app.context=SDL_GL_CreateContext(app.window);
    if(!app.context){app.error=SDL_GetError();return 1;}
    SDL_GL_SetSwapInterval(1);emscripten_set_main_loop(frame,0,0);return 0;
}
