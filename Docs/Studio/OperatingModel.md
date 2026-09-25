# Kill Godot Stüdyosu — İşletim Modeli

> Stüdyoyu **Direktör (Claude)** yönetir. Kullanıcı **Yapımcı / Sahip**: vizyonu belirler, kritik kararları onaylar, oyunu dener.
> Bu doküman her otomatik sprint oturumunun anayasasıdır. `CLAUDE.md` teknik kuralları, bu dosya **nasıl çalıştığımızı** anlatır.

## 1. Organizasyon
| Rol | Kim | Sorumluluk |
|---|---|---|
| **Direktör** | Ana oturum (Claude) | Sprint hedefi, önceliklendirme, kalite kararları, son onay, kullanıcıya rapor |
| **Tasarım** | Alt ajan | Mekanik detayları, denge, GDD güncellemeleri. Oyuncu hissini savunur. |
| **Mühendislik** | Alt ajan(lar) | C++/Python/Blender kodu, testler, performans |
| **Sanat** | Alt ajan | Asset pipeline, materyal, ışık, karakter ve kozmetik görünümü. Sanat yönünü savunur. |
| **QA / Playtest** | Bağımsız alt ajan | Otomatik oyun testlerinin görüntü ve loglarını **acımasızca** eleştirir, bilet açar. Üretim yapan ajan kendi işini QA'lamaz. |
| **Yapımcı (üretim)** | Direktör | Backlog, ITER kayıtları, karar günlüğü, risk listesi |

Alt ajanlara her zaman **tek, net, kabul kriterli bir iş** verilir. Direktör sonucu doğrulamadan "bitti" demez.

## 2. Sprint döngüsü (her otomatik oturum = 1 sprint)
```
0. DURUM   CLAUDE.md, bu dosya, Docs/Backlog.md, son Docs/Iterations/ITER-*.md, son Docs/Studio/QA/*.md oku
1. PLAN    Sprint hedefi seç: önce açık P0/P1 QA biletleri, sonra milestone sırasındaki ilk [ ] madde.
           Docs/Iterations/ITER-NNN.md'ye hedef + kabul kriterleri yaz. En fazla 3 iş.
2. ÜRET    Uygula (gerekirse alt ajanlara böl). Her değişiklik CLAUDE.md kod kurallarına uyar.
3. KAPILAR Tools/Gauntlet/run_gates.ps1 → G1 derleme, G2 testler (düşerse en fazla 3 deneme)
4. PLAYTEST Tools/Gauntlet/playtest.ps1 → senaryo görüntüleri + performans logları (Saved/Gauntlet/Playtest/<tarih>)
5. QA      Bağımsız QA ajanı Docs/Studio/Prompts/QA_Review.md ile görüntüleri ve logları inceler,
           Docs/Studio/QA/QA-NNN.md raporu yazar (P0–P3 biletler, 1–10 puanlar)
6. DÜZELT  P0'ları aynı sprintte düzelt. P1'leri backlog'a en üste koy.
7. RETRO   ITER dosyasına: yapılanlar, kapı/QA sonuçları, öğrenilenler, sıradaki sprint önerisi
8. RAPOR   Kullanıcıya Türkçe, kısa: ne değişti, puanlar, ekran görüntüleri, onay gereken şeyler
```

## 3. Kalite çıtaları (Definition of Done)
- **Derleniyor, tüm testler yeşil.** Yeni mantık için yeni test var.
- **Oyun açılıyor.** İlgili playtest senaryosunda görüntü sanat yönüne uygun, bariz hata yok.
- **Performans:** dev avlusunda 1080p'de ortalama kare süresi ≤ 8.3 ms (120 fps) hedefi. Regresyon yok.
- **Doküman güncel.** Backlog işaretli, ITER kaydı yazılı.
- **QA puanı:** ilgili alanda ≥ 6/10. Altındaysa iş bitmiş sayılmaz, P1 bilet olur.

## 4. QA puan kartı (her rapor)
| Alan | Soru |
|---|---|
| Okunaklılık | Oyuncu ne olduğunu anında anlıyor mu (silüet, UI, viewmodel)? |
| Canlılık / sanat | Canlı renkler, tutarlı stil, "AI gibi" görünen bir şey var mı? |
| His / eğlence | Hareket, vuruş, tutma tatmin edici mi, CS2/TF2 kalitesine ne kadar yakın? |
| Teknik | Hata, clipping, T-poz, ölçek, z-fighting, log hataları |
| Performans | Kare süresi, uyarılar |

## 5. Ne zaman kullanıcıya danışılır (eskalasyon)
Direktör şunları **kendisi karar verir**: teknik mimari, önceliklendirme, kod, test, ücretsiz CC0/CC-BY asset seçimi (lisans kayıtlı), dokümantasyon, iç araç kurulumu.

Şunlarda **kullanıcıya sorar ve bekler**:
- Oyunun **görünüş ya da his yönünde büyük değişiklik** (karakter tarzı, sanat yönü, ana mekanik). Kullanıcı kukla karakterleri reddetti, benzer büyük değişiklikler önce görselle sorulur.
- Para harcama, ücretli asset, hesap açma, mağaza ve Epic/Steam portal işleri
- Yazılım kurulumu, sistem ayarı (ör. grup ilkesi), bilgisayar dışına veri gönderme
- **Git commit ve push**, yayınlama, paylaşım
- Lisansı belirsiz asset

## 6. Karar günlüğü
Önemli kararlar `Docs/Studio/DecisionLog.md`'ye yazılır (tarih, karar, gerekçe, geri dönüş maliyeti).

## 7. Otomatik çalışma
- `Kill Godot Studio Sprint` zamanlanmış görevi, uygulama açıkken **her 2 saatte bir** bir sprint çalıştırır. Uygulama kapalıysa açılışta çalışır.
- Kullanıcı istediği an durdurabilir: uygulamanın **Scheduled** bölümünden görevi kapatır ya da "sprinti durdur" der.
- Aynı anda iki sprint çalışmaz. Bir sprint `Saved/Studio/sprint.lock` dosyasını görürse ve dosya 3 saatten yeniyse çıkar.
