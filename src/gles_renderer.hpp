// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <GLES3/gl3.h>
#include "web_assets.hpp"
#include "ground.hpp"
#include <stdexcept>

namespace gfx {
using Matrix = std::array<float, 16>;
inline Matrix identity() { return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; }
inline Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix c{}; for (int col = 0; col < 4; ++col) for (int row = 0; row < 4; ++row) for (int k = 0; k < 4; ++k) c[col*4+row] += a[k*4+row]*b[col*4+k]; return c;
}
inline Matrix perspective(float aspect) {
    float f = 1.f/std::tan(65.f*3.14159265f/360.f), n = .2f, z = 30000.f;
    return {f/aspect,0,0,0, 0,f,0,0, 0,0,(z+n)/(n-z),-1, 0,0,2*z*n/(n-z),0};
}
inline V3 normalized(V3 v) { float l = std::sqrt(dot(v,v)); return l > 1e-6f ? v*(1.f/l) : V3{}; }
inline Matrix lookAt(V3 eye, V3 target) {
    V3 f = normalized(target-eye), r = normalized(cross(f,{0,1,0})), u = cross(r,f);
    return {r.x,u.x,-f.x,0, r.y,u.y,-f.y,0, r.z,u.z,-f.z,0, -dot(r,eye),-dot(u,eye),dot(f,eye),1};
}
class GLESRenderer : public Renderer {
    struct Batch { uint32_t first, count; int texture; };
    struct Group { std::vector<Batch> batches; std::vector<std::array<float,5>> selectors; };
    struct Mesh { GLuint vao=0,vbo=0,ebo=0; size_t floats=0; std::vector<TextureId> textures; std::vector<Group> groups; };
    GLuint program=0, white=0; GLint matrixLocation=-1,gainLocation=-1; Matrix vp=identity(); V3 camera{};
    std::vector<GLuint> textures; std::vector<Mesh> meshes;
    static GLuint shader(GLenum type, const char* source) {
        GLuint s=glCreateShader(type); glShaderSource(s,1,&source,nullptr); glCompileShader(s); GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if (!ok) { char log[2048]; glGetShaderInfoLog(s,sizeof log,nullptr,log); glDeleteShader(s); throw std::runtime_error(log); } return s;
    }
public:
    unsigned drawCalls=0,visibleGroups=0;
    GLESRenderer() {
        const char* vs=R"(#version 300 es
precision highp float;
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 color;
uniform mat4 mvp;
out vec2 texcoord; out vec4 vertexcolor;
void main(){gl_Position=mvp*vec4(position,1.0);texcoord=uv;vertexcolor=color;}
)";
        const char* fs=R"(#version 300 es
precision highp float;
in vec2 texcoord; in vec4 vertexcolor;
uniform sampler2D image; uniform float gain;
out vec4 fragment;
void main(){vec4 c=texture(image,texcoord)*vertexcolor;fragment=vec4(c.rgb*gain,c.a);}
)";
        GLuint a=shader(GL_VERTEX_SHADER,vs),b=shader(GL_FRAGMENT_SHADER,fs); program=glCreateProgram(); glAttachShader(program,a); glAttachShader(program,b); glLinkProgram(program); glDeleteShader(a);glDeleteShader(b);
        GLint ok=0; glGetProgramiv(program,GL_LINK_STATUS,&ok); if(!ok) { char log[2048]; glGetProgramInfoLog(program,sizeof log,nullptr,log); glDeleteProgram(program); program=0; throw std::runtime_error(log); }
        matrixLocation=glGetUniformLocation(program,"mvp");gainLocation=glGetUniformLocation(program,"gain");
        glUseProgram(program);glUniform1i(glGetUniformLocation(program,"image"),0);
        Texture t{1,1,{255,255,255,255}}; TextureId id=createTexture(t); white=textures[id];
    }
    ~GLESRenderer() override {
        for(auto& m:meshes){glDeleteVertexArrays(1,&m.vao);glDeleteBuffers(1,&m.vbo);glDeleteBuffers(1,&m.ebo);}
        for(GLuint t:textures)glDeleteTextures(1,&t); if(program)glDeleteProgram(program);
    }
    // Context loss already frees these GPU objects. Their handles must never be
    // deleted through the restored context (WebGL rejects stale-context objects).
    void abandonContext(){meshes.clear();textures.clear();program=white=0;}
    TextureId createTexture(const Texture& t) override {
        GLint max=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max);if(t.w>max||t.h>max)throw std::runtime_error("Textura excede el límite de esta GPU");
        GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,t.w,t.h,0,GL_RGBA,GL_UNSIGNED_BYTE,t.rgba.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        textures.push_back(id);return int(textures.size()-1);
    }
    MeshId createMesh(const std::vector<float>& v,const std::vector<uint32_t>& i,const std::vector<TextureId>& ts,bool dynamic) override {
        Mesh m;m.textures=ts;m.floats=v.size();glGenVertexArrays(1,&m.vao);glBindVertexArray(m.vao);glGenBuffers(1,&m.vbo);glBindBuffer(GL_ARRAY_BUFFER,m.vbo);glBufferData(GL_ARRAY_BUFFER,v.size()*4,v.data(),dynamic?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
        glGenBuffers(1,&m.ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,i.size()*4,i.data(),GL_STATIC_DRAW);
        const int counts[3]={3,2,4};const size_t offsets[3]={0,12,20};for(GLuint a=0;a<3;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,counts[a],GL_FLOAT,GL_FALSE,40,reinterpret_cast<void*>(offsets[a]));}
        Group g;
        for(uint32_t a=0;a<i.size();){int tid=int(v[size_t(i[a])*10+9]);uint32_t b=a+3;while(b<i.size()&&int(v[size_t(i[b])*10+9])==tid)b+=3;g.batches.push_back({a,b-a,tid});a=b;}
        m.groups.push_back(std::move(g));meshes.push_back(std::move(m));return int(meshes.size()-1);
    }
    MeshId upload(const webassets::Model& m) {
        std::vector<TextureId> ts;for(const auto& t:m.textures)ts.push_back(createTexture(t));
        MeshId id=createMesh(m.vertices,m.indices,ts,false);auto& mesh=meshes[id];
        if(m.chunks.empty())return id;
        mesh.groups.clear();
        auto group=[&](uint32_t first,uint32_t count,const std::vector<std::array<float,5>>& sels){
            Group g;g.selectors=sels;uint32_t end=first+count;
            for(uint32_t a=first;a<end;){int tid=int(m.vertices[size_t(m.indices[a])*10+9]);uint32_t b=a+3;while(b<end&&int(m.vertices[size_t(m.indices[b])*10+9])==tid)b+=3;g.batches.push_back({a,b-a,tid});a=b;}mesh.groups.push_back(std::move(g));
        };
        for(const auto& c:m.chunks)group(c.first,c.count,c.selectors);
        return id;
    }
    void updateVertices(MeshId id,const std::vector<float>& v) override {auto& m=meshes.at(id);if(v.size()!=m.floats)return;glBindBuffer(GL_ARRAY_BUFFER,m.vbo);glBufferSubData(GL_ARRAY_BUFFER,0,v.size()*4,v.data());}
    void begin(const Matrix& viewProjection,V3 eye){vp=viewProjection;camera=eye;drawCalls=visibleGroups=0;glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glActiveTexture(GL_TEXTURE0);}
    void draw(MeshId id,const float model[16],float gain) override {
        if(id<0)return;auto& m=meshes.at(id);Matrix transform;std::copy(model,model+16,transform.begin());Matrix matrix=multiply(vp,transform);glUseProgram(program);glUniformMatrix4fv(matrixLocation,1,GL_FALSE,matrix.data());glUniform1f(gainLocation,gain);glBindVertexArray(m.vao);
        for(const auto& g:m.groups){bool visible=true;for(const auto& s:g.selectors){V3 d=camera-V3{s[0],s[1],s[2]};float lim=std::sqrt(s[4])+s[3];if(dot(d,d)>lim*lim){visible=false;break;}}if(!visible)continue;++visibleGroups;
            for(const auto& b:g.batches){GLuint t=b.texture>=0&&size_t(b.texture)<m.textures.size()?textures.at(m.textures[b.texture]):white;glBindTexture(GL_TEXTURE_2D,t);glDrawElements(GL_TRIANGLES,b.count,GL_UNSIGNED_INT,reinterpret_cast<void*>(size_t(b.first)*4));++drawCalls;}}
    }
    void drawQuads2D(const std::vector<Quad>&,int,int) override {throw std::runtime_error("HUD web se presenta en HTML");}
    void drawTexQuads2D(TextureId,const std::vector<TexQuad>&,int,int) override {throw std::runtime_error("HUD texturizado pendiente en web");}
};
}
