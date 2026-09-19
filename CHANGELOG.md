# Changelog

## 1.1.1

- Added `README.txt` to the release archive.

## 1.1.0

- Removed the INI and the log; the plugin has nothing to configure.

## 1.0.1

- Added version information to the plugin file.
- Removed `README.txt` from the release archive; the repository README is the
  documentation.

## 1.0.0

- Added the fix for a ped being unhittable while it is at a vehicle. The game
  switches the ped's collision off from the moment it reaches the door, and
  `CWorld::ProcessLineOfSightSectorList` skips a ped without collision, so
  bullets travel straight through somebody standing in plain sight beside the
  car. The plugin extends the condition the game already uses for bikers, so
  such a ped is included in the line test the same way a biker is.
- Added coverage for getting in: `CAR_ALIGN`, `CAR_OPEN_DOOR_FROM_OUTSIDE`,
  `CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE`, `BIKE_PICK_UP` and `CAR_GET_IN`. The
  window opens in `CTaskComplexEnterCar::CreateNextSubTask` before the align
  task is even created, so it is often two or three seconds long.
- Added coverage for getting out: `CAR_GET_OUT`, `CAR_CLOSE_DOOR_FROM_OUTSIDE`,
  `CAR_JUMP_OUT` and `CAR_FALL_OUT`. Closing the door matters as much as the
  exit animation: a police ped was measured standing on the road with its
  collision still gone for the 200-300 frames that task lasts, which is what
  made damage feel like it stopped landing part way through a ped getting out.
- Added coverage for carjacking on both sides, the ped dragging a driver out
  and the driver being dragged out, which happen in the same window.
- Added the `CWorld::bIncludeBikers` gate to the new condition, so the change
  is only visible to weapon fire and to the weapon target search that drives
  auto-aim, which are the two things that raise that flag. AI line of sight,
  camera collision and every other query see exactly what they saw before, and
  ped physics, collision flags and tasks are never written to.
- Kept every task during which the ped is really seated inside the vehicle out
  of the covered set, including `CAR_WAIT_TO_SLOW_DOWN`, which also runs without
  collision while the ped waits for the car to slow down before getting out. An
  occupant still cannot be shot through the body of the car.
- Added verification of the 28 instruction bytes the plugin replaces and of
  four byte sequences inside `CTaskManager::FindActiveTaskByType` that pin the
  task manager layout. Nothing is written unless all of them match the 1.0 US
  build, so any other executable version is left untouched.
- Added an optional `VehiclePedDamageFix.log` recording the result of the
  patch, a note if another modification later overwrites the patch site, and a
  note the first time a ped at a vehicle is included in a weapon line test.
- Added generation of `VehiclePedDamageFix.ini` when it is missing, byte for
  byte identical to the canonical `Config\VehiclePedDamageFix.ini` compiled
  into the plugin.
