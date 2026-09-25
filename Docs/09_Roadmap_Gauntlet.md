# KILL GODOT — Yol Haritası ve Gauntlet Döngüsü

## 1. Gauntlet Döngüsü (her iterasyonda)
Her iş parçası aynı "eldiven koridorundan" geçer. Bir kapıdan geçemeyen iş **bitmiş sayılmaz**.

```
 ┌─► 1. SEÇ     Backlog.md'den sıradaki işaretsiz maddeyi al (milestone sırasıyla)
 │   2. PLANLA  Docs/Iterations/ITER-XXX.md: hedef, dosyalar, kabul kriterleri
 │   3. YAP     C++ / Blueprint / Python / Blender
 │   4. GAUNTLET KAPILARI
 │      G1 Derleme    : Build.bat KillGodotEditor Win64 Development → 0 hata, 0 yeni uyarı
 │      G2 Testler    : UnrealEditor-Cmd -ExecCmds="Automation RunTests KillGodot" → hepsi yeşil
 │      G3 Ağ duman   : 1 listen host + 3 istemci (localhost) 60 sn bot → crash yok, desync yok
 │      G4 Performans : bench haritasında stat unit CSV → bütçe aşımı yok (06/05 dokümanları)
 │      G5 İnceleme   : kod incelemesi (code-review), doküman ile tutarlılık
 │      G6 Doküman    : ilgili Docs/ güncellendi, Backlog işaretlendi
 │   5. KAYDET  ITER log'una kapı sonuçları. Kullanıcı izin verdiyse commit.
 └── 6. Bir kapı düşerse düzelt ve tekrarla (en fazla 3 deneme), sonra kullanıcıya sor.
```
- **Kapı script'i:** `Tools/Gauntlet/run_gates.ps1`. Kapılar M1'de kurulur, her iterasyonda çalışır.
- **Host migration kapısı (G3+):** M6'dan sonra her gece Gauntlet ile host'u öldürme testi koşar.

## 2. Milestone'lar
| # | Ad | Çıktı / kabul kriteri |
|---|---|---|
| **M0** | Temel ✅ (bu oturum) | Dokümanlar, araştırmalar, proje iskeleti, Blender araçları, palet, kukla prototipi |
| **M1** | Araç zinciri | VS kurulu. Proje derleniyor ve editörde açılıyor. Git + LFS, kapı script'leri, ilk otomasyon testi yeşil. |
| **M2** | FP karakter | Hareket, FP kamera ve **viewmodel** (native FP rendering), sway/bob/recoil yayları, etkileşim, **fizik tutuş, taşıma ve fırlatma**, yakın dövüş temeli, 1 alet + inspect |
| **M3** | Ağ çekirdeği | EOS giriş (Dev Auth), lobi kur/ara/katıl, P2P listen server, 6–20 bot testi, relevancy ve dormancy temeli, `MatchClock`, `FKGRng`, `SnapshotComponent` |
| **M4** | Ses | EOS RTC lobi sesi, proximity (faz 1), ölü/diri kuralları, **ağız zarfı → çene**, sustur/engelle/raporla |
| **M5** | Oyun döngüsü | Faz makinesi, rol üretici, 8 çekirdek rol (Şerif, Doktor, Bekçi, Gardiyan, Saat Ustası, Tetikçi, Casus, Soytarı), toplantı, oylama, mahkeme, asma, kazanma, hayaletler, vasiyet |
| **M6** | Host migration v1 | Snapshot, halef seçimi, yeniden bağlanma ≤ 15 sn. Gauntlet host-öldürme testi yeşil. |
| **M7** | Graybox köy | Morrowmere blockout: 4 ev arketipi (2 kat + mahzen), meydan, liman, yeraltı parçası. Bölge kapıları, bench haritası. |
| **M8** | Görevler | 12 görev (taşıma, istasyon, ortak, doğrulanabilir), sabotaj, hazırlık barı, meslekler, Bakır ve NPC dükkânı |
| **M9** | Sanat geçişi 1 | Asset pipeline, palet, kukla karakter ve 4 Yüz, ev iç mekânları, gün-gece, bulutlar, rüzgâr, su |
| **M10** | Menü ve lobi | Ana menü 3D sahne, lobi tarayıcı, meyhane lobisi, sohbet paneli, popup'lar, TR/EN/RU |
| **M11** | Roller tam | 51 rolün hepsi, güç bütçesi, telemetri, Dedektif grupları, gece çözümleyici testleri |
| **M12** | Canlı köy | Rastgele olaylar, Sis Kuşatması, ağaç devirme, kırılabilirler, mini oyunlar (satranç, tavla, dart…), ısınma ve epilog, Araf düellosu ve hortlak, easter egg'ler |
| **M13** | Kozmetik | Dolap, Yüzler, slotlar, paint kit (aşınma, tohum, çetele), koliler ve makara, Almanak (yerel/dev modu), backend kararı |
| **M14** | Arcade | Atış poligonu, Tomorrow's Arsenal, silah seti, silah skinleri |
| **M15** | Optimizasyon | HLOD, cull, ISM, ölçeklenebilirlik kademeleri, 20 oyuncu bench, ses faz 2 (3D) |
| **M16** | Kapalı test | Arkadaş grubu playtest'leri, denge ayarı, hata avı |
| **M17** | Yayın hazırlığı | Mağaza (Steam/EGS), backend, ekonomi, PEGI/ESRB, raporlama |
| **M18** | Mobil | Android hedefi, dokunmatik UI, mobil profil, iOS araştırması |

## 3. Kritik yol ve riskler
1. **VS kurulumu** olmadan C++ derlenemez. Bu M1'in önünde duran tek blokaj.
2. **Host migration** her sisteme dokunur. Bu yüzden `Snapshot` / `MatchClock` / `Rng` kuralları **M3'te**, oynanıştan önce kurulur.
3. **Fizik tutuşunun ağ hissi** zor (R.E.P.O. ekibi bunu en zor iş diye anlatmış). M2'de yerel, M3'te ağ üzerinden prototiplenir.
4. **EOS RTC oda sınırı** doğrulanmalı (≥ 20). M4'ün ilk işi.
5. **Kapsam büyük.** Her milestone sonunda "oynanabilir dilim" olmalı. **M5 sonunda arkadaşlarla ilk gerçek test** yapılır: graybox, 8 rol.
