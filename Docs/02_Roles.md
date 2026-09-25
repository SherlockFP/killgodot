# KILL GODOT — Roller

**51 rol:** 22 Kasaba, 21 Sabırsız ve 8 Nötr. Oyunda hepsi `URoleDefinition` data asset'i olarak tanımlanır (bkz. `05_Tech_Architecture.md`).

**Sütunlar:**
- **Uyg.:** yeteneğin nasıl kullanıldığı. *D* = Gece Defteri'nden seçilir, şafakta çözülür. *F* = fiziksel, oraya gitmek ya da eylemi yapmak gerekir. *P* = pasif.
- **Şerif:** Şerif'in sorgusunda çıkan sonuç.
- **Güç:** Güç Skoru. Kasaba için +, Sabırsız için −, Nötr için tehdit değeri.
- **Grup:** Dedektif grubu (aşağıda).

## KASABA — Bekleyenler (22)

### Araştırma
| # | Rol (TR / EN) | Yetenek | Uyg. | Şerif | Güç | Grup |
|---|---|---|---|---|---|---|
| 1 | **Şerif** / Sheriff | Bir kişiyi sorgular: *Şüpheli / Şüphesiz*. | D | Şüphesiz | +6 | 1 |
| 2 | **Dedektif** / Investigator | Hedefin 3 olası rolünü öğrenir (bkz. gruplar). | D | Şüphesiz | +6 | 16 |
| 3 | **Gözcü** / Lookout | Çatı penceresinden ya da çan kulesinden dürbünle bir evi izler, o geceki ziyaretçileri görür. Maskeliyse "maskeli biri" ve ayak izi yönü görünür. | F | Şüphesiz | +5 | 5 |
| 4 | **İz Sürücü** / Tracker | Seçtiği kişinin ayak izlerini 90 sn boyunca parlak görür, şafakta o kişinin kimi ziyaret ettiğini öğrenir. | D+F | Şüphesiz | +5 | 5 |
| 5 | **Adli Tabip** / Coroner | Cesedi inceler: ölüm saati, silah sınıfı (*"yassı, metalik, muhtemelen tava"*), katilin tarafı. 3 incelemede bir kez katilin **mesleği**. | F | Şüphesiz | +5 | 10 |
| 6 | **Kulak Misafiri** / Eavesdropper | Sabırsız telsizini **pitch'i bozulmuş** duyar, kimin konuştuğu anlaşılmaz. Sabah Sabırsızların kimi ziyaret ettiğini öğrenir. | P | Şüphesiz | +5 | 15 |

### Ruhani
| # | Rol | Yetenek | Uyg. | Şerif | Güç | Grup |
|---|---|---|---|---|---|---|
| 7 | **Medyum** / Medium | Gece hayaletlerle **sesli** konuşur. Maçta 1 kez **Seans Çanı** çalar: seçilen bir hayalet 15 sn boyunca toplantıda herkese duyulur. | P+D | Şüphesiz | +4 | 7 |
| 8 | **Kâhin** / Seer | İki kişi seçer: *aynı taraf mı, farklı mı?* | D | Şüphesiz | +6 | 1 |

### Koruma
| # | Rol | Yetenek | Uyg. | Şerif | Güç | Grup |
|---|---|---|---|---|---|---|
| 9 | **Doktor** / Doctor | Bir kişiyi korur: o gece öldürülürse baygın düşer, şafakta 30 canla kalkar. Kendine 1 kez. Gündüz bandajla 40 can verir. | D+F | Şüphesiz | +6 | 2 |
| 10 | **Koruma** / Bodyguard | Bir kişiyi korur. Saldırgan gelirse saldırganı da öldürür, kendisi de ölür. 1 kez zırh. | D | Şüphesiz | +6 | 3 |
| 11 | **Bekçi** / Watchman | Gece fenerle **devriye** gezer. Sokağa çıkma yasağında yakaladığını kelepçeler (2 sn güreş QTE'si). Kelepçelenen hapse gider, eylemi iptal olur, **maskesi düşer**. 2 kelepçe hakkı. | F | Şüphesiz | +7 | 13 |
| 12 | **Rahip** / Priest | Bir evi kutsar: izinsiz giren ilk kişi kilise çanını çaldırır ve mesleği duyurulur. Vampir, Kurtadam ve Boğulmuş kutsal eve giremez. | D | Şüphesiz | +6 | 9 |
| 13 | **Çilingir** / Locksmith | Bir kapıyı o gece **kırılmaz** yapar. Gündüz her kilidi açar ve anahtar kopyalar (bunu yaparken görülmek şüpheli). | D+F | Şüphesiz | +5 | 8 |

### Öldürme
| # | Rol | Yetenek | Uyg. | Şerif | Güç | Grup |
|---|---|---|---|---|---|---|
| 14 | **İnfazcı** / Vigilante | Gizli çakmaklı tabanca, toplam 3 mermi. Gündüz fiziksel ya da gece Defter'den atış yapar. Masum vurursa ertesi gece **vicdan azabından** ölür. | F+D | Şüphesiz | +6 | 4 |
| 15 | **Emekli Asker** / Veteran | 3 kez **Nöbet** tutar: o gece evine gelen herkesi vurur, dost ya da düşman. | D | Şüphesiz | +6 | 4 |
| 16 | **Avcı** / Hunter | 3 **ayı kapanı** kurar. Kapan basanı 8 sn tutar, kurban çığlık atar. Canavar formundaki Kurtadamı öldürür. Dikkatli bakan kapanı fark edebilir. | F | Şüphesiz | +5 | 13 |

### Destek / Güç (⭐ = maçta tek)
| # | Rol | Yetenek | Uyg. | Şerif | Güç | Grup |
|---|---|---|---|---|---|---|
| 17 | ⭐ **Muhtar** / Mayor | Toplantıda kendini açıklar, oyu **3** sayılır. Açıklandıktan sonra Doktor onu koruyamaz. | F | Şüphesiz | +8 | 11 |
| 18 | ⭐ **Gardiyan** / Jailor | Gündüz sonunda birini seçer, o kişi gece **hücrede** uyanır. Özel ses kanalında konuşurlar, Gardiyan'ın sesi filtrelenir. 3 infaz hakkı var, masum infaz ederse hakkını kaybeder. | D | Şüphesiz | +9 | 12 |
| 19 | **Meyhaneci** / Tavern Keeper | Birini **sarhoş eder**: o gecenin eylemi iptal olur, ertesi sabah sendeleyerek yürür (herkes görür). Seri Katil'i sarhoş ederse SK onu ziyaret eder. | D | Şüphesiz | +5 | 6 |
| 20 | ⭐ **Zangoç** / Bell Ringer | Maçta 2 kez **Gece Çanı** çalar: 10 sn boyunca ev dışındaki herkesin silueti duvarların arkasından görünür. Zangocun yeri de belli olur. | D | Şüphesiz | +6 | 14 |
| 21 | **Çoban** / Shepherd | Bir **köpeği** var. Köpek sevilebilir ve kozmetik skin alabilir. "Koklat" komutuyla kılıklı ya da dönüşmüş birine **havlar** (Casus, Suret, insan formundaki Kurtadam). "İz sür" komutuyla olay yerindeki son saldırganın izini 30 m takip eder. Gece kapının önünde uyur, yaklaşan olursa havlar. Köpek öldürülebilir, ölürse **bütün kasaba üzülür**. | F | Şüphesiz | +6 | 17 |
| 22 | **Barut Ustası** / Powder Master | 20 m içindeki barutu, bombaları, Kundakçının yağını ve kaçak silahları HUD'da **sezer**. Bombayı 1 sn'de doğru kabloyla söker. Maçta 1 kez gece **fişek** atar: bir bölge 15 sn aydınlanır. | P+F | Şüphesiz | +5 | 17 |

---

## SABIRSIZLAR — The Impatient (21)

### Saat Kırıcılar (takım, "mafya")
- Takım üyeleri birbirini bilir. Gece **Saat Kırıcı telsizi** açıktır.
- **Kurban Hakkı:** takımın gecede 1 öldürme hakkı var, N ≥ 14 ise 2. Hakkı sadece Saat Ustası, Tetikçi ve Casus kullanabilir. Bombacının bombası biri öldürürse de hak harcanır.
- Gündüz öldürmek o gecenin hakkından düşer.
- Destek rolleri de gündüz kavgasında normal dövüşebilir.

| # | Rol | Yetenek | Uyg. | Şerif | Güç | Grup |
|---|---|---|---|---|---|---|
| 1 | ⭐ **Saat Ustası** / Clockmaster | Lider. Gece 1 zırh. Kurban hakkını kendisi kullanır ya da Tetikçi'ye **emir** verir (emir hedefi Tetikçi'nin ekranında işaretlenir). | F+D | **Şüphesiz** | −9 | 11 |
| 2 | **Tetikçi** / Enforcer | Kurban hakkını kullanan kas. Saat Ustası ölürse lider olur. | F | Şüpheli | −7 | 3 |
| 3 | **Muhbir** / Informant | Hedefin **tam rolünü** öğrenir. | D | Şüpheli | −6 | 1 |
| 4 | **İftiracı** / Framer | Hedefi o gece Şerif'e şüpheli gösterir. Fiziksel olarak da birinin evine **sahte delil** bırakabilir: kanlı bez, maske parçası. | D+F | Şüpheli | −5 | 14 |
| 5 | **Temizlikçi** / Cleaner | 3 kez cesedin rolünü ve vasiyetini gizler. Cesedi sürükleyip kuyuya veya denize atabilir, sabah duyurusu "**Kayıp**" olur. | D+F | Şüpheli | −5 | 10 |
| 6 | **Şantajcı** / Blackmailer | Hedef ertesi gün **sesli ve yazılı konuşamaz**, sadece emote kullanır. Mahkemede savunma yapamaz. | D | Şüpheli | −6 | 12 |
| 7 | **Sahtekar** / Forger | 2 kez kurbanın vasiyetini değiştirir. Sahte mektup yazıp posta kutusuna bırakabilir. | D+F | Şüpheli | −4 | 14 |
| 8 | **Casus** / Spy *(TF2 tarzı)* | Arkadan **suikast** yaptığı kişinin kılığına **anında** girer: görünüş, isim etiketi, meslek aleti, ev anahtarı. Kurban hakkını kullanabilir (SU ve TT ile birlikte 3. rol). Ayrıca günde 1 kez, 90 sn boyunca canlı bir oyuncunun kılığına girebilir (Kılık Çantası). **Ceset olduğu yerde kalır**, Casus saklamazsa bulunur. Ayrıntılar aşağıda. | F | Şüpheli | −7 | 15 |
| 9 | **Baştan Çıkarıcı** / Charmer | Birinin gece eylemini iptal eder (Meyhanecinin kötü ikizi). | D | Şüpheli | −5 | 6 |
| 10 | **Sabotajcı** / Saboteur | Sabotaj bekleme süreleri yarıya iner. Özel sabotajları: köprü kırma, kapı sıkıştırma, feneri söndürme. | F | Şüpheli | −5 | 8 |
| 11 | **Kaçakçı** / Smuggler | Gizli tünel haritasını bilir, maymuncuğu vardır. Maçta 1 kez gizli zuladan takıma **tabanca** çıkarır. | F | Şüpheli | −5 | 5 |
| 12 | **Pusucu** / Ambusher | Bir noktaya pusu kurar, oraya ilk gelen ölür (Doktor ve Koruma dahil). Pusu kurarken görülürse ifşa olur. | D+F | Şüpheli | −6 | 3 |
| 13 | **Bombacı** / Bomber | **Saatli bomba** kurar (Saat Kırıcı temasına uygun). Kurması 3 sn sürer ve görünür. Süreyi 20–60 sn arası kendisi ayarlar. **Tik-tak sesi** 8 m'den duyulur, fitil parlar. Patlama 5 m yarıçaplıdır: öldürür, kapıları uçurur, kırılabilirleri parçalar, ragdoll ve fizik kaosu yaratır. Aynı anda 1 aktif bomba olabilir. Maçta 2 bomba hakkı var (N ≥ 12 ise 3). Bomba biri ölürse Kurban Hakkı harcar. Ayrıntılar aşağıda. | F | Şüpheli | −6 | 17 |

### Yalnız katiller ve diğer tehditler (kendi kazanma koşulları var)
| # | Rol | Yetenek | Uyg. | Şerif | Güç | Grup |
|---|---|---|---|---|---|---|
| 14 | **Seri Katil** / Serial Killer | Her gece 1 fiziksel öldürme. Gece zırhlıdır. Onu sarhoş eden ya da engelleyeni öldürür. | F | Şüpheli | −8 | 10 |
| 15 | **Kurtadam** / Werewolf | Dolunayda (2. ve 4. gece ya da Dolunay olayında) canavara dönüşür: çok hızlı, kapıyı tek vuruşta kırar, evdeki herkesi parçalar. İnsan formunda Şerif'e şüphesiz görünür. | F | Şüphesiz* | −8 | 9 |
| 16 | **Kundakçı** / Arsonist | Kapılara ve evlere **yağ döker** (koku ve leke izi kalır). Bir gece hepsini ateşler, yangın yayılır. Kovalarla söndürülebilir. | F | Şüpheli | −7 | 13 |
| 17 | **Zehirci** / Poisoner | Yemeğe, içeceğe ya da kuyuya zehir katar. Zehirlenen ertesi gece ölür, arada **öksürür**. Doktor panzehir verebilir. | F | Şüpheli | −6 | 2 |
| 18 | **Vampir** / Vampire | 2 gecede bir birini **dönüştürür**, takım büyür. Avcı ve Rahip vampire karşı güçlüdür. | F | Şüpheli | −7 | 9 |
| 19 | **Veba Taşıyıcı** / Plaguebearer | 5 sn yakın durarak bulaştırır. Herkes enfekte olunca **Kıyamet**'e dönüşür: yenilmez olur, her gece bir ev yok eder. | F | Şüphesiz→Şüpheli | −7 | 2 |
| 20 | **Boğulmuş** / The Drowned | Sudan çıkan katil. Su tünellerinde hızlı gider, suya yakın kurbanı denize çeker (ceset kaybolur). Islak ayak izi bırakır. | F | Şüpheli | −7 | 7 |
| 21 | **Suret** / Doppelgänger | Öldürdüğü kişinin kimliğini **kalıcı olarak** alır: görünüş, isim, ev, meslek. Casustan farkı: kurbanın cesedi **yok olur** (duyuru "kayıp" der), kılık ölene kadar sürer, yalnız çalışır. **Sesi değişmez.** Casus ile aynı maçta çıkmaz. | F | Şüpheli | −8 | 15 |

### Casus'u yakalamanın yolları (TF2 dengesi)
Casus kılığa girdiğinde "gerçek" kişi ortada dolaşıyormuş gibi görünür. Kasabanın bunu anlaması için birçok yol vardır:
1. **Cesedi bulmak.** *"Ahmet'in cesedi mahzende ama Ahmet meydanda yürüyor!"* Bu en güçlü delildir. Bu yüzden Casus cesedi saklamak zorundadır (sürükle, denize at, Temizlikçi'ye bırak) ve bu da ona zaman kaybettirir.
2. **Ses.** Kılık görünüşü kopyalar ama **sesi kopyalamaz**. Arkadaş grubunda en eğlenceli ifşa budur.
3. **Ayak izleri.** Her ayakkabı kozmetiğinin kendine özgü bir taban deseni vardır. Kılık görünüşü değiştirir, **ayak izi yine Casus'un gerçek ayakkabısıdır**.
4. **Köpek.** Çoban'ın köpeği kılıklı birine havlar (TF2'deki "spy-check" gibi).
5. **Kutsal su.** Kilisedeki kutsal su kılığı düşürür. Rahip suyu şişeye doldurup birine serpebilir (günde 1).
6. **Kılık bozulur:** Casus saldırınca, 25'ten fazla hasar alınca ya da akşam çanı çalınca (toplantıya kurbanın kılığında giremez).
7. **Bilgi açığı.** Casus kurbanın rolünü, notlarını ve dün kimle ne konuştuğunu bilmez. Arkadaşların sorduğu "bizim aramızdaki soru" onu yakalar.
8. **Ev anahtarı.** Kurbanın evine girip çıkabilir ama bu, komşuların gözünde tuhaf saatlerde görülme riskidir.

### Bombacı'ya karşı oyun
- Tik-tak sesi ve fitilin ışığı bombayı ele verir. **Herkes** 4 sn basılı tutarak bomba söküp etkisiz hale getirebilir. Kırmızı ya da mavi kablo seçilir ve %50 şanstır, yanlış kablo 3 sn içinde patlatır. **Barut Ustası** her zaman doğru kabloyu bilir ve 1 sn'de söker.
- Bomba taşınabilir. Denize atılırsa söner. Kalabalıktan uzağa fırlatmak da bir kahramanlık anıdır.
- Patlama sonrası iz: yanık izi ve saat çarkı parçaları. Adli Tabip bunları "saatli mekanizma" diye tanır.

---

## NÖTRLER — Gezginler (8)
| # | Rol | Hedef / Yetenek | Şerif | Tehdit | Grup |
|---|---|---|---|---|---|
| 1 | **Soytarı** / Fool (Jester) | **Toplantıda asılırsa kazanır.** Başka yolla ölürse kaybeder. Asıldıktan sonra suçlu oyu veren birini **perili ederek** öldürür. Deli gibi davranmak işidir. | Şüphesiz | 3 | 16 |
| 2 | **Cellat** / Executioner | Belirli bir kasabalıyı astırmalı. Hedef başka yolla ölürse Soytarı'ya dönüşür. | Şüphesiz | 4 | 12 |
| 3 | **Hayatta Kalan** / Survivor | Maç sonuna kadar yaşamalı. 4 yeleği var (gece zırhı). | Şüphesiz | 0 | 16 |
| 4 | **Hafızasız** / Amnesiac | Bir ölünün rolünü hatırlar ve o rol olur (1 kez). | Şüphesiz | 2 | 7 |
| 5 | **Cadı** / Witch | Birinin gece eylemini başka hedefe yönlendirir. Sabırsızlarla birlikte kazanır. | Şüpheli | 5 | 6 |
| 6 | **Pozzo** / Pozzo | 1. gece gizlice bir **Lucky** (uşak) seçer. İkisi de sona kadar yaşarsa Pozzo kazanır, uşak da kendi tarafıyla kazanabilir. Gündüz 40 m'den fazla ayrılırlarsa "ip gerilir" ve ikisi de yavaşlar. *(Beckett selamı)* | Şüphesiz | 1 | 11 |
| 7 | **Koleksiyoncu** / Collector | Belirli 5 eşyayı, bazıları başkalarının evinde olmak üzere, fiziksel olarak **çalıp** gizli sandığına götürmeli. | Şüphesiz | 2 | 8 |
| 8 | **Korsan** / Pirate | Gece 2 kez baskın yapar: hedefle **Kılıç / Tabanca / Tüfek** düellosu (taş-kağıt-makas). Kazanırsa hedef ölür. 2 zaferde kazanır ve gemisiyle sise yelken açar (sinematik). | Şüpheli | 4 | 4 |

**Özel durum, Hortlak:** rol değildir, bir durumdur. Araf Düellosu'nu kazanan ölü mezardan kimliği gizli bir zombi olarak döner (bkz. `01_GDD_Core.md §11.1`). Asıl tarafını korur ama yeteneği yoktur.

**Mod özel:** **Godot** (*Kill Godot* modu). Kasabalılardan biri gizlice Godot'dur ve bunu sadece Muhtar ile Kâhin bilir. 2 zırhı ve 1 kez "Kutsal Işık" yeteneği vardır (15 m'deki maskeleri düşürür). Godot ölürse Sabırsızlar kazanır.

---

## Dedektif grupları
Her grupta 3 rol var, Dedektif hedefin hangi grupta olduğunu öğrenir.

| Grup | Tema | Roller |
|---|---|---|
| 1 | "Gizli şeyler bilir" | Şerif, Muhbir, Kâhin |
| 2 | "İlaç ve zehir kokuyor" | Doktor, Zehirci, Veba Taşıyıcı |
| 3 | "Şiddete yatkın" | Koruma, Tetikçi, Pusucu |
| 4 | "Barut kokuyor" | İnfazcı, Emekli Asker, Korsan |
| 5 | "Geceleri gezer" | Gözcü, İz Sürücü, Kaçakçı |
| 6 | "Başkalarının aklını bulandırır" | Meyhaneci, Baştan Çıkarıcı, Cadı |
| 7 | "Ölülerle bağı var" | Medyum, Hafızasız, Boğulmuş |
| 8 | "Mekanizmalara meraklı" | Çilingir, Sabotajcı, Koleksiyoncu |
| 9 | "Gece yaratıklarını tanır" | Rahip, Vampir, Kurtadam |
| 10 | "Kan ve ceset" | Adli Tabip, Temizlikçi, Seri Katil |
| 11 | "Yönetmeyi sever" | Muhtar, Saat Ustası, Pozzo |
| 12 | "Birinin sesini keser" | Gardiyan, Şantajcı, Cellat |
| 13 | "Ateş, metal, tuzak" | Bekçi, Avcı, Kundakçı |
| 14 | "Haber yayar" | Zangoç, İftiracı, Sahtekar |
| 15 | "Başkalarını dinler, taklit eder" | Kulak Misafiri, Casus, Suret |
| 16 | "Tuhaf davranır" | Soytarı, Hayatta Kalan, Dedektif |
| 17 | "Şafaktan önce uyanır" | Çoban, Barut Ustası, Bombacı |

## Gece çözüm önceliği
Fiziksel eylemler (öldürme, kapan, kelepçe, yağ dökme) **gerçek zamanlı** olur. Defter eylemleri şafakta şu sırayla çözülür:
1. **Kontrol:** Gardiyan hapsi, Cadı, Meyhaneci, Baştan Çıkarıcı
2. **Koruma:** Doktor, Koruma, Çilingir, Rahip, Emekli Asker nöbeti, Hayatta Kalan yeleği
3. **Soruşturma:** Şerif, Dedektif, Kâhin, Muhbir, İz Sürücü, Gözcü (fiziksel ziyaretler de kaydedilir)
4. **Defter öldürmeleri:** İnfazcı, Pusucu, Korsan, Kundakçı ateşlemesi, zehir ölümleri
5. **Sonrası:** Temizlikçi, Sahtekar, Hafızasız, Vampir dönüşümü, Veba yayılımı

Fiziksel olarak öldürülen ama Doktor tarafından korunan kişi **baygın** düşer. Katil öldürdüğünü sanır, şafakta kurban ayağa kalkar.

## Rol listesi şablonları
Kısaltmalar:
- **TI** Kasaba Araştırma, **TP** Kasaba Koruma, **TK** Kasaba Öldürme, **TS** Kasaba Destek, **RT** Rastgele Kasaba
- **SU** Saat Ustası, **TT** Tetikçi, **RSK** Rastgele Saat Kırıcı, **NK** Yalnız Katil
- **NE** Nötr Kötü (Soytarı, Cellat, Cadı), **NB** Nötr İyi Huylu (Hayatta Kalan, Hafızasız, Pozzo), **NC** Nötr Kaos (Koleksiyoncu, Korsan)

| N | Liste |
|---|---|
| 6 | TI, TP, RT, RT · SU · NE |
| 8 | TI, TP, TS, RT, RT · SU, TT · NE |
| 10 | TI, TI, TP, TS, RT, RT · SU, TT, NK · NE |
| 12 | Gardiyan, TI, TI, TP, TK, RT, RT · SU, TT, NK · NE, NB/NC |
| 15 | Gardiyan, TI, TI, TP, TK, TS, RT, RT, RT · SU, TT, RSK, NK · NE, NB/NC |
| 20 | Gardiyan, TI, TI, TI, TP, TP, TK, TS, TS, RT, RT, RT · SU, TT, RSK, NK, NK · NE, NB, NC |

Ara değerlerde üretici `01_GDD_Core.md §3` tablosuna göre slot ekler. Sonra **Güç bütçesi** kontrol edilir: hedef bandın dışına çıkılırsa RT/RSK slotları yeniden çekilir (en fazla 20 deneme). Kısıtlar: Casus ile Suret aynı maçta çıkmaz, Bombacı sadece N ≥ 10'da çıkar, NK sayısı 2'yi geçmez, Vampir sadece N ≥ 12'de, Veba sadece N ≥ 14'te çıkar.

## Denge notları (ilk tahmin, telemetriyle ayarlanacak)
- Gerçek zamanlı dövüş ve kalabalık bonusu kasabayı güçlendirir. Maske, telsiz ve suikast Sabırsızları dengeler.
- Kasaba maçların **%60'ından fazlasını** kazanırsa Kurban Hakkı süresi ve suikast mesafesi ayarlanır. **%40'ın altına** düşerse Gece Defteri koruma rollerinin olasılığı artırılır.
- Her rol için ayrı telemetri tutulur: kazanma oranı, hayatta kalma süresi, yetenek kullanım oranı, "rol açığa çıkma" süresi.
