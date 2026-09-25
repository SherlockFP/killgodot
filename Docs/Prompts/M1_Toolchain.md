# M1: Araç zinciri promptu

```
Kill Godot M1 (araç zinciri) milestone'unu tamamla. Önkoşul: Visual Studio 2026 (18.x, MSVC >= 14.50.35723)
veya VS 2022 17.14 (MSVC >= 14.44.35211) + "Game development with C++" + Windows SDK 10.0.22621 kurulu.

1. vswhere ile VS'yi doğrula. Yoksa dur ve bana kurulum adımlarını Türkçe listele.
2. "D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" KillGodotEditor Win64 Development
   -Project="D:\Kill Godot\KillGodot.uproject" -WaitMutex ile derle. Hataları düzelt (G1).
3. Tools/Gauntlet/run_gates.ps1 ile KillGodot.* otomasyon testlerini koştur (G2). Rol üretici, RNG, backstab
   geometrisi ve ağız eğrisi testleri yeşil olmalı. Rol üreticisinin denge bandı testi düşerse bandı
   Docs/02_Roles.md ile tutarlı biçimde ayarla ve nedenini ITER dosyasına yaz.
4. Content/KillGodot/Maps/L_Dev_Greybox haritasını Python commandlet ile oluştur: zemin, 4 kutu ev,
   PlayerStart, DirectionalLight, SkyAtmosphere, ExponentialHeightFog. GameDefaultMap olarak ayarla.
5. Enhanced Input asset'lerini (IMC_KG_Default, IA_Move/Look/Jump/Sprint/Interact/Attack/Inspect) oluştur,
   BP_KG_Character'da ata.
6. Unreal MCP eklentisini (Experimental/ModelContextProtocol) editörde etkinleştirmeyi dene ve
   CLAUDE.md'ye bağlantı adımlarını yaz.
7. Backlog'u güncelle, bana özet ver. Commit için izin iste.
```
