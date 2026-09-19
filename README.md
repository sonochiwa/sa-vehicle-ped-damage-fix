# Vehicle Ped Damage Fix

`VehiclePedDamageFix.asi` is a GTA San Andreas plugin that makes a ped
shootable while it is getting into or out of a vehicle.

In the stock game a ped loses its collision the moment it reaches a vehicle
door and keeps it off until it is seated, or until it has closed the door
behind it on the way out. Bullets pass straight through somebody standing in
plain sight next to a car. The plugin includes such a ped in weapon line
tests the same way the game already includes bikers, and touches nothing
else.

## Features

- Peds getting in, getting out, bailing out or being carjacked can be shot
  and auto-aimed, for the player and every other ped alike.
- A ped seated inside the vehicle still cannot be shot through the body.
- Ped physics, collision, animations and AI vision are unchanged.
- Verifies the bytes it replaces before writing and refuses to patch any
  other executable.

## Requirements

- GTA San Andreas 1.0 US (Compact or Hoodlum executable), or a SA-MP
  installation based on it.
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.

Other executable versions are left untouched.

## Installation

1. Extract `VehiclePedDamageFix.asi` into the GTA San Andreas directory or its
   `scripts` directory.
2. Start the game.

There is nothing to configure. Remove the file to uninstall.

## Release Integrity

Releases are built by GitHub Actions from the tagged commit and carry a
SHA-256 file and a build-provenance attestation:

```text
gh attestation verify VehiclePedDamageFix-vX.Y.Z.zip -R sonochiwa/sa-vehicle-ped-damage-fix
```

## License

MIT. See [LICENSE](LICENSE).
