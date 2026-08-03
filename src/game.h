#pragma once

#include <cstddef>
#include <cstdint>

// Everything the plugin knows about GTA San Andreas 1.0 US (gta_sa.exe,
// 14,383,616 bytes). Every address is an absolute virtual address in the
// default 0x400000 image, and every byte sequence is verified against the
// running executable before it is used.
namespace game {

// A range of bytes the plugin expects to find in the image.
struct Anchor {
    uintptr_t address;
    const uint8_t* bytes;
    size_t size;
};

// CWorld::ProcessLineOfSightSectorList @ 0x566EE0.
//
// The ped branch of the entity switch decides whether a ped's collision model
// takes part in the line test at all:
//
//   0056705C  test al,1                              ; CEntity::m_bUsesCollision
//   0056705E  jne  00567097                          ; -> test the col model
//   00567060  cmp  dword ptr [edi+0FCh],ebp          ; CPhysical::m_pAttachedTo
//   00567066  jne  00567097
//   00567068  mov  al,byte ptr [esp+16h]             ; CWorld::bIncludeDeadPeds
//   0056706C  test al,al
//   0056706E  je   0056707B
//   00567070  mov  ecx,edi
//   00567072  call 005E0170                          ; CPed::IsAlive
//   00567077  test al,al
//   00567079  je   00567097
//   0056707B  mov  al,byte ptr [esp+15h]             ; CWorld::bIncludeBikers
//   0056707F  test al,al
//   00567081  je   0056727E                          ; -> skip the ped
//   00567087  test dword ptr [edi+478h],100000h      ; bTestForShotInVehicle
//   00567091  je   0056727E                          ; -> skip the ped
//   00567097  movsx eax,word ptr [edi+22h]           ; test the col model
//
// The last two conditions are the whole patch surface: 28 bytes from 0x56707B
// up to, but not including, 0x567097.
constexpr uintptr_t kBikerCheck = 0x56707B;
constexpr uintptr_t kBikerCheckEnd = 0x567097;
constexpr size_t kBikerCheckSize = kBikerCheckEnd - kBikerCheck;

// Where both of those conditions jump when the ped is not shootable.
constexpr uintptr_t kSkipPed = 0x56727E;

// The stock 28 bytes. They double as the executable check: an image that does
// not hold exactly these bytes here is not the build this plugin was mapped
// against, and nothing is written.
constexpr uint8_t kBikerCheckOriginal[kBikerCheckSize] = {
    0x8A, 0x44, 0x24, 0x15,                         // mov  al,[esp+15h]
    0x84, 0xC0,                                     // test al,al
    0x0F, 0x84, 0xF7, 0x01, 0x00, 0x00,             // je   0056727E
    0xF7, 0x87, 0x78, 0x04, 0x00, 0x00,             // test dword ptr [edi+478h],
    0x00, 0x00, 0x10, 0x00,                         //      100000h
    0x0F, 0x84, 0xE7, 0x01, 0x00, 0x00,             // je   0056727E
};

// CWorld::bIncludeBikers. Set by the weapon fire paths and by the player's
// weapon target search for the duration of one line test and cleared
// afterwards, so it is what separates a bullet from an AI or camera line of
// sight query.
constexpr uintptr_t kIncludeBikers = 0xB7CD6F;

// CPed layout.
constexpr size_t kPedFourthFlags = 0x478;        // CPed::m_nFourthPedFlags
constexpr uint32_t kPedTestForShot = 1u << 20;   // bTestForShotInVehicle
constexpr size_t kPedIntelligence = 0x47C;       // CPed::m_pIntelligence

// CPedIntelligence::m_TaskMgr follows CPedIntelligence::m_pPed.
constexpr size_t kIntelligenceTaskManager = 0x4;

// CTaskManager: five primary task slots followed by six secondary ones, and
// CTask: GetSubTask at vtable+0x8, GetTaskType at vtable+0x10. All four facts
// are read straight out of CTaskManager::FindActiveTaskByType @ 0x681740,
// which the plugin walks the task tree the same way as:
//
//   00681750  cmp   dword ptr [edi+eax*4],0   ; find the first primary slot
//   00681754  jne   0068175E                  ; that is set, out of five
//   00681756  inc   eax
//   00681757  cmp   eax,5
//   0068175A  jl    00681750
//   ...
//   00681769  mov   eax,dword ptr [esi]       ; walk that task's chain
//   0068176B  mov   ecx,esi
//   0068176D  call  dword ptr [eax+10h]       ; CTask::GetTaskType
//   00681770  cmp   eax,dword ptr [esp+10h]
//   00681774  jne   00681778
//   00681776  mov   ebx,esi
//   00681778  mov   edx,dword ptr [esi]
//   0068177A  mov   ecx,esi
//   0068177C  call  dword ptr [edx+8]         ; CTask::GetSubTask
//   ...
//   0068178A  add   edi,14h                   ; then the six secondary slots
//   0068178D  mov   ebp,6
//
// These four short anchors pin the layout without depending on the function's
// prologue, which another modification may legitimately have hooked.
constexpr size_t kTaskManagerPrimaryCount = 5;
constexpr size_t kTaskManagerSecondary = 0x14;
constexpr size_t kTaskManagerSecondaryCount = 6;
constexpr size_t kTaskGetSubTask = 0x8;
constexpr size_t kTaskGetTaskType = 0x10;

inline constexpr uint8_t kPrimarySlotScan[] = {
    0x83, 0x3C, 0x87, 0x00,                         // cmp dword ptr [edi+eax*4],0
    0x75, 0x08,                                     // jne 0068175E
    0x40,                                           // inc eax
    0x83, 0xF8, 0x05,                               // cmp eax,5
};
inline constexpr uint8_t kGetTaskTypeCall[] = {
    0x8B, 0x06,                                     // mov  eax,[esi]
    0x8B, 0xCE,                                     // mov  ecx,esi
    0xFF, 0x50, 0x10,                               // call dword ptr [eax+10h]
};
inline constexpr uint8_t kGetSubTaskCall[] = {
    0x8B, 0x16,                                     // mov  edx,[esi]
    0x8B, 0xCE,                                     // mov  ecx,esi
    0xFF, 0x52, 0x08,                               // call dword ptr [edx+8]
};
inline constexpr uint8_t kSecondarySlotScan[] = {
    0x83, 0xC7, 0x14,                               // add edi,14h
    0xBD, 0x06, 0x00, 0x00, 0x00,                   // mov ebp,6
};

inline constexpr Anchor kTaskLayoutAnchors[] = {
    { 0x681750, kPrimarySlotScan,   sizeof(kPrimarySlotScan) },
    { 0x681769, kGetTaskTypeCall,   sizeof(kGetTaskTypeCall) },
    { 0x681778, kGetSubTaskCall,    sizeof(kGetSubTaskCall) },
    { 0x68178A, kSecondarySlotScan, sizeof(kSecondarySlotScan) },
};

// eTaskType. Every task during which the game has switched the ped's collision
// off although the ped is at the vehicle rather than sitting in it.
//
// The window opens in CTaskComplexEnterCar::CreateNextSubTask, which calls
// PreparePedForVehicleEnter @ 0x63AC80 (`SetUsesCollision(false)`) as soon as
// the ped has reached the door and before the align task is created. It does
// not close where CTaskSimpleCarSetPedOut::ProcessPed @ 0x647D10 restores the
// collision: a ped leaving a car runs CAR_CLOSE_DOOR_FROM_OUTSIDE afterwards
// and was measured in game still holding `m_bUsesCollision == 0` for the
// 200-300 frames that task lasts. The list therefore follows what the game was
// observed to do, not where the collision was expected to come back.
//
// Tasks during which the ped really is inside the vehicle are deliberately not
// listed, so an occupant still cannot be shot through the body of the car:
// CAR_SHUFFLE, CAR_SET_PED_IN_AS_DRIVER, CAR_CLOSE_DOOR_FROM_INSIDE and
// CAR_WAIT_TO_SLOW_DOWN, the last of which was also observed without collision
// while the ped sat waiting for the car to slow down before getting out.
//
// Covering the complex parent CAR_SLOW_BE_DRAGGED_OUT instead of only its
// leaves keeps the whole drag-out covered whichever leaf is running; while a
// leaf that restores the collision runs, the predicate is never reached at all.
constexpr int32_t kAtVehicleTasks[] = {
    801,  // TASK_SIMPLE_CAR_ALIGN
    802,  // TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE
    803,  // TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE
    804,  // TASK_SIMPLE_BIKE_PICK_UP
    806,  // TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE
    807,  // TASK_SIMPLE_CAR_GET_IN
    813,  // TASK_SIMPLE_CAR_GET_OUT
    814,  // TASK_SIMPLE_CAR_JUMP_OUT
    817,  // TASK_SIMPLE_CAR_QUICK_DRAG_PED_OUT
    818,  // TASK_SIMPLE_CAR_QUICK_BE_DRAGGED_OUT
    820,  // TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT
    821,  // TASK_SIMPLE_CAR_SLOW_BE_DRAGGED_OUT
    823,  // TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT
    834,  // TASK_SIMPLE_CAR_FALL_OUT
};

}  // namespace game
