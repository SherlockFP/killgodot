# Gauntlet Döngüsü: ana prompt

Yeni bir Claude Code oturumunda ya da `/loop` ile kullanmak için bu metni kopyala:

```
Kill Godot projesinde (D:\Kill Godot) Gauntlet döngüsünü bir tur çalıştır.
1. CLAUDE.md ve Docs/Backlog.md'yi oku. Sıradaki [ ] maddeyi seç (milestone sırasına uy, [!] blokajlıları atla).
2. İlgili tasarım dokümanını (Docs/0x_*.md) ve gerekiyorsa Docs/Research/TechResearch.md bölümünü oku.
3. Docs/Iterations/ITER-NNN.md dosyasına kısa bir plan yaz: hedef, dokunulacak dosyalar, kabul kriterleri.
4. Uygula. CLAUDE.md'deki kod kurallarına (MatchClock, FKGRng, Snapshot, PUID, COND_OwnerOnly) uy.
5. Tools/Gauntlet/run_gates.ps1 çalıştır. Düşen kapıyı en fazla 3 denemede düzelt.
6. Backlog'u işaretle, ITER dosyasına kapı sonuçlarını yaz, bana Türkçe kısa bir özet ver:
   ne yapıldı, kapı sonuçları, sıradaki madde, benden gereken bir şey var mı.
İndirme, kurulum ya da commit gerekiyorsa önce bana sor.
```

**Tek seferlik milestone promptları** bu klasörde: `M1_Toolchain.md`, `M2_FPCharacter.md`, `M3_Network.md`.
**Sanat promptları:** `ArtPrompts.md`.
