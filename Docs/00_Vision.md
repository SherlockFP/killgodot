# KILL GODOT — Vizyon

> **"Herkes bekliyor. Biri sabırsız."**
> *Everyone's waiting. Someone's impatient.*

**Lore ve marka:** dünya, bölgeler, rol sözleri ve oyun içi metin bankası → [`Lore/KillGo_Lore.md`](Lore/KillGo_Lore.md) (logo, key art ve yükleme ekranları: `Art/Brand/`).

## Tek cümlede oyun
Denize kıyısı olan, dağlarla çevrili, sisin içinde zamana hapsolmuş bir balıkçı köyünde geçen, **first-person**, **proximity sesli chat** odaklı bir sosyal çıkarım oyunu. Gündüz herkes mesleğinde çalışıp görev yapar, gece Sabırsızlar avlanır. Kasaba konuşarak, delil toplayarak ve gerekirse tavayla dövüşerek kendini savunur.

## Referanslar ve her birinden ne aldığımız
| Oyun | Ondan aldığımız |
|---|---|
| **Town of Salem** | Rol havuzu, gece/gündüz döngüsü, mahkeme/oylama/asma, Last Will, Jester. Sanat ruhu: renkli, biraz karikatürize koloni köyü. |
| **Garry's Mod – Murder** | Gerçek zamanlı öldürme, nadir tek mermili silah, masuma ateş edenin cezası, loot. |
| **Lockdown Protocol** | Fiziksel görevler (eşyayı taşı, makineye koy), fake atılabilen görevler, toplantı masası. |
| **R.E.P.O.** | Konuştukça ağız oynaması, fizikle tutup fırlatma, sesli chat'in komikliği. |
| **Among Us** | Herkesin yapabildiği görevler, sabotajlar, hayalet olunca görev yapmaya devam etme. |
| **Counter-Strike 2** | Ayrı render edilen viewmodel, akıcı silah animasyonları, inspect, skin'ler (wear/pattern), kasa açma heyecanı. |
| **Source Engine / Garry's Mod** | Fizik sandbox hissi: her şey itilir, düşer, kırılır, istiflenir. |

## Tasarım sütunları
1. **Sesin kendisi oyundur.** Proximity voice, duvar arkasından boğuk gelen sesler, konuşunca açılıp kapanan ağızlar. Hayaletler kendi kanalında, katiller gece telsizinde konuşur. Susturulmak (Şantajcı) gerçekten susturulmaktır.
2. **Derin ama okunaklı çıkarım.** 48 rol, fiziksel delil (ayak izi, kan, düşen eşya) ve maske mekaniği. Her ölümün arkasında çözülebilecek bir hikâye olur.
3. **Fiziksel, canlı dünya.** Kapılar kilitlenir, barikat kurulur, kasalar kırılır. Eşyalar taşınır, fırlatılır, balık bile silah olur.
4. **Canlı renkler, süper optimizasyon.** Az poligon, tek palet texture'ı, doygun renkler ve güçlü silüetler. 20 oyunculu P2P maç orta seviye PC'de 120+ FPS vermeli, ileride telefonda da akmalı.
5. **Keşif ve gizem.** Mahzenler, kaçakçı tünelleri, deniz mağaraları, kırılabilir duvarlar, lore sayfaları ve bol easter egg. Köy her maçta biraz daha fazlasını anlatır.
6. **Adil denge ve ölçek.** 6 ile 20 oyuncu arasında rol dağılımı, harita bölgeleri, görev sayısı ve süreler otomatik ölçeklenir. Denge telemetriyle sürekli ayarlanır.
7. **Ölen oyuncu sıkılmaz.** Hayalet formunda keşif yapar, hayalet görevleri yapar, medyumla konuşur.

## Temel parametreler
| | |
|---|---|
| Motor | Unreal Engine **5.8.3** (C++ çekirdek + Blueprint içerik) |
| Modelleme | Blender **5.2.2 LTS** (headless Python pipeline) |
| Kamera | **First-person**, ayrı render edilen viewmodel (CS2 tarzı) |
| Oyuncu | **min 6 – max 20**, her şey oyuncu sayısına göre ölçeklenir |
| Ağ | **P2P listen-server** (EOS), CoD tarzı **host migration** |
| Online | **Epic Online Services**: lobi, P2P relay, voice, giriş. Steam'e de çıkabilir. |
| Platform | PC (Windows) önce, sonra Android/iOS |
| Maç süresi | ~20–35 dk (oyuncu sayısına göre) |
| Yaş hedefi | Teen/PEGI 12. Stilize ölüm, kan yok, konfeti/tüy/balık efektleri var. |

## Oyun modları
- **Morrowmere (Klasik):** ana sosyal çıkarım modu.
- **Kill Godot (VIP):** kasabalılardan biri gizlice *Godot*'dur. Sabırsızlar onu bulup öldürürse kazanır.
- **Arcade:** Tomorrow's Arsenal (gun game), Parcel Rush (loot kapışması), Deathmatch ve lobi ısınma poligonu. Silahların ve skin'lerin sahnesi burası.
- **Özel lobi:** rol listesi editörü, süre ayarları, mod kuralları.

## Şimdilik kapsam dışı
Dedicated server, trade/market (backend gerektirir), UGC harita editörü, ev editörü (sadece altyapısı düşünülüyor, sonraki fazda gelecek).
