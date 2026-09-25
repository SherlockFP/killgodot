# SPRINT-027 — Character variety and viewmodel animation polish

User (2026-09-25):
- "The viewmodel hand animations look very janky."
- "We need more than one character model; every role should look different."

**Design note (the charter's secrecy rule):** in a social deduction game, a role that shows on the body reveals the role
to everyone. So variety is **per player, not per role**. Every villager looks distinct: body type, outfit, hat, hair,
colours from the Quaternius villager and outfit kits plus the cosmetics. Role identity can only show where it is safe:
- the reveal card;
- your own first-person hands/sleeves, which only you see: a subtle role-coloured cuff or ring;
- publicly revealed roles after death or at the trial: a role sash appears on the corpse or the revealed player.

## Acceptance (fixed)
1. **Villager variety.**
   - At least 12 visually distinct villager archetypes (male/female/older/stocky/tall), with modular outfit sets
     (fisher, baker, smith, farmer, sailor, clergy, merchant, noble…) and a per-player randomised palette.
   - Seeded per match; no two players identical in a 20-player lobby.
   - Players can lock a preferred look in the cosmetics (the loadout overrides the random one).
   Evidence: a contact sheet of 20 generated villagers (offscreen render) and a uniqueness test.
2. **Safe role visuals.** Your own FP sleeve/cuff tinted by alignment (owner-only), and a public "revealed role" sash
   when a role becomes public. A test proves other clients never receive role-dependent appearance data.
3. **Viewmodel animation polish (FPArms2).**
   - Fix the jank:
     - pops between clips (proper blend times per transition);
     - an idle breathing/sway layer;
     - a walk/run bob tied to speed;
     - landing reactions;
     - look-sway lag;
     - no snapping when switching items.
   - Re-author the worst clips in the generator: punch, slash_b's upright frame, reach_grab's palm orientation, the
     shovel/rod grips.
   Evidence: per-frame metrics (joint velocity spikes below a threshold, no single-frame tip jumps > 0.1 screen), and
   an offscreen capture strip of every transition.
4. **Invariants** pass.

## Writable scope
- Character/ (the appearance component + FP arms anim code)
- Cosmetics/ (loadout integration)
- Tools/Blender/kg_make_fp_arms2.py + the villager/outfit tools
- new assets
- tests, docs

## Limits
- 2 look rounds
- 3 fix attempts per failing check
- plateau stop

## Result, SPRINT-027a (items 1, 2, 4; 2026-09-25)
User widened the scope mid-sprint ("everyone's model should be random: women and men, witches, superheroes, whatever;
lots of models; random on join"). Delivered, all headless:
- 38 archetypes (`Character/KGVillagerLook.cpp`): 20 men / 18 women, 8 body meshes on the ONE shipped villager skeleton
  (shipped M/F + 6 new Blender-composed bodies: bald peasant, ranger, mixed), 5 builds (regular/tall/stocky/short/slim),
  6 UBC hairstyles + bald, beard, hood, pauldrons, cape, apron, 4 hats (witch, straw, toque, crown), 3 dyes each,
  5 skin tones, 8 hair colours, 6 elderly. Every archetype has a unique silhouette (tested).
- Per PLAYER, never per role: `UKGCosmeticsComponent::Look` (public, seeded by match seed + player id, unique in the
  lobby, cosmetics loadout overrides the archetype's hat/beard/pauldrons, `kg.Look.Lock <Archetype>` locks one).
- Safe role visuals: owner-only cuff on the FP arms from `PrivateRoleId` (COND_OwnerOnly), public sash from
  `RevealedRoleId`. `UKGAppearanceComponent` replicates nothing (test `KillGodot.Appearance.RoleDataNeverPublic`).
- Evidence: `Saved/Screenshots/Villagers/` (lobby20_idle, archetypes_*_{idle,sit,dance}, fp_cuff, sash_reveal),
  `Tools/Unreal/kg_villager_sheet.ps1`; tests `KillGodot.Appearance.*` (4).
- Not done: the 59 external Quaternius-2019 bodies (`/Game/KillGodot/Env/Ext/Characters`) are on a different
  skeleton; retargeting is a sprint of its own (Backlog proposal). Item 3 (viewmodel polish) is SPRINT-027b.
