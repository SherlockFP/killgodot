# M2: First-person karakter ve viewmodel promptu

```
Kill Godot M2'yi uygula: CS2 akıcılığında first-person karakter.
Referans: Docs/05_Tech_Architecture.md §6, Docs/01_GDD_Core.md §4, Docs/08_UI_UX.md §7.1.

- AKGCharacter: yürü 320 / koş 580 cm/s, stamina, çömel, zıpla. Native First Person Rendering (FOV 70-74,
  Scale 0.6). Gövde bOwnerNoSee + bCastHiddenShadow.
- UKGViewmodelComponent: preset 1/2/3 + offset/FOV/sol el ayarları, sway/bob/recoil yayları, TF2 tarzı
  "backstab ready" pozu. Ayarlar ekranı için canlı önizleme widget'ı.
- Etkileşim: IKGInteractable, kapı (kilit, kır), fizik tutuş/taşıma/fırlatma/döndürme (PhysicsHandle,
  bırakınca 150 ms blend). Ağ versiyonu M3'te.
- Yakın dövüş: vuruş, itme, blok, stamina. Tava aleti: çekme, saldırı ve inspect montajları (placeholder anim).
- Kukla kolları: Tools/Blender/kg_puppet_prototype.py'deki parçaları kullanarak SK_KG_FP_Arms'ı Blender'da
  export et, UE'ye Python ile import et.
- Otomasyon testleri: viewmodel yay kararlılığı (30 ve 300 fps'te aynı sonuç), backstab geometrisi.
Kapıları çalıştır, Backlog'u güncelle, özet ver.
```
