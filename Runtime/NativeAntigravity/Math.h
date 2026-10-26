#pragma once
#include <algorithm>
#include <cmath>

namespace ag {
struct V3 { float x{},y{},z{}; };
struct Q { float x{},y{},z{},w{1}; };
inline V3 operator+(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V3 operator-(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V3 operator-(V3 a){return {-a.x,-a.y,-a.z};}
inline V3 operator*(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(V3 a){return std::sqrt(dot(a,a));}
inline bool finite(V3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline V3 unit(V3 a){float n=length(a);return std::isfinite(n)&&n>.00001f?a*(1/n):V3{};}
inline V3 plane(V3 a,V3 n){return a-n*dot(a,n);}
inline Q operator*(Q a,Q b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline Q unit(Q q){float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return n>.00001f?Q{q.x/n,q.y/n,q.z/n,q.w/n}:Q{};}
inline Q inverse(Q q){return {-q.x,-q.y,-q.z,q.w};}
inline V3 rotate(Q q,V3 v){V3 t=cross({q.x,q.y,q.z},v)*2;return v+t*q.w+cross({q.x,q.y,q.z},t);}
inline Q axis(V3 n,float radians){n=unit(n);float s=std::sin(radians*.5f);return {n.x*s,n.y*s,n.z*s,std::cos(radians*.5f)};}
inline Q fromTo(V3 a,V3 b){a=unit(a);b=unit(b);float d=std::clamp(dot(a,b),-1.f,1.f);if(d<-.9999f)return axis(unit(cross(a,std::abs(a.x)<.9f?V3{1,0,0}:V3{0,0,1})),3.141592654f);V3 c=cross(a,b);return unit(Q{c.x,c.y,c.z,1+d});}
inline Q slerp(Q a,Q b,float t){float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;if(d<0){b={-b.x,-b.y,-b.z,-b.w};d=-d;}float x=1-t,y=t;if(d<.9995f){float theta=std::acos(std::clamp(d,-1.f,1.f)),s=std::sin(theta);x=std::sin((1-t)*theta)/s;y=std::sin(t*theta)/s;}return unit(Q{a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y,a.w*x+b.w*y});}
inline Q look(V3 f,V3 up){f=unit(f);V3 r=unit(cross(up,f)),u=cross(f,r);float t=r.x+u.y+f.z;Q q;if(t>0){float s=std::sqrt(t+1)*2;q={(u.z-f.y)/s,(f.x-r.z)/s,(r.y-u.x)/s,s*.25f};}else if(r.x>u.y&&r.x>f.z){float s=std::sqrt(1+r.x-u.y-f.z)*2;q={s*.25f,(u.x+r.y)/s,(f.x+r.z)/s,(u.z-f.y)/s};}else if(u.y>f.z){float s=std::sqrt(1+u.y-r.x-f.z)*2;q={(u.x+r.y)/s,s*.25f,(f.y+u.z)/s,(f.x-r.z)/s};}else{float s=std::sqrt(1+f.z-r.x-u.y)*2;q={(f.x+r.z)/s,(f.y+u.z)/s,s*.25f,(r.y-u.x)/s};}return unit(q);}
inline float angle(V3 a,V3 b){return std::acos(std::clamp(dot(unit(a),unit(b)),-1.f,1.f));}
inline float signedAngle(V3 a,V3 b,V3 up){a=unit(a);b=unit(b);return std::atan2(dot(cross(a,b),up),dot(a,b));}
inline float blend(float dt,float seconds){return 1-std::exp(-std::clamp(dt,0.f,.1f)/seconds);}
inline float toward(float a,float b,float step){return a+std::clamp(b-a,-step,step);}
inline constexpr V3 WorldUp{0,1,0}, Forward{0,0,1};
inline constexpr float Rad=3.141592654f/180.f;
}
