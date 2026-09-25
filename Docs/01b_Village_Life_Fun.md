# KILL GODOT — Köy Hayatı, Eğlence ve Sosyal İçerik

> Maç "tak diye" başlamaz, "tak diye" bitmez. Köy, rol oyunu bitince bile oynanası bir yer olmalı.

## 1. Maçın ritmi: giriş ve çıkış

```
Meyhane Lobisi ──► Yükleme (lore ipuçları) ──► KÖY ISINMASI (90–150 sn)
   ──► Haberci Çocuğun Kayığı (sinematik) ──► Rol Kartları ──► MAÇ
   ──► Final Anı (slow-mo) ──► EPİLOG (90 sn) ──► "Yarın tekrar?" ──► Meyhane Lobisi (aynı parti)
```

### Köy Isınması (Sohbet Turu)
- Herkes köyde **rolsüz** doğar. Hasar kapalıdır, sadece eğlence itmeleri ve ragdoll var.
- Serbest dolaşılır: mini oyunlar, meslek seçimi (belediyedeki tahtadan), evini görme, kozmetikleri gösterme, arkadaşlarla sohbet.
- Hazır olan meydandaki **çana dokunur**. Herkes hazır olunca ya da süre bitince sis aralanır, **Haberci Çocuğun kayığı** iskeleye yanaşır ve replik gelir: *"Bay Godot bu akşam gelemeyecek… ama aranızda onu bekleyen biri var. Ve biri… sabırsız."* Kartlar dağıtılır.
- Geç katılan (host izin verdiyse) ısınma bitene kadar girebilir.

### Final ve Epilog
- Son öldürme ya da son asılma anı **slow-motion** ve sinematik kamerayla oynar ("Final Kill Cam").
- **Epilog (90 sn, "Perde"):** herkes, ölüler ve hortlaklar dahil, meydanda yeniden doğar. İsim etiketlerinde **roller açık**, hasar kapalı.
  - Belediye tahtasında **olay zaman çizelgesi** görünür: kim kimi, ne zaman, neyle öldürdü. Ghost'ların bildiklerini birbirine anlatma zamanı.
  - **Ödül kürsüsü** 3 unvan verir. Örnekler: *En Sabırsız*, *+1000 Aura*, *Soytarının Kurbanı*, *Kasabanın Kalkanı*, *Ağaç Devirme Kazası*, *Hiç Konuşmayan*, *Bağıran Adam*.
  - **Portreci NPC** herkesi kürsüye toplar ve grup fotoğrafı çeker. Poz emote'u seçilir, fotoğraf koleksiyona kaydedilir.
  - **"Yarın tekrar?"** oylaması yapılır. Çoğunluk evet derse aynı parti meyhaneye döner ve ayar değişmeden yeni maç başlayabilir.

## 2. Köy eğlenceleri (mini oyunlar)
Bunlar ısınmada, gündüz, epilogda ve lobide oynanır. Maç içinde oynamak **kamuflajdır**: "Ben tüm gün satranç oynadım, yanımdakine sorun."

| Aktivite | Yer | Mekanik | Not |
|---|---|---|---|
| **Satranç** | Meyhane, meydan taş masaları | Tam kurallı, 1v1. Tahtadaki taşlar fiziksel, devrilebilir 😄. Seyirci olunabilir. Bot rakip: "Saatçi Anselm". | Hamle kontrolü C++'ta küçük bir satranç çekirdeği. |
| **Tavla** | Çay ocağı | 1v1, zar fiziği gösterisi. | Türk oyunculara selam. |
| **Dama** | Meydan | 1v1. | |
| **Yalancı Zarı** | Meyhane | 2–6 kişi, blöf oyunu. | Sosyal çıkarım oyununun içinde blöf oyunu. |
| **Dart** | Meyhane | Fizik atış, sallanan nişangah. | Sarhoşken daha zor. |
| **Bilek Güreşi** | Meyhane | Karşılıklı ritim ve tuş bitirme QTE'si. | Kazanan +aura. |
| **Kuka (Bowling)** | Meyhane arkası | Fiziksel top ve kukalar. | Source fiziği vitrini. |
| **Nal Fırlatma** | Ahır | Fırlatma fiziği. | |
| **Taş Sektirme** | Sahil | Su yüzeyinde sekme fiziği, rekor tablosu. | Köy rekoru tahtada yazar. |
| **Balık Tutma** | İskele, dere, kayık | Olta mini oyunu, 40+ balık türü, koleksiyon. Nadir balıklar evdeki akvaryuma konur. | Balıkçı mesleğinin görevleri. |
| **Köy Maçı** ⚽ | Çayır | Domuz mesanesinden fizik top, iki kale. | Ücretsiz kaos. |
| **Kayık Yarışı** | Liman | Kürek ritmi, şamandıraların etrafından. | Pruvada **"Aura Dansı"** emote'u. |
| **Enstrüman** | Meyhane piyanosu, lavta, akordeon | Tuşlara nota eşleşmesi, herkes duyar. | Gizli melodi mahzeni açar (easter egg). |
| **Denizci Şarkısı Karaokesi** | Meyhane sahnesi | Tahtada sözler akar, **gerçek sesle** söylenir. Ses şiddetiyle alkış ölçülür. | Kamu malı shanty'ler, kendi kayıtlarımızla. |
| **Fıçı Yuvarlama / Çuval Yarışı** | Festival olayı | Fizik yarışı. | |
| **Portre ve Fotoğraf Modu** | Her yerde | Poz, filtre, kare. | Sosyal medya paylaşımı için. |

## 2.1 Parti mini oyunları: "Panayır Gecesi" modu (öneri)
**Soru:** Parti oyunu mantığı bu oyuna yakışır mı?
**Görüş:** Evet, ama **ana modun yerine değil, yanında**. Aynı köy, aynı kuklalar ve aynı fizik kullanıldığı için maliyeti düşük. Arkadaş grubunun "bu akşam kafa dinleyelim" modu olur ve yayıncılar için mükemmeldir.

**Üç katmanda:**
1. **Köy içi mini oyunlar** (zaten var, §2): ısınmada, epilogda ve gündüz kamuflaj olarak.
2. **Panayır olayı** (ana modda rastgele olay): 3 dk boyunca meydanda kısa bir yarışma açılır. Kazanan bakır ve küçük bir avantaj alır (fener yağı, tek kullanımlık kilit). Sabırsızlar da katılır, kalabalık da cinayet için örtüdür.
3. **Panayır Gecesi (ayrı mod, Sezon 2 hedefi):** 6–20 kişi, 8–10 kısa tur, puan tablosu, finalde "Köyün Şampiyonu" tacı. Tur örnekleri:

| Tur | Nasıl |
|---|---|
| **Fıçı Yarışı** | Yokuş aşağı fıçı yuvarlayarak liman yarışı (fizik) |
| **Tavuk Kovala** | Meydandaki kaçak tavukları kümese taşı |
| **Müzikli Sandalye** | Meyhanede piyano durunca otur. Sandalyeler fizikle devrilir. |
| **Halat Çekme** | İki takım, iskele üstünde, kaybeden suya düşer |
| **Çan Kulesi Tırmanışı** | Parkur ve çanı ilk çalan kazanır |
| **Kim Sabırsız?** | 60 sn'lik mini sosyal çıkarım: 1 kişi gizli "taklitçi", gerisi bulur |
| **Balık Avı Düellosu** | 90 sn'de en ağır balığı tutan kazanır |
| **Saklambaç** | 1 ebe, köyün yarısı, yeraltı dahil |
| **Taş Sektirme Finali** | En çok sekiş |
| **Domuz Rodeosu** | Domuzun üstünde en uzun kalan |

**Risk:** kapsam kayması. Bu yüzden Panayır Gecesi **ana mod oynanabilir olduktan sonra** (M16+) gelir. Şimdilik sadece altyapısı düşünülür: tur tabanlı mod çerçevesi ve puan tablosu.

## 3. Ağaç kesme ve doğa fiziği (The Forest hissi)
- **Balta** ile ağaca vurulur. Vurulan tarafta **çentik** açılır, çentik mesh'i vuruş yönüne göre derinleşir.
- Ağacın tipine ve balta seviyesine göre 6–10 vuruş sonra ağaç **çentiğin tersi yöne** doğru, **kütüğün etrafında menteşe gibi dönerek** yavaşça devrilir, sonra kopar. Çatırtı, "Timbeer!" sesi, yaprak parçacıkları, kamera sarsıntısı olur.
- Devrilen ağaç **gerçek fizik gövdesidir**: çiti kırar, arabayı devirir, kinetik enerjiye göre oyuncuya hasar verir. **Kaza süsü verilmiş cinayet** mümkündür ve Adli Tabip bunu *"ezilme"* olarak raporlar.
- Yerdeki gövde 3–4 **kütüğe** bölünür. Kütükler ağırdır, 2 kişiyle taşınır. Kereste fabrikasında **kalas** olur, kalaslar Marangoz görevlerine ve Sis Kuşatması barikatlarına gider.
- Kütük yerinde kalır. Maç boyunca devrilen ağaçlar haritanın kalıcı hikâyesidir.
- **Optimizasyon:**
  - Ağaçlar normalde **HISM foliage** instance'ıdır. İlk vuruşta tek bir aktöre dönüşür.
  - Aynı anda en fazla **3 ağaç** fiziksel devrilir.
  - Kesilebilir ağaç sayısı ~150'dir. Ev ve yol çevresindekiler kesilemez, harita düzeni korunur.
  - Devrilme sunucuda simüle edilir. Kütük dinlendikten sonra dormant olur.
- **Kırılabilirler:** çitler, kasalar, fıçılar, çömlekler, pencereler, kapılar (can değerleri var), pazar tezgâhları, köprü tahtaları (sabotaj), gizli duvarlar.

## 4. NPC'ler ve satıcılar
NPC'ler az sayıda (8–14), karakterli ve ucuzdur: StateTree'yle günlük rutin, animasyon LOD'u.
- **Bakır:** maç içi para birimi. Görevlerden kazanılır, maç sonunda sıfırlanır. Meta para birimi olan **Altın** ile karıştırılmaz.

| NPC | Yer | Ne yapar |
|---|---|---|
| **Bay Thimble** (Genel Mağaza) | Çarşı | Kilit, fener, ip, bandaj, ekmek, kova. Bakır ile. |
| **Çaycı Rıza** | Çay ocağı | Çay satar (60 sn stamina yenilenmesi). Tavla masası burada. **Dedikodu** satar: 10 bakır'a rastgele bir söylenti, **%60 doğru, %40 yanlış**. Yanlışlar da gerçek olaylardan üretilir. |
| **Maraşlı Usta** (Dondurmacı) | Liman meydanı | Dondurma satar ama **külahı üç kez geri çeker** (tam zamanında yakala mini oyunu). Yakalayan: "+1000 aura". |
| **Davulcu** | Sokaklar | **Şafakta** davul çalarak sokakları dolaşır, köy uyanır. Şafak fazının sesi ve görüntüsü odur. |
| **Madam Brine** | Balık pazarı | Yem, olta yükseltmesi, balık alım-satımı. |
| **Kaçak Satıcı** | Deniz mağarası (sadece gece) | Kara pazar: tek kurşun, maymuncuk, sis şişesi, panzehir, barut. **Herkes** gidebilir. Oraya girip çıkarken görülmek şüphe uyandırır. |
| **Pazar Gemisi Kaptanı** | Liman (olay) | Nadir eşyalar, egzotik yemek. |
| **Portreci Madam Vellum** | Meydan | Portre ve epilog grup fotoğrafı. |
| **Mim Sanatçısı** | Meydan | Yanında emote yapanı **taklit eder**. |
| **Haberci Çocuk** | İskele | Duyurular. Lore'un yüzü. |
| **Hayvanlar** | Her yerde | Sevilebilen **kedi ve köpekler**. **Martılar** elindeki ekmeği kapıp kaçar. Tavuklar ve domuzlar kaos yaratır. |

## 5. Güncel mizah ve meme içerikleri
Meme'ler hızlı eskir. Bu yüzden **veriyle yönetilen** bir sistem kuruyoruz: `DT_MemeRotation`. İçinde ses satırları, emote'lar, spray'ler, NPC laf atmaları, eşya isimleri ve epilog unvanları var. Her sezon yama ya da hotfix ile güncellenir, kod değişmez.

**Kurallar (hukuki güvenlik):**
- Gerçek kişilerin yüzü, adı, sesi kullanılmaz.
- Telifli görsel, müzik ya da klip kullanılmaz. Markalı isim kullanılmaz (ör. "Deagle" değil, "El Topu .50").
- Meme'in **konsepti ve esprisi** kendi sanatımız ve kendi cümlelerimizle parodi olarak işlenir.
- Tescil başvurusu yapılmış popüler meme karakterleri (bazı "brainrot" karakterleri gibi) doğrudan kopyalanmaz. Onların **tarzında** özgün karakterler yapılır.

**İlk rotasyon fikirleri (2025–26 mizahından esinli, hepsi özgün uygulama):**
- **Aura:** epilogda "+1000 aura / −500 aura" istatistikleri. Kayık pruvasında **"Aura Dansı"** emote'u (kayık yarışı çocuğu esprisi, özgün koreografi).
- **Tok tok tok:** şafak davulcusu. Ramazan davulcusu geleneğiyle o meşhur "kapıya vuran kütük" esprisinin birleşimi. Özgün bir karakter.
- **Dubai çikolatası:** fırında satılan ve absürt pahalı bir maç içi atıştırmalık.
- **Martı hırsızlığı:** martılar ekmeğini çalar. Başarım: *"Martıya yenildin."*
- **NPC esprisi:** Mim NPC'si oyuncuları taklit eder. Bir emote'un adı *"NPC modu"*: yerinde sallanıp aynı cümleyi tekrarlar.
- **"Clanker":** fener merceğindeki robot kafasına NPC'lerin taktığı lakap.
- **6–7 hareketi:** genel bir el hareketi emote'u.
- **Brainrot tarzı köy yaratıkları:** madenin karanlığında absürt isimli, özgün tasarımlı sesli yaratık heykelleri (*Martılino Simitino*, *Kalamarino Balonçino*…). Hiçbiri mevcut bir karakterin kopyası değil.
- **Türk mizahı:** dondurmacı trolü, çay ocağı dedikodusu, *"Abi naptın?"*, *"Bi dakika…"* ses satırları, "Kanka" unvanı.
- **Epilog unvanları:** "Köyün Rizz Ustası", "Sigma Balıkçı", "Delulu Soytarı", "Sessiz Sinema".

## 6. Ev hayatı
- Yatakta **uyuma** emote'u vardır. Gece uyuyan oyuncu şafak duyurusunu daha iyi duyar, ama savunmasızdır.
- Mutfakta **yemek pişirme** (çorba = can), şöminede ateş yakma (gece ışığı, ama dışarıdan görünür).
- Akvaryum, koleksiyon rafı, portreler. Ev editörü ileride gelecek, ama evdeki kupalar ve balıklar şimdiden görünür.
- **Misafir ağırlama:** kapı zili ve kapı tokmağı var. Pencereden bakıp kimin geldiğini görebilirsin.
