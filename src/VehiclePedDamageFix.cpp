// Vehicle Ped Damage Fix
//
// A ped at a vehicle cannot be shot. From the moment it reaches the door until
// it is sitting inside, and again from the start of the exit animation until
// it has finished closing the door behind it, the game has switched its
// collision off while it is still in plain sight beside the car.
//
// The window opens in CTaskComplexEnterCar::CreateNextSubTask, which calls
// PreparePedForVehicleEnter @ 0x63AC80 as soon as the ped has reached the door
// and before the align task is even created:
//
//     void CTaskComplexEnterCar::PreparePedForVehicleEnter(CPed* ped) {
//         ped->SetUsesCollision(false);
//         ...
//     }
//
// so it already covers aligning to the door, opening it and the whole get-in
// animation. CTaskSimpleCarSetPedOut::ProcessPed @ 0x647D10 gives the collision
// back on the way out:
//
//     mov edx,[esi+1Ch]        CEntity::m_nFlags
//     mov ecx,[esi+46Ch]       CPed::m_nPedFlags
//     and ecx,0FFFFFEFFh       bInVehicle       = 0
//     or  edx,1                m_bUsesCollision = 1
//
// but that is not where the window ends. A ped leaving a car then runs
// CAR_CLOSE_DOOR_FROM_OUTSIDE, and a police ped was measured in game holding
// `m_bUsesCollision == 0` for the whole 200-300 frames that task lasts, long
// after the exit animation was over. CWorld::ProcessLineOfSightSectorList skips
// a ped without collision, so for that whole stretch bullets travel straight
// through it. The covered set therefore follows what the tasks were observed
// to do rather than where the collision was expected to come back.
//
// The game already has a mechanism for this. Peds on bikes and in open-topped
// vehicles have no collision either, and they are shootable because the ped
// flag bTestForShotInVehicle is set on them and the weapon fire paths raise
// CWorld::bIncludeBikers for the duration of the line test. The plugin extends
// exactly that condition: a ped running one of the tasks that place it at the
// vehicle rather than in it takes part in the line test the same way a biker
// does.
//
// Nothing else changes. The new condition is gated on CWorld::bIncludeBikers
// just as the original one was, so it is only visible to weapon fire and to the
// player's weapon target search, the two things that raise that flag, and never
// to an AI line of sight query or to camera collision. The ped's collision
// flag, its physics and its tasks are all left exactly as the game set them.

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iterator>

#include "config.h"
#include "game.h"
#include "log.h"
#include "patch.h"

namespace {

constexpr char kVersion[] = "1.0.0";

constexpr DWORD kWatchPollMs = 1000;

// The bytes the plugin wrote over the two conditions, kept so that the watcher
// can tell when another modification has overwritten them.
uint8_t g_installed[game::kBikerCheckSize] = {};

// Incremented by the game thread whenever the fix includes a ped that is
// getting in or out of a vehicle. The watcher thread reports the first one, so
// no file is ever opened from inside a line of sight test.
volatile LONG g_atVehicleHits = 0;

config::Settings g_settings;

int32_t GetTaskType(const void* task) {
    const auto vtable = *reinterpret_cast<const uintptr_t*>(task);
    const auto get = *reinterpret_cast<int32_t(__thiscall* const*)(const void*)>(
        vtable + game::kTaskGetTaskType);
    return get(task);
}

const void* GetSubTask(const void* task) {
    const auto vtable = *reinterpret_cast<const uintptr_t*>(task);
    const auto get =
        *reinterpret_cast<const void*(__thiscall* const*)(const void*)>(
            vtable + game::kTaskGetSubTask);
    return get(task);
}

bool IsAtVehicleTask(int32_t type) {
    for (const auto candidate : game::kAtVehicleTasks) {
        if (candidate == type)
            return true;
    }
    return false;
}

// Walks a task and every sub-task below it.
bool ChainHasAtVehicleTask(const void* task) {
    for (; task; task = GetSubTask(task)) {
        if (IsAtVehicleTask(GetTaskType(task)))
            return true;
    }
    return false;
}

// The same traversal CTaskManager::FindActiveTaskByType @ 0x681740 performs:
// the chain below the first primary slot that is set, which is the active
// task, and then the chains below all six secondary slots. Done once for the
// whole task set rather than once per task type.
bool IsAtVehicle(uintptr_t ped) {
    const auto intelligence =
        *reinterpret_cast<const uintptr_t*>(ped + game::kPedIntelligence);
    if (!intelligence)
        return false;

    const auto* slots = reinterpret_cast<const void* const*>(
        intelligence + game::kIntelligenceTaskManager);

    for (size_t i = 0; i < game::kTaskManagerPrimaryCount; ++i) {
        if (slots[i])
            return ChainHasAtVehicleTask(slots[i]);
    }

    const auto* secondary = reinterpret_cast<const void* const*>(
        reinterpret_cast<const uint8_t*>(slots) + game::kTaskManagerSecondary);
    for (size_t i = 0; i < game::kTaskManagerSecondaryCount; ++i) {
        if (ChainHasAtVehicleTask(secondary[i]))
            return true;
    }
    return false;
}

}  // namespace

// Replaces the two conditions at 0x56707B. The game reaches this with a ped
// that has no collision, is not attached to anything, and is either alive or
// not being included as a dead one. Returns whether its collision model should
// take part in the line test.
//
// External linkage and an explicit calling convention, because the only caller
// is patched game code: the address escapes into a `call rel32` that the
// compiler cannot see.
extern "C" bool __fastcall PedTakesPartInLineTest(void* pedPointer) {
    // CWorld::bIncludeBikers. Only the weapon fire paths and the player's
    // weapon target search raise it, so this keeps the plugin out of every
    // other line of sight query, exactly as the condition it replaces did.
    if (*reinterpret_cast<const uint8_t*>(game::kIncludeBikers) == 0)
        return false;

    const auto ped = reinterpret_cast<uintptr_t>(pedPointer);
    const auto fourthFlags =
        *reinterpret_cast<const uint32_t*>(ped + game::kPedFourthFlags);

    // bTestForShotInVehicle: what the game sets on bikers and on the occupants
    // of open-topped vehicles.
    if ((fourthFlags & game::kPedTestForShot) != 0)
        return true;

    if (!IsAtVehicle(ped))
        return false;

    InterlockedIncrement(&g_atVehicleHits);
    return true;
}

namespace {

// Assembles and writes the replacement. The 28 bytes of the two conditions
// become a call into PedTakesPartInLineTest and the same conditional jump to
// the same target, padded with nops:
//
//   mov  ecx,edi                 the ped the loop is on
//   call PedTakesPartInLineTest
//   test al,al
//   je   0056727E                where both original conditions jumped
bool Install() {
    if (!patch::BytesEqual(game::kBikerCheck, game::kBikerCheckOriginal)) {
        logging::Write("unexpected bytes at %08X, nothing was patched",
                       static_cast<unsigned>(game::kBikerCheck));
        return false;
    }

    for (const auto& anchor : game::kTaskLayoutAnchors) {
        if (!patch::BytesEqual(anchor.address, anchor.bytes, anchor.size)) {
            logging::Write("unexpected bytes at %08X, the task manager layout "
                           "could not be confirmed, nothing was patched",
                           static_cast<unsigned>(anchor.address));
            return false;
        }
    }

    uint8_t code[game::kBikerCheckSize];
    std::memset(code, 0x90, sizeof(code));

    size_t at = 0;

    code[at++] = 0x8B;  // mov ecx,edi
    code[at++] = 0xCF;

    code[at++] = 0xE8;  // call rel32
    const auto afterCall = game::kBikerCheck + at + sizeof(int32_t);
    const auto target = reinterpret_cast<uintptr_t>(&PedTakesPartInLineTest);
    const auto callRelative = static_cast<int32_t>(target - afterCall);
    std::memcpy(code + at, &callRelative, sizeof(callRelative));
    at += sizeof(callRelative);

    code[at++] = 0x84;  // test al,al
    code[at++] = 0xC0;

    code[at++] = 0x0F;  // je rel32
    code[at++] = 0x84;
    const auto afterJump = game::kBikerCheck + at + sizeof(int32_t);
    const auto jumpRelative = static_cast<int32_t>(game::kSkipPed - afterJump);
    std::memcpy(code + at, &jumpRelative, sizeof(jumpRelative));
    at += sizeof(jumpRelative);

    if (!patch::WriteMemory(game::kBikerCheck, code, sizeof(code))) {
        logging::Write("could not write to %08X, nothing was patched",
                       static_cast<unsigned>(game::kBikerCheck));
        return false;
    }

    std::memcpy(g_installed, code, sizeof(code));

    logging::Write("patched CWorld::ProcessLineOfSightSectorList: "
                   "%u bytes at %08X, %u of them nop padding",
                   static_cast<unsigned>(sizeof(code)),
                   static_cast<unsigned>(game::kBikerCheck),
                   static_cast<unsigned>(sizeof(code) - at));
    logging::Write("  ped predicate at %08X, skip target %08X, %u tasks covered",
                   static_cast<unsigned>(target),
                   static_cast<unsigned>(game::kSkipPed),
                   static_cast<unsigned>(std::size(game::kAtVehicleTasks)));
    return true;
}

// Another modification can rewrite the same instructions afterwards, which
// would undo the fix without leaving any trace. The site is re-read so that
// this is reported once rather than silently ignored. Never returns.
[[noreturn]] void Watch() {
    bool siteReported = false;
    bool hitReported = false;

    for (;;) {
        Sleep(kWatchPollMs);

        if (!siteReported &&
            !patch::BytesEqual(game::kBikerCheck, g_installed,
                               sizeof(g_installed))) {
            logging::Write("the patch site at %08X no longer holds the "
                           "plugin's bytes, another modification has "
                           "overwritten it",
                           static_cast<unsigned>(game::kBikerCheck));
            siteReported = true;
        }

        if (!hitReported &&
            InterlockedCompareExchange(&g_atVehicleHits, 0, 0) != 0) {
            logging::Write("a ped at a vehicle was included in a weapon line "
                           "test for the first time");
            hitReported = true;
        }
    }
}

DWORD WINAPI PluginThread(LPVOID parameter) {
    const auto module = static_cast<HMODULE>(parameter);

    char path[MAX_PATH] = {};
    if (!config::GetPath(module, path))
        return 0;

    config::CreateDefault(module, path);
    g_settings = config::Load(path);

    if (g_settings.log)
        logging::Enable(module);

    logging::Write("Vehicle Ped Damage Fix v%s", kVersion);

    if (!Install())
        return 0;

    // Nothing else is written from here, and with the log off there is nothing
    // left to report.
    if (!g_settings.log)
        return 0;

    Watch();
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        const HANDLE thread =
            CreateThread(nullptr, 0, PluginThread, instance, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
