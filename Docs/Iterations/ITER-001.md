# ITER-001 — M1 Araç zinciri (2026-09-24)

**Hedef:** Derleyici kurulumu, projenin ilk derlenmesi, testler, geliştirme haritası, oyunun ilk açılışı.

| Adım | Sonuç |
|---|---|
| VS 2026 Community | ❌ EdgeUpdate grup ilkesi WebView2'yi engelliyor (VS ön kontrolü, hata 8008). İlke değiştirilmedi. |
| VS 2026 Build Tools 18.10 | ✅ `D:\VS2026\BuildTools`, MSVC 14.50.35739 (LTSC, UBT seçti) + 14.51, Win SDK 22621 |
| .NET Framework 4.8 SDK | ✅ SwarmInterface kural hatası için eklendi |
| G1 Derleme | ✅ 4 düzeltmeden sonra: UHT `GetNumPlayers` override, `IsAnyRigidBodyAwake` const, live-coding kilidi (`-NoHotReloadFromIDE`) |
| G2 Testler | ✅ 5/5 (Roles.Catalog, Roles.Generator, Core.RngDeterminism, Combat.BackstabGeometry, Voice.MouthCurve) |
| Greybox haritası | ✅ `/Game/KillGodot/Maps/L_Dev_Greybox` (Python commandlet, `Tools/Unreal/kg_build_dev_greybox.py`) |
| Runtime input | ✅ WASD / fare / Space / Shift (`AKGCharacter::CreateDefaultInput`) |
| İlk açılış | ✅ `-KGAutoShot` ile 1280×720 ekran görüntüsü: `Art/Concept/M1_FirstBoot.png` |
| Asset indirme | ✅ 20 kaynak, CC0, workflow (indir → bağımsız denetim) |

**Notlar / borçlar:**
- CommonUI, `CommonGameViewportClient` olmadan uyarı veriyor → M10'da viewport client ayarlanacak.
- Test logunda motor içi `MTAccessDetector` ensure'u (UnrealEd, commandlet) — bizim kodla ilgisiz, izleniyor.
- Unreal MCP eklentisi denemesi M2 başına ertelendi.
