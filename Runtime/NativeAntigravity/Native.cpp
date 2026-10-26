#include "Game.h"
#include "vendor/minhook/include/MinHook.h"
#include <atomic>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <vector>

using namespace ag;
namespace {
Game g;HMODULE self{};std::mutex setup;std::atomic<bool> ready{};bool bound{},fault{};
std::filesystem::path logPath;
void log(const std::string& s){std::ofstream out(logPath,std::ios::app);out<<GetTickCount64()<<" "<<s<<"\n";}
void fail(const std::exception& e){if(!fault)log(std::string("Runtime disabled: ")+e.what());fault=true;}
using Step=void(*)(void*,float,const void*);
using Rotation=void(*)(void*,Q*,float,const void*);
using Velocity=void(*)(void*,V3*,float,const void*);
using Input=void(*)(void*,bool,const void*);
using Steering=void(*)(void*,V3,V3*,V3*,float*,float,float,float,float,const void*);
using Camera=void(*)(void*,float,V3*,Q*,float*,const void*);
using Axis=void(*)(void*,V3*,float,const void*);
using Contact=bool(*)(void*,void*,const void*);
using Void=void(*)(void*,const void*);
using Slope=void(*)(void*,V3*,V3*,float*,float,const void*);
Step beforeOriginal{},afterOriginal{},jumpGroundOriginal{},aiOriginal{},visualKartOriginal{};
Rotation rotationOriginal{};Velocity velocityOriginal{},physicsVelocityOriginal{},jumpOriginal{};
Input controllerJumpOriginal{},physicsJumpOriginal{},controllerDriftOriginal{},physicsDriftOriginal{};
Steering steeringOriginal{};Camera cameraOriginal{};Axis axisOriginal{};
Contact contactOriginal{};Void bounceOriginal{},resetOriginal{},visualAnimatorOriginal{},visualCharacterOriginal{};
Slope slopeOriginal{};
void* controllerTypeObject{};void* getParentComponent{};
int visualPlayerField{};
struct State {
 void *controller{},*motor{},*physics{},*player{};uintptr_t root{};
 V3 up{WorldUp},forward{Forward},roadRight{1,0,0};
 int layers{};float elapsed{},hopTime{},forwardSpeed{},aiSteer{},aiSide{};bool hop{},cpu{},canDrift{},guided{};
};
std::array<State,64> states;
struct CamState {void* camera{};void* player{};Q frame{};V3 up{WorldUp},forward{Forward},smoothed{};float distance{},weight{},clearance{};bool engaged{},hasClearance{};};
std::array<CamState,16> cameras;
State* state(void* c){for(auto& s:states)if(s.controller==c&&c)return &s;return nullptr;}
State* physicsState(void* p){return p?state(field<void*>(p,g.pController)):nullptr;}
bool cpu(State* s){return s&&s->cpu;}
bool mustUnground(State& s){return field<bool>(s.motor,g.mUnground)||field<float>(s.motor,g.mUngroundTime)>0;}
void* report(State& s){return static_cast<char*>(s.motor)+g.ground;}
bool found(State& s){return field<bool>(report(s),g.gFound);}
bool onlineAllowed(){return !g.internet();}
bool context(){
 static int last=-1;static bool allowed{};int frame=g.statik<int>(g.frameCount);
 if(frame!=last){last=frame;allowed=!fault&&onlineAllowed()&&g.staticField<int>(g.raceState)==g.running&&g.statik<float>(g.timeScale)>0&&g.staticField<void*>(g.menu)==nullptr;}
 return allowed;
}
bool racer(void* c){
 if(!g.alive(c))return false;auto p=field<void*>(c,g.parent),physics=field<void*>(c,g.cPhysics),outside=field<void*>(c,g.outside);
 return g.local(p)&&g.alive(physics)&&g.alive(outside)&&!field<bool>(outside,g.respawning)&&!field<bool>(c,g.tank)&&!g.instance<bool>(g.isReplay,c)&&!g.instance<bool>(g.isTeleport,physics)&&!field<bool>(physics,g.catapult);
}
bool marked(void* collider){
 if(!g.alive(collider))return false;auto transform=g.instance<void*>(g.transform,collider);
 return transform&&g.instance<void*>(g.find,transform,g.marker)!=nullptr;
}
bool cast(V3 origin,float radius,V3 direction,float distance,int mask,void*& collider,V3& normal,float* hitDistance=nullptr){
 alignas(16) std::array<char,128> hit{};
 if(!g.statik<bool>(g.sphere,origin,radius,direction,hit.data(),distance,mask,1))return false;
 collider=g.instance<void*>(g.hitCollider,hit.data());normal=field<V3>(hit.data(),g.hitNormal);
 if(hitDistance)*hitDistance=field<float>(hit.data(),g.hitDistance);return collider!=nullptr&&finite(normal);
}
V3 bottom(State& s){return field<V3>(s.motor,g.mPosition)+rotate(field<Q>(s.motor,g.mRotation),g.instance<V3>(g.motorBottom,s.motor));}
bool close(State& s){
 if(found(s)&&marked(field<void*>(report(s),g.gCollider)))return true;
 void* collider{};V3 normal{};return cast(bottom(s)+s.up*.3f,.02f,-s.up,.45f,field<int>(s.motor,g.mLayers),collider,normal)&&marked(collider);
}
void axes(State& s,V3 velocity){
 field<V3>(s.physics,g.axesF)=s.forward;field<V3>(s.physics,g.axesR)=unit(cross(s.up,s.forward));field<V3>(s.physics,g.axesU)=s.up;
 V3 signedForward=field<bool>(s.physics,g.backward)?-s.forward:s.forward;
 V3 acceleration=unit(unit(plane(velocity,s.up))+signedForward);
 field<V3>(s.physics,g.acceleration)=dot(acceleration,acceleration)>.5f?acceleration:s.forward;
}
void release(State& s){
 if(g.alive(s.motor)){
  field<int>(s.motor,g.mStable)=s.layers;
  V3 f=unit(plane(s.forward,WorldUp));if(dot(f,f)<.5f)f=Forward;
  g.instance<void>(g.setRotation,s.motor,look(f,WorldUp));
  if(g.alive(s.physics)){s.forward=f;s.up=WorldUp;axes(s,field<V3>(s.motor,g.mBase));field<Q>(s.physics,g.yaw)=look(f,WorldUp);}
 }
 g.unroot(s.root);s=State{};
}
void clear(){for(auto& s:states)if(s.controller)release(s);cameras={};}
void after(void* c,float dt,const void* mi){
 afterOriginal(c,dt,mi);if(!ready)return;
 try{
  State* s=state(c);if(!context()){if(s&&(fault||!onlineAllowed()||g.staticField<int>(g.raceState)!=g.running))release(*s);return;}
  if(!racer(c)){if(s)release(*s);return;}
  void* motor=field<void*>(c,g.cMotor);if(!g.alive(motor))return;
  void* report=static_cast<char*>(motor)+g.ground;bool grounded=field<bool>(report,g.gFound);void* collider=grounded?field<void*>(report,g.gCollider):nullptr;
  V3 normal=grounded?field<V3>(report,g.gNormal):V3{};dt=std::clamp(dt,0.f,.05f);
  if(grounded&&!marked(collider)){if(s)release(*s);return;}
  if(!s){
   if(!grounded||!marked(collider)||!finite(normal)||length(normal)<.5f)return;
   for(auto& slot:states)if(slot.controller&&!g.alive(slot.controller))release(slot);
   for(auto& slot:states)if(!slot.controller){s=&slot;break;}if(!s)return;
   s->controller=c;s->motor=motor;s->physics=field<void*>(c,g.cPhysics);s->player=field<void*>(c,g.parent);s->root=g.root(c);s->cpu=g.cpu(c);s->layers=field<int>(motor,g.mStable);
   s->up=rotate(field<Q>(motor,g.mRotation),WorldUp);s->forward=rotate(field<Q>(motor,g.mRotation),Forward);log("Marked surface attached; CPU="+std::to_string(s->cpu));
  }
  if(s->hop){s->hopTime+=dt;if(!mustUnground(*s)&&s->hopTime>.08f&&dot(field<V3>(motor,g.mBase),s->up)<=.5f&&close(*s)){s->hop=false;s->hopTime=0;}}
  if(!grounded){
   s->elapsed+=dt;float radius=std::clamp(field<float>(motor,g.mRadius)*.5f,.08f,.4f);
   if(!cast(bottom(*s)+s->up*(radius+.1f),radius,-s->up,(s->hop?2.5f:1.5f)+radius,field<int>(motor,g.mLayers),collider,normal)||!marked(collider)){
    if(s->elapsed>(s->hop?.8f:.35f))release(*s);return;
   }
  }
  s->elapsed=0;normal=unit(normal);Q transport=fromTo(s->up,normal);
  normal=unit(rotate(slerp(Q{},transport,blend(dt,.025f)),s->up));transport=fromTo(s->up,normal);
  s->forward=unit(plane(rotate(transport,s->forward),normal));if(s->hop)field<V3>(motor,g.mBase)=rotate(transport,field<V3>(motor,g.mBase));s->up=normal;
  int excluded=field<int>(c,g.wall1)|field<int>(c,g.wall2);field<int>(motor,g.mStable)=(s->layers|field<int>(motor,g.mLayers))&~excluded;
  float speed=dot(field<V3>(motor,g.mBase),s->forward);if(speed>1)s->forwardSpeed=std::max(s->forwardSpeed,std::min(speed,160.f));axes(*s,field<V3>(motor,g.mBase));
 }catch(const std::exception& e){fail(e);}
}
void before(void* c,float dt,const void* mi){
 if(ready)try{if(auto s=state(c);s&&context())g.instance<void>(g.setRotation,s->motor,look(s->forward,s->up));}catch(const std::exception& e){fail(e);}
 beforeOriginal(c,dt,mi);
}
void rotation(void* c,Q* q,float dt,const void* mi){rotationOriginal(c,q,dt,mi);if(ready)try{if(auto s=state(c);s&&context())*q=look(s->forward,s->up);}catch(const std::exception& e){fail(e);}}
void axisDirections(void* p,V3* v,float dt,const void* mi){axisOriginal(p,v,dt,mi);if(ready)try{if(auto s=physicsState(p);s&&context())axes(*s,*v);}catch(const std::exception& e){fail(e);}}
void slope(void* p,V3* v,V3* a,float* value,float dt,const void* mi){if(auto s=ready?physicsState(p):nullptr;s&&context()&&angle(s->up,WorldUp)>=70*Rad)return;slopeOriginal(p,v,a,value,dt,mi);}
struct RestoreBool {void* object{};int offset{};bool value{};~RestoreBool(){if(object)field<bool>(object,offset)=value;}};
void physicsVelocity(void* p,V3* v,float dt,const void* mi){
 State* s=nullptr;RestoreBool cap{},kick{};
 if(ready)try{if(context())s=physicsState(p);if(s){if(angle(s->up,WorldUp)>70*Rad||s->hop){cap={p,g.verticalCap,field<bool>(p,g.verticalCap)};field<bool>(p,g.verticalCap)=false;}
  if(s->cpu){kick={p,g.driftKick,field<bool>(p,g.driftKick)};field<bool>(p,g.driftKick)=false;}}}catch(const std::exception& e){fail(e);s=nullptr;}
 physicsVelocityOriginal(p,v,dt,mi);
 if(s)try{if(!found(*s)&&(angle(s->up,WorldUp)>45*Rad||s->hop)){V3 scratch{},delta{};g.instance<void>(g.gravity,p,&scratch,&delta,dt);*v=*v-delta+rotate(fromTo(WorldUp,s->up),delta);}}catch(const std::exception& e){fail(e);}
}
void velocity(void* c,V3* v,float dt,const void* mi){
 velocityOriginal(c,v,dt,mi);if(!ready)return;
 try{auto s=state(c);if(!s||!context()||s->hop||mustUnground(*s)||!finite(*v))return;
  float speed=length(*v);V3 tangent=unit(plane(*v,s->up));if(length(tangent)>.5f)*v=tangent*speed;
  if(angle(s->up,WorldUp)>65*Rad&&!field<bool>(s->physics,g.backward)){
   float forward=dot(*v,s->forward),target=std::max(forward,s->forwardSpeed-35*std::clamp(dt,0.f,.05f));
   if(target>8&&forward<target-1.5f)*v=*v+s->forward*(target-forward);s->forwardSpeed=target;
  }
  if(s->cpu&&s->guided&&close(*s)){V3 right=unit(plane(s->roadRight,s->up));float side=dot(*v,right),next=std::clamp(toward(side,s->aiSide,35*std::clamp(dt,0.f,.05f)),-8.f,8.f);*v=*v+right*(next-side);}
  float normal=dot(*v,s->up);if(normal>-1)*v=*v-s->up*(normal+1);
 }catch(const std::exception& e){fail(e);}
}
void steering(void* p,V3 before,V3* v,V3* a,float* scalar,float dt,float x,float y,float z,const void* mi){
 State* s=nullptr;Q yaw{},toSurface{};V3 saved[4]{};bool applied=false;
 if(ready)try{if(context())s=physicsState(p);if(s){yaw=field<Q>(p,g.yaw);V3 flatForward=unit(plane(rotate(yaw,Forward),WorldUp));if(length(flatForward)>.5f){
  Q flat=look(flatForward,WorldUp);toSurface=look(s->forward,s->up)*inverse(flat);Q toFlat=inverse(toSurface);
  saved[0]=field<V3>(p,g.axesF);saved[1]=field<V3>(p,g.axesR);saved[2]=field<V3>(p,g.axesU);saved[3]=field<V3>(p,g.acceleration);
  before=rotate(toFlat,before);*v=rotate(toFlat,*v);*a=rotate(toFlat,*a);field<V3>(p,g.axesF)=flatForward;field<V3>(p,g.axesR)=cross(WorldUp,flatForward);field<V3>(p,g.axesU)=WorldUp;
  field<V3>(p,g.acceleration)=rotate(toFlat,saved[3]);field<V3>(p,g.centripetal)=rotate(toFlat,field<V3>(p,g.centripetal));applied=true;
 }}}catch(const std::exception& e){fail(e);}
 steeringOriginal(p,before,v,a,scalar,dt,x,y,z,mi);
 if(applied){*v=rotate(toSurface,*v);*a=rotate(toSurface,*a);field<V3>(p,g.centripetal)=rotate(toSurface,field<V3>(p,g.centripetal));field<V3>(p,g.axesF)=saved[0];field<V3>(p,g.axesR)=saved[1];field<V3>(p,g.axesU)=saved[2];field<V3>(p,g.acceleration)=saved[3];
  Q delta=inverse(yaw)*field<Q>(p,g.yaw);if(delta.w<0)delta={-delta.x,-delta.y,-delta.z,-delta.w};float turn=2*std::atan2(delta.y,delta.w);
  if(!s->cpu||!s->guided)s->forward=unit(plane(rotate(axis(s->up,turn),s->forward),s->up));axes(*s,*v);
 }
}
void jump(void* p,V3* v,float dt,const void* mi){
 State* s=ready?physicsState(p):nullptr;if(s&&context()&&s->cpu)return;V3 old=*v;jumpOriginal(p,v,dt,mi);
 if(s&&context()){V3 delta=*v-old;if(length(delta)>.001f){s->hop=true;s->hopTime=0;}*v=old+rotate(fromTo(WorldUp,s->up),delta);float n=dot(*v,s->up),ceiling=std::max(0.f,field<float>(p,g.jumpStrength));if(n>ceiling)*v=*v-s->up*(n-ceiling);}
}
void jumpGround(void* p,float dt,const void* mi){
 State* s=ready?physicsState(p):nullptr;void* r=nullptr;bool old{};
 if(s&&context()&&!s->hop&&!mustUnground(*s)&&close(*s)){r=report(*s);old=field<bool>(r,g.gFound);field<bool>(r,g.gFound)=true;}
 jumpGroundOriginal(p,dt,mi);if(r)field<bool>(r,g.gFound)=old;
}
void inputJump(void* object,bool pressed,const void* mi,bool controller){
 void* p=controller?field<void*>(object,g.cPhysics):object;State* s=ready?physicsState(p):nullptr;bool active=s&&context();if(active&&s->cpu)pressed=false;
 (controller?controllerJumpOriginal:physicsJumpOriginal)(object,pressed,mi);
 if(active&&pressed&&!s->hop&&close(*s))field<bool>(p,g.runJump)=true;
}
void controllerJump(void* c,bool b,const void* mi){inputJump(c,b,mi,true);}void physicsJump(void* p,bool b,const void* mi){inputJump(p,b,mi,false);}
void controllerDrift(void* c,bool b,const void* mi){if(auto s=ready?state(c):nullptr;s&&context()&&s->cpu)b&=s->canDrift;controllerDriftOriginal(c,b,mi);}
void physicsDrift(void* p,bool b,const void* mi){if(auto s=ready?physicsState(p):nullptr;s&&context()&&s->cpu)b&=s->canDrift;physicsDriftOriginal(p,b,mi);}
void ai(void* object,float dt,const void* mi){
 if(!ready){aiOriginal(object,dt,mi);return;}
 try{auto c=field<void*>(object,g.aiController);auto s=state(c);if(!s||!s->cpu||!context()){aiOriginal(object,dt,mi);return;}
  V3 direction=s->forward,right=cross(s->up,s->forward);float lateral=0;bool valid=false;auto follower=field<void*>(object,g.aiFollower);
  if(follower){auto spline=field<void*>(follower,g.fSpline);float t=field<float>(follower,g.fProgress);if(spline&&std::isfinite(t)){
   V3 offset=g.instance<V3>(g.splinePoint,spline,t)-field<V3>(s->motor,g.mPosition),tangent=unit(plane(g.instance<V3>(g.splineTangent,spline,t),s->up));
   lateral=dot(offset,cross(s->up,tangent));valid=finite(offset)&&length(tangent)>.5f&&std::abs(dot(offset,s->up))<4&&std::abs(lateral)<12;
   if(valid){right=cross(s->up,tangent);direction=tangent*12+right*lateral;}
  }}
  float a=signedAngle(s->forward,direction,s->up),limit=100*Rad*std::clamp(dt,0.f,.05f);s->forward=unit(rotate(axis(s->up,std::clamp(a,-limit,limit)),s->forward));s->aiSteer=std::clamp(a/(45*Rad),-1.f,1.f);
  s->roadRight=valid?right:cross(s->up,s->forward);s->aiSide=valid?std::clamp(lateral*2,-6.f,6.f):0;s->guided=true;s->canDrift=valid&&std::abs(lateral)<3&&std::abs(a)<15*Rad;
  bool drift=field<bool>(s->physics,g.drifting);if(drift&&std::abs(a)>5*Rad&&a*field<float>(s->physics,g.driftDir)<0)s->canDrift=false;
  if(!s->canDrift){g.instance<void>(g.stopDrift,s->physics);field<bool>(c,g.cDrift)=field<bool>(s->physics,g.pDrift)=false;}
  field<float>(c,g.cSteer)=field<float>(s->physics,g.steer)=s->aiSteer;
  auto input=field<void*>(object,g.aiInput);if(input){auto data=field<void*>(input,g.inputData);if(data){field<float>(data,g.inputSteer)=s->aiSteer;if(!s->canDrift)field<bool>(data,g.inputDrift)=false;}}
  if(field<bool>(object,g.aiBoost)&&field<bool>(s->physics,g.drifting))g.instance<bool>(g.boost,object);
 }catch(const std::exception& e){fail(e);}
}
struct VisualScope {
 void* visual{};bool ground{},shown{};V3 normal{};float distance{};
 explicit VisualScope(void* v){if(!v||!ready)return;for(auto& s:states)if(s.controller&&s.player==field<void*>(v,g.vPlayer)&&context()&&!s.hop&&!mustUnground(s)&&close(s)){
  visual=v;ground=field<bool>(v,g.vGround);shown=field<bool>(v,g.vVisualGround);normal=field<V3>(v,g.vNormal);distance=field<float>(v,g.vDistance);
  field<bool>(v,g.vGround)=field<bool>(v,g.vVisualGround)=true;field<V3>(v,g.vNormal)=s.up;field<float>(v,g.vDistance)=0;break;
 }}
 ~VisualScope(){if(visual){field<bool>(visual,g.vGround)=ground;field<bool>(visual,g.vVisualGround)=shown;field<V3>(visual,g.vNormal)=normal;field<float>(visual,g.vDistance)=distance;}}
};
void visualKart(void* o,float dt,const void* mi){VisualScope scope(field<void*>(o,g.visualKart));visualKartOriginal(o,dt,mi);}
void visualAnimator(void* o,const void* mi){VisualScope scope(field<void*>(o,g.visualAnimator));visualAnimatorOriginal(o,mi);}
void visualCharacter(void* o,const void* mi){VisualScope scope(field<void*>(o,g.visualCharacter));visualCharacterOriginal(o,mi);}
void camera(void* o,float dt,V3* position,Q* rotation,float* fov,const void* mi){
 cameraOriginal(o,dt,position,rotation,fov,mi);if(!ready||fault)return;
 try{
  auto p=field<void*>(o,g.camPlayer);if(!p||field<int>(p,g.pType)!=g.human||field<bool>(o,g.camSpectator)||field<float>(o,g.camStrength)<.95f)return;
  State* s=nullptr;for(auto& e:states)if(e.controller&&e.player==p){s=&e;break;}
  CamState* cs=nullptr;for(auto& e:cameras)if(e.camera==o){cs=&e;break;}
  bool active=s&&context();if(!active&&!cs)return;
  if(!cs){for(auto& e:cameras)if(!e.camera){cs=&e;break;}if(!cs)return;cs->camera=o;cs->player=p;}
  if(active){float tilt=angle(s->up,WorldUp);cs->engaged=tilt>(cs->engaged?44:52)*Rad;active=cs->engaged;}
  auto visual=field<void*>(p,g.pVisual);if(!visual)return;V3 kart=field<V3>(visual,g.vPosition);if(!cs->distance)cs->distance=std::max(3.f,length(*position-kart));
  dt=std::clamp(dt,0.f,.1f);if(active){cs->up=s->up;cs->forward=s->forward;cs->frame=slerp(cs->frame,fromTo(rotate(cs->frame,WorldUp),s->up)*cs->frame,blend(dt,.08f));}
  cs->weight=toward(cs->weight,active?1.f:0.f,dt/(active?.45f:.8f));if(cs->weight<=0){*cs=CamState{};return;}
  float w=cs->weight*cs->weight*(3-2*cs->weight);V3 up=rotate(cs->frame,WorldUp),f=unit(plane(cs->forward,up)),prior=unit(plane(cs->smoothed,up));
  if(length(prior)>.5f)f=unit(rotate(slerp(Q{},fromTo(prior,f),blend(dt,.035f)),prior));cs->smoothed=f;
  V3 desired=kart-f*(cs->distance*.94f)+up*(cs->distance*.35f);desired=desired+cs->up*std::max(0.f,1.25f-dot(desired-kart,cs->up));V3 pivot=kart+cs->up*1.25f,offset=desired-pivot;float distance=length(offset);
  if(s&&distance>.01f){void* collider{};V3 normal{};float hit{};if(cast(pivot,.35f,unit(offset),distance,field<int>(s->motor,g.mLayers),collider,normal,&hit))distance=std::max(0.f,hit-.1f);}
  cs->clearance=!cs->hasClearance||distance<cs->clearance?distance:cs->clearance+(distance-cs->clearance)*blend(dt,.08f);cs->hasClearance=true;desired=pivot+unit(offset)*cs->clearance;
  Q q=look(kart+f+up*.65f-desired,up);*rotation=slerp(*rotation,q,w);*position=*position+(desired-*position)*w;
 }catch(const std::exception& e){fail(e);}
}
bool contact(void* c,void* collider,const void* mi){
 if(ready&&context()&&collider&&g.local(field<void*>(c,g.parent))){auto other=g.instance<void*>(getParentComponent,collider,controllerTypeObject);if(other&&other!=c&&(state(c)||state(other)))return false;}
 return contactOriginal(c,collider,mi);
}
void bounce(void* c,const void* mi){
 if(ready&&context()){auto m=field<void*>(c,g.cMotor);bool skip=state(c)!=nullptr;if(m)for(auto& s:states)if(s.controller){float reach=field<float>(s.motor,g.mRadius)+field<float>(m,g.mRadius)+2;if(length(field<V3>(m,g.mPosition)-field<V3>(s.motor,g.mPosition))<reach){skip=true;break;}}
  if(skip){field<bool>(c,g.bounceFlag)=false;field<V3>(c,g.bounceImpulse)={};return;}}
 bounceOriginal(c,mi);
}
void reset(void* o,const void* mi){if(ready)clear();resetOriginal(o,mi);}
bool denyOnline(){static std::atomic<bool> warned{};if(!warned.exchange(true))log("Online room operation blocked while native Antigravity is loaded.");return false;}
void denyLeaderboard(){log("Leaderboard upload blocked while native Antigravity is loaded.");}
struct Hook {void* target{};void* detour{};void** original{};};std::vector<Hook> hooks;
template<class T>void hook(void* method,T detour,T& original){hooks.push_back({g.rt.pointer(method),reinterpret_cast<void*>(detour),reinterpret_cast<void**>(&original)});}
void bindHooks(){
 auto cm=[&](const char* n,std::initializer_list<const char*> p,const char* r="System.Void"){return g.cm(n,p,r);};
 hook(cm("BeforeCharacterUpdate",{"System.Single"}),before,beforeOriginal);hook(cm("AfterCharacterUpdate",{"System.Single"}),after,afterOriginal);
 hook(cm("UpdateRotation",{"UnityEngine.Quaternion&","System.Single"}),rotation,rotationOriginal);hook(cm("UpdateVelocity",{"UnityEngine.Vector3&","System.Single"}),velocity,velocityOriginal);
 hook(g.rt.method(g.physics,"UpdateVelocity",{"UnityEngine.Vector3&","System.Single"},"System.Void"),physicsVelocity,physicsVelocityOriginal);
 hook(g.rt.method(g.physics,"CalculatePhysicsAxisDirections",{"UnityEngine.Vector3&","System.Single"},"System.Void"),axisDirections,axisOriginal);
 hook(g.rt.method(g.physics,"UpdateSlopeVelocity",{"UnityEngine.Vector3&","UnityEngine.Vector3&","System.Single&","System.Single"},"System.Void"),slope,slopeOriginal);
 hook(g.rt.method(g.physics,"UpdateSteeringCentiperalForceAndOrient",{"UnityEngine.Vector3","UnityEngine.Vector3&","UnityEngine.Vector3&","System.Single&","System.Single","System.Single","System.Single","System.Single"},"System.Void"),steering,steeringOriginal);
 hook(g.rt.method(g.physics,"TriggerPhysicsJump",{"UnityEngine.Vector3&","System.Single"},"System.Void"),jump,jumpOriginal);
 hook(g.rt.method(g.physics,"CheckAndDecideIfAllowedToJumpInPhysics",{"System.Single"},"System.Void"),jumpGround,jumpGroundOriginal);
 hook(g.rt.method(g.controller,"JumpInput",{"System.Boolean"},"System.Void"),controllerJump,controllerJumpOriginal);hook(g.rt.method(g.physics,"JumpInput",{"System.Boolean"},"System.Void"),physicsJump,physicsJumpOriginal);
 hook(g.rt.method(g.controller,"DriftInput",{"System.Boolean"},"System.Void"),controllerDrift,controllerDriftOriginal);hook(g.rt.method(g.physics,"DriftInput",{"System.Boolean"},"System.Void"),physicsDrift,physicsDriftOriginal);
 hook(g.rt.method(g.rt.type("","AILogicController"),"RefreshSteeringToPoint",{"System.Single"},"System.Void"),ai,aiOriginal);
 hook(g.rt.method(g.camera,"GetRacingCamPositionAndRotation",{"System.Single","UnityEngine.Vector3&","UnityEngine.Quaternion&","System.Single&"},"System.Void"),camera,cameraOriginal);
 hook(g.rt.method(g.rt.type("","Ant_VisualKart"),"UnityCustomUpdate",{"System.Single"},"System.Void"),visualKart,visualKartOriginal);
 hook(g.rt.method(g.rt.type("","Ant_KartAnimatorWorker"),"Update",{},"System.Void"),visualAnimator,visualAnimatorOriginal);hook(g.rt.method(g.rt.type("","Ant_CharacterWorker"),"Update",{},"System.Void"),visualCharacter,visualCharacterOriginal);
 hook(g.rt.method(g.controller,"UpdatePlayerPlayerCollision",{},"System.Void"),bounce,bounceOriginal);hook(cm("IsColliderValidForCollisions",{"UnityEngine.Collider"},"System.Boolean"),contact,contactOriginal);
 hook(g.rt.method(g.rt.type("","PlayerRaceLogic"),"OnRaceResetted",{},"System.Void"),reset,resetOriginal);
 auto co=g.rt.type("UnityEngine","Component");getParentComponent=g.rt.method(co,"GetComponentInParent",{"System.Type"},"UnityEngine.Component");
 auto type=g.rt.api<const void*(*)(void*)>("il2cpp_class_get_type")(g.controller);controllerTypeObject=g.rt.api<void*(*)(const void*)>("il2cpp_type_get_object")(type);
 const char* operations[]={"JoinLobby","JoinRoom","JoinRandomRoom","JoinOrCreateRoom","JoinRandomOrCreateRoom","CreateRoom","ReconnectAndRejoin"};
 for(auto name:operations){bool found=false;void* it{};while(auto m=g.rt.api<void*(*)(void*,void**)>("il2cpp_class_get_methods")(g.photon,&it)){
  if(std::strcmp(g.rt.api<const char*(*)(void*)>("il2cpp_method_get_name")(m),name)||g.rt.typeName(g.rt.api<const void*(*)(void*)>("il2cpp_method_get_return_type")(m))!="System.Boolean")continue;
  hooks.push_back({g.rt.pointer(m),reinterpret_cast<void*>(denyOnline),nullptr});found=true;
 }if(!found)throw std::runtime_error(std::string("Missing online guard: ")+name);}
 void* leader{};void* it{};auto klass=g.rt.type("","KartersLeaderboardsManager");
 while(auto m=g.rt.api<void*(*)(void*,void**)>("il2cpp_class_get_methods")(klass,&it)){
  if(std::strcmp(g.rt.api<const char*(*)(void*)>("il2cpp_method_get_name")(m),"RaceFinished_UploadLeaderboard")||g.rt.api<uint32_t(*)(void*)>("il2cpp_method_get_param_count")(m)!=5)continue;
  if(leader)throw std::runtime_error("Ambiguous leaderboard guard.");
  auto parameterType=[&](unsigned n){return g.rt.typeName(g.rt.api<const void*(*)(void*,uint32_t)>("il2cpp_method_get_param")(m,n));};
  if(parameterType(0)!="System.Single"||parameterType(1)!="System.Single[]"||parameterType(2).find("CCheckpointTimes")==std::string::npos||parameterType(3)!="System.Action"||parameterType(4)!="System.Action"||g.rt.typeName(g.rt.api<const void*(*)(void*)>("il2cpp_method_get_return_type")(m))!="System.Void")throw std::runtime_error("Changed leaderboard upload signature.");leader=m;
 }if(!leader)throw std::runtime_error("Missing leaderboard guard.");
 hooks.push_back({g.rt.pointer(leader),reinterpret_cast<void*>(denyLeaderboard),nullptr});
}
}
extern "C" __declspec(dllexport) DWORD WINAPI Initialize(void* probe){
 std::lock_guard<std::mutex> lock(setup);
 struct Detach {~Detach(){if(g.workerThread){g.rt.api<void(*)(void*)>("il2cpp_thread_detach")(g.workerThread);g.workerThread=nullptr;}}} detach;
 try{
  if(logPath.empty()){wchar_t path[32768]{};GetModuleFileNameW(self,path,32768);logPath=std::filesystem::path(path).parent_path()/L"TK2-Antigravity.log";}
  if(ready){log("Already active.");return 2;}
  V3 up=WorldUp,forward=Forward;
  for(int i=1;i<=72;i++){
   V3 next=rotate(axis({1,0,0},i*5*Rad),WorldUp);forward=rotate(fromTo(up,next),forward);up=next;Q frame=look(forward,up);
   if(!finite(forward)||length(rotate(frame,WorldUp)-up)>.001f||length(rotate(inverse(frame),forward)-Forward)>.001f)throw std::runtime_error("Road-frame math self-check failed.");
  }
  g.rt.module=GetModuleHandleW(L"GameAssembly.dll");if(!g.rt.module)throw std::runtime_error("GameAssembly.dll is not loaded.");
  auto domain=g.rt.api<void*(*)()>("il2cpp_domain_get")();if(!domain)throw std::runtime_error("Wait until the main menu.");
  g.workerThread=g.rt.api<void*(*)(void*)>("il2cpp_thread_attach")(domain);
  if(!bound){g.bind();bindHooks();bound=true;log("All native metadata, field and hook contracts resolved: "+std::to_string(hooks.size()));}
  if(probe){g.unroot(g.markerHandle);g.markerHandle=0;bound=false;hooks.clear();log("Compatibility-only check passed. No hooks installed.");return 1;}
  if(g.internet())throw std::runtime_error("Leave the online room before loading Antigravity.");
  g.marker=g.rt.api<void*(*)(const char*)>("il2cpp_string_new")("__TK2_Antigravity_v1");g.markerHandle=g.root(g.marker);
  auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)throw std::runtime_error("Cannot initialize native hooks.");
  std::vector<void*> created;
  for(auto& h:hooks){if(MH_CreateHook(h.target,h.detour,h.original)!=MH_OK){for(auto p:created)MH_RemoveHook(p);throw std::runtime_error("A native hook could not be created; none enabled.");}created.push_back(h.target);}
  for(auto p:created)if(MH_QueueEnableHook(p)!=MH_OK){for(auto q:created)MH_RemoveHook(q);throw std::runtime_error("A native hook could not be queued.");}
  if(MH_ApplyQueued()!=MH_OK){for(auto p:created){MH_DisableHook(p);MH_RemoveHook(p);}throw std::runtime_error("Native hook activation failed.");}
  ready=true;log("READY: opt-in marked surfaces only; online room operations and leaderboard uploads blocked. Native runtime 0.1.0-alpha.1.");return 2;
 }catch(const std::exception& e){if(!ready){g.unroot(g.markerHandle);g.markerHandle=0;bound=false;hooks.clear();}log(std::string("Initialization failed: ")+e.what());return 0;}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);}return TRUE;}
