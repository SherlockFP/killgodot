# KILLGO

> *Herkes bekliyor. Biri sabırsız.*

**Morrowmere** adlı sisli bir balıkçı köyünde geçen, first-person, proximity sesli chat'li sosyal çıkarım oyunu.
Town of Salem × Garry's Mod Murder × Lockdown Protocol × R.E.P.O. hissinde, CS2 kalitesinde viewmodel var.
Karakterler çeneleri menteşeli, boyalı ahşap kuklalar. Unreal Engine 5.8.3 + Blender 5.2 ile yapılıyor.

## Nereden başlamalı
| Okumak istediğin | Dosya |
|---|---|
| Oyun ne? | [Docs/00_Vision.md](Docs/00_Vision.md) |
| Maç nasıl oynanıyor? | [Docs/01_GDD_Core.md](Docs/01_GDD_Core.md) |
| Köy hayatı, mini oyunlar, NPC'ler, meme'ler, Panayır | [Docs/01b_Village_Life_Fun.md](Docs/01b_Village_Life_Fun.md) |
| 51 rol | [Docs/02_Roles.md](Docs/02_Roles.md) |
| Lore ve 30 easter egg | [Docs/03_Lore.md](Docs/03_Lore.md) |
| Harita | [Docs/04_Map_Village.md](Docs/04_Map_Village.md) |
| Teknik mimari (P2P, host migration, ses, viewmodel) | [Docs/05_Tech_Architecture.md](Docs/05_Tech_Architecture.md) |
| Sanat yönü | [Docs/06_Art_Direction.md](Docs/06_Art_Direction.md) |
| Kozmetik, kasalar, Almanak | [Docs/07_Economy_Cosmetics.md](Docs/07_Economy_Cosmetics.md) |
| Menüler, lobi, HUD, diller | [Docs/08_UI_UX.md](Docs/08_UI_UX.md) |
| Yol haritası ve Gauntlet döngüsü | [Docs/09_Roadmap_Gauntlet.md](Docs/09_Roadmap_Gauntlet.md) · [Docs/Backlog.md](Docs/Backlog.md) |
| Araştırmalar | [Docs/Research/](Docs/Research/) |
| Hazır promptlar | [Docs/Prompts/](Docs/Prompts/) |

## Klasörler
```
KillGodot.uproject     UE 5.8 projesi (C++ modül: Source/KillGodot)
Config/                Motor, oyun ve input ayarları
Source/KillGodot/      Core (GameMode/State, MatchClock, Rng) · Roles · Character (FP + viewmodel) · Voice · Online
Tools/Blender/         Headless Blender pipeline: palet, kukla üretici, varyasyon sayfaları
Tools/Gauntlet/        Kalite kapıları (derleme + testler)
Art/                   Palet texture'ı, konsept render'lar, varyasyon sayfaları
Docs/                  Tasarım, araştırma, promptlar, backlog
```

## Durum
**M0 (Temel) tamam.** Sıradaki **M1 (Araç zinciri)**, ama önce **Visual Studio kurulumu** gerekiyor: [Docs/Prompts/M1_Toolchain.md](Docs/Prompts/M1_Toolchain.md).
