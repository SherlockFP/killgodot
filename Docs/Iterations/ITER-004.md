# ITER-004 — Sprint 2 (2026-09-24)

**Hedef ve kabul kriterleri**
1. **P1 Saç kafaya otursun:** dummy_front ve backstab görüntülerinde saçla kafa arasında boşluk yok.
2. **P2 Nişangâh ve minimal HUD:** merkezde nokta nişangâh, sol altta can ve stamina çubukları. Nişangâh backstab hazırken kırmızı olur.
3. **P2 Eşya tutuşu eşyaya özgü:** şemsiye doğru yönde ve ölçekte, sapı avuçta.

## Sonuç (retro)
| Hedef | Durum |
|---|---|
| P1 Saç kafaya otursun | ✅ Kök neden: UBC saç/kaş statik mesh (kemiksiz). Saç `Head` kemiğine %100 bağlandı; kopya beyaz kaş dosyası çıkarıldı (kaşlar gövdede zaten var) |
| P2 Nişangâh + HUD | ✅ Nokta nişangâh (piksel tabanlı), backstab'da kırmızı nokta + 4 çentik, can/stamina çubukları sadece dolu değilken; renkler sRGB'den (QA-002-01/04 düzeltildi) |
| P2 Eşyaya özgü tutuş | 🟡 Şemsiye doğru yönde; sap-avuç teması ve ölçek hâlâ kaba → QA-002-02 (Grip soketi + DataAsset) |

**Kapılar:** G1 ✅ G2 8/8 ✅ · **QA-002:** ortalama 4.6 → 5.2 (Okunaklılık 5, Sanat 5, His 4, Teknik 5, Perf 7) · **Perf:** ort. 3.2–5.2 ms, p95 ≤ 7.0 ms
**Öğrenilen:** Paket içi yardımcı mesh'lerin (saç/kaş/aksesuar) skinli olup olmadığını import öncesi otomatik kontrol et.
**Sonraki sprint:** QA-002-03 yüz/karakter ışığı (rim + toon ramp + kontur) → QA-002-02 grip soketi/DataAsset → M3 ağ çekirdeğine başla (localhost host + istemciler).
