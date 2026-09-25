# KILL GODOT — Ekonomi, Kozmetik, Kasalar, Almanak (Battle Pass)

> **Temel ilke:** Sadece kozmetik satılır, **pay-to-win yok**. Hukuki çerçeve için bkz. `Research/TechResearch.md §10`. Özet:
> - Ücretli rastgele eşya PEGI 16 alır ve Brezilya'da reşit olmayanlara yasaktır. Belçika'da kumar sayılır.
> - Bu yüzden oyun çıkışta **para ile rastgele kutu satmaz**. Kasa açma heyeti **tamamen oynayarak** yaşanır.

## 1. Para birimleri
| Birim | Nasıl kazanılır | Ne için | Not |
|---|---|---|---|
| **Bakır** | Maç içi görevler | Maç içi NPC dükkânları | Maç sonunda sıfırlanır |
| **Altın** | Maç oynayarak, günlük/haftalık görevler, Almanak | Kozmetik dükkânı (Altın fiyatlı rafı), Mektup Açacağı, ev eşyaları | **Asla satılmaz** |
| **Yarın Sikkesi** (premium) | Gerçek para | Doğrudan kozmetik satın alma, Almanak Premium | Fiyatlar gerçek para karşılığıyla da gösterilir |

## 2. Godot Kolileri (kasalar)
**Lore:** Denizden adresi "G." olan koliler vurur. Açılmamaları gerekir. Herkes açar.
- **Kazanma:** maç sonu düşüş şansı (haftalık limitli), Almanak kademeleri, başarımlar, etkinlikler. Maç içinde Posta İskelesi'nde bulunan fiziksel koli de bir kasa verir.
- **Açma:** **Mektup Açacağı** gerekir (anahtar). Bu da sadece oynayarak ya da Altın ile alınır.
- **Makara animasyonu:**
  - Sonuç **önceden** güvenilir serviste belirlenir, makara sadece gösterimdir.
  - ~60 slot bulunur, gerçek sonuç ~50. slotta durur. 5–7 sn cubic-out yavaşlama, küçük rastgele taşma, her slot geçişinde "tık" sesi.
  - Sonunda nadirlik rengine göre ışık patlaması ve konfeti olur.
  - Hızlı geç butonu var. **Olasılık tablosu her kolinin ekranında görünür.**
- **Merhamet sayacı (pity):** 40 kolide garanti bir "Efsane" çıkar.
- **İleride ücretli anahtar** gündeme gelirse **X-Ray modeli** uygulanır (içeriği satın almadan önce gör) ve ülke bazlı kapatılır.

### Nadirlik kademeleri
| Kademe | Renk | Oran (örnek) |
|---|---|---|
| Sıradan | Gri | %55 |
| Kasaba | Mavi | %25 |
| Nadir | Mor | %12 |
| Efsane | Pembe | %6 |
| Mitik | Kırmızı | %1.8 |
| **Godot'nun** | Altın ✦ | %0.2 (özel yakın dövüş skinleri, "bıçak" muadili) |

## 3. Skin sistemi (CS tarzı)
- **Aşınma (Tuz Aşınması):** 0.00–1.00 arası.

  | Durum | Aralık |
  |---|---|
  | Fabrika Yeni | 0.00–0.07 |
  | Az Kullanılmış | 0.07–0.15 |
  | Denize Dayanıklı | 0.15–0.38 |
  | Yıpranmış | 0.38–0.45 |
  | Batıktan Çıkma | 0.45–1.00 |

- **Desen tohumu:** 0–999, desenin kayması ve dönüşünü belirler. Bazı tohumlar koleksiyoncu favorisi olur.
- **Çetele (StatTrak muadili):** eşyanın üstünde bir sayaç sayar. Neyi saydığı eşyaya göre değişir: öldürme, asılma sayısı, tutulan balık, kesilen ağaç…
- **Mühürler (sticker):** silah ve alete en fazla 4 balmumu mühür.
- **Teknik:** paint kit DataAsset, Custom Primitive Data (aşınma, tohum, çetele) ve UV-uzayı mühür atlası. Her silah için ayrı dinamik materyal gerekmez.

### Skin kategorileri (başlangıç hedefi: 300+ kozmetik)
| Kategori | Hedef adet (S1) | Örnekler |
|---|---|---|
| Şapkalar | 40 | üç köşeli, melon, korsan, aşçı, cadı, taç, balık kafası şapka |
| Saç/Sakal/Bıyık | 30 | |
| Üst giyim | 30 | redingot, balıkçı kazağı, rahip cüppesi |
| **Pelerinler** | 20 | gece pelerini, fırtına pelerini, yaldızlı pelerin |
| **Eldivenler** | 20 | viewmodel'de de görünür |
| Alt giyim | 20 | |
| **Ayakkabılar** | 25 | her birinin kendi taban izi var |
| Aksesuar ve sırt | 25 | |
| Gövde malzemesi (kukla) | 12 | |
| **Yakın dövüş skinleri** | 25 | şemsiye kılıç, çekiç, tava, kılıçbalığı, baget, yelkovan bıçağı, tırpan, kauçuk tavuk… |
| Silah skinleri (arcade) | 40 | El Topu .50, çakmaklı, harpon, tüfek… |
| Araf kılıçları | 10 | |
| Emote ve dans | 25 | |
| Ölüm efektleri | 12 | |
| Hayalet izi, hortlak kefeni | 10 | |
| Köpek, kayık, fener skinleri | 15 | |

## 4. Almanak (battle pass)
- Sezonluk (~10 hafta), **Ücretsiz + Premium** yol, 60 kademe.
- **Kazanılan kademe hiç kaybolmaz.** Kaçırılan günler için ceza yoktur (PEGI ve AB yönü).
- Premium Yarın Sikkesi ile alınır, ya da her sezon oynayarak biriktirilen Altın ile.
- İçerik: kozmetikler, Godot Kolileri, Mektup Açacakları, **lore sayfaları**, unvanlar, yeni Yüz.
- Sezon görevleri: *"Tavada bir Sabırsızı devir"*, *"Soytarı olarak asıl"*, *"Taş sektirmede 8 sekiş yap"*…

## 5. Rütbe ve ilerleme
- **Seviye (XP):** sınırsız. Her seviye Altın verir.
- **Sabır Rütbesi (ranked):** Yolcu → Bekleyen → Sabırlı → Nöbetçi → Fenerci → Kâhin → Yarın → **Godot** (ilk 500). Gizli MMR, **taraf ağırlıklı Elo** ile hesaplanır: tarafın kazanma oranı ve rakip ortalaması hesaba katılır.
- **İstatistikler:** rol başına kazanma, doğru oy oranı, "yakalandığı maske anı", toplam aura.
- **Başarımlar:** 150+. Easter egg başarımları gizli.

## 6. Ev ve kişiselleştirme (sonraki faz, altyapısı şimdi)
- Her oyuncunun maçtaki **evi kendi evidir**: dekorasyon teması, portreler, akvaryum, kupalar.
- Ev editörü ileride gelecek. Mobilyalar Altın veya kasalardan çıkar, grid tabanlı yerleştirilir.
- **Optimizasyon:** maç içinde ev dekoru önceden tanımlı slotlara sınırlı sayıda eşya olarak yüklenir, oyuncu başına ≤ 25 dekor eşyası. Ev içi değişiklikler oynanışı etkilemez: saklanma yerleri ve kapılar sabittir.

## 7. Mağaza
- **Günlük Vitrin** (6 eşya) ve **Haftalık Paketler**. Paket fiyatı, tek tek fiyatların toplamıyla birlikte gösterilir.
- Terzi NPC'si **maç içinde** vitrin olur: oyuncu dükkâna girince 3D olarak dener ve ana menüden satın alır.
- **Ebeveyn kilidi:** harcamalar kapalı başlayabilir (PEGI 7 seçeneği).

## 8. Sahiplik ve hile
- Envanter backend'de ya da Steam Inventory'de tutulur. İstemcide saklanan hiçbir şey güvenilir kabul edilmez.
- Maç ödülleri çoğunluk raporuyla doğrulanır, saatlik XP sınırı vardır (bkz. `05_Tech_Architecture.md §10`).
- Takas ve pazar çıkışta kapalıdır. Belki ileride açılır, ama hukuki inceleme gerekir.
