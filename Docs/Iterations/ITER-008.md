# ITER-008 — Kapılar, akşam ışığı ve oynanabilir maç döngüsü (2026-09-24)

**Yetki:** "gauntlet loop gibi geliştir, devam et dememi bekleme" → sprintler zincirleme, onay beklemeden.

## Sprint 1 — Kapılar + akşam
- ✅ KGDoor artık kit kapısı (`Door_1_Round`, kemere oturacak şekilde %15 geniş), her evin ön kapısında, içeri açılır, replike, kilitlenir/kırılır. Kapı mesh'ine kutu çarpışma (ince karmaşık yaprağı ışınlar kaçırıyordu).
- ✅ 204 çevre mesh'inde çarpışma açıldı (glTF import bölüm çarpışmasını kapalı bırakıyor; `kg_fix_env_collision.py`).
- ✅ Sokak lambaları (3 m ahşap direk + kit feneri), ev kapısı yanında fener, her katta sıcak iç ışık → 65 hareketli, gölgesiz ışık. Masa kapıyı tıkamıyor.
- Öğrenilen: `unreal.Color` konumsal sırası BGRA (güneş ve fenerler maviydi).

## Sprint 2 — Maç döngüsü
- ✅ Maç kendiliğinden başlar (3 sn), `kg.BotFill`=6'ya kadar bot (isimli köylüler: Balıkçı Rıza, Fırıncı Nuri...). Isınma 25 sn → rol kartı → Gün 1 → Gece → Şafak → Gün 2 → Toplantı...
- ✅ HUD: faz + süre + canlı sayısı, köşede rolün, rol kartı (renk: Köy yeşil / Sabırsız kırmızı / Nötr mor), "[E] Open" ipucu, ÖLDÜN ekranı, tur sonu kazanan + herkesin rolü, yeni tur geri sayımı.
- ✅ Bıçak (B) artık sadece Sabırsız rollerde, sunucu doğrular. Ölüm GameMode'a gider: hayalet olur, rolü açıklanır, kazanma kontrolü (Sabırsız 0 → Köy; Sabırsız ≥ diğerleri → Sabırsız). Tur sonu `ServerTravel ?Restart`.
- ✅ Faza göre ışık: gündüz sıcak, toplantı/tur sonu alacakaranlık, gece mavi ay ışığı, şafak pembe (6 sn yumuşak geçiş).
- ✅ Botlar dolaşır; Sabırsız bot arkası dönük birini görünce bıçak çekip saplar (beni gündüz 1'de öldürdü). Ölüm kamerası (kollar gizli, göz yerde).
- 🟡 Gece avı: katil bot avını kaybediyor (takılınca rastgele hedef) → kalıcı av hedefi sıradaki header döngüsünde.
- Dev: `kg.SkipPhase 1`, `kg.WarmupSeconds`, `kg.EpilogueSeconds`, `kg.BotFill`, `kg.AutoStart`.

**Kapılar:** G1 ✅ G2 8/8 ✅
**Sıradaki (Sprint 3):** Toplantı oylaması → yargılama → darağacı (Town of Salem çekirdeği), bot oyları, kalıcı gece avı, hayalet izleyici.
