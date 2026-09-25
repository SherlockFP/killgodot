# Kill Godot — Epic Online Services (EOS) kurulumu

Bu belge, lobi tarayıcısını **internet üzerinden** çalıştırmak için **senin** yapman gereken adımları anlatır.
Hesap açma ve anahtar girme işlerini ajanlar yapmaz; bunlar senin hesabınla, senin elinle yapılır.

## Model: oyuncu sunucusu (P2P), Epic ücretsiz altyapısı üzerinden

- Epic **ücretsiz dedicated sunucu vermez**. Bizim modelimiz: maçı bir oyuncu kurar (listen server), diğerleri ona bağlanır.
- EOS'un ücretsiz verdikleri: **Lobbies** (maç ilanı ve listeleme), **P2P** bağlantı, **NAT traversal** ve gerekince
  **relay** (router ayarı gerekmez), **Connect** girişi. Bunlar için ücret yok.
- Kod tarafı hazır: `Source/KillGodot/Online/KGSessions.*` online subsystem'in session arayüzünü kullanır.
  Bugün **OnlineSubsystemNull** ile LAN'da çalışır; EOS profili açılınca aynı kod EOS Lobbies + NetDriverEOS ile çalışır.
- EOS ayarları ayrı bir katmanda durur: `Config/Custom/EOS/DefaultEngine.ini`. Bu dosya **yalnızca**
  `-CustomConfig=EOS` ile (ya da Target.cs'te `CustomConfig = "EOS"`) yüklenir. Kimlik bilgileri boşken oyun
  normal şekilde Null/LAN ile açılır, hiçbir şey bozulmaz.

## 1. Developer Portal'da ürün (bir kere, ~15 dk)

1. https://dev.epicgames.com/portal adresine kendi Epic hesabınla gir. İlk seferse bir **Organization** oluştur (ücretsiz).
2. **Create Product** → ad: `Kill Godot`.
3. Ürünün sol menüsünde **Product Settings** → **SDK Download & Credentials** (bazı sürümlerde "Product Settings > General").
   Şunları not et:
   - **Product ID**
   - **Sandbox ID** → geliştirme için **Dev** sandbox'ını kullan.
   - **Deployment ID** → Dev sandbox'ının deployment'ı.
4. **Product Settings → Clients → Add New Client**
   - Client policy olarak hazır **Peer2Peer** politikasını seç (ya da yeni politika oluşturup en az şunları aç:
     *Connect (login)*, *Lobbies*, *P2P*, *Sessions*, *Presence*).
   - Oluşunca **Client ID** ve **Client Secret** görünür. İkisini not et.
5. **Epic Account Services** (sol menü) → **Create Application**
   - **Permissions**: *Basic Profile*, *Online Presence*, *Friends*.
   - **Linked Clients**: 4. adımda oluşturduğun client'ı bağla.
   - **Brand Settings** ve inceleme yalnızca herkese açık yayın için gerekir. Dev sandbox'ında organizasyon
     üyeleri inceleme olmadan giriş yapabilir (test için yeterli).
6. **Şifreleme anahtarı** (64 karakter hex) üret. PowerShell:
   ```powershell
   -join ((1..32) | ForEach-Object { '{0:X2}' -f (Get-Random -Maximum 256) })
   ```

## 2. Değerleri projeye yaz

`Config/Custom/EOS/DefaultEngine.ini` içindeki satırı doldur (tırnakların içine):

```ini
+Artifacts=(ArtifactName="KillGodot",ClientId="<Client ID>",ClientSecret="<Client Secret>",ProductId="<Product ID>",SandboxId="<Dev Sandbox ID>",DeploymentId="<Dev Deployment ID>",ClientEncryptionKey="<64 hex>")
```

> Client Secret'ı herkese açık bir repoya koyma.

## 3. Test (iki oyuncu, aynı ya da farklı bilgisayar)

Geliştirmede en kolay giriş: EOS SDK'daki **Developer Authentication Tool**.

1. Portal'da **SDK Download & Credentials** sayfasından **EOS C SDK**'yı indir (bu indirme senin onayınla, senin elinle).
   Arşivdeki `Tools/EOS_DevAuthTool-*.zip` dosyasını aç ve aracı çalıştır.
2. Araçta port **6300** ile başlat, organizasyondaki iki ayrı Epic hesabıyla giriş yap, kimliklere ad ver:
   `Oyuncu1`, `Oyuncu2`.
3. Editor kapalıyken iki pencere aç:
   ```powershell
   $E = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
   & $E "D:\Kill Godot\KillGodot.uproject" -game -windowed -ResX=1280 -ResY=720 -CustomConfig=EOS -AUTH_TYPE=developer -AUTH_LOGIN=localhost:6300 -AUTH_PASSWORD=Oyuncu1
   & $E "D:\Kill Godot\KillGodot.uproject" -game -windowed -ResX=1280 -ResY=720 -CustomConfig=EOS -AUTH_TYPE=developer -AUTH_LOGIN=localhost:6300 -AUTH_PASSWORD=Oyuncu2
   ```
4. 1. pencere: **Play → Host game** → isim ver → **Start hosting**.
   2. pencere: **Play** → liste **"Player-hosted matches · Epic Online Services"** yazmalı, maç listede görünür → **Join**.
5. Log kontrolü (`Saved/Logs/KillGodot.log`): `LogEOS` giriş başarılı, `KG_SESSIONS advertise '...': ok`,
   istemcide `KG_SESSIONS join -> EOS:...`.

Epic hesabıyla normal giriş denemek için `-AUTH_TYPE=accountportal` kullan (tarayıcı/overlay ile giriş açılır;
sonraki açılışlarda kalıcı giriş kullanılır).

## 4. Yayın (shipping)

- `Source/KillGodot.Target.cs` içine `CustomConfig = "EOS";` ekle (Game target). Böylece paketli oyun EOS profilini
  her zaman yükler.
- Oyuncular Epic hesabıyla girer: ilk açılışta account portal, sonra kalıcı giriş. Epic Games Store'dan açılan oyuna
  launcher kimliği otomatik verir.
- Steam'de yayın düşünülürse: OnlineSubsystemSteam platform OSS olarak eklenir ve `bUseEAS=False` yapılır; oyuncular
  Epic hesabı istemeden, Steam kimliğiyle **EOS Connect** üzerinden girer. Lobiler ve P2P aynı kalır.
- Herkese açık yayından önce Epic Account Services için **Brand Settings** doldurulup incelemeye gönderilir.

## 5. Bugün (EOS yokken) nasıl test edilir

- Tek bilgisayar: Editor → Play (2 oyuncu, **Standalone** net mode) veya iki `-game` penceresi. Biri **Host game**,
  diğeri **Play** listesinde maçı görür (LAN yayını).
- Aynı ağdaki iki bilgisayar: aynı şekilde, LAN listesinde görünür.
- İnternet üzerinden (EOS olmadan): **Join by IP** + host tarafında UDP **7777** port yönlendirmesi.
