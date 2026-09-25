# M3: Ağ çekirdeği promptu

```
Kill Godot M3'ü uygula: EOS P2P ağ çekirdeği. Referans: Docs/Research/TechResearch.md §1-3,
Docs/05_Tech_Architecture.md §4 ve §9.

Önkoşul (benden): Epic Developer Portal'da ürün, ProductId/SandboxId/DeploymentId/ClientId. Yoksa önce
bana adım adım Türkçe rehber ver ve dur.

- Config/DefaultEngine.ini içindeki EOS bloğunu aç (NetDriverEOS, AllowRelays, bUseEOSConnect).
- UKGSessionSubsystem: Dev Auth ile giriş, lobi kur/ara/katıl, lobi ve üye attribute'ları
  (KG_BUILD, KG_STATE, KG_EPOCH, KG_HOST_URL...).
- Localhost IpNetDriver ile EOS'suz test modu (Gauntlet için).
- Bölge tabanlı relevancy, dormancy, MaxPlayers=20, adaptif net frekansı.
- Fizik tutuşun ağ versiyonu: sunucu otoritesi + yerel görsel takip.
- Gauntlet C# testi: 1 listen host + 5 istemci (bot), 120 sn, crash ve desync yok (G3 kapısı).
Kapıları çalıştır, Backlog'u güncelle, özet ver.
```
