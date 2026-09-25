# KillGo — Tasarım Sütunları (Design Charter) v1.1

> Durum: **Taslak, kullanıcı onayı bekliyor** (2026-09-25).
> Kaynak: kullanıcının 2026-09-25 tarihli tasarım prompt'u. Baş tasarımcı revizyonuyla eksik 1. ve 3. bölümler
> tamamlandı, koruyucu kurallar eklendi ve her sütuna sayısal hedef kondu.
> **v1.1:** ölçülebilirlik incelemesi uygulandı. Tek sabitler tablosu (§0.3), örneklem ve kapı kuralı (§0.2),
> tasarım metriği / bot uyumu ayrımı, çelişen bantlar düzeltildi. Yeni özellik eklenmedi. Ayrıntı: belge sonundaki
> v1.1 günlüğü.
> Kardeş belgeler: `Pillar_Audit.md` (bugünkü oyunun bu belgeye göre puanı), `Pillar_Roadmap.md` (sıralı sprint
> önerileri).

## 0. Bu belge nedir, nasıl kullanılır

Bu belge oyunun **puanlama cetveli** ve **karar filtresi**dir. Kendisi bir sprint hedefi değildir.
`Docs/Process/LoopContract.md` her sprintin tek bir Backlog maddesi ve 2–6 ölçülebilir kabul kriteriyle
başlamasını ister. Bu belge o kriterlerin **nereden geleceğini** söyler: her sprint, aşağıdaki hedef kodlarından
(ör. `S9.3`) en az birini hareket ettirdiğini sayıyla göstermelidir.

Kullanıcının orijinal metni hâlâ bu belgenin kalbidir. Değişiklikler üç türdür:
1. **Tamamlama:** numarasız ilk blok **1. KARARLAR** oldu, eksik 3. bölüm **3. İZ VE DELİL** olarak yazıldı.
2. **Ölçüm:** "daha fazla X" türündeki her hedef bir sayı bandına çevrildi. Sınırsız "optimize et" hedefi,
   kullanıcının daha önce reddettiği sınırsız döngü sorununun ta kendisidir.
3. **Koruyucu kurallar (G1–G7):** ses, ölçek, okunabilirlik, kontrollü varyans, anti-snowball, bot adaleti ve
   anti-cheat. Bunlar "daha fazlası" istenen şeyler değil, hiçbir sütunun delmemesi gereken sınırlardır.

### 0.1 Kapsam kuralı (en önemli satır)
> **Yeni rol veya yeni sistem icat edilmez.** 51 rollük tasarım, Gece Defteri, maske, vasiyet, Hazırlık barı ve
> deliller zaten kâğıt üzerinde var. Bu charter, var olanları **bu mercekten geçirerek hayata geçirmek** içindir.
> Önce ortak veri (Olay Defteri) ve M5 çekirdek sekizi. Yeni fikirler ve mevcut tasarımın yeniden tasarımları
> Backlog "Proposed (needs approval)" bölümüne gider; bu belgede hedef hücresine yazılmaz.

### 0.2 Ölçüm kaynakları ve kapı kuralları
| Kod | Kaynak | Ne zaman geçerli |
|---|---|---|
| **[T]** | Otomasyon testi (`KillGodot.*`) | Her zaman |
| **[U]** | Bot **uyum** kontrolü: bir bot davranış yolunun çalıştığını ve logladığını doğrular (ör. "rapor yolu tetiklenir, `KG_CASE` yazılır"). **Yüzde bandı yoktur.** | Her zaman. Aynı sprintin yazdığı bot davranışının ürettiği sayı her zaman [U]'dur, asla [B] değil. |
| **[L]** | Veri lint'i: rol kataloğu, layout JSON, üretici simülasyonu (10.000 tohum) | Her zaman. Her [L] etiketi bir [B] varlık kontrolüyle eşlenir (kural 3). |
| **[B]** | Bot soak: `kg_pillar_soak.ps1`, başsız, **x1 ya da global zaman genişlemesi** (hareket de ölçeklenir). Süreler **oyun saniyesi**dir (`FKGMatchClock`). | Botlar sadece kendi algılarıyla karar verdiğinde (G6) **ve** kural 2 sağlandığında. Ondan önce ölçülen şey tasarım değil, bottur. |
| **[O]** | İnsan oyun testi telemetrisi + kısa anket | M5 arkadaş testinden sonra. Planlanan hacim: **~150 maç**. |
| **[R]** | Render ölçümü: referans makine, ekran dışı render turu (bkz. S3.5) | Pencere açmadan yapılabildiği zaman |

**Kural 1 — örneklem ve kapı.** Her [B]/[O] bandının bir **n_min**'i vardır (birimi olay: cinayet, toplantı,
oyuncu-faz; maç değil). Soak her metrik için n'yi ve %95 aralığını (Wilson) yazar.
- **Geçer:** n ≥ n_min ve %95 aralığı tamamen bandın içinde (tek yönlü hedefte: aralığın ilgili sınırı eşiği sağlıyor).
- **Kalır:** %95 aralığı tamamen bandın dışında.
- **Sonuçsuz:** diğer her durum, ya da n < n_min. Sonuçsuz bir metrik I5'i **kırmaz**, raporda öyle yazılır.
- Bant hedeflerinde n_min = ⌈1.96² · p(1−p) / h²⌉ (p = bant ortası, h = bant yarı genişliği): geçmenin mümkün olduğu
  en küçük örneklem. Tek yönlü hedeflerde varsayılan n_min = 40.
- §0.2'deki eski "her N'de ≥ 5 tohum" kapsama tabanıdır, anlamlılık değil.

**Kural 2 — dondurulmuş bot parametresi.** Bir [B] tasarım metriği ancak onu doğrudan süren bot parametreleri
(refleks süreleri, olasılıklar, ağırlıklar) koşudan önce dondurulmuş ve soak başlığına yazılmışsa kapı olabilir.

**Kural 3 — [L] ↔ [B] varlık eşlemesi.** Etiketli her bölge sebebi, `WorldFootprint` ve rol "temel kullanımı",
bulunduğu soak'ların ≥ %50'sinde o türden ≥ 1 defter olayı üretir. Üretmiyorsa etiket "beyan edildi, uygulanmadı"
sayılır. [L] hedefleri bu yüzden iki sayı yazar: **beyan** / **uygulanan**.

**Kural 4 — kabul soak'u ve gece soak'u.** Sprint kapısı olan soak küçük ve sabittir: N = 8 ve 20, 2 sabit tohum,
gün 1–3, toplam ≤ ~45 dk. Büyük tohum × N matrisleri (100–300 maç) **gece koşusudur, sadece rapor**, hiçbir sprinti
bekletmez. Saf doğruluk sayımları (kazanma çözücüsü sonuçları, birlikte kazanan grupları) G6.5 geldikten sonra x30'da
ya da mantık-seviyesi simülasyonla ölçülebilir. `DevClockScale` tek başına sadece faz saatini hızlandırır, botlar gerçek
hızda yürür: süreye bağlı her metrik bu modda **geçersizdir**.

"Bugün" sütunu, `Pillar_Audit.md`'deki mevcut ölçümdür. `?` = henüz ölçülemiyor. "anekdot" = tek koşu, geçerli [B] değil.

### 0.3 Sabitler (tek kaynak)
Aşağıdaki değerler charter'ın önerisidir. Kodda ve yol haritası kabul satırlarında **isimle** anılır, sayı tekrar
yazılmaz. Değişirse sadece burada değişir.

| Sabit | Değer | Anlamı |
|---|---|---|
| `KG_SIGHT_DAY` | 30 m | Gündüz görüş menzili |
| `KG_SIGHT_NIGHT_LIT` | 12 m | Gece, hedef bir ışık kaynağının aydınlattığı alandayken |
| `KG_SIGHT_UNLIT` | 5 m | Gece, ışıksız alanda |
| `KG_SIGHT_CONE` | 120° (±60°) | Görüş konisi; görüş hattı (LOS) şart |
| `KG_IDENT_RANGE` | 8 m | **Tanımlama = isim etiketi menzili (tek sınır).** LOS + maskesiz + görüş menzili içinde. Ötesinde isim yok. |
| `KG_HEAR_KILL` | 15 m | Backstab / cinayet sesi |
| `KG_HEAR_SCREAM` | 30 m | Backstab dışı yaralanma çığlığı |
| `KG_HEAR_DOOR` | 25 m | Kapı kırma |
| `KG_HEAR_DIG` | 40 m | Mezar kazısı (bugünkü kod değeri, `Dig/KGDigTypes.h`) |
| `KG_HEAR_FISH` | 18 m | Balık atışı/çekişi (bugünkü değer) |
| `KG_HEAR_WASH` | 10 m | Kuyuda/çeşmede yıkanma |
| `KG_WALL_FACTOR` | 0.5 | Duvar arkasında duyma yarıçapı çarpanı |
| `KG_ENCOUNTER` | ≤ 15 m, LOS, ≥ 3 sn | Karşılaşma |
| `KG_GATHER_WINDOW` | 35 sn | Acil toplantı toplanma penceresi (= S4.5 en fazla). Pencere sonunda meydan halkasında olmayan "Geç Kalan"dır. |
| `KG_DEAD_WINDOW` | 15 sn | Ölü zaman penceresi |
| `KG_REGION_DEBOUNCE` | 20 sn | "Bölgeye git" fiilinin tekrar sayılması için en kısa süre |
| `KG_PHASE_DAY_S` | 282 + 12N sn / gün | Şafak 12 + Gündüz (150 + 6N) + Toplantı (45 + 3N) + Gece (75 + 3N), `KGGameMode.cpp:62-86`. 1. gün toplantısızdır; her yargılama +43 sn. Vakasız toplantı ≤ 30 sn (S9.7) olunca 267 + 9N. |

### 0.4 Sözlük (hedeflerin dayandığı tanımlar)
| Terim | Tanım |
|---|---|
| **Olay** (Event) | Sunucunun Olay Defteri'ne (Event Ledger) yazdığı dünya gerçeği: kim, ne, nerede (AKGMapInfo bölgesi), ne zaman, kim gördü/duydu. Kimliği `EventId`. |
| **Dünya Olayı** | Şafakta önceden bildirilen, tohumlu köy olayı (Sis, Fırtına…). "Olay"dan ayrı sayılır. |
| **Tanık** | Olay anında algı filtresini geçen canlı oyuncu. **Gördü:** `KG_SIGHT_*` menzili, `KG_SIGHT_CONE`, LOS. **Duydu:** olayın `KG_HEAR_*` yarıçapı (duvar arkasında × `KG_WALL_FACTOR`). **Tanımlı tanık:** gördü + `KG_IDENT_RANGE` içinde + fail maskesiz. |
| **Karar fiili** | Defterde loglanan fiiller: bölgeye git (sadece bölge değişiminde, `KG_REGION_DEBOUNCE`), görev başlat/bırak, birini takip et, kefil ol, cesedi incele, cesedi raporla, yetenek kullan, bıçak çek, maske tak, saklan, kapı kilitle / kır / barikat, suçla, oy ver, iddia paylaş, vasiyet yaz. Düz hareket karar fiili değildir. |
| **Anlamlı karar** | Farklı risk veya bilgi sonucu olan en az 2 seçenek arasından seçim ve bu seçimin Olay Defteri'ne düşmesi. |
| **Ölü zaman** | Canlı bir oyuncu için son `KG_DEAD_WINDOW` içinde **hem** algılanan yeni olay **hem de** karar fiili olmayan her saniye. |
| **Yalnız** | `KG_IDENT_RANGE` içinde LOS'ta başka canlı oyuncu yok. |
| **Sert kanıt** | Tek başına bir oyuncunun tarafını kesinleştiren bilgi. Bugünkü kaynakları: tanımlı tanıklı cinayet ve fener aydınlanması. Rol sonuçları S2.6 gereği **asla** sert kanıt değildir. **Yumuşak ipucu** = yorum isteyen, sahtelenebilen, süresi dolan bilgi. |
| **Bilgi parçası** | Farklı bir `EventId`. |
| **Vaka** | Raporlanan bir ceset: Vaka Dosyası (kurban, bulunduğu bölge, raporlayan, ölüm zamanı aralığı, yara sınıfı). |
| **İddia** | Yapılandırılmış toplantı satırı (GÖRDÜM / ORADAYDIM / GÖREVİ YAPTIM / ROL_SONUCU), bir `EventId`'ye ya da oyuncunun beyanına bağlı. |
| **Çelişki** | Beyan ettiği gerçek, ±20 sn içindeki bir defter olayı ya da Kasaba Panosu olayıyla çatışan iddia. |
| **Role özgü bilgi** | Bir yetenek olayına bağlı `ROLE_RESULT` türünde iddia. |
| **Karşılaşma** | `KG_ENCOUNTER`. |

---

## ANA İLKE (kullanıcının metni, korunarak)

Oyunu şunlar için optimize et:

- DAHA ÇOK ANLAMLI KARAR
- DAHA ÇOK İŞE YARAR BİLGİ
- DAHA ÇOK OYUNCU ETKİLEŞİMİ
- DAHA ÇOK SONUÇ
- DAHA AZ ÖLÜ ZAMAN
- DAHA AZ **SONUCU BELİRLEYEN** RASTGELELİK *(değişti, bkz. G4)*
- DAHA AZ KOPUK SİSTEM

Bu yedi satır bir **yön**dür, bir hedef değildir. Her birinin sınırı olan sayısal bandı aşağıdaki sütunlardadır.
Ayrıca "daha çok bilgi" sınırsız istenmez: sert kanıtın fazlası çıkarımı öldürür (bkz. S2 bilgi bütçesi).

### Döngü (kullanıcının zinciri, geri besleme eklendi)
Kullanıcının zinciri: OYUNCU KARARI → BİLGİ → SONUÇ → SOSYAL GERİLİM → TEKRAR OYNANABİLİRLİK.
Eksik olan, sonucun **yeni bir karar doğurması**ydı. Charter'ın döngüsü:

```
 KARAR ──► İZ (Olay Defteri'ne düşer) ──► BİLGİ (kim gördü, ne kaldı)
   ▲                                              │
   │                                              ▼
 YENİ KARAR ◄── SONUÇ (asma, ölüm, ifşa) ◄── ŞÜPHE ──► TOPLANTI (iddia, çelişki, oy)
```

**Bağlı sistemler kuralı:** "Daha az kopuk sistem" bir slogan olarak kalmaz. Her mekanik **tek bir ortak kayda**
(Olay Defteri) yazar ve oradan okur. Defter, toplantı, roller, botlar ve epilog aynı kaydı okur. Bir mekanik bu
kayda hiçbir şey yazmıyorsa, başka hiçbir sistem onu göremez, dolayısıyla kopuktur.

---

## 1. KARARLAR *(kullanıcının numarasız ilk bloğu)*

Oyuncunun sürekli anlamlı seçimleri olmalı:
- nereye gideceği
- kime güveneceği
- kimi takip edeceği
- araştırıp araştırmayacağı
- saklanıp saklanmayacağı
- raporlayıp raporlamayacağı
- rol yeteneğini kullanıp kullanmayacağı
- risk alıp almayacağı

**Düzeltme:** "sürekli" (constantly) 10. sütunla çelişiyordu: erken oyunun sakin keşfe ihtiyacı var. "Sürekli"
yerine ölçülebilir bir **tavan** konur: hiçbir canlı oyuncu uzun süre kararsız kalmaz.
**Kural:** Her karar en az iki seçenek sunar ve seçenekler farklı bir risk veya bilgi sonucu taşır. Seçeneklerden
biri açıkça üstünse bu bir karar değil, bir angaryadır.

**Karar türü ↔ fiil eşlemesi** (S1.4 bununla sayılır):

| Tür | Sayılan fiiller |
|---|---|
| Nereye | bölgeye git |
| Güven | kefil ol, suçla, oy ver |
| Takip | birini takip et |
| Araştır | cesedi incele, iddia paylaş |
| Saklan | saklan, maske tak |
| Raporla | cesedi algıladıktan sonra 60 sn içinde raporla **ya da** raporlamadan uzaklaş (algı loglandığı için ikisi de kayıtlıdır) |
| Yetenek | yetenek kullan |
| Risk | bıçak çek, kapı kır, gece risk katmanı "tehlikeli" bir bölgeye gir (S4.3 katman alanı) |

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S1.1 | En uzun ölü zaman aralığı, canlı oyuncu başına, p90 | Gündüz ≤ 45 sn, Gece ≤ 60 sn | [O]; [B] sadece bilgi | 40 oyuncu-faz | ? (Kasaba için gece tamamen boş: 93–135 sn) |
| S1.2 | Ölü zamanın faz içindeki payı | Gündüz ≤ %20, Gece ≤ %35 | [O]; [B] sadece bilgi | 40 oyuncu-faz | ? |
| S1.3 | Dakika başına karar fiili (DPM), canlı oyuncu medyanı (düz hareket hariç) | Gündüz ≥ 2, Gece ≥ 1, Toplantı ≥ 1 | [O]; [B] sadece bilgi | 40 oyuncu-faz | ? |
| S1.4 | Karar çeşitliliği: ≥ 2 gün hayatta kalan oyuncunun kullandığı karar türü (eşleme tablosundan) | Medyan ≥ 4 | [B] [O] | 30 oyuncu | 3 tür (nereye: git; güven: suçla/oy; risk: bıçak çek); araştır, saklan, raporla, yetenek yok |
| S1.5 | Gündüzün herhangi bir anında Kasaba oyuncusunun ≥ 2 farklı bölgede açık hedefi olması | Zamanın ≥ %90'ı | [B] | 10 gündüz | ? |

---

## 2. BİLGİ OYNANIŞI

Görevler, deliller, roller, karşılaşmalar, hareket ve toplantılar birbirini beslemeli.
Oyuncular **ve botlar** fikirlerini şunlardan kurmalı:
- tanık olunan davranış
- konumlar
- zamanlama
- deliller
- alibiler
- çelişkiler
- geçmiş eylemler

**Eklenen kurallar:**
1. **Dünya önce gelir.** Bir oyuncunun keşfedebileceği bilgi asla otomatik duyurulmaz. Ceset bulunur, raporlanır,
   çan çalınır. Rol, keşiften önce açıklanmaz.
2. **Bilgi bütçesi.** Sert kanıt nadirdir. İpuçlarının çoğu yumuşak, sahtelenebilir ve süresi dolan türdendir.
3. **Rol bilgisi dünyaya bağlıdır.** Rol sonuçları (Şerif, Dedektif, Kâhin…) kısmidir, bir dünya olayına veya
   nesnesine dayanır ve sahtelenebilir (İftiracı, Sahtekar). Delille birleşir, onun yerine geçmez.
4. **Kimlik tanınabilir olmalı.** "Tanık olunan davranış" kimi gördüğünü bilmeyi gerektirir: bakınca beliren isim
   etiketi (`KG_IDENT_RANGE`, LOS; maske ve kılık gizler) ve ayırt edilebilir köylü görünümleri.

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S2.1 | Cinayetlerde ≥ 1 canlı, katil olmayan tanık (gördü **veya** duydu) oranı | %50–80 (alt sınır: iz bırakır; üst sınır: katil yine de yapabilir) | [B] | 40 cinayet | 0 (algı modeli yok) |
| S2.2 | Cinayetlerde **tanımlı** tanık oranı (insan ve bot katiller; botlar için de tek bant budur, bkz. G6.4) | %10–20 | [B] [O] | 200 cinayet (gece soak'u havuzu) | 0 |
| S2.3 | Vakalı toplantıda, canlıların defterlerinin birleşiminde vakayla ilgili (aynı bölge veya ±90 sn) bilgi parçası (farklı `EventId`) | Medyan ≥ 3 | [B] | 20 vakalı toplantı | 0 (defter yok) |
| S2.4 | Sert kanıt bütçesi: sert kanıt olayı / cinayet (N'den bağımsız); cinayet dışı sert kanıt kaynağı | ≤ 0.20 (= S2.2 üst sınırı); cinayet dışı kaynak 0 (fener G4.2 ile düzelince) | [B] | 200 cinayet | Fener aydınlanması: bardan bağımsız rastgele 1 sert kanıt |
| S2.5 | Oyuncunun keşfedebileceği ama otomatik duyurulan bilgi sayısı | 0 | [T] | — | Gündüz ölümü anında adı + rolüyle köy çapında duyurulur |
| S2.6 | Bilgi veren rol sonuçlarının bir defter olayına/dünya nesnesine dayanma oranı | %100 (beyan / uygulanan) | [L] + [B] varlık | — | Tasarımda Şerif, Dedektif, Kâhin, Muhbir, Baştan Çıkarıcı menüden kesin sonuç. **Bu 5 rolün yeniden tasarımı onay bekliyor** (Roadmap, Onay listesi R1); onaylanmazsa S2.6 bu 5 rol için muaf yazılır. |
| S2.7 | Kasaba botlarının toplantıdaki birinci şüphelisinin kötü taraf olma oranı; taban = canlı kötü / (canlı − 1), toplantı anında | ≥ taban + max(taban, 20 puan), en fazla %95; gün 2–4 havuzlanır | [B] | 100 bot×toplantı | Rastgele (hash) |

---

## 3. İZ VE DELİL *(eksik bölüm, yeniden yazıldı)*

Prompt hangi delilin var olduğunu, ne kadar sürdüğünü, kimin görebildiğini ve nasıl sahtelenebildiğini hiç
söylemiyordu. 6. ve 9. sütunlar bunsuz ayakta durmaz. Bu bölüm GDD §4 ve §6'daki tasarımı kural hâline getirir.

**Delil yaşam döngüsü:** her delil türünün tablosu şunları tanımlar: **kaynak** (hangi fiil bırakır),
**görünürlük** (herkes / yakındaki / belirli rol), **ömür**, **karşı hamle** (kötü taraf nasıl yok eder veya
önler), **sahte yolu** (başkasına nasıl yıkılır). Ömürler **her izin kendi oluşma anından** sayılır.

| Delil | Kaynak | Ömür | Karşı hamle | Sahte yolu |
|---|---|---|---|---|
| Ceset | Cinayet | Raporlanana veya akşam tellalına kadar | Sürükle, sakla (kuyu, deniz, mahzen) → "Kayıp" | Cesedi başka bölgeye taşı |
| Boya/talaş lekesi | Bıçak/yakın dövüş cinayeti, yaralanma | 45–60 sn | Kuyuda/çeşmede yıkan (7 sn, görünür, `KG_HEAR_WASH`) | — |
| Ayak izi | Yumuşak zemin (layout JSON'daki zemin alanı: çamur, kum, bahçe toprağı, kar) | 60–120 sn | Taş sokaktan yürü | Başkasının yönüne yürüyerek sahte iz |
| Sürükleme izi | Ceset taşıma | 60 sn | Taşı, sürükleme | — |
| Yara sınıfı | Ceset (bıçak / künt / boğulma) | Kalıcı | Silah seçimi | Tava ile öldür, bıçakçıyı aklat |
| Görsel görev etkisi | Gerçek görev (çan, duman, fener…) | Anlık + Kasaba Panosu'nda o gün | Sahte görev etki vermez (zaten kodda) | Başkasının çaldığı çanı sahiplen |
| Açık mezar | Kazı | Şafağa kadar | Geceyi bekle | — |
| Kırık kapı, yanan/sönen lamba | Kapı kırma, sabotaj | Onarılana kadar | Kilidi aç (Çilingir) | — |
| Vasiyet | Oyuncu yazar | Kalıcı | Sahtekar değiştirir | Sahtekar |

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S3.1 | Her kötü fiilin (öldürme, maske, sürükleme, sabotaj, sahte görev) bıraktığı gözlemlenebilir iz türü | ≥ 1 (beyan / uygulanan) | [L] + [B] varlık | — | Sadece ceset ve (bıçak çekilirken) bıçak |
| S3.2 | Her iz türünün ≥ 1 karşı hamlesi ve bir ömrü var; iz türlerinin ≥ %50'sinin sahte yolu var | %100 / ≥ %50 | [L] | — | — |
| S3.3 | **Tanıksız** gündüz cinayeti → cesedin ilk kez (bir tanık olmayan tarafından) algılanması, medyan | 30–120 sn | [B] | 20 tanıksız cinayet | Anında (otomatik duyuru) |
| S3.4 | Akşama kadar raporlanmayan gündüz cesetleri | %10–30 (saklamak bazen işe yarar) | [B] | 62 gündüz cesedi | %0 |
| S3.5 | Delil havuzu tavanı (replike iz decal'ı) / GPU maliyeti | ≤ 256 [T] / ≤ 0.2 ms, p95, 60 sn, 1080p, referans makine (RTX 5070 + Ryzen 7 7800X3D), ekran dışı render turu [R] | [T] [R] | — | — |

---

## 4. HARİTA HAREKETLİLİĞİ

3D dünya boş hissettirmemeli. Her büyük bölgenin şunlarla bir var olma sebebi olmalı:
- görevler
- deliller
- karşılaşmalar
- sabotaj
- rol etkileşimleri
- risk/ödül
- değişen durumlar

**Eklenen kurallar:** bölge sebepleri, bölgenin **risk katmanı** (güvenli / orta / tehlikeli) ve **açılma eşiği**
(bölge → en küçük N) layout JSON'da **veri** olarak etiketlenir (lint edilebilir). Risk eğimi (güvenli meydan ve
iskele, orta halkalar, tehlikeli dar geçitler) oyunun cinayet dağılımında görünmelidir. Oyuncu sayısı az olduğunda
harita küçülür (Bölge Kapıları), yoksa 6 kişilik maçta kimse kimseyi görmez. GDD §3'teki kümeler v1 bölgelerini
adlandırır; v2'nin kapıları layout JSON'daki eşik alanından okunur.

k = açık bölge sayısı.

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S4.1 | Açık her bölgenin yukarıdaki 7 sebepten sahip olduğu sayı | ≥ 3 (beyan / uygulanan) | [L] + [B] varlık | — | Görev: 8 bölgenin hepsinde; gözcü noktaları var ama hiçbir yetenek kullanmıyor; sabotaj ve değişen durum yok |
| S4.2 | Gündüz canlı oyuncu-saniyelerinin bölge dağılımı (toplanma hariç) | Açık her bölge ≥ 0.4/k ve ≤ 2.5/k (k = 8'de ≥ %5, ≤ %31) | [B] | 20 gündüz | ? (bot gezinmesi 8 bölgeyi ziyaret ediyor) |
| S4.3 | Cinayetlerin bölge dağılımı | Hiçbir bölge > %35; cinayetlerin ≥ %60'ı risk katmanı orta/tehlikeli bölgelerde | [B] | 200 cinayet (havuz) | ? (botlar arkası dönük herkesi her yerde öldürüyor) |
| S4.4 | Gündüz karşılaşma sıklığı ve yalnızlık, canlı oyuncu medyanı | Dakikada ≥ 1 karşılaşma; gündüz zamanının %20–50'si **yalnız** (§0.4) | [B] | 40 oyuncu-gündüz | ? |
| S4.5 | Toplanma: her yerden meydana varış | p90 ≤ 30 sn, en fazla ≤ `KG_GATHER_WINDOW` (35 sn) | [B] | 100 varış | p90 32.9 sn, en fazla 33.4 sn (anekdot: tek koşu, 20 varış) |
| S4.5b | Acil toplantı başına Geç Kalan oranı | Medyan ≤ canlıların %10'u | [B] | 20 acil toplantı | — (acil toplantı yok) |
| S4.6 | Gün başına görünür dünya durumu değişikliği (sabotaj, sönük lamba, kırık kapı, açık mezar, Dünya Olayı) | N ≥ 8'de medyan ≥ 2 | [B] | 20 gündüz | ~0 (sadece görsel görev etkileri) |

---

## 5. ROL ETKİLEŞİMİ

Roller birbirini etkilemeli. Yalnız başına mini oyun olan rollerden kaçınılmalı.

Her rolün olmalı *(düzeltildi)*:
- ~~benzersiz bir hedefi~~ → **benzersiz bir fiili** *(22 Kasaba rolü tek bir kazanma koşulunu paylaşır; "benzersiz
  hedef" sadece nötrler için geçerlidir)*
- benzersiz bilgisi, **kısmi, dünyaya bağlı ve sahtelenebilir** olarak
- anlamlı kararları
- başka rollerle etkileşmek için sebepleri: en az 1 ortak ve en az 1 karşı rol
- iyi ve kötü kullanıldığında sonuçları, **önceden bildirilmiş** olarak (kendine zarar veren roller: İnfazcı, Emekli Asker,
  Gardiyan)
- **dünyada bir ayak izi:** başka bir oyuncunun gözlemleyebileceği bir şey

**Eklenen kural:** Bir karşı rol, hedefi maçta yokken de işe yaramalıdır (temel kullanım) ya da rol üreticisi onu
hedefiyle birlikte seçmelidir. Aynı fiil ve bilgi türünü paylaşan "ayna" roller birleştirilir veya ayrıştırılır.

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S5.1 | Etkileşim grafiği: ≥ 2 kenarı ve boş olmayan `WorldFootprint`'i olan rol sayısı | 51/51, **beyan** (veri) ve **uygulanan** (hizalanma dışı yeteneği kodda) ayrı yazılır | [L] + [B] varlık | — | Beyan 0/51 (alanlar yok); uygulanan 0/51 (hizalanma dışı yeteneği olan rol yok) |
| S5.2 | Bir karşı rol maçtayken hedefinin de maçta olma oranı; ya da rolün temel kullanımı var **ve** hedefin olmadığı soak'ların ≥ %50'sinde ≥ 1 defter olayı üretiyor | ≥ %50 | [L] + [B] varlık | — | %9–36 (Çilingir %9, Adli Tabip %10, Meyhaneci %12…) |
| S5.3 | Yetenek kullanımlarının, kullanan dışında ≥ 1 tanığı olan bir olay **ya da** herkese açık bir duyuru üretme oranı | ≥ %90 | [B] | 50 kullanım | — |
| S5.4 | N ≥ 8'de gece başına rol→rol etkileşim olayı (bir yeteneğin başkasının eylemini değiştirmesi) | Medyan ≥ 1 | [B] | 20 gece | 0 |
| S5.5 | Rol sonucunu belirleyen yazı-tura sayısı | 0 | [L] | — | Korsan (taş-kâğıt-makas), Bombacı (%50 kablo) |
| S5.6 | Rol başına kazanma oranının kendi tarafının ortalamasından sapması | %95 aralığı tamamen ± 10 puanın dışında olan rol 0 (sadece ≥ 100 görünümü olan roller; kalanı "yetersiz veri") | [O] | 100 görünüm / rol | ? |

---

## 6. KÖTÜ TARAF OYNANIŞI

Öldürmek, kötü tarafın tüm oyun döngüsü olmamalı. Şunun için optimize et:

**öldürme + konumlanma + aldatma + delil manipülasyonu + alibi + risk**

Katil, eylemlerinin geride bıraktığı hikâyeyi sürekli düşünmeli.

**Eklenen kurallar:**
1. **Temel aldatma kiti her oyuncu sayısında.** Maske ve pelerin, ceset sürükleme/saklama, sahte görev (var) ve bir
   sabotaj, sadece N ≥ 14'te çıkan rollere bırakılmaz. Her Sabırsız bunlara sahiptir. İftiracı, Temizlikçi, Sabotajcı
   ve Casus bu ortak fiilleri **yükseltir**, onlara sahip olmaz.
2. **Gündüz cinayeti gerçekten risklidir** (GDD: "mümkün ama çok riskli"): tanık, kalabalık bonusu, iz.
3. **Her kötü fiilin bir izi ve bir karşısı vardır** (S3.1).

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S6.1 | Her Sabırsızın maç başına kullandığı farklı öldürme-dışı fiil (maske, sürükleme, sabotaj, sahte görev, iddia/çerçeveleme) | ≥ 2, maçların ≥ %70'inde | [O]; botlar için [U] (her fiil yolu çalışır ve loglar) | 40 Sabırsız-maç | Sadece sahte görev var |
| S6.2 | Gündüz cinayetleri | Sabırsız başına gün başına medyan ≤ 1; tüm cinayetlerin %20–45'i | [B] | 55 cinayet | 240 sn'lik tek günde 10 cinayet, 20 bot (anekdot) |
| S6.3 | Tanımlı tanıklı cinayetlerde katilin bir sonraki toplantıda yargılanma oranı | ≥ %60 | [B] | 40 tanımlı tanıklı cinayet | 0 (tanık yok, yargılama yok) |
| S6.4 | Aldatma aracı bulunan maç oranı | Her N'de %100 (temel kit); N 10–13'te ≥ %60 maçta öldürmeyen bir kötü rol (üretici değişikliği onay bekliyor, Roadmap R4) | [L] | — | N < 14'te %0 |
| S6.5 | Sabırsız kazanma oranı | %25–50 [O] (hedef merkez ~%37); bot bandı %30–50, sadece rapor, 028'den sonra kapı | [O] [B] | 58 maç [O] (planlanan ~150); 100 maç [B] gece soak'u | Ölçülmüyor; smoke'ta yıpratmayla Sabırsız kazanıyor (anekdot) |
| S6.6 | Blöf: Sabırsızın kendi kurbanını raporlaması | Bot: yol çalışır ve loglanır [U]; insan: maçların ≥ %10'unda [O] | [U] [O] | 40 maç [O] | Rapor fiili yok |

---

## 7. İYİ / NÖTR OYNANIŞ

Kasabalıların maçı araştırmak ve etkilemek için proaktif yolları olmalı.
Nötr rollerin, iyi ya da kötü rollerin zayıf kopyaları gibi hissettirmek yerine kendi teşvikleri ve fırsatları
olmalı.

**Eklenen kurallar:**
1. **Rolsüz araştırma:** her Kasaba oyuncusu, rolü ne olursa olsun, en az şu fiillere sahiptir: cesedi raporla,
   cesedi/izi incele, defterden iddia paylaş, birine kefil ol veya onu takip et.
2. **Tarafa sadık kazanma:** yalnız katiller, vampirler, veba ve her nötr kendi kazanma koşuluyla çözülür. "Sabırsız
   olmayan herkes" diye tek kova yoktur.
3. **Nötr hedefi "X tarafıyla kazan" olamaz** ve başka bir rolün koşulsuz kopyası olamaz.

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S7.1 | Rolü ne olursa olsun her Kasaba oyuncusunun araştırma fiili | ≥ 3 | [T] | — | 1 (suçla + oy) |
| S7.2 | Kasaba kazanma oranı | %38–62 [O] (hedef merkez ~%50); bot bandı %35–65, sadece rapor, 028'den sonra kapı | [O] [B] | 67 maç [O] (planlanan ~150); 100 maç [B] gece soak'u | Ölçülmüyor |
| S7.3 | Her nötrün, bulunduğu maçlarda kendi hedefini tamamlama oranı | %20–40 | [B] [O] | 62 görünüm / nötr | 0 (nötr hedefi kodda yok; nötr = habersiz Kasaba) |
| S7.4 | "X tarafıyla kazan" veya koşulsuz kopya olan nötr hedefi sayısı | 0 | [L] | — | 3 (Cadı, Hafızasız, Hayatta Kalan) |
| S7.5 | Aynı maçta Sabırsız kliği ve bir yalnız katilin birlikte kazanan listelenmesi | 0 | [T] + mantık simülasyonu | — | Mümkün (tek kova) |

---

## 8. ÖLÜM OYNANIŞI

Ölüm sadece "artık izliyorsun" demek olmamalı.
Ölü oyuncuların kısıtlı hedefleri, bilgileri veya etkileşimleri olabilir; bunlar çıkarım döngüsünü bozmadan oynanış
yaratmalı.

**Eklenen kural (çıkarımı korumak için):** ölüler yaşayanlara **sadece pahalı, nadir ve kayıtlı** kanallardan
ulaşır: Medyum seansı (maçta 1 kez), poltergeist (hayalet başına gecede 1 kez), hayalet görevlerinin %30 verimli bar
katkısı. Hepsi Olay Defteri'ne yazılır. Discord'dan "hayalet sızdırma" hilesinin değerini düşürmek için hayaletler
canlıların rollerini **görmez**, sadece kendi katilini (hayalet olunca) bilir (bkz. G7).

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S8.1 | Ölü oyuncunun her fazda kullanabileceği eylem | ≥ 1 | [T] | — | Serbest uçuş + ölü sohbeti |
| S8.2 | Ölü oyuncu başına 2 dakikada eylem (hayalet görevi, poltergeist, keşif hedefi, Araf) | İnsan: medyan ≥ 1 [O]; bot: her hayalet eylem yolu çalışır ve loglar [U] | [O] [U] | 30 ölü oyuncu | 0 |
| S8.3 | Hayaletten canlıya ücretsiz bilgi kanalı | 0 (seans ≤ 1/maç, poltergeist ≤ 1/gece, hepsi kayıtlı) | [T] | — | 0 (sohbet kuralları doğru kapsamlı) |
| S8.4 | Ölenlerin epiloga kadar bağlı kalma oranı | ≥ %60 | [O] | 40 ölü oyuncu | ? |
| S8.5 | 1. gece ölenlerin maça verdiği puan | Medyan ≥ 3/5 | [O] | 30 oyuncu | ? |

---

## 9. TOPLANTILAR

Toplantılar birikmiş oynanışın sonucu olmalı. İyi bir toplantı şunları içerir:
- delil
- suçlamalar
- savunmalar
- çelişkiler
- belirsizlik
- role özgü bilgi

Oylama, dünyada olanların bir sonucu gibi hissettirmeli.

**Eklenen kurallar:**
1. **Toplantıyı olay tetikler.** Ceset raporu → çan → acil toplantı. Toplanma penceresi `KG_GATHER_WINDOW`
   (GDD §1'deki 20 sn, S4.5 ölçümüyle çeliştiği için bu değerle değiştirilmek üzere önerilir); pencere sonunda
   halkada olmayan Geç Kalan o tur oy veremez. Çan ve acil toplantı tavanı: oyuncu başına günde 1, toplam günde 2;
   tavanı aşan raporlar kaybolmaz, **akşam toplantısının Vaka Dosyası'na** eklenir. Sabit akşam toplantısı kalır ama
   o gün vaka açılmadıysa kısadır. Konuşacak bir şey yokken 45 + 3N sn boş zaman yoktur.
2. **Vaka Dosyası:** toplantı ekranı vakayla açılır. Kurbanın rol kartı karardan (veya şafaktan) önce kapalı kalır,
   böylece tartışılacak bir şey olur.
3. **Toplantı güvenlidir:** Toplantı, Mahkeme ve Epilog'da saldırı yoktur.
4. **Mekanik ve sosyal katman ayrılır:** Vaka Dosyası, defter satırları, yapılandırılmış iddialar
   (GÖRDÜM / ORADAYDIM / GÖREVİ YAPTIM / ROL_SONUCU) ve oylar **mekaniktir**, botlar okur ve yazar. Suçlama ve
   savunmanın tonu **sesle** olur (G1).
5. **Belirsizlik korunur:** asma doğruluğunun bir **üst sınırı** da vardır. Her toplantı doğru kişiyi asıyorsa bilgi
   bütçesi bozulmuştur.

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S9.1 | Toplantı, Mahkeme ve Epilog'daki ölümler | 0 | [T] [B] | — | 1 smoke'ta 2: toplantıda 1, epilogda 1 (anekdot) |
| S9.2 | Vakalı toplantının yargılamaya ulaşma oranı | ≥ %60 | [B] | 40 vakalı toplantı | Hiç vaka yok; 7 toplantıda 0 yargılama (anekdot, x30: bot oy zamanlayıcısı ölçeklenmiyor) |
| S9.3 | Vakasız akşam toplantısının yargılamaya ulaşma oranı | ≤ %40 (oy, bir şey olduğunda gelir) | [B] | 40 vakasız toplantı | 0 (anekdot) |
| S9.4 | Asma doğruluğu (asılanın kötü taraf olma oranı) | %55–75 (alt: çıkarım çalışıyor; üst: belirsizlik var). Rastgele taban raporla birlikte yazılır | [B] | 88 asma | Ölçülmüyor; bot oyu %60 yazı-tura |
| S9.5 | Vakalı toplantı içeriği: iddia; çelişki (§0.4) | Medyan ≥ 2 iddia; toplantıların ≥ %30'unda ≥ 1 çelişki | [B] | 30 vakalı toplantı | 0 |
| S9.6 | Çekimser olmayan bot oylarından bir `EventId`'ye dayananların oranı | ≥ %90; `random` gerekçesi 0 | [B] | 100 oy | %0 |
| S9.7 | Vakasız toplantı süresi | ≤ 30 sn | [T] | — | 45 + 3N sn |
| S9.8 | N ≥ 8'de role özgü bilgi (§0.4) içeren toplantı oranı (roller geldikten sonra) | ≥ %50 | [B] | 30 toplantı | 0 |

---

## 10. TEMPO

Maç tırmanmalı:

| Evre | İçerik |
|---|---|
| Erken | keşif + ilk ipuçları |
| Orta | şüphe + olaylar + rol etkileşimleri |
| Geç | birikmiş bilgi + daha yüksek risk + zor kararlar |

İki şeyden de kaçınılmalı:
- hiçbir şeyin önemli olmadığı boş dönemler
- stratejik kontrolü ortadan kaldıran durmak bilmeyen rastgele olaylar

**Eklenen kurallar:** faz süreleri botları da sayar. Dünya Olayları önceden (şafakta) bildirilir. Maç, mümkünse,
yıpratmayla değil bir kararla biter.

| Kod | Hedef | Değer | Ölçüm | n_min | Bugün |
|---|---|---|---|---|---|
| S10.1 | Maç süresi (duvar saati, ısınma ve açılış hariç) | **20–35 dk** (Vizyon). Gün bandı bundan türetilir, `KG_PHASE_DAY_S` ile: N = 6 → 4–6 gün, N = 12 → 3–5, N = 20 → 3–4. Faz süreleri değişirse gün bandı formülden yeniden hesaplanır. | [B] [O] | 20 maç / N | Smoke: 8 gün (anekdot, x30) |
| S10.2 | İlk ölüm (mekanizma: 1. gündüzün ilk 90 sn'sinde bıçak çekilemez [T]) | 1. gündüzün ilk 90 sn'sinde 0; maçların ≥ %80'inde 2. şafaktan önce | [T] [B] | 50 maç | Koruma süresi yok |
| S10.3 | Tırmanma: defter olayı (hareket hariç) / canlı oyuncu-dakikası, 3. gün ve sonrası ÷ 1. gün | ≥ 1.5× | [B] | 20 maç | ? |
| S10.4 | Son iki günden toplantıya ulaşanlar içinde ≥ 1 yargılama olanların oranı | ≥ %60 | [B] | 30 gün | 0 (anekdot) |
| S10.5 | Son elemesi asma ya da rol yeteneği olan (bıçak cinayeti değil) maç oranı | ≥ %40 | [B] | 50 maç | Smoke: yıpratma (anekdot) |
| S10.6 | Dünya Olayları | Faz başına ≤ 1, ≥ 20 sn önceden (şafakta) bildirilir, sonucu belirleyen 0 | [T] [L] | — | Henüz Dünya Olayı yok |
| S10.7 | Faz süresi hesabı botları sayar | Evet | [T] | — | Hayır (`GetNumPlayers()` botları saymıyor; 12 botlu maç 6 kişilik sürelerde) |

---

## KORUYUCU KURALLAR (G1–G7) *(eklendi)*

Sütunlar neyin artacağını söyler. Koruyucu kurallar, artarken neyin **bozulmayacağını** söyler. Bir mekanik bir
sütunu iyileştirip bir koruyucu kuralı deliyorsa reddedilir.

### G1. Ses ve yakınlık sohbeti: ana bilgi kanalı
Vizyon sütunu 1: "Sesin kendisi oyundur." Prompt sesi hiç anmıyordu. Suçlama, savunma, alibi ve çığlık yakınlık
sesiyle olur; duvar arkası boğuk gelir, gece katil telsizi vardır.

| Kod | Hedef | Değer | Ölçüm |
|---|---|---|---|
| G1.1 | Sadece sesle çalışan her mekaniğin sessiz yolu (yapılandırılmış iddia, emote, yazı) | %100 (rol tablosunda "sessiz yol" sütunu) | [L] |
| G1.2 | Menziller ve duvar | Normal 20 m, fısıltı 3 m, bağırma 40 m; duvar arkası ≥ 12 dB zayıflama ve ≤ 1.5 kHz alçak geçiren | [T] |
| G1.3 | Ses gecikmesi (P2P) | p95 ≤ 200 ms, iki uçta loglanan ses paketi zaman damgalarından | [T] (iki makine) |
| G1.4 | Susturma etkileri | Hiçbir etki tüm iletişimi kesmez. Şantajcı: emote + 1 yazılı savunma satırı kalır | [L] |
| G1.5 | Ses özellikleri sessize alma / engelleme / raporlama ile gelir | %100 (CLAUDE.md kuralı) | [T] |

Ses yokken (bugün `Voice/` sadece ağız bileşeni) oyun **yazı ve yapılandırılmış iddialarla** oynanabilir olmalıdır.
Botların sese katılımı iddialar üzerinden olur.

### G2. Ölçek: 6'dan 20'ye
Her [B] hedefi N = 6, 8, 12 ve 20'de ölçülür (kapsamı N ile sınırlı olanlar kendi kapsamında).

| Kod | Hedef | Değer | Ölçüm |
|---|---|---|---|
| G2.1 | [B] hedeflerinin tutması | Kapsamı yazılı olanlar kendi kapsamındaki her N'de; kapsamsız olanlar N = 6, 8, 12'de | [B] |
| G2.2 | Bölge Kapıları: bölgeler layout JSON'daki eşik alanına (bölge → en küçük N) göre açılır | Test bu alanı okur; kapalı bölgede görev ve ev ataması 0 | [T] |
| G2.3 | Karşılaşma sıklığı (S4.4) N = 6 ile N = 20 arasında | Oran ≤ 2× (n_min: N başına 40 oyuncu-gündüz) | [B] |
| G2.4 | Her N'de ≥ 1 aldatma kaynağı ve ≥ 1 kendi hedefli nötr | %100 | [L] |

### G3. Okunabilirlik ve alıştırma
51 rol derinlik için var, yeni oyuncuyu boğmak için değil.

| Kod | Hedef | Değer | Ölçüm |
|---|---|---|---|
| G3.1 | Rol kartı: hedef + ≤ 2 yetenek satırı | Gönderilen her dilde ≤ 25 kelime | [L] |
| G3.2 | Başlangıç havuzu: lobi seçeneği (varsayılan açık) | 14–16 rol (M5 sekizi + temel araştırmacılar) | [L] |
| G3.3 | İlk kez oynayanın rol açılışından sonra kazanma koşulunu doğru söylemesi | ≥ %90 (n_min 40 oyuncu) | [O] |
| G3.4 | Gizli kural yok: rol verisindeki her yetenek ve değiştirici satırının boş olmayan bir HUD metni var | %100 | [L] |
| G3.5 | Defter olayları tek satır, bölge adı + faz saati ile | %100 | [T] |

### G4. Kontrollü varyans ("daha az rastgelelik" yerine)
Tekrar oynanabilirlik varyans ister. Sorun rastgeleliğin **nerede** olduğudur.
- **İyi (kurulum ve girdi):** rol listesi, evler, görev listesi, şafakta duyurulan Dünya Olayı. Hepsi tohumlu `FKGRng`,
  önceden bildirilir, oyuncu ona göre plan yapar.
- **Kötü (sonuç):** bir sonucu oyuncu kararından bağımsız belirleyen yazı-tura.

| Kod | Hedef | Değer | Ölçüm |
|---|---|---|---|
| G4.1 | Sonucu belirleyen yazı-tura | 0 | [L] |
| G4.2 | Bugünkü dört ihlal (fener aydınlanması, Korsan, Bombacı, bot oyları) | Sonucu belirleyen yazı-tura 0. Nasıl düzeltileceği bu belgenin işi değildir: öneriler Roadmap Onay listesinde (R2, R5). | [L] |
| ~~G4.3~~ | *Kaldırıldı: S10.6 ile aynı.* | | |
| G4.4 | Tekrar oynanabilirlik: 20 tohumda (N = 12) ilk cinayet bölgesi | ≥ 5 farklı bölge; rol listesi tekilliği ≥ %95 | [B] [L] |

### G5. Anti-snowball ve anti-frustrasyon

| Kod | Hedef | Değer | Ölçüm |
|---|---|---|---|
| G5.1 | Geri dönüş: 3. gün şafağında canlı/başlangıç oranı öteki taraftan ≥ 0.2 düşük olan tarafın kazanması | ≥ %20 (n_min 40 böyle maç) | [B] [O] |
| ~~G5.2~~ | *Kaldırıldı: S10.2 ile aynı (mekanizma orada).* | | |
| G5.3 | Kendine zarar veren sonuçlar (İnfazcı vicdanı, Emekli Asker, Gardiyan infazı) | Önceden gösterilir + onay ister | [L] |
| G5.4 | Bir etkinin kapattığı iletişim kanalı | ≤ 1 (G1.4) | [L] |
| G5.5 | AFK (90 sn hareketsiz → "uyuyan", oyu sayılmaz, GDD §14) | Var | [T] |
| G5.6 | Bir oyuncunun karşı hamlesi olmayan olumsuz etki (ölüm hariç) | 0: her olumsuz etki satırının karşı hamle alanı veride var olan bir fiile/role işaret eder | [L] |

### G6. Botlar inandırıcı oyuncudur
Karar: botlar hem **ölçüm aracı** hem de 6'dan az kişili lobileri dolduran **oyuncu**dur. İkisi için de aynı kural
geçerlidir.

| Kod | Hedef | Değer | Ölçüm |
|---|---|---|---|
| G6.1 | **Adalet kuralı:** bir bot sadece kendi algısını ve kendi takımının verisini okur; başkasının gizli durumunu (rol, defter, gece seçimi) asla okumaz | 0 ihlal. `run_invariants` içinde grep: `Source/KillGodot/AI/` altında `KGBotView.cpp` dışında `PrivateRoleId`, `AlignmentOf`, özel defter ve gece seçimi erişimcilerine 0 referans | [T] |
| G6.2 | Her bot suçlaması ve oyu `KG_VOTE ... reason=<eventId\|abstain>` loglar | %100; `random` 0 | [U] |
| G6.3 | Takım koruma ele vermesi: Sabırsız bot, yargılanan takım arkadaşına diğer oylarıyla **aynı kanıt kuralıyla** oy verir (özel bilgiye dayanan koruma terimi yok) | Evet | [T] |
| G6.4 | Sabırsız botların tanımlı tanıklı cinayeti | Ayrı bant yok: botlar S2.2'yi bantta tutar | [B] (S2.2) |
| G6.5 | Bot karar zamanlayıcıları saat hızıyla ölçeklenir (x30 smoke gerçekten oylar) | Evet | [T] |
| G6.6 | Toplantı başına bot iddiası (toplam) | Vakalı: 2 ile min(8, canlı/2) arası; vakasız: 0–2 | [B] |

### G7. Adalet ve anti-cheat (gizli bilgi)

| Kod | Hedef | Değer | Ölçüm |
|---|---|---|---|
| G7.1 | Sahibi olmayan istemciye ulaşan gizli alan (rol, defter, gece seçimi, vasiyet taslağı) | 0 (iki süreçli başsız test) | [T] |
| G7.2 | Oyun sonuçları sunucuda doğrulanır | Şu RPC'lerin her biri için ≥ 1 red testi (yanlış faz, menzil dışı, ölü gönderen): saldırı, oy, görev tamamlama, rapor, iddia | [T] |
| G7.3 | Hayaletler canlıların rolünü görmez; hayalet → canlı kanalı sadece S8.3 | Evet | [T] |
| G7.4 | Görüş dışındaki uzak oyuncuların konum replikasyonu (duvar hilesi yüzeyi) | **Ertelendi (M3).** Ölçütü M3 sprintinde yazılır. | — |
| G7.5 | Yayıncı modu (SPRINT-015) defter ve Vaka Dosyası'ndaki isimleri de maskeler | Evet | [T] |

---

## SÜTUNLAR ARASI SIRA (bağımlılık)

Prompt 10 paralel hedef gibi okunuyordu. Aslında sıralılar:

1. **S2 Bilgi (Olay Defteri + tanık filtresi):** 3, 5, 6, 8 ve 9 bundan okur.
2. **S9 Toplantı:** rapor, Vaka Dosyası, güvenli toplantı.
3. **G6 Botlar:** ölçüm aracı çalışmadan diğer [B] hedefleri anlamsızdır.
4. **S3 + S6:** izler ve temel aldatma kiti.
5. **S7 tarafa sadık kazanma + S5 Gece Çözücü ve M5 çekirdek sekizi.**
6. **S8 ölüm, S10 tempo, S4 bölge kapıları.**
7. **G1 ses:** çevrimiçi yığın (M3) ve EOS ürünü hazır olunca. O zamana kadar her şey yazı ve iddialarla çalışır.

Sprint karşılıkları: `Pillar_Roadmap.md`.

---

## MEKANİK KONTROL LİSTESİ (kullanıcının "son sorusu", somut hâli)

Orijinal soru: *"Bu, oyuncuyu düşündürüyor, seçtiriyor, şüphelendiriyor, risk aldırıyor ya da bir şeye tepki
verdiriyor mu?"* Sorun: "bir şeye tepki verdiriyor" her rastgele olayı geçirir. Keskinleştirilmiş hâli:

> **"Oyuncunun seçimi bundan sonra olacakları değiştiriyor mu, ve başka biri için bilgi bırakıyor mu?"**

Her yeni mekanik (ve her sprint önerisi) bu tabloyla gelir. Cevaplar **sadece Evet / Hayır**. **Ç1–Ç4 zorunludur**;
kalan 8 sorudan en az 6'sı Evet olmalıdır. "Hayır"ların her biri bir cümleyle gerekçelendirilir.
**Sonuç kuralı:** yeni mekanik zorunlu bir maddede Hayır alırsa reddedilir. **Mevcut** mekanik zorunlu bir maddede
Hayır alırsa **Koşullu** olur: "Çx, SPRINT-0yy sonunda Evet olmalı, olmazsa mekanik kesilir."

| # | Soru | Zorunlu |
|---|---|---|
| Ç1 | **Karar:** oyuncu farklı risk/bilgi sonucu olan ≥ 2 seçenek arasından seçiyor mu? | Evet |
| Ç2 | **İz:** Olay Defteri'ne yazıyor mu, başka biri görebiliyor/duyabiliyor mu? | Evet |
| Ç3 | **Karşı hamle:** karşı tarafın buna bir cevabı var mı (önleme, fark etme, sahteleme)? | Evet |
| Ç4 | **Ölçüm:** hangi hedef kodunu (S/G) hangi sayıyla hareket ettiriyor? | Evet |
| Ç5 | **Bağlantı:** en az 2 başka sistem bunu okuyor mu (defter, toplantı, rol, bot, epilog, harita)? | |
| Ç6 | **Ölçek:** N = 6'da ve N = 20'de anlamlı mı? | |
| Ç7 | **Varyans:** sonucu yazı-tura belirlemiyor mu? Rastgelelik varsa kurulumda ve önceden bildirilmiş mi? | |
| Ç8 | **Bot:** botlar bunu kullanabiliyor ve algılayabiliyor mu (sadece kendi algılarıyla)? | |
| Ç9 | **Sessiz yol:** ses olmadan (yazı, iddia, emote) çalışıyor mu? | |
| Ç10 | **Okunabilirlik:** HUD'da veya kartta tek cümleyle anlatılabiliyor mu? | |
| Ç11 | **Frustrasyon:** etkiyi yiyen oyuncunun önceden görebileceği bir işaret ve bir cevabı var mı? | |
| Ç12 | **Tempo:** hangi evreye (erken/orta/geç) hizmet ediyor, ölü zamanı azaltıyor mu? | |

### Örnek puanlamalar (bugünkü hâlleriyle)
| Mekanik | Ç1 | Ç2 | Ç3 | Ç4 | Diğer Evet (8'den) | Sonuç |
|---|---|---|---|---|---|---|
| Balık tutma (ITER-013) | Evet (nerede, ne zaman) | Hayır (misina ve ses algılanıyor ama deftere düşmüyor) | Evet (arkası dönük, tehlikeli an) | Hayır | 6: Ç6, Ç7, Ç9, Ç10, Ç11, Ç12 | **Koşullu:** Ç2 SPRINT-020b (balık yayıcısı), Ç4 SPRINT-024 (S2.3 alibi parçası) sonunda Evet olmalı, olmazsa balık maçtan ayrı bir yan etkinlik olarak kalır ve maç içinde kapanır |
| Sabırsız backstab | Evet | Hayır (tanık, iz yok; gündüzse anında köy çapında duyuru) | Evet (sırtını dönme, kalabalıkta kalma) | Hayır | 3: Ç6, Ç9, Ç10 | **Koşullu:** Ç2 ve Ç4 (S2.1, S6.2) SPRINT-020b sonunda Evet olmalı; diğer Evet sayısı 025a (kalabalık bonusu: Ç11) ve 023 (Ç8) ile 6'ya çıkmalı |
| Fener aydınlanması | Hayır (Kasaba seçmiyor, rastgele biri yanar) | Evet | Hayır | Hayır | 1: Ç10 | **Koşullu:** yeniden tasarım (Onay R2) onaylanır ve SPRINT-031b sonunda Ç1–Ç4 Evet olur; onaylanmazsa fener aydınlanması kesilir |
| Sahte görev | Evet (nerede, kimin önünde) | Evet (etki yok = iz) | Evet (görsel görev karşılaştırması) | Evet (S6.1) | 6: Ç6, Ç7, Ç8, Ç9, Ç10, Ç12 | **Kalır (model mekanik).** Ç5 Hayır: bugün sadece HUD okuyor; 024'teki Kasaba Panosu ile Evet olur |

---

## Kullanıcı metninden farklar (değişiklik günlüğü)

| Orijinal | Bu belgede | Neden |
|---|---|---|
| Numarasız ilk blok, 1. ve 3. bölüm yok | 1. KARARLAR; 3. İZ VE DELİL | Numaralandırma 2'den 4'e atlıyordu. Delil yaşam döngüsü 6. ve 9. sütunların temeli. |
| "constantly have meaningful choices" | En uzun ölü zaman tavanı (S1.1) | "Sürekli", 10. sütundaki sakin erken oyunla çelişiyordu. |
| "Optimize for more X" | Her hedef bir bant | Sınırsız hedef = sınırsız döngü (LoopContract). |
| "LESS RANDOMNESS" | "Sonucu belirleyen rastgelelik" (G4) | Tekrar oynanabilirlik kurulum varyansı ister. |
| Her rolün "unique objective"i | Benzersiz fiil + dünyada ayak izi; hedef sadece nötrlerde | 22 Kasaba rolü aynı koşulla kazanır. |
| Her rolün "unique information"ı | Kısmi, dünyaya bağlı, sahtelenebilir | Menüden kesin cevap, 9. sütundaki belirsizliği ve 2. sütundaki "tanık olunan davranış"ı ezer. |
| "MORE USEFUL INFORMATION" | Bilgi bütçesi (S2.4) + asma doğruluğu üst sınırı (S9.4) | Fazla sert kanıt çıkarımı bitirir. |
| "react to something" | Keskinleştirilmiş son soru + 12 maddelik liste | Her rastgele olay "tepki" sayılıyordu. |
| Zincir tek yönlüydü | Sonuç → yeni karar döngüsü | Tekrar oynanabilirlik döngüden gelir. |
| Ses, ölçek, okunabilirlik, bot adaleti, anti-cheat yok | G1–G7 | KillGo ses odaklı, 6–20 kişilik ve bot dolgulu bir oyun. |
| Öncelik yok | Bağımlılık sırası | Bilgi katmanı diğer her şeyin önkoşulu. |
| Kapsam yok | Kapsam kuralı (§0.1) | 51 rolün hiçbirinin hizalanma dışı yeteneği kodda yokken yeni tasarım eklemek yerine var olanı hayata geçir. |

## v1.1 günlüğü (ölçülebilirlik incelemesi, 2026-09-25)

| Konu | Değişiklik |
|---|---|
| Çelişen bantlar | S10.1 artık tek hedef (20–35 dk), gün bandı `KG_PHASE_DAY_S`'ten N'ye göre türetilir. S4.5 ↔ toplanma: pencere 35 sn, Geç Kalan koruması S4.5b. S3.3 sadece tanıksız cinayetler. S2.2 ve G6.4 tek bant (%10–20). S2.4 cinayet başına oran, [L] kaldırıldı, "kesin rol sonucu" sert kanıt tanımından çıktı. |
| Tek sabitler tablosu | §0.3: görüş, tanımlama (= isim etiketi, 8 m), olay başına duyma yarıçapı, toplanma, ölü zaman penceresi, faz formülü. |
| Kurgusu gereği geçen metrikler | Yeni [U] (bot uyumu) kodu; S6.1 (bot), S6.6, S8.2, G6.2, G6.3 buna geçti. Dondurulmuş parametre kuralı (§0.2 kural 2). |
| Örneklem | Her [B]/[O] satırında n_min; geçer / kalır / sonuçsuz kuralı; [O] bantları ~150 maçlık hacme göre genişletildi (S6.5, S7.2); S5.6 ≥ 100 görünümle sınırlı. Kabul soak'u küçük, büyük matrisler gece raporu. |
| Düzyazı satırlar | G1.2 dB/Hz, G1.3 paket zaman damgası, G3.1 dil başına, G3.2 14–16, G3.4 daraltıldı, G4.2 yeniden tasarım listesi çıktı, G7.2 RPC listesi, G7.4 M3'e ertelendi, "olay yoğunluğu" tanımlandı, "Dünya Olayı" terimi eklendi. |
| Tanımsız terimler | Ölü zaman (15 sn pencere, düz hareket hariç), yalnız, çelişki, role özgü bilgi, bilgi parçası, karar türü ↔ fiil eşlemesi, S2.7 tabanı, G5.1 "geride", S10.4 ve S10.5. |
| Diğer | S4.2 k'ye göre, S4.3 cinayet havuzu + risk katmanı alanı, S5.3 "gözlemlenebilir" yol haritası tanımıyla, S9.6 çekimserler hariç, G6.6 N'ye göre toplam, G2.1 kapsama göre, G2.2 layout JSON eşik alanı, S3.5 referans makine ve [R], [L] ↔ [B] varlık eşlemesi, kontrol listesi sadece Evet/Hayır + Koşullu. G4.3 ve G5.2 kopya oldukları için kaldırıldı. |
