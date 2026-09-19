# Roadmap

Implementation status and validation status are tracked separately. Code that
compiles and matches the executable byte for byte is not the same as code that
has been seen working in game.

## Implemented and validated

### In a running game

A diagnostic build recorded, for every ped a weapon line test asked about, its
active task chain, its health and whether the fix included it. One SA-MP
session produced 51,782 samples over 46,856 frames and 20 peds. It confirmed:

- The mechanism works. While a police ped ran `CAR_GET_OUT` the fix included it
  and its health fell in steps as it was shot, for example 100 to 80 across
  three frames on ped `17BA3A04` and 100 to 80 to 60 to 40 to 20 on ped
  `17BA6E60`.
- The window does not close where `CTaskSimpleCarSetPedOut` restores the
  collision. A police ped went straight from `CAR_GET_OUT` into
  `CAR_CLOSE_DOOR_FROM_OUTSIDE` and stayed without collision for frames 36980
  to 37278, with its health frozen at 80 the whole time. Three more police peds
  showed the same thing for 296, 234 and 197 frames. Closing the door is
  covered because of this measurement, not because of anything in the code.
- `CAR_FALL_OUT` under `COMPLEX_CAR_SLOW_BE_DRAGGED_OUT` also runs without
  collision, in 40,514 of the samples, which is the only evidence for covering
  it.
- `CAR_WAIT_TO_SLOW_DOWN` runs without collision too, but there the ped is
  sitting in the car, so leaving it uncovered is correct.
- `CAR_GET_IN` and `CAR_JUMP_OUT` were included by the fix as intended.
- Peds with `bTestForShotInVehicle` set were still included through the game's
  own condition, so the vanilla biker behaviour is reproduced.

### Statically

Validated against three different retail `gta_sa.exe` 1.0 US images, all
14,383,616 bytes, with SHA-256:

- `6e0cff98c75ef7b1ef75702dffda166634b72d71ba5487f66726b294ee48764c`
- `dd4fc723481481d3fa688868c2e4d08c294c8cf3524c1f974e19d860a14ad0f6`
- `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`

They differ outside the patched code and produced identical results:

- All three hold the same 28 bytes at `0x56707B`, the two conditions the plugin
  replaces, and the same four byte sequences inside
  `CTaskManager::FindActiveTaskByType @ 0x681740` that pin the task manager
  layout: five primary slots, six secondary slots at `+0x14`,
  `CTask::GetSubTask` at vtable`+0x8` and `CTask::GetTaskType` at
  vtable`+0x10`.
- The two conditions are the only thing standing between a bullet and a ped
  that has no collision: `0x56705C` and `0x567060` already let a ped with
  collision, or one attached to another entity, through to the collision model
  test at `0x567097`.
- `CTaskComplexEnterCar::PreparePedForVehicleEnter @ 0x63AC80` opens the window
  before the align task is created.
- `bTestForShotInVehicle`, bit 20 of `CPed::m_nFourthPedFlags` at `0x478`, is
  named by only three instructions in the whole executable: the condition at
  `0x567087` reads it, and `0x62D497` and `0x644685` set it for occupants of
  bikes and open-topped vehicles. Extending the same condition therefore cannot
  reach any other part of the game.
- `CWorld::bIncludeBikers` at `0xB7CD6F` is set in eight places: inside
  `CBulletInfo::Update`, `CWeapon::FireInstantHit`,
  `CWeapon::FireInstantHitFromCar2` and `CWeapon::FireM16_1stPerson`, in the
  player's weapon target search at `0x60B650`, and in
  `CWorld::ProcessLineOfSightSector`, which only restores it between the sector
  lists of a query one of the others started. The gate therefore confines the
  change to weapon fire and to weapon target selection.
- `CWorld::ResetLineTestOptions @ 0x5631C0` runs only after all four line tests
  in `CWeapon::FireInstantHit`, so the gate cannot drop shots part way through
  firing.
- Health is applied per hit, synchronously. `CPedDamageResponseCalculator::`
  `ComputeDamageResponse @ 0x4B5AC0` calls `0x4B3210`, which writes
  `CPed::m_fHealth` at `+0x540`. The queued `CEventDamage` drives the reaction,
  not the health, so a hit that registers is never lost to the event queue.
- The replacement assembles to 15 bytes of code and 13 `nop`, its `call` is a
  correctly formed `rel32` to the plugin's predicate, and its `je` targets
  `0x56727E`, the same address both replaced conditions jumped to.
- The compiled predicate takes its argument in `ecx`, preserves `ebx`, `esi`
  and `edi` and does not touch `ebp`, so the registers the patched loop keeps
  live across the call, in particular `edi` holding the ped and `ebp` holding
  zero, are safe.
- `Release|Win32` rebuilds cleanly with warnings as errors and produces an
  x86 DLL.

## Implemented, validation pending

Nothing in this list has been observed in a running game yet:

- The getting-in half. `CAR_GET_IN` was seen included, but `CAR_ALIGN`,
  `CAR_OPEN_DOOR_FROM_OUTSIDE`, `CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE` and
  `BIKE_PICK_UP` never appeared in the samples, because the player never
  happened to be aiming along a line that crossed such a ped.
- Carjacking, either side. None of the four drag tasks appeared.
- That the effect stops the moment the ped is seated, so an occupant still
  cannot be shot through the car body.
- Whether covering `CAR_CLOSE_DOOR_FROM_OUTSIDE` makes damage feel continuous
  from the door opening to the ped walking away.
- Shotgun pellets and the sniper rifle, which reach the same line test through
  `CWeapon::FireInstantHit` and `CWeapon::FireM16_1stPerson` but with different
  parameters.
- The weapon auto-aim locking on to a ped at a vehicle, which follows from the
  same gate and has to be confirmed as an improvement rather than an annoyance.
- Absence of any effect on AI behaviour and on the chase camera, which is what
  keeping the `CWorld::bIncludeBikers` gate is meant to guarantee.
- The cost of the task traversal with many occupied vehicles in range, which is
  the only thing the plugin adds to a weapon line test.

## Required validation

- Repeat the diagnostic run against the released build and confirm that the
  stretch which read "not shootable" during `CAR_CLOSE_DOOR_FROM_OUTSIDE` now
  reads the other way and that health keeps falling through it.
- Drive the getting-in half deliberately, aiming at a ped walking up to a car
  door, so that `CAR_ALIGN` and `CAR_OPEN_DOOR_FROM_OUTSIDE` actually appear.
- Play a SA-MP round with the plugin loaded and confirm that no anti-cheat
  reacts to the patched bytes.

## Not implemented or intentionally out of scope

- `TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP` and
  `TASK_COMPLEX_CAR_QUICK_BE_DRAGGED_OUT`, the two remaining complex drag-out
  parents. They were never observed and no site turning the collision off was
  traced to them, so they are not in the list on the same rule as everything
  else.
- Damage that does not go through a line of sight test.
  `CWorld::TriggerExplosionSectorList` and `CWorld::SetPedsOnFire` skip any ped
  with `bInVehicle` set outright, and melee attacks resolve against a sphere
  rather than a line, so a ped at a vehicle stays immune to explosions, fire
  and melee exactly as in the stock game. Changing those means changing
  decisions the game makes deliberately, not a missed one.
- Restoring the patch after another modification has overwritten the site.
- Executable versions other than 1.0 US. The plugin refuses to patch them
  rather than guessing at different addresses.
- A runtime toggle hotkey. Applying the patch is a one-way operation in this
  version.

## Release criteria

1.0.0 ships with the getting-out half watched working in game and the rest only
reasoned about. Before the fix can be called fully validated:

- Everything under "Implemented, validation pending" observed in game, in
  single player and on a SA-MP server.
- No new warnings in a clean `Release|Win32` rebuild.
- Version strings in `srcersion.h`, `CHANGELOG.md` and the release
  archive name in agreement.
