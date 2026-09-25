# Web / Tarayıcı Portu Fizibilitesi (KillGo, UE 5.8.3)

Tarih: 2026-09-25. Hazırlayan: araştırma ajanı (salt okunur; kod, level ve asset değişmedi, indirme yapılmadı).
Etiketler: **[WEB]** kaynaklı bilgi (link §9'da), **[MOTOR]** kurulu `UE_5.8` klasöründe doğrulandı, **[TAHMİN]** hesap
ya da tecrübeye dayalı tahmin, **[DOĞRULANMADI]** kaynak zayıf, karar vermeden önce kontrol edilmeli.

> **English TL;DR (CLAUDE.md: research in English).** UE5 has no official web export (HTML5 was dropped after 4.24).
> The only client-side route is SimplyStream / Wonder Interactive's WebGPU+WASM source fork of UE 5.8. It works for
> demos, but our listen-server P2P + EOS design cannot run in a browser: browsers cannot host, and the EOS SDK has no
> web build. So a web version needs dedicated servers, a WebSocket/WebTransport net driver and a non-EOS voice and lobby
> stack. Pixel Streaming works today but costs roughly $0.5 per player-hour, so a free game cannot afford it. Recommendation: ship a
> low-min-spec **Windows build first** (free Steam Playtest / itch.io), make it **Steam Deck (Proton) playable**,
> then opt into **GeForce NOW** for "play in the browser" at no infra cost. Revisit a real WebGPU port only after
> launch, as a separate time-boxed spike.

---

## 1. Kısa cevap

**"Sonra web'e taşıyabilir miyiz?"** Teknik olarak *kısmen* mümkün ama **bugünkü mimarimizle ucuz ve kolay değil.**
Tarayıcıdan oynatmanın en ucuz yolu, oyunu tarayıcıya derlemek değil. En ucuz yol, **Windows sürümünü bulut oyun
servisinden (GeForce NOW) açtırmak**. Bunun altyapı maliyeti bize sıfırdır.

**"Herkes oynayabilsin, takılmasın" isteği için doğru sıra:**
1. Düşük sistem gereksinimli **Windows** sürümü. Ücretsiz dağıtım: Steam Playtest ya da itch.io.
2. **Steam Deck**: aynı Windows sürümü Proton üzerinden çalışır.
3. **Tarayıcı**: GeForce NOW (Steam sürümüne bağlı, ücretsiz katman tarayıcıda çalışır).
4. **Mac / Linux**: sonra, talep olursa.
5. **Gerçek web portu (WebGPU)**: çıkıştan sonra, süresi sınırlı bir deneme işi olarak.

## 2. UE5 resmi olarak web'e çıkabiliyor mu?

- **Hayır.** Epic HTML5 desteğini UE 4.24'ten sonra motordan çıkardı. UE5'te yerleşik HTML5/WebGL/WebGPU hedefi yok. [WEB: spawnd, fpsio]
- Bizim kurulumda da yok **[MOTOR]**: `Engine/Platforms` altında sadece `Android, IOS, VisionOS, Windows` var.
  Binary'ler `Win64` ve `Mac`. Linux hedefi de kurulu değil.
- Motorda **deneysel** `WebSocketNetworking` eklentisi var (`Engine/Plugins/Experimental/WebSocketNetworking`,
  `WebSocketNetDriver`) **[MOTOR]**. Açıklamasındaki not: diğer bütün NetDriverDefinitions ve Steam auth gibi
  PacketHandler bileşenleri kapatılmalı. `PlatformAllowList` = Mac, Win64, Linux; tarayıcı istemcisi yok.
  Yani sadece sunucu tarafı işe yarar.

## 3. Topluluk ve ticari seçenekler

| Seçenek | Durum (2026) | Maliyet | Sınırlar ve riskler |
|---|---|---|---|
| **SimplyStream / Wonder Interactive** (WebGPU + WASM) | UE 5.8'in **kaynak fork'u**. Site "UE 4.27 → 5.8" diyor. Forum duyurusu 5.6/5.7 için (Ocak 2026), GamesBeat haberi 5.6 ve 5.8 için (Haziran 2026). Çok iş parçacıklı WASM, 64-bit WASM, Basis+ZSTD sıkıştırma, CDN'den asenkron asset akışı. İlk indirme "8 MB'a kadar düşebilir" iddiası var. [WEB] | İstemci tarafı WebGPU çizimi **ücretsiz**. Bulut GPU (kredi), çok oyunculu sunucular, Studio/Enterprise destek **ücretli**. Rakamlar yayında değil. [WEB] | Fork'a geçmek demek: launcher 5.8.3 yerine onların kaynak motorunu derlemek (build pipeline, Live Coding ve MCP iş akışı değişir). **Lumen Lite ve Nanite "geliştiriliyor"**; bizde ikisi de kapalı, bu iyi. Şirket tohum yatırımı topluyor, olgunluk riski var. Çok oyunculu ağ protokolü belgelenmemiş. [DOĞRULANMADI] |
| Eski UE4 HTML5 topluluk dalı (UnrealEngineHTML5) | UE4 için. WebGL2, tek iş parçacığı. [WEB] | Ücretsiz | UE5'e uygulanamaz, fiilen ölü. |
| **Pixel Streaming 2** (Epic, motorda var) | Resmi. Kurulumda `Plugins/Media/PixelStreaming2` var **[MOTOR]**. Oyun bulutta çalışır, tarayıcıya WebRTC video gider. | Oyuncu başına GPU; aşağıda §4. | Pahalı, gecikme ekler, ölçekleme işi bizde. |
| **GeForce NOW** (NVIDIA bulut oyun) | Geliştirici portalından Steam oyunları kataloğa alınabiliyor. Ücretsiz katman tarayıcıda çalışır: 1 saatlik oturum, reklamlı kuyruk, RTX 3050 sınıfı, 1080p/60. [WEB] | **Bize altyapı maliyeti yok.** | Önce Steam'de olmak gerekir. Katalog kabulü NVIDIA'nın kararı [DOĞRULANMADI: küçük indie kabul kriterleri]. Ücretsiz katmanda "Install-to-Play" yok; oyunun "Ready-to-Play" kataloğunda olması lazım. EOS ve anti-cheat uyumu test edilmeli. |

## 4. Pixel Streaming ekonomisi (sunucuda render, tarayıcıya video)

**Temel gerçek:** her oyuncu kendi UE istemcisini bulutta çalıştırır. 20 kişilik bir maç = **20 akış + 1 oyun
sunucusu**. Epic de "her eşzamanlı akış için bir UE örneği" diyor. Eski matchmaker 5.5'te kullanımdan kaldırıldı,
dinamik ölçekleme önerisi var. NVIDIA'da bir GPU'da 8. akıştan sonra yazılım kodlayıcıya düşülüyor. [WEB]

| Kalem | Rakam |
|---|---|
| AWS g4dn.xlarge (T4 GPU), us-east-1 on-demand | **$0.526/saat** [WEB]. Asya-Pasifik bölgesi ≥ $0.92/saat [WEB]. |
| Low-poly oyunda GPU başına akış | 720p'de 2 akış [TAHMİN] → oyuncu başına ~$0.26/saat GPU |
| Video çıkış trafiği | 1080p ~5–8 Mbps ≈ 2–3.5 GB/saat × ~$0.09/GB ≈ $0.2–0.3/saat [TAHMİN] |
| **Toplam (kendi AWS'imiz)** | **≈ $0.5 / oyuncu-saat** [TAHMİN] |
| Streampixel (yönetilen) | €99/ay (2 CCU dahil) + **€45/ay her ek CCU** [WEB] → 20 CCU ≈ €909/ay, 100 CCU ≈ €4.5k/ay |
| Arcware (yönetilen) | €0.08–0.15/dakika [WEB] → **€5–9 / oyuncu-saat**. 20 kişilik 20 dakikalık tek maç ≈ €35–60 |
| Eagle 3D Streaming | Core $29/ay, "10 eşzamanlı akışa kadar" [WEB; donanım dahil mi, DOĞRULANMADI] |

**Aylık kaba hesap (kendi AWS'imiz, ~$0.5/oyuncu-saat):** 1.000 oyuncu-saat ≈ $500. 10.000 oyuncu-saat ≈ $5.000.
Ortalama 30 CCU'yu 7/24 tutmak ≈ 30 × 730 × 0.5 ≈ **$11k/ay**. Ücretsiz ya da ucuz bir sosyal oyun bunu karşılayamaz.

**Gecikme:** hedef yerelde < 100 ms, internette < 150 ms uçtan uca. Pratikte ~80 ms tur süresi görülüyor [WEB].
Bu, ağ gecikmesinin **üstüne** eklenir. Birinci şahıs aim ve kovalamaca hissi bozulur, sesli sohbet de ayrı bir yol ister.

**Sonuç:** Pixel Streaming yalnızca **fuar ya da tanıtım demosu** için mantıklı (örneğin birkaç CCU, Streampixel
deneme sürümü). Asıl dağıtım yolu olamaz.

## 5. Tarayıcıda ağ kısıtları (bizim mimariyi en çok etkileyen kısım)

1. **Tarayıcı sunucu olamaz.** Gelen bağlantı dinleyemez, UDP soketi açamaz. HTML5 döneminden kalan kural da aynı:
   tarayıcı istemcisi oyuna *katılabilir* ama *host olamaz* [WEB: UnrealEngineHTML5 docs]. **Listen-server P2P
   modelimiz ve host migration tasarımımız tarayıcı oyuncusu için geçersiz.**
2. **Tarayıcıdaki taşıma yolları:** WebSocket (TCP, güvenilir, head-of-line blocking var), **WebRTC DataChannel**
   (P2P olabilir, güvenilmez mod var, ama signaling ve STUN/TURN ister; UE'de WebRTC NetDriver yok, Pixel Streaming
   WebRTC'yi sadece video için kullanır), **WebTransport** (HTTP/3 + QUIC, güvenilmez datagramlar; Safari 26.4 ile
   Mart 2026'da tüm büyük tarayıcılarda "Baseline" oldu) [WEB].
3. **HTTPS sayfası sadece `wss://` ya da WebTransport'a bağlanabilir.** Sunucunun geçerli TLS sertifikası ve alan
   adı olmalı. Oyuncunun ev bilgisayarındaki host bunu pratikte sağlayamaz [TAHMİN, tarayıcıların karışık içerik
   kuralı]. Bu yüzden web oyuncuları için **bizim işlettiğimiz dedicated sunucular** (ya da relay) gerekir.
4. **EOS tarayıcıda yok.** EOS SDK Windows, macOS, Linux, iOS, Android ve konsollar için var; WebAssembly/JS SDK'sı
   yok [WEB: EOS platform support]. Bizim kurulumdaki EOS kütüphaneleri de sadece Win64 **[MOTOR]**. Tarayıcı
   sürümünde **EOS lobi, P2P, relay, RTC sesli sohbet ve Connect login** yerine başka bir şey gerekir: kendi lobi
   servisimiz, WebRTC ses (örneğin LiveKit ya da benzeri) ve web auth.
5. **Karma oyun (Windows + tarayıcı aynı maçta):** Sunucunun iki net driver'ı birlikte çalıştırması gerekir
   (EOS/IP ve WebSocket). Motor eklentisi "diğer NetDriverDefinitions kapatılmalı" diyor **[MOTOR]**. Bu da
   özel motor ya da ağ işi demek. Yüksek risk.

**Maliyet:** 20 oyunculuk low-poly bir UE dedicated sunucu ≈ 1–2 vCPU. Maç başına ~$0.02–0.05/saat, küçük bir
filo ~$20–100/ay [TAHMİN]. Asıl maliyet para değil, **mühendislik**: dedicated sunucu build'i (kaynak motor ister,
launcher build dedicated sunucu hedefi derlemez [DOĞRULANMADI: 5.8'de de aynı mı]), sunucu yönetimi, yeni lobi ve
ses sistemi.

## 6. İndirme boyutu ve bellek

| Konu | Sınır |
|---|---|
| itch.io HTML5 | Açılmış toplam ≤ **500 MB**, tek dosya ≤ **200 MB**, ≤ **1.000 dosya** [WEB] |
| CrazyGames | İlk indirme < **50 MB** [WEB] |
| Poki | İlk indirme < **8 MB** hedefi [WEB] |
| WASM32 bellek | En fazla 4 GB. **Memory64** Chrome 133 ve Firefox 134'te var, Safari'de yok. Tarayıcı üst sınırı 16 GB. Memory64'ün performans bedeli olabilir [WEB] |
| WebGPU | Chrome/Edge (Win/Mac/ChromeOS), Chrome Android 12+ (Qualcomm/ARM GPU), Firefox 141 (Windows), Firefox 145 (macOS ARM), Safari 26 (macOS/iOS). **Linux, Intel Mac ve Firefox Android henüz yok** [WEB] |
| Bizim içerik | `Content/` = **497 MB, cook edilmemiş hâli** **[MOTOR ölçümü]**. Paketli Windows build (motor dahil) ~1–2 GB [TAHMİN]. Web için sıkıştırma ve akışla ilk yükleme 50–150 MB bandına inebilir [TAHMİN, SimplyStream iddiasına göre] |
| Mobil tarayıcı | Sekme başına bellek genelde 1–2 GB civarında kesilir [TAHMİN]. 20 oyunculu 3D bir sahne telefonda risklidir |

## 7. Benzer oyunlar ne yaptı?

| Oyun | Motor / platform | Ders |
|---|---|---|
| **LOCKDOWN Protocol** (en yakın rakip, 16 oyuncu, FPS sosyal çıkarım) | UE5, **sadece Windows**, €9.99. Minimum: i3-4150 / Ryzen 3 1200, **GTX 1050**, DX11, 2 GB disk [WEB: Steam] | Web yok, Mac yok. Düşük minimum sistem + Windows ile büyüdü. |
| **Town of Salem** | Flash tarayıcı oyunu olarak doğdu. Steam (Win/Mac) 2014, mobil 2018. Flash sürümü 2020'de öldü. **ToS2 sadece Steam** [WEB] | Tarayıcıdan başlayanlar bile 3D devam oyununda Steam'e geçti. |
| **Goose Goose Duck** | Ücretsiz, Windows/Mac/iOS/Android arası crossplay. Steam zirvesi **702.845 CCU** (Ocak 2023), 67M kayıtlı kullanıcı [WEB] | "Herkes oynasın" = **ücretsiz + düşük sistem + mobil**. Web şart değil. |
| Among Us | Unity. PC, mobil ve konsol; resmi tarayıcı sürümü yok [genel bilgi, kaynak aranmadı] | Web şart değil. |

**Ortak desen:** Bu türde "herkes oynasın" = **ücretsiz ya da ucuz + düşük sistem gereksinimi + arkadaşını kolay
davet**. Tarayıcı sürümü yapan başarılı 3D FPS sosyal çıkarım oyunu bulamadım [arama kapsamı sınırlı].

## 8. Seçenekler: çaba, maliyet ve risk sırası

| # | Seçenek | Çaba | Para | Risk | Kime ulaşır |
|---|---|---|---|---|---|
| **1** | **Düşük sistem gereksinimli Windows build** + Steam Playtest (ücretsiz) / itch.io (ücretsiz) | Düşük–orta | $0 (Steam sayfası için $100 Steam Direct, $1k gelirden sonra geri alınır [WEB]) | Düşük | Windows PC'lerin büyük çoğunluğu |
| **2** | **Steam Deck (Proton ile Windows build)** | Düşük | $0 | Orta: EOS ses ve Proton [WEB: Redpoint changelog'da Proton'da ses çökmesi düzeltmesi var, 3. parti] | Deck ve Linux Steam kullanıcıları |
| **3** | **GeForce NOW kataloğu** (tarayıcı, Chromebook, düşük PC, telefon) | Düşük | $0 | Orta: kabul NVIDIA'da, önce Steam gerekir | "Tarayıcıdan oynayayım" diyenler |
| 4 | Mac native | Orta–yüksek | Mac donanımı + Apple Developer $99/yıl | Orta: EOS Mac kütüphaneleri bizde yok **[MOTOR]**, Mac'te build/imza gerekir | Mac kullanıcıları |
| 5 | Linux native | Orta | $0 | Orta: cross-compile toolchain kurulmalı, Linux hedefi kurulu değil **[MOTOR]**. Proton çoğu zaman yeter | Küçük kitle |
| 6 | Pixel Streaming (sadece demo) | Orta | €99+/ay ya da ~$0.5/oyuncu-saat | Düşük teknik, **yüksek maliyet** | Fuar veya landing page demosu |
| 7 | **SimplyStream WebGPU portu** | **Yüksek**: motor fork'u, dedicated sunucu, WebSocket/WebTransport net driver, EOS yerine lobi, ses ve auth | Araç ücretsiz; sunucu ve destek ücretli | **Yüksek**: olgunluk, fork takibi, karma crossplay | Chrome/Edge/Safari masaüstü. Mobil tarayıcı zayıf |
| 8 | Ayrı web-native istemci (Three.js / Godot-web ile ikinci oyun) | Çok yüksek | — | Çok yüksek | Önerilmez |

### Öneri (net)
- **Şimdi:** Seçenek **1 + 2**. Tarayıcı isteği için ilk adım **3** (Steam sayfası açıldıktan sonra GeForce NOW
  geliştirici portalına başvuru).
- **Gerçek web portu (7):** Çıkış ve oyuncu doğrulamasından **sonra**. 1–2 haftalık, süresi sınırlı bir spike:
  SimplyStream fork'unda boş bir haritayla "tarayıcı istemcisi + Windows dedicated sunucu, 2 oyuncu, WebSocket"
  denenir. Başarı ölçütleri: ilk yükleme < 100 MB, 60 fps, bağlantı kurulur. Başarısızsa bırakılır.
- **Mimari kararı değiştirme:** Web için bugünden listen-server/EOS tasarımını bozmaya gerek yok. Ama yeni ağ kodu
  **net driver'dan bağımsız** yazılmaya devam etsin (zaten Iris-ready kuralı var). Oyuncu kimliği EOS PUID'e
  sıkı bağlanmasın, bir soyutlama katmanı arkasında kalsın. Böylece ileride dedicated ya da web modu açık kalır.

### "Takılmasın, herkes oynasın" için somut düşük-gereksinim önerileri (öneri, onay gerekir)
1. **DX11 / SM5 geri dönüş yolu:** `Config/DefaultEngine.ini` şu an **SM5'i kaldırıyor, sadece DX12 SM6**
   derliyor **[MOTOR]**. Eski ya da entegre GPU'lar (LOCKDOWN Protocol minimumu GTX 1050 + DX11) için SM5 ya da DX11
   seçeneği geri eklenebilir. Motorda D3D11RHI ve VulkanRHI mevcut **[MOTOR]**. Karşılığında cook süresi ve boyut
   artar.
2. **PSO precaching + bundled PSO cache:** UE5'teki "shader stutter"ın ana çaresi. `stat PSOPrecache` ile ölçülür,
   hitch eşiği `r.PSO.RuntimeCreationHitchThreshold` (varsayılan 20 ms). Global grafik shader'ları precache kapsamı
   dışında kalabilir, bu yüzden yükleme ekranında ısıtma yapılmalı [WEB: Epic PSO docs].
3. **Hedef min. sistem:** GTX 1050 / RX 560 / Iris Xe, 8 GB RAM, 720p–900p "Düşük" kademede 60 fps. Steam
   anketinde en yaygın RAM 16 GB (%41.2), en yaygın GPU RTX 3060 [WEB, Ağustos 2026]. Hedefimiz bunun epey altında
   kalmalı.
4. **Yeni dekor asset'leri** (kullanıcı izin verdi, ama indirme listesi ayrıca onaylanmalı): ISM/HISM, palet atlası,
   LOD'lar, `06_Art_Direction §8` bütçeleri (≤ 1500 draw call PC). Bu kurallar hem PC'de takılmayı önler hem de
   ileride web portunu mümkün kılar. Web için en büyük düşman fazla benzersiz texture ve materyal.
5. **Steam Deck hedefi:** 800p, 30–40 fps. "Düşük" kademe Deck'te ölçülsün (ölçümü kullanıcı yapar).

## 9. Kaynaklar
- SimplyStream ana sayfa: https://simplystream.com/
- GamesBeat, SimplyStream UE5 WebGPU (Haziran 2026): https://gamesbeat.com/simplystream-unlocks-web-compatibility-for-unreal-engine-5-enabling-high-quality-webgpu-gaming-in-the-browser/
- Epic forum, WebGPU for UE 5.6/5.7 (Ocak 2026): https://forums.unrealengine.com/t/webgpu-for-unreal-engine-5-5-6-and-5-7-support/2693960
- Epic forum, WebGPU ilgisi: https://forums.unrealengine.com/t/anyone-interested-in-webgpu-support-for-unreal-engine-5/1910658
- GitNation, Unreal in WASM/WebGPU: https://gitnation.com/contents/unreal-engine-in-webassemblywebgpu
- Spawnd, Web porting in Unreal: https://docs.spawnd.gg/docs/web-porting-in-unreal-wip
- FPS.io, UE ve tarayıcı (2025): https://fpsio.com/2025/08/17/does-unreal-engine-have-plans-for-browser-gaming/
- UE4 HTML5 dokümanları (host olamaz kuralı): https://github.com/UnrealEngineHTML5/Documentation/blob/master/Platforms/HTML5/HowTo/README.2.advanced.UE4.HTML5.md
- HTML5Networking (WebSocketNetDriver kökeni): https://github.com/ankitkk/HTML5Networking
- Epic, WebSocketNetworking eklentisi: https://dev.epicgames.com/documentation/unreal-engine/API/PluginIndex/WebSocketNetworking
- Pixel Streaming 2 (5.8): https://dev.epicgames.com/documentation/en-us/unreal-engine/pixel-streaming-2-overview-in-unreal-engine
- Pixel Streaming barındırma ve ağ (5.8): https://dev.epicgames.com/documentation/en-us/unreal-engine/hosting-and-networking-guide-for-pixel-streaming-in-unreal-engine
- Azure, Pixel Streaming at scale: https://learn.microsoft.com/en-us/gaming/azure/reference-architectures/unreal-pixel-streaming-at-scale
- Streampixel fiyat: https://www.streampixel.io/pricing
- Arcware fiyat: https://www.arcware.com/pricing
- Eagle 3D fiyat: https://www.eagle3dstreaming.com/pricing
- AWS g4dn.xlarge fiyat: https://instances.vantage.sh/aws/ec2/g4dn.xlarge
- AWS Pixel Streaming notları: https://thegabmeister.com/p/unreal-pixel-stream-aws/
- Pixel Streaming gecikme: https://eagle3dstreaming.com/blog/performance-optimization-for-pixel-streaming-the-complete-guide
- EOS platform desteği: https://dev.epicgames.com/docs/epic-online-services/platform-support
- EOS sesli sohbet (5.8): https://dev.epicgames.com/documentation/en-us/unreal-engine/voice-chat-with-epic-online-services
- Redpoint EOS changelog (Proton ses düzeltmesi): https://docs.redpoint.games/docs/changelog/
- WebTransport Baseline (Nisan 2026): https://webrtc.ventures/2026/04/webtransport-is-now-baseline-what-it-means-for-real-time-media/
- WebKit Safari 26.4: https://webkit.org/blog/17862/webkit-features-for-safari-26-4/
- WebGPU destek durumu: https://github.com/gpuweb/gpuweb/wiki/Implementation-Status , https://web.dev/blog/webgpu-supported-major-browsers
- WASM Memory64: https://spidermonkey.dev/blog/2025/01/15/is-memory64-actually-worth-using.html , https://platform.uno/blog/the-state-of-webassembly-2025-2026/
- itch.io HTML5 limitleri: https://itch.io/docs/creators/html5
- Poki gereksinimleri: https://sdk.poki.com/new-requirements
- CrazyGames rehberi: https://app.cinevva.com/guides/publish-game-crazygames
- GeForce NOW geliştirici portalı: https://developer.geforcenow.com/
- GeForce NOW ücretsiz katman: https://tech-insider.org/geforce-now-free-tier-steam-setup-2026/
- LOCKDOWN Protocol Steam: https://store.steampowered.com/app/2780980/LOCKDOWN_Protocol/
- Town of Salem: https://en.wikipedia.org/wiki/Town_of_Salem
- Goose Goose Duck 700k CCU: https://newsletter.gamediscover.co/p/how-goose-goose-duck-hit-700k-ccu , https://steamdb.info/app/1568590/charts/
- Steam Direct ücreti: https://partner.steamgames.com/doc/gettingstarted/appfee
- Steam Deck quick start (5.8): https://dev.epicgames.com/documentation/en-us/unreal-engine/steam-deck-quick-start-in-unreal-engine
- Steam donanım anketi (Ağustos 2026): https://bottleneckpc.com/blog/steam-hardware-survey-16gb-vram
- PSO precaching (5.8): https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine

## 10. Backlog'a önerilenler ("Proposed (needs approval)" için; bu ajan Backlog'u düzenlemedi)
1. Min-spec sprint: SM5/DX11 geri dönüşü, "Düşük" kademe, PSO bundled cache, GTX 1050 sınıfında ölçüm.
2. Steam Deck (Proton) doğrulama sprinti: EOS ses, gamepad, 800p arayüz ölçeği.
3. Steam sayfası ve Playtest açıldıktan sonra GeForce NOW başvurusu.
4. (Çıkış sonrası) SimplyStream WebGPU spike'ı: 1–2 hafta, başarı ölçütleri §8'de.
