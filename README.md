# Vehicle Ped Damage Fix

`VehiclePedDamageFix.asi` is a standalone GTA San Andreas plugin that makes a ped
shootable while it is getting into or out of a vehicle. It fixes damage to the
ped, not to the car.

In the stock game that ped cannot be hit. The moment it reaches the door the
game switches its collision off, and it stays off through aligning to the door,
opening it, the whole get-in animation and until the ped is seated. On the way
out the window runs from the start of the exit animation until the ped has
finished closing the door behind it, which on a police ped was measured at
around five seconds. Because `CWorld::ProcessLineOfSightSectorList` skips a ped
without collision, bullets travel straight through somebody who is standing in
plain sight next to the car, which is why damage on peds around vehicles feels
like it lands only sometimes.

The game already solves the same problem elsewhere. Peds on bikes and in
open-topped vehicles have no collision either, and they are shootable because
the ped flag `bTestForShotInVehicle` is set on them and the weapon fire paths
raise `CWorld::bIncludeBikers` for the duration of the line test. This plugin
extends exactly that condition: a ped running one of the tasks that place it at
the vehicle rather than inside it is included in the line test in the same way
a biker is.

Nothing else is touched. The new condition is gated on `CWorld::bIncludeBikers`
just as the original one was, so it is only visible to weapon fire and to
weapon target selection, the two things that raise that flag, and never to an
AI line of sight query or to camera collision. The ped's collision flag, its
physics and its tasks are all left exactly as the game set them.

## Features

- Makes a ped hit by bullets, and reachable by the weapon auto-aim, for the
  whole time the game leaves it standing at a vehicle without collision, for
  the player and for every other ped alike.
- Covers getting in: aligning to the door, opening a door, opening a locked
  door, picking up a bike and the get-in animation.
- Covers getting out: the exit animation, closing the door from outside
  afterwards, bailing out of a moving vehicle and falling out of one.
- Covers carjacking, on both sides: the ped dragging a driver out and the
  driver being dragged out.
- Leaves a ped that is actually seated inside the vehicle alone, so it still
  cannot be shot through the car body.
- Leaves ped physics, collision flags and tasks untouched, so the animations,
  door handling and ped positioning are unchanged.
- Leaves AI vision, camera collision and every other line of sight query
  unchanged.
- Verifies the 28 instruction bytes it replaces and four byte sequences that
  pin the task manager layout before writing anything, and refuses to patch any
  other executable.
- Creates the default INI automatically when it is missing.
- Optional log file recording what was patched, whether another modification
  later overwrote it, and whether the fix has taken effect in game.

## Requirements

- GTA San Andreas 1.0 US (Compact or Hoodlum executable), or a SA-MP
  installation based on it.
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.

The plugin compares the bytes at every address it depends on with the bytes the
1.0 US build has there. Any other executable version is left completely
untouched.

## Installation

1. Extract `VehiclePedDamageFix.asi` and `VehiclePedDamageFix.ini` into the GTA San
   Andreas directory or into its `scripts` directory.
2. Start the game.

To uninstall the plugin, delete both files.

## Configuration

The complete default `VehiclePedDamageFix.ini` is:

```ini
# Vehicle Ped Damage Fix v1.0.1
# Created by sonochiwa
# Source code: https://github.com/sonochiwa/sa-vehicle-ped-damage-fix

[general]
log=0
```

| Setting | Default | Meaning |
| --- | ---: | --- |
| `general.log` | `0` | Set to `1` to write `VehiclePedDamageFix.log` next to the plugin with the result of the patch, a note if another modification later overwrites the patch site, and a note the first time a ped at a vehicle is included in a weapon line test. |

The setting is read once when the game starts. The plugin has no hotkey; edit
the INI and restart the game to change it.

There is no master switch. The plugin does nothing but apply this one fix, so
deleting `VehiclePedDamageFix.asi` is how you turn it off.

## Building

Visual Studio 2022 (v143), `Release|Win32`. Open `VehiclePedDamageFix.sln` or
run:

```powershell
msbuild VehiclePedDamageFix.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32
```

The plugin is written to `build\VehiclePedDamageFix.asi` next to a copy of the
INI. `Config\VehiclePedDamageFix.ini` is compiled into the plugin as an
`RCDATA` resource, so the INI written when the file is missing is byte for
byte the canonical one.

## Repository Layout

```text
VehiclePedDamageFix.sln
README.md
CHANGELOG.md
ROADMAP.md                      Validation status and planned work
LICENSE
.github\workflows\release.yml   Tagged release build, checksum and attestation
Config\
  VehiclePedDamageFix.ini       Canonical configuration, embedded as RCDATA
src\
  VehiclePedDamageFix.cpp       DllMain, patch installation and the watcher
  VehiclePedDamageFix.rc        Version resource and the embedded INI
  VehiclePedDamageFix.vcxproj
  addresses.h                   Game addresses, offsets and expected bytes
  config.cpp / config.h         INI creation and loading
  log.cpp / log.h               Optional log file
  patch.cpp / patch.h           Readable-memory checks and protected writes
  resource.h
  version.h
```

## How It Works

### Where the window comes from

`CTaskComplexEnterCar::CreateNextSubTask` switches the ped's collision off as
soon as it has reached the door, before it even creates the align task:

```text
GO_TO_CAR_DOOR_AND_STAND_STILL finished
  -> SetVehicleFlags / PrepareVehicleForPedEnter
  -> PreparePedForVehicleEnter @ 0x63AC80    ped->SetUsesCollision(false)
  -> CAR_ALIGN -> OPEN_DOOR_FROM_OUTSIDE -> CAR_GET_IN -> SET_PED_IN_AS_*
```

`CTaskSimpleCarSetPedOut::ProcessPed @ 0x647D10` gives it back on the way out:

```text
mov edx,dword ptr [esi+1Ch]        CEntity::m_nFlags
mov ecx,dword ptr [esi+46Ch]       CPed::m_nPedFlags
and ecx,0FFFFFEFFh                 bInVehicle       = 0
or  edx,1                          m_bUsesCollision = 1
```

but that is not where the window ends. Measured in game, a police ped leaving
its car reads:

```text
f 36796..36957  hp=100  shootable      COP_IN_CAR, LEAVE_CAR, CAR_GET_OUT
f 36958..36979  hp=80   shootable      the exit animation, damage lands
f 36980..37278  hp=80   NOT shootable  COP_IN_CAR, LEAVE_CAR,
                                       CAR_CLOSE_DOOR_FROM_OUTSIDE
```

The ped is standing on the road closing the door behind it, and its collision
is still gone for the roughly 300 frames that takes. The covered set below
therefore follows what the tasks were observed to do rather than where the
collision was expected to come back.

### Where it is decided

The ped branch of `CWorld::ProcessLineOfSightSectorList @ 0x566EE0` decides
whether a ped's collision model is tested at all:

```text
0056705C  test  al,1                            CEntity::m_bUsesCollision
0056705E  jne   00567097                        -> test the col model
00567060  cmp   dword ptr [edi+0FCh],ebp        CPhysical::m_pAttachedTo
00567066  jne   00567097
00567068  mov   al,byte ptr [esp+16h]           CWorld::bIncludeDeadPeds
0056706C  test  al,al
0056706E  je    0056707B
00567070  mov   ecx,edi
00567072  call  005E0170                        CPed::IsAlive
00567077  test  al,al
00567079  je    00567097
0056707B  mov   al,byte ptr [esp+15h]           CWorld::bIncludeBikers
0056707F  test  al,al
00567081  je    0056727E                        -> skip the ped
00567087  test  dword ptr [edi+478h],100000h    bTestForShotInVehicle
00567091  je    0056727E                        -> skip the ped
00567097  movsx eax,word ptr [edi+22h]          test the col model
```

The plugin replaces the last two conditions, 28 bytes from `0x56707B` up to
`0x567097`, with a call to its own predicate and the same conditional jump to
the same target:

```text
mov  ecx,edi                 the ped the loop is on
call PedTakesPartInLineTest
test al,al
je   0056727E
```

with the remaining 13 bytes filled with `nop`. The predicate reproduces the two
conditions it replaced and adds one more:

```text
CWorld::bIncludeBikers == 0                  -> not tested
CPed::bTestForShotInVehicle                  -> tested   (unchanged behaviour)
an active task from the list below           -> tested   (the fix)
otherwise                                    -> not tested
```

### Which tasks count

Every task during which the game has taken the collision away although the ped
is at the vehicle rather than in it:

```text
getting in            getting out           carjacking
801  CAR_ALIGN        813  CAR_GET_OUT      817  CAR_QUICK_DRAG_PED_OUT
802  CAR_OPEN_DOOR..  814  CAR_JUMP_OUT     818  CAR_QUICK_BE_DRAGGED_OUT
803  CAR_OPEN_LOCK..  806  CAR_CLOSE_DOOR   820  CAR_SLOW_DRAG_PED_OUT
804  BIKE_PICK_UP          _FROM_OUTSIDE    821  CAR_SLOW_BE_DRAGGED_OUT
807  CAR_GET_IN       834  CAR_FALL_OUT     823  COMPLEX_CAR_SLOW_BE
                                                 _DRAGGED_OUT
```

Tasks during which the ped really is inside the vehicle are deliberately not in
the list, so a seated occupant still cannot be shot through the body of the
car: `CAR_SHUFFLE`, `CAR_SET_PED_IN_AS_DRIVER`, `CAR_CLOSE_DOOR_FROM_INSIDE`
and `CAR_WAIT_TO_SLOW_DOWN`. The last of those was also observed without
collision, but the ped is sitting in the car waiting for it to slow down before
getting out, so leaving it uncovered is the point rather than an oversight.

Covering the complex parent `CAR_SLOW_BE_DRAGGED_OUT` as well as its leaves
keeps the whole drag-out covered whichever leaf happens to be running. While a
leaf that has already restored the collision runs, the predicate is never
reached at all, so the extra entry cannot widen anything.

The task tree is walked the same way `CTaskManager::FindActiveTaskByType @
0x681740` walks it: the chain below the first primary slot that is set, then
the chains below all six secondary slots. The plugin does that traversal once
for the whole list rather than once per task type. The four facts it needs, the
five primary slots, the six secondary slots at `+0x14`, `CTask::GetSubTask` at
vtable`+0x8` and `CTask::GetTaskType` at vtable`+0x10`, are read out of that
function's own code and verified byte by byte before the patch is written.

### What it does not reach

Keeping the `CWorld::bIncludeBikers` gate is what confines the change. The flag
is raised in eight places in the executable and cleared again straight after
the line test:

- `CBulletInfo::Update`, `CWeapon::FireInstantHit`,
  `CWeapon::FireInstantHitFromCar2` and `CWeapon::FireM16_1stPerson`, the paths
  a bullet takes;
- the player's weapon target search at `0x60B650`, called from the on-foot
  player task, which is what decides who the auto-aim locks on to;
- `CWorld::ProcessLineOfSightSector @ 0x56B5E0`, which only restores the flag
  between the sector lists of a query one of the above already started.

So a ped at a vehicle becomes shootable and lockable-on, in exactly the same
places a biker already is, and stays invisible to every other line of sight
query in the game. Once the bullet has found the ped it follows the ordinary
path, the same one that already damages a ped shot off a bike:
`CWeapon::CheckForShootingVehicleOccupant @ 0x73F480` returns immediately for a
ped, and `CWeapon::DoBulletImpact @ 0x73B550` generates the damage event.

Damage that does not go through a line of sight test is not affected, because
its code takes a different decision. `CWorld::TriggerExplosionSectorList` and
`CWorld::SetPedsOnFire` skip any ped with `bInVehicle` set outright, and melee
attacks resolve against a sphere rather than a line. A ped at a vehicle is
still immune to those, as it is in the stock game.

The plugin also does not change when damage is applied. `CWeapon::DoBulletImpact`
queues a `CEventDamage` into the ped's event group and the ped acts on it in its
own AI update, which is how the game handles damage for every ped and is not
specific to vehicles.

Patched function:

- `CWorld::ProcessLineOfSightSectorList @ 0x566EE0`, 28 bytes at `0x56707B`

## Roadmap

Planned and unvalidated work is tracked in [ROADMAP.md](ROADMAP.md).

## Release Integrity

Tagged releases are built by GitHub Actions from the tagged commit. Each
release carries `VehiclePedDamageFix-vX.Y.Z.zip`, its SHA-256 in
`VehiclePedDamageFix-vX.Y.Z.zip.sha256` and a signed build-provenance attestation,
which proves that the archive was produced by this repository's workflow
from that revision. It does not prove the code is bug-free.

```text
gh attestation verify VehiclePedDamageFix-vX.Y.Z.zip -R sonochiwa/sa-vehicle-ped-damage-fix
```

## License

MIT. See [LICENSE](LICENSE).
