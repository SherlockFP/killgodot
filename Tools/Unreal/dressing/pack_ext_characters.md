# pack_ext_characters: external rigged character / animal packs

Imported 2026-09-25 by `Tools/Unreal/kg_import_ext_chars.py` (headless) under `/Game/KillGodot/Env/Ext/Characters/<Pack>/<File>/…`
as skeletal meshes with their OWN skeletons, their shipped materials/textures and (FBX packs) their animation clips.
Nothing is on the shipped villager skeleton: see "Skeleton compatibility" before using them as player models.
Rig facts come from `Tools/Blender/kg_ext_rig_info.py` (`Saved/Setup/ext_rig_info*.json`). Result JSON: `Saved/Logs/kg_import_ext_chars_*.json`.

## Reference: the shipped villager skeleton
- `Quaternius Universal Base Characters` (UBC) / Universal Animation Library (UAL): **65 bones, UE-mannequin names**
  (`root, pelvis, spine_01..03, neck_01, head, clavicle_l, upperarm_l, lowerarm_l, hand_l, thumb_01_l …, thigh_l, calf_l, foot_l, ball_l`),
  1.81 m, shipped as `/Game/KillGodot/Characters/Villager/SK_KG_Villager_M_Skeleton`.
- **None of the packs below use those bone names.** Priority-1 (same skeleton) packs do not exist for free: Quaternius' older
  packs pre-date UBC. Everything here needs IK-Rig retargeting (UE IK Retargeter, humanoid chains) onto the villager skeleton,
  or is used as an NPC/animal with its own clips.

## Skeleton compatibility table
| Pack | Files → skeletal meshes | Bones | Root / naming | Height | Clips in file | Retarget notes |
|---|---|---|---|---|---|---|
| Quaternius_UltimateModularWomen (10: Adventurer, Casual, Formal, Medieval, Punk, SciFi, Soldier, Suit, **Witch**, Worker) | glTF, each character = 4-5 skinned part meshes (Body, Head, Legs, Feet [, Backpack]) sharing one skeleton per character | 62 | `Root, Body, Hips, Abdomen, Torso, Chest, Neck, Head, Shoulder.L, UpperArm.L, LowerArm.L, Hand.L, fingers…, UpperLeg.L, LowerLeg.L, Foot.L` (Quaternius humanoid, Blender `.L/.R` suffixes) | 2.05 m | 24 clips per glTF (Idle, Walk, Run, Jump, Attack, Death…) | Humanoid, full finger set: retarget with an IK Rig (chains spine/neck/head/arms/legs/fingers). A separate **Mixamo-style "Humanoid Rig" FBX** per character is on disk (`Art/Source/…/Humanoid Rigs/Individual Characters/FBX/*.fbx`, 38 bones `Root, Hips, Abdomen, Torso, Chest, Neck, Shoulder.L…`) if the UE Mixamo retarget path is preferred. Parts: attach with SetLeaderPoseComponent or merge in Blender. |
| Quaternius_UltimateModularMen (11: Adventurer, Beach, Casual_2, Casual_Hoodie, Farmer, **King**, Punk, Spacesuit, Suit, Swat, Worker) | same layout as the women | 62 | same | 1.9 m | 24 | same as above (`Humanoid Rig/Individual Characters/FBX/*.fbx` on disk) |
| Quaternius_AnimatedCharacters (52: Casual/Chef/Cowboy/Doctor/Elf/Goblin/Kimono/Knight/Ninja/OldClassy/**Pirate**/Soldier/Suit/Viking/**Witch**/**Wizard**/Worker/Zombie, most in M+F, + Cow, Pug, hats/hair) | FBX, one skeletal mesh + 11 anim sequences each | 23 | `Bone` (root) → `Body, Hips, Abdomen, Torso, …, Foot.L` — Quaternius 2019 rig, no fingers | ~1.9 m after FBX scale | 11 (Idle, Walk, Run, Jump, Punch, Death…) | Simple humanoid (no fingers, no twist bones): IK-Rig retarget works for locomotion; hands stay stiff. Textures are one palette PNG per character. ~2.7k tris (Pirate) … 7k (Witch). |
| Quaternius_RPGCharacters (6: Cleric, Monk, Ranger, Rogue, Warrior, Wizard) | FBX, 1 mesh + clips each; weapons in `Only Weapons/` (not imported) | 34 | `Root` → `Body, Hips, Abdomen, Torso…`; simple fingers | 1.8 m | 15 | Same family as AnimatedCharacters; best free "fantasy class" set (wizard hat, cleric robe). |
| Quaternius_AnimatedKnight (1) | FBX + separate Sword/Katana/Club/Helmet/ShoulderPads static FBX on disk | 31 | `Bone` root, Quaternius 2018 rig | 1.8 m | 12 (Idle, Walk, Run, Roll, Attack…) | Same family; armour variants via the extra FBX meshes (socket them). |
| KayKit_Adventurers (6: Barbarian, Knight, Mage, Ranger, Rogue, Rogue_Hooded) + `Rig_Medium_General`, `Rig_Medium_MovementBasic` (animation-only GLBs, mannequin mesh) | GLB, each character = several skinned parts (Body, ArmLeft/Right, Head, Legs…) on one skeleton | 23 | `root, hips, spine, chest, upperarm.l, lowerarm.l, wrist.l, hand.l, head, upperleg.l, lowerleg.l, foot.l, toes.l …` (KayKit "Rig_Medium") | 2.5 m (KayKit chunky proportions; scale ≈0.72 for 1.8 m) | 0 in character files; 15 + n clips in the two Rig_Medium GLBs (share the same skeleton) | Retarget the Rig_Medium clips onto the characters (same skeleton), or IK-retarget villager animations. Proportions are big-head chibi: fits as NPC/mascot rather than a player body. Textures: one gradient atlas. |
| KayKit_Skeletons (4: Skeleton_Mage, Minion, Rogue, Warrior) + same two Rig_Medium anim GLBs | as Adventurers | 23 | same Rig_Medium skeleton as Adventurers | 2.6 m | 0 / 15+ | Catacomb / Storm Manor crypt props or undead NPCs; Halloween match cosmetic. |
| Animals_Quaternius (12: Cat, Dog, Eagle, Piranha, **Wolf** (forest wolf for SPRINT-033c), Cow, Horse, Llama, Pig, Pug, Sheep, Zebra) | FBX, 1 mesh + clips each | 24-28 | quadruped rigs (`root/Bone` → `Body, FrontLeg.R, FrontUpLeg.R …`), IK helper bones present | wolf 0.9 m, horse 1.7 m | Wolf 2 clips in file (Idle/Walk-family, check names), farm animals 6 (Idle, Walk, Run, Jump, Death, Eat) | Use with their own clips (AnimSequences imported next to each mesh). Wolf ~620 tris. |
| Fish_Quaternius (7: Dolphin, Fish1-3, Manta ray, Shark, Whale) | FBX | 8 | `Root, Spine1-3, Tail…` | 0.3-3 m | 1 (Swim) | Harbour water / aquarium dressing; M_KG_Fish is not needed since they have a real swim clip. |
| Monsters_Quaternius (4: Bat, Dragon, Skeleton, Slime) | FBX | 8-12 | custom | 0.5-5 m | 5 | Bat for the Storm Manor attic/tower, Skeleton for catacombs. |

## Where the assets are
- Static parts / hats: `Chef_Hat`, `Cowboy_Hair`, `Ninja_Male_Hair`, `VikingHelmet` came in as skeletal meshes on the character rig
  (attach to the head bone). Knight `Helmet1-3`, `ShoulderPads`, weapons and the RPG `Only Weapons` FBX stay on disk (not imported).
- Every glTF/GLB import produced `…/<File>/SkeletalMeshes/*`, `…/Skeletons/*`, `…/Materials/*`, `…/Textures/*` and
  `…/Animations/*` (Interchange layout); FBX imports produced `<File>` (mesh), `<File>_Skeleton`, `<File>_Anim_*`, `<File>_PhysicsAsset` is off.
- Sizes on disk (Art/Source): AnimatedCharacters 83 MB, UltimateModularMen 40 MB, UltimateModularWomen 35 MB, RPGCharacters 21 MB,
  Skeletons 10 MB, Adventurers 7 MB, FarmAnimals 9 MB, AnimatedAnimals 3 MB, Knight 3 MB, Monsters 3 MB, Fish 1 MB.

## Suggested use (for the villager-variety agent)
1. Player variety with the least work: **Quaternius_AnimatedCharacters + RPGCharacters + Knight** (same 23-34-bone family):
   build ONE IK Rig for the Quaternius-2019 rig and one for the villager skeleton, then a single IK Retargeter maps all 59
   bodies onto the villager animations (UAL). Witch, Wizard, Pirate M/F, Knight (golden), Viking, Kimono, Elf, Cowboy, Suit,
   OldClassy, Chef, Doctor, Worker, Zombie give the "random weird player models" the user asked for.
2. Highest quality: **Ultimate Modular Men/Women** (62-bone, fingers, 24 clips, swappable Body/Head/Legs/Feet across the
   10+11 characters = hundreds of combos). Needs the finger-aware IK Rig or the Humanoid-Rig FBX variant.
3. KayKit Adventurers/Skeletons: chibi proportions; NPC/cosmetic mascots or the Storm Manor undead, not player bodies.
4. Wolf → `/Game/KillGodot/Env/Ext/Characters/Animals_Quaternius/Wolf/Wolf` with its clips (forest wolf, SPRINT-033c).
