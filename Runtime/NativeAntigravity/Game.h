#pragma once
#include "Metadata.h"
#include "Math.h"
#include <array>

namespace ag {
template<class T>T& field(void* object,int offset){return *reinterpret_cast<T*>(static_cast<char*>(object)+offset);}
struct Game {
 Runtime rt;
 void* workerThread{};
 void *controller{},*physics{},*motor{},*player{},*config{},*camera{},*visual{},*time{},*photon{};
 void *online{},*toBool{},*raceState{},*menu{},*frameCount{},*timeScale{};
 void *transform{},*find{},*sphere{},*hitCollider{},*isReplay{},*isTeleport{},*gravity{},*motorBottom{},*setRotation{};
 void *splinePoint{},*splineTangent{},*boost{},*stopDrift{},*marker{};
 int parent{},cPhysics{},cMotor{},outside{},respawning{},tank{},catapult{},pType{},nativePtr{};
 int ground{},gFound{},gNormal{},gCollider{},mPosition{},mRotation{},mBase{},mLayers{},mStable{},mRadius{},mUnground{},mUngroundTime{};
 int wall1{},wall2{},pController{},yaw{},axesF{},axesR{},axesU{},acceleration{},centripetal{},verticalCap{},driftKick{},steer{},cSteer{},drifting{},driftDir{},jumpStrength{},runJump{},backward{};
 int pVisual{},vPlayer{},vPosition{},vGround{},vVisualGround{},vNormal{},vDistance{},camPlayer{},camStrength{},camSpectator{};
 int visualKart{},visualAnimator{},visualCharacter{},bounceFlag{},bounceImpulse{};
 int aiController{},aiFollower{},aiInput{},aiBoost{},inputData{},inputSteer{},inputDrift{},cDrift{},pDrift{},fSpline{},fProgress{},splineCount{};
 int human{},ai{},running{};
 int hitSize{},hitNormal{},hitDistance{};
 uintptr_t markerHandle{};

 template<class T,class... A>T instance(void* method,void* object,A... args) const {
  return reinterpret_cast<T(*)(void*,A...,const void*)>(rt.pointer(method))(object,args...,method);
 }
 template<class T,class... A>T statik(void* method,A... args) const {
  return reinterpret_cast<T(*)(A...,const void*)>(rt.pointer(method))(args...,method);
 }
 void* invoke(void* method,void* object,void** args=nullptr) const {
  void* exception{};auto result=rt.api<void*(*)(void*,void*,void**,void**)>("il2cpp_runtime_invoke")(method,object,args,&exception);
  if(exception)throw std::runtime_error("A game method rejected a runtime call.");return result;
 }
 template<class T>T staticField(void* f) const {T value{};rt.api<void(*)(void*,void*)>("il2cpp_field_static_get_value")(f,&value);return value;}
 uintptr_t root(void* o) const {return o?rt.api<uintptr_t(*)(void*,bool)>("il2cpp_gchandle_new")(o,false):0;}
 void unroot(uintptr_t h) const {if(h)rt.api<void(*)(uintptr_t)>("il2cpp_gchandle_free")(h);}
 bool alive(void* o) const {return o&&field<void*>(o,nativePtr)!=nullptr;}
 bool local(void* p) const {return p&&(field<int>(p,pType)==human||field<int>(p,pType)==ai);}
 bool cpu(void* c) const {auto p=field<void*>(c,parent);return p&&field<int>(p,pType)==ai;}
 bool internet() const {
  auto boxed=invoke(online,nullptr);if(!boxed)throw std::runtime_error("Online state unavailable.");
  void* value=rt.api<void*(*)(void*)>("il2cpp_object_unbox")(boxed);void* args[]{value};
  auto result=invoke(toBool,nullptr,args);if(!result)throw std::runtime_error("Online boolean unavailable.");
  return *static_cast<bool*>(rt.api<void*(*)(void*)>("il2cpp_object_unbox")(result));
 }
 int valueOffset(void* type,const char* name,int expected,int base) const {return rt.offset(type,name,expected)-base;}
 void* cm(const char* name,std::initializer_list<const char*> params,const char* returns="System.Void") const {
  std::string full="KinematicCharacterController.ICharacterController.";full+=name;
  try{return rt.method(controller,full.c_str(),params,returns);}catch(const std::runtime_error&){
   full="KinematicCharacterController_ICharacterController_";full+=name;return rt.method(controller,full.c_str(),params,returns);
  }
 }
 void bind(){
  rt.module=GetModuleHandleW(L"GameAssembly.dll");if(!rt.module)throw std::runtime_error("GameAssembly.dll is not loaded.");
  auto domain=rt.api<void*(*)()>("il2cpp_domain_get")();if(!domain)throw std::runtime_error("Wait until the game's main menu.");
  workerThread=rt.api<void*(*)(void*)>("il2cpp_thread_attach")(domain);
  controller=rt.type("","PixelEasyCharMoveKartController");physics=rt.type("","PixelKartPhysics");
  motor=rt.type("KinematicCharacterController","KinematicCharacterMotor");player=rt.type("","Ant_Player");
  config=rt.type("","Ant_CurrentGameConfiguration");camera=rt.type("","PixelGameKartCamera");visual=rt.type("","VisualInstanceSyncedParams");
  time=rt.type("UnityEngine","Time");photon=rt.type("Photon.Pun","PhotonNetwork");
  online=rt.method(config,"get_IsOnlineGame_InRoom_WithInternet",{},"CodeStage.AntiCheat.ObscuredTypes.ObscuredBool");
  auto obscured=rt.type("CodeStage.AntiCheat.ObscuredTypes","ObscuredBool");
  toBool=rt.method(obscured,"op_Implicit",{"CodeStage.AntiCheat.ObscuredTypes.ObscuredBool"},"System.Boolean");
  raceState=rt.field(config,"eCurrentRaceState");running=rt.enumMember(rt.fieldClass(config,"eCurrentRaceState"),"E_RACE_RUNNING");
  human=rt.enumMember(rt.fieldClass(player,"ePlayerType"),"E_HUMAN_LOCAL");ai=rt.enumMember(rt.fieldClass(player,"ePlayerType"),"E_AI_LOCAL");
  menu=rt.field(rt.type("","MenuManager"),"Instance");
  frameCount=rt.method(time,"get_frameCount",{},"System.Int32");timeScale=rt.method(time,"get_timeScale",{},"System.Single");
  nativePtr=rt.offset(rt.type("UnityEngine","Object"),"m_CachedPtr",0x18);
  auto co=rt.type("UnityEngine","Component");transform=rt.method(co,"get_transform",{},"UnityEngine.Transform");
  find=rt.method(rt.type("UnityEngine","Transform"),"Find",{"System.String"},"UnityEngine.Transform");
  sphere=rt.method(rt.type("UnityEngine","Physics"),"SphereCast",{"UnityEngine.Vector3","System.Single","UnityEngine.Vector3","UnityEngine.RaycastHit&","System.Single","System.Int32","UnityEngine.QueryTriggerInteraction"},"System.Boolean");
  auto hit=rt.type("UnityEngine","RaycastHit");uint32_t alignment{};
  hitSize=rt.api<int(*)(void*,uint32_t*)>("il2cpp_class_value_size")(hit,&alignment);
  if(hitSize<1||hitSize>128)throw std::runtime_error("Unsupported RaycastHit layout.");
  int hitBase=rt.offset(hit,"m_Point",17);hitNormal=valueOffset(hit,"m_Normal",17,hitBase);hitDistance=valueOffset(hit,"m_Distance",12,hitBase);
  hitCollider=rt.method(hit,"get_collider",{},"UnityEngine.Collider");
  isReplay=rt.method(controller,"IsPlayingReplayData",{},"System.Boolean");isTeleport=rt.method(physics,"IsTeleportingFromAtoB",{},"System.Boolean");
  gravity=rt.method(physics,"UpdateGravityWhenNotGrounded",{"UnityEngine.Vector3&","UnityEngine.Vector3&","System.Single"},"System.Void");
  motorBottom=rt.method(motor,"get_CharacterTransformToCapsuleBottom",{},"UnityEngine.Vector3");
  setRotation=rt.method(motor,"set_TransientRotation",{"UnityEngine.Quaternion"},"System.Void");
  auto o=[&](void* c,const char* n,int t){return rt.offset(c,n,t);};
  parent=o(controller,"parentPlayer",0);cPhysics=o(controller,"kartPhysics",0);cMotor=o(controller,"Motor",0);outside=o(controller,"outsideMapValidator",0);
  respawning=o(rt.fieldClass(controller,"outsideMapValidator"),"bIsPlayerRespawning",2);tank=o(controller,"bIsInTankForm",2);catapult=o(physics,"bCatapultUsedLowerLateralFriction",2);pType=o(player,"ePlayerType",8);
  ground=o(motor,"GroundingStatus",17);auto report=rt.fieldClass(motor,"GroundingStatus");int reportBase=o(report,"FoundAnyGround",2);
  gFound=0;gNormal=valueOffset(report,"GroundNormal",17,reportBase);gCollider=valueOffset(report,"GroundCollider",0,reportBase);
  if(gNormal!=4||gCollider<16||gCollider>96)throw std::runtime_error("Unsupported grounding report layout.");
  mPosition=o(motor,"_transientPosition",17);mRotation=o(motor,"_transientRotation",17);mBase=o(motor,"BaseVelocity",17);
  mLayers=o(motor,"CollidableLayers",17);mStable=o(motor,"StableGroundLayers",17);mRadius=o(motor,"CapsuleRadius",12);mUnground=o(motor,"_mustUnground",2);mUngroundTime=o(motor,"_mustUngroundTimeCounter",12);
  wall1=o(controller,"wallRespawn_Flat_ColliderLayer",17);wall2=o(controller,"wallRespawn_Always_ColliderLayer",17);
  pController=o(physics,"kartController",0);yaw=o(physics,"qSteeringOrientation",17);
  axesF=o(physics,"vForward_SteeringChar_AlignedToSurfacce_Normalized",17);axesR=o(physics,"vRight_SteeringChar_AlignedToSurfacce_Normalized",17);axesU=o(physics,"vUP_CharacterMotorAligned",17);
  acceleration=o(physics,"vAccelerationForwardDirection",17);centripetal=o(physics,"vVelocityDrivingCentiperalAddVal",17);verticalCap=o(physics,"bLimitVerticalUpVelocity",2);
  driftKick=o(physics,"bAddLateralForceAtStartOfTheDriftToTweakVel",2);steer=o(physics,"steer_input",12);cSteer=o(controller,"steer_input",12);
  drifting=o(physics,"bIsDrifting",2);driftDir=o(physics,"fDriftingSteerDir",12);jumpStrength=o(physics,"fJumpStrength",12);
  runJump=o(physics,"bPlayerJumpedWhenGrounedOrJustAfterMissedGround_RunJump",2);backward=o(physics,"bIsMovingBackward",2);
  pVisual=o(player,"visualInstanceSyncedParams",0);vPlayer=o(visual,"parentPlayer",0);vPosition=o(visual,"vCurPos",17);vGround=o(visual,"bIsPlayerGrouned",2);vVisualGround=o(visual,"bIsPlayerVisualGrouned",2);vNormal=o(visual,"vPlayerVisualGrounedNormal",17);vDistance=o(visual,"fPlayerDistanceToGround",12);
  camPlayer=o(camera,"antPlayer",0);camStrength=o(camera,"fRacingCameraStrength",12);camSpectator=o(camera,"bIsSpectatorCamera",2);
  visualKart=o(rt.type("","Ant_VisualKart"),"visualInstanceSyncedParams",0);visualAnimator=o(rt.type("","Ant_KartAnimatorWorker"),"visualSyncedParams",0);visualCharacter=o(rt.type("","Ant_CharacterWorker"),"visualInstanceParams",0);
  bounceFlag=o(controller,"bPerformPlayerPlayerCollisionBounceEffect",2);bounceImpulse=o(controller,"vBouncePlayerPlayerImpulse",17);
  auto aiClass=rt.type("","AILogicController");aiController=o(aiClass,"kartController",0);aiFollower=o(aiClass,"playerBezierFollower",0);aiInput=o(aiClass,"pixelAIKartInput",0);aiBoost=o(aiClass,"bUseBoosting",2);
  auto input=rt.fieldClass(aiClass,"pixelAIKartInput");inputData=o(input,"inputData",0);auto data=rt.fieldClass(input,"inputData");inputSteer=o(data,"fTurningInput",12);inputDrift=o(data,"bDriftInput",2);
  cDrift=o(controller,"bIsDriftInput",2);pDrift=o(physics,"bIsDriftInput",2);auto follower=rt.fieldClass(aiClass,"playerBezierFollower");fSpline=o(follower,"currentUsedBezierSpline",0);fProgress=o(follower,"fCurrentGreenMatchedSplineNormalized",12);
  auto spline=rt.fieldClass(follower,"currentUsedBezierSpline");splineCount=0;
  splinePoint=rt.method(spline,"GetPoint",{"System.Single"},"UnityEngine.Vector3");splineTangent=rt.method(spline,"GetTangent",{"System.Single"},"UnityEngine.Vector3");
  boost=rt.method(aiClass,"UpdateBoostUsingWhileDrifting",{},"System.Boolean");stopDrift=rt.method(physics,"ForceStopDrifting",{},"System.Void");
 }
};
}
