# TPS weapon animation

Firing previously selected a full-body Fire state for 0.10 seconds per shot, then
returned to Idle/Aim/locomotion. That repeatedly raised/lowered the rifle and
replaced leg animation. The shot clock now controls effects only.

Locomotion and the weapon pose run independently:

| Input | Body | Weapon |
| --- | --- | --- |
| Standing + LMB | Idle | Ready pose + recoil |
| Standing + RMB + LMB | Idle | Ready pose + recoil, ADS camera |
| Moving + LMB | Directional weapon locomotion | Ready pose + recoil |
| Moving + RMB + LMB | Directional weapon locomotion | Ready pose + recoil, ADS camera |
| Shift + forward + LMB | AimRun | Ready pose + recoil |
| Shift + forward + RMB + LMB | AimRun, 3.7 m/s aimed jog | Ready pose + recoil, ADS camera |

The human uses its directional Aim clips for strafing/backward movement. The base
mannequin keeps its smaller Walk/Run set. Unarmed sprint remains 7 m/s; aimed
movement is 2.3 m/s or 3.7 with Shift. Hip-fire sprint keeps its existing speed.
Side/back clips are still authored walk cycles, not a continuous speed blend space.

The reusable Animator upper-body layer masks a named spine and all descendants.
The pelvis and legs retain the base pose and clock. It samples a fixed Aim pose
and adds a small procedural local-spine recoil pulse per accepted shot; the
source full-body pistol Fire clip is intentionally not played. This is not hand
IK, aim-pitch correction, or newly authored rifle mocap. Hip fire and ADS currently
share the weapon pose, with different camera/movement behavior.

Weapon weight rises over 0.10 s and falls over 0.18 s. Held fire keeps it raised
between shots; a 0.28 s grace period after the last shot prevents tap-fire pumping.
Reload now plays on the same masked upper-body layer while locomotion continues.
Its sample time is normalized to Weapon::ReloadDuration (2.1 seconds), so changing
movement cannot restart it. Reload suppresses recoil. Aim/reload transitions use
0.12-second crossfades from the displayed upper-body pose, including interrupted
fades; reload completion returns directly to aim or fades the layer down to idle.
Hit and death suppress the weapon layer.
Hit, Land, Jump, Reload and Death clips do not loop. Base-state crossfades can be
interrupted from their current blended pose instead of waiting out stale input.

## Verification

`bash Scripts/TestWeaponAnimation.sh Assets/Cooked/TPS/Human.plasset` checks both
rigs: repeated shots, ADS toggles, release grace, suppression, unchanged lower-body
poses/clocks, finite matrices, reload without leg interruption, and zero-time continuity when
interrupting both body and weapon-action fades.
It requires the pinned Ozz library from the existing asset test build.
`bash Scripts/TestTPS.sh` covers the existing gameplay regression suite.

These checks run on Linux; Windows visual acceptance remains necessary. Pull
prototype, regenerate projects, build Sandbox, and test all six rows above,
including changing direction, releasing fire while holding aim, reload during
fire, jumping, taking damage, and death/redeploy. No asset recook is required for
this change. Lighting and textures are unchanged.
