# Pack: KG_DressVillage (village & farm props)

Made by `Tools/Blender/kg_make_dress_village.py` (procedural, Quaternius-style low poly, vertex colours), imported with
`Tools/Unreal/kg_import_dress_pack.py`. Checked in-engine (spawn row + captures). Sizes are the imported UE bounds in cm
(X x Y x Z).

**Batch 1 folder:** `/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/`
Use `P1 = "/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/SM_KG_"` and `C.place(P1 + "Fountain", ...)`.

## Orientation rules (read once)
- **Front = +Y in UE** (door, steps, sign faces, window box front, laundry basket side). yaw=0 -> front faces +Y
  (towards the sea). To make the front face a point (tx, ty): `yaw = math.degrees(math.atan2(ty - y, tx - x)) - 90`.
- Pivot = bottom centre (min Z = 0) unless listed otherwise below.
- Materials: **VC** = M_KG_PropVCLinear, **Sway** = M_KG_JapanFoliage (vertex-alpha wind: cloth, ribbons, leaves;
  the sway grows with height above the actor origin, so ground-level cloth moves gently).
- Collision: **complex** = walkable/accurate, **box** = one box, **none** = walk-through (small clutter / overhead).

## Batch 1 (landmarks, festival, signs, planters)

| Prop | Size cm | Pivot | Mat | Collision | How to place |
|---|---|---|---|---|---|
| `Fountain` | 340 x 340 x 334 | bottom centre | VC | complex | ICONIC 3-tier stone fountain: octagonal basin (rim h 76, sit-able), golden leaping fish on top spouting water, lily pads + coins. Square centrepiece; keep 2 m clear around it. `cull` none. |
| `Maypole` | 137 x 134 x 655 | bottom centre (pole axis) | Sway | box | ICONIC candy-striped 6.5 m pole in a flower tub, flower crown (hoop at z 505, r 58), pennant on top. **Always add `Maypole_Ribbons` at the same location/yaw.** |
| `Maypole_Ribbons` | 440 x 468 x 412 (z 91..503) | same as Maypole (pole axis, ground) | Sway | none | 8 coloured ribbons fanning from the crown down to ~1 m height, radius ~2.3 m (walk-through). Place with the identical transform as the Maypole. |
| `Windmill_Body` | 842 x 624 x 1042 | tower centre, ground | VC | complex | ICONIC tower mill: stone base, whitewashed tower, reefing gallery at z 272 (r 312), red ogee cap, tail pole + wheel reaching X -530. Door + steps face **+Y**, the sail shaft points **+X**. Needs ~11 x 7 m of flat ground (use `C.ground_min(x, y, 300)`). |
| `Windmill_Sails` | 79 x 825 x 825 | **hub centre** | VC | none | 4 lattice sails (r 470) in the local YZ plane, rotate about local X. Place at `body + rotate(yaw) * (275, 0, 860)` with the body's yaw, via `C.mover(path, x, y, z, yaw=yaw, spin=(0, 0, 15..25))` (roll spin). Lowest sail tip clears the ground by ~4 m. |
| `Stage` | 358 x 306 x 345 | platform centre, ground (steps stick out 102 cm on +Y) | VC | complex | 3 x 2 m speaker's platform, deck z 80, steps + handrails on the +Y front, railings, lectern with a bell and scroll, navy town banner with a golden fish, pennants. Town-crier / meeting spot. Put `C.seat` benches in front. |
| `Bunting` | 607 x 9 x 80 (z -76..4) | **west end of the string** | Sway | none | 6 m pennant string along +X with a 45 cm sag. Hang between two points of equal height (eaves, posts, lamp posts at ~400 cm): place at the west anchor with `yaw = atan2(dy, dx)` and scale X = distance / 600. Optional: `C.mover(path, ..., sway=6, sway_hz=0.3)` swings it about the line between its ends (looks great). |
| `LaundryLine` | 423 x 111 x 206 | bottom centre (between posts) | Sway | complex | Two T-posts 4 m apart (x = +-200), sheet, shirt, trousers, socks, gingham towel, green dress, clothes pegs, laundry basket on the +Y side. Backyards / between houses. |
| `FlowerBox` | 123 x 32 x 49 (z -15..34) | **box bottom centre**, back face at y = -14 | VC | none | 1.2 m teal window box with geraniums and trailing ivy + iron brackets below. Mount under windows: z = sill (~100-110 cm above floor), 14-16 cm out from the wall, front (+Y) facing out. |
| `Planter_Large` | 121 x 121 x 133 | bottom centre | VC | box | Blue plank planter with a round topiary bush and flowers, gold post finials. Plaza edges, doorsteps of important buildings. |
| `Planter_Pot` | 54 x 53 x 79 | bottom centre | VC | box | Terracotta pot with red geraniums. Doorsteps, stairs, window sills (scale 0.6), in pairs by doors. `cull=6000`. |
| `HangingSign_Fish` | 74 x 9 x 64 (z -60..4) | **hinge (top bar axis)** | VC | none | Fishmonger sign (blue board, white fish). Both faces painted. |
| `HangingSign_Bread` | 74 x 9 x 64 | hinge | VC | none | Bakery sign (red board, golden loaf). |
| `HangingSign_Ale` | 74 x 9 x 64 | hinge | VC | none | Inn / tavern sign (green board, frothy tankard). |
| `HangingSign_Anvil` | 74 x 9 x 64 | hinge | VC | none | Smithy sign (orange board, anvil + sparks). |
| `HangingSign_Herb` | 74 x 9 x 64 | hinge | VC | none | Herbalist / apothecary sign (cream board, mortar + sprig). |
| `SignBracket` | 101 x 9 x 49 (x 0..101, z -39..10) | **wall contact at bar height**, arm along +X | VC | none | Wrought-iron wall bracket with scrolls. Put the pivot on the wall face at ~300 cm, yaw so +X points away from the wall. The sign hangs at `bracket + rotate(yaw) * (55, 0, 0)` with the same yaw. |
| `SignPost` | 121 x 38 x 313 | post centre, ground | VC | complex | Free-standing wooden gallows post (arm along +X) with a paper notice nailed on the +Y face. The sign hangs at `post + rotate(yaw) * (60, 0, 270)` with the same yaw (board bottom ~210 cm, walkable underneath). |

### Hanging signs: make them swing
```python
import math
def hang_sign(kind, px, py, pz, yaw, on_post=True):
    base = P1 + ("SignPost" if on_post else "SignBracket")
    C.place(base, px, py, pz, yaw=yaw, cull=12000)
    hx, hz = (60.0, 270.0) if on_post else (55.0, 0.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    C.mover(P1 + f"HangingSign_{kind}", px + c * hx, py + s * hx, pz + hz, yaw=yaw, sway=5.0, sway_hz=0.35)
```
Sway swings the board about its local X hinge (the bar), i.e. it flaps perpendicular to its faces.

### Windmill recipe
```python
x, y, yaw = ..., ..., 30.0
z = C.ground_min(x, y, 300)
C.place(P1 + "Windmill_Body", x, y, z, yaw=yaw, claim_r=450)
c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
C.mover(P1 + "Windmill_Sails", x + c * 275, y + s * 275, z + 860, yaw=yaw, spin=(0.0, 0.0, 18.0))
```

## Batch 2 (farm, market goods, clutter, animals)
In progress - will be added below as `KG_DressVillage2_Clean` (Firewood_Stack, Sack, SackPile, Wheelbarrow, Broom,
RainBarrel, Goods_*, Awning, Haystack, HayBales, Scarecrow, Beehive, StoneWall, WaterTrough, Pumpkin, Cabbage,
WheatClump, Sheep, Chicken, Cat_Sleeping, Coop).
