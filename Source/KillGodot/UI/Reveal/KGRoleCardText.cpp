#include "UI/Reveal/KGRoleCardText.h"

#include "Core/KGGameUserSettings.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"

namespace KGRoleCardPrivate
{
	struct FRow
	{
		const TCHAR* Id;
		const TCHAR* NameEn;
		const TCHAR* NameTr;
		const TCHAR* AbilityEn[2];
		const TCHAR* AbilityTr[2];
		const TCHAR* FlavourEn;
		const TCHAR* FlavourTr;
		/** Neutrals and some killers have their own goal; null = the faction goal. */
		const TCHAR* GoalEn = nullptr;
		const TCHAR* GoalTr = nullptr;
	};

	// Abilities: Docs/02_Roles.md, cut to card length. Flavour: Docs/Lore/KillGo_Lore.md section 2.4 (verbatim).
	const FRow Rows[] = {
		// --- Town -------------------------------------------------------------------------------------------------
		{TEXT("Sheriff"), TEXT("Sheriff"), TEXT("Şerif"),
		 {TEXT("Night: question one villager - suspicious or not?"), nullptr},
		 {TEXT("Gece: birini sorgula - şüpheli mi, değil mi?"), nullptr},
		 TEXT("Someone in this village is lying. Start with everyone."), TEXT("Bu köyde biri yalan söylüyor. Herkesten başla.")},
		{TEXT("Investigator"), TEXT("Investigator"), TEXT("Dedektif"),
		 {TEXT("Night: learn three roles your target could be."), nullptr},
		 {TEXT("Gece: hedefin olabileceği üç rolü öğren."), nullptr},
		 TEXT("Every hand smells of something: fish, flour or gunpowder."), TEXT("Her el bir şey kokar: balık, un ya da barut.")},
		{TEXT("Lookout"), TEXT("Lookout"), TEXT("Gözcü"),
		 {TEXT("Watch a house from a rooftop and see who visits it."), nullptr},
		 {TEXT("Bir evi çatıdan izle, o gece kimin geldiğini gör."), nullptr},
		 TEXT("The best view in Morrowmere is someone else's front door."), TEXT("Morrowmere'in en güzel manzarası başkasının kapısıdır.")},
		{TEXT("Tracker"), TEXT("Tracker"), TEXT("İz Sürücü"),
		 {TEXT("Your target's footprints glow for 90 seconds."), TEXT("At dawn, learn who they visited.")},
		 {TEXT("Hedefin ayak izleri 90 saniye parlar."), TEXT("Şafakta kimi ziyaret ettiğini öğren.")},
		 TEXT("Mud remembers every step. People only remember the convenient ones."), TEXT("Çamur her adımı hatırlar. İnsanlar sadece işine geleni.")},
		{TEXT("Coroner"), TEXT("Coroner"), TEXT("Adli Tabip"),
		 {TEXT("Examine bodies: time of death, weapon, the killer's side."), nullptr},
		 {TEXT("Cesetleri incele: ölüm saati, silah, katilin tarafı."), nullptr},
		 TEXT("The dead can't talk. Luckily, they can't lie either."), TEXT("Ölüler konuşamaz. Neyse ki yalan da söyleyemezler.")},
		{TEXT("Eavesdropper"), TEXT("Eavesdropper"), TEXT("Kulak Misafiri"),
		 {TEXT("You hear the Impatient radio, garbled."), TEXT("At dawn, learn who the Impatient visited.")},
		 {TEXT("Sabırsızların telsizini bozuk duyarsın."), TEXT("Şafakta kimi ziyaret ettiklerini öğren.")},
		 TEXT("The walls in Morrowmere are thin. The consciences, thinner."), TEXT("Morrowmere'de duvarlar incedir. Vicdanlar daha da ince.")},
		{TEXT("Medium"), TEXT("Medium"), TEXT("Medyum"),
		 {TEXT("Speak with the dead at night."), TEXT("Once: ring the Seance Bell - a ghost speaks at the meeting.")},
		 {TEXT("Gece ölülerle konuş."), TEXT("Bir kez: Seans Çanı - bir hayalet toplantıda konuşur.")},
		 TEXT("The dead are waiting too. They're just quieter about it."), TEXT("Ölüler de bekliyor. Sadece daha sessizce.")},
		{TEXT("Seer"), TEXT("Seer"), TEXT("Kâhin"),
		 {TEXT("Pick two villagers: same side, or different?"), nullptr},
		 {TEXT("İki kişi seç: aynı taraf mı, farklı mı?"), nullptr},
		 TEXT("Two faces, one question: do they wait for the same tomorrow?"), TEXT("İki yüz, tek soru: aynı yarını mı bekliyorlar?")},
		{TEXT("Doctor"), TEXT("Doctor"), TEXT("Doktor"),
		 {TEXT("Night: protect one villager from death."), TEXT("Day: bandage the wounded for 40 health.")},
		 {TEXT("Gece: birini ölümden koru."), TEXT("Gündüz: yaralıya bandajla 40 can ver.")},
		 TEXT("Nobody dies on my watch. Tonight, at least."), TEXT("Benim nöbetimde kimse ölmez. En azından bu gece.")},
		{TEXT("Bodyguard"), TEXT("Bodyguard"), TEXT("Koruma"),
		 {TEXT("Guard someone tonight. Their attacker dies with you."), nullptr},
		 {TEXT("Bu gece birini koru. Saldırgan seninle birlikte ölür."), nullptr},
		 TEXT("If tomorrow comes, it comes for both of us."), TEXT("Yarın gelecekse ikimize birden gelsin.")},
		{TEXT("Watchman"), TEXT("Watchman"), TEXT("Bekçi"),
		 {TEXT("Patrol the curfew with a lantern."), TEXT("Two cuffs: whoever you catch goes to jail.")},
		 {TEXT("Sokak yasağında fenerle devriye gez."), TEXT("İki kelepçe: yakaladığın hapse gider.")},
		 TEXT("Curfew starts at dusk. Excuses start at dawn."), TEXT("Sokak yasağı akşam başlar, bahaneler sabah.")},
		{TEXT("Priest"), TEXT("Priest"), TEXT("Rahip"),
		 {TEXT("Bless a house: the first intruder rings the church bell."), nullptr},
		 {TEXT("Bir evi kutsa: izinsiz giren ilk kişi çanı çaldırır."), nullptr},
		 TEXT("Bless the door, bar the dark. If it knocks, the bell answers."), TEXT("Kapıyı kutsa, karanlığı kilitle. Kapıyı çalarsa çan cevap verir.")},
		{TEXT("Locksmith"), TEXT("Locksmith"), TEXT("Çilingir"),
		 {TEXT("Make one door unbreakable tonight."), TEXT("By day you open any lock (do not get seen).")},
		 {TEXT("Bu gece bir kapıyı kırılmaz yap."), TEXT("Gündüz her kilidi açarsın (görülme).")},
		 TEXT("Every lock in town is mine. Tonight, one of them is yours."), TEXT("Kasabadaki her kilit benim. Bu gece biri senin.")},
		{TEXT("Vigilante"), TEXT("Vigilante"), TEXT("İnfazcı"),
		 {TEXT("A hidden pistol with three bullets."), TEXT("Shoot an innocent and guilt takes you the next night.")},
		 {TEXT("Üç mermili gizli bir tabanca."), TEXT("Masum vurursan ertesi gece vicdan azabı seni alır.")},
		 TEXT("Three bullets. Choose as if it's your last day. It never is."), TEXT("Üç kurşun. Son gününmüş gibi seç. Hiçbir zaman değildir.")},
		{TEXT("Veteran"), TEXT("Veteran"), TEXT("Emekli Asker"),
		 {TEXT("Three nights on alert: you shoot anyone at your door."), nullptr},
		 {TEXT("Üç gece nöbet: kapına gelen herkesi vurursun."), nullptr},
		 TEXT("I've waited three hundred years. I can wait behind this door."), TEXT("Üç yüz yıl bekledim. Bu kapının ardında da beklerim.")},
		{TEXT("Hunter"), TEXT("Hunter"), TEXT("Avcı"),
		 {TEXT("Set three bear traps. A trap holds its victim for 8 s."), nullptr},
		 {TEXT("Üç ayı kapanı kur. Kapan kurbanını 8 sn tutar."), nullptr},
		 TEXT("Traps don't ask questions. That's my job."), TEXT("Kapan soru sormaz. O benim işim.")},
		{TEXT("Mayor"), TEXT("Mayor"), TEXT("Muhtar"),
		 {TEXT("Reveal yourself at a meeting: your vote counts three times."), nullptr},
		 {TEXT("Toplantıda kendini açıkla: oyun üç sayılır."), nullptr},
		 TEXT("Vote early, vote often. Well, three times."), TEXT("Erken oy ver, çok oy ver. Yani üç kere.")},
		{TEXT("Jailor"), TEXT("Jailor"), TEXT("Gardiyan"),
		 {TEXT("Jail one villager each night and talk in private."), TEXT("Three executions; an innocent one costs you the rest.")},
		 {TEXT("Her gece birini hapse at, baş başa konuş."), TEXT("Üç infaz hakkı; masum infaz edersen gerisi gider.")},
		 TEXT("One cell, one guest, one very long conversation."), TEXT("Bir hücre, bir misafir, çok uzun bir sohbet.")},
		{TEXT("TavernKeeper"), TEXT("Tavern Keeper"), TEXT("Meyhaneci"),
		 {TEXT("Get someone drunk: their night action fails."), TEXT("They stagger all the next morning.")},
		 {TEXT("Birini sarhoş et: gece eylemi boşa gider."), TEXT("Ertesi sabah sendeleyerek yürür.")},
		 TEXT("The Latecomer never closes. It just makes you forget to leave."), TEXT("Geç Kalan hiç kapanmaz. Sadece gitmeyi unutturur.")},
		{TEXT("BellRinger"), TEXT("Bell Ringer"), TEXT("Zangoç"),
		 {TEXT("Twice: ring the Night Bell - everyone outside shows through walls."), nullptr},
		 {TEXT("İki kez: Gece Çanı - dışarıdaki herkes duvar ardından görünür."), nullptr},
		 TEXT("When the bell rings, everyone outside is seen. Me included."), TEXT("Çan çalınca dışarıdaki herkes görünür. Ben de.")},
		{TEXT("Shepherd"), TEXT("Shepherd"), TEXT("Çoban"),
		 {TEXT("Your dog barks at disguises and tracks the last attacker."), nullptr},
		 {TEXT("Köpeğin kılıklılara havlar, son saldırganın izini sürer."), nullptr},
		 TEXT("Good dog. Bad person. Bark."), TEXT("İyi köpek. Kötü insan. Hav.")},
		{TEXT("PowderMaster"), TEXT("Powder Master"), TEXT("Barut Ustası"),
		 {TEXT("Sense powder, bombs and oil within 20 m."), TEXT("Defuse a bomb in one second.")},
		 {TEXT("20 m içindeki barutu, bombayı ve yağı sez."), TEXT("Bombayı bir saniyede sök.")},
		 TEXT("I can smell a fuse through three walls and one lie."), TEXT("Fitili üç duvarın ve bir yalanın ardından koklarım.")},

		// --- Clockbreakers ----------------------------------------------------------------------------------------
		{TEXT("Clockmaster"), TEXT("Clockmaster"), TEXT("Saat Ustası"),
		 {TEXT("Leader: pick the victim - strike, or order the Enforcer to."), TEXT("Armoured for one night.")},
		 {TEXT("Lider: kurbanı seç - kendin vur ya da Tetikçi'ye emret."), TEXT("Bir gece zırhlısın.")},
		 TEXT("Every clock can be stopped. Start with the one on the square."), TEXT("Her saat durdurulabilir. Meydandakinden başla.")},
		{TEXT("Enforcer"), TEXT("Enforcer"), TEXT("Tetikçi"),
		 {TEXT("The team's blade: you carry out the kills."), TEXT("If the Clockmaster dies, you lead.")},
		 {TEXT("Takımın bıçağı: öldürmeleri sen yaparsın."), TEXT("Saat Ustası ölürse lider sensin.")},
		 TEXT("The minute hand does the work. The hour hand takes the bow."), TEXT("İşi yelkovan yapar, alkışı akrep toplar.")},
		{TEXT("Informant"), TEXT("Informant"), TEXT("Muhbir"),
		 {TEXT("Night: learn your target's exact role."), nullptr},
		 {TEXT("Gece: hedefin tam rolünü öğren."), nullptr},
		 TEXT("I don't care who you are. What you are is enough."), TEXT("Kim olduğun umurumda değil. Ne olduğun yeter.")},
		{TEXT("Framer"), TEXT("Framer"), TEXT("İftiracı"),
		 {TEXT("Make someone look suspicious to the Sheriff."), TEXT("Plant false evidence in their house.")},
		 {TEXT("Birini Şerif'e şüpheli göster."), TEXT("Evine sahte delil bırak.")},
		 TEXT("Evidence is just a story someone left lying around."), TEXT("Delil, birinin ortada bıraktığı bir hikâyedir.")},
		{TEXT("Cleaner"), TEXT("Cleaner"), TEXT("Temizlikçi"),
		 {TEXT("Three times: hide a body's role and last will."), TEXT("Drag bodies to the well or the sea.")},
		 {TEXT("Üç kez: cesedin rolünü ve vasiyetini gizle."), TEXT("Cesedi kuyuya ya da denize sürükle.")},
		 TEXT("Nobody died last night. Someone just left early."), TEXT("Dün gece kimse ölmedi. Biri sadece erken ayrıldı.")},
		{TEXT("Blackmailer"), TEXT("Blackmailer"), TEXT("Şantajcı"),
		 {TEXT("Your target cannot speak or type tomorrow."), nullptr},
		 {TEXT("Hedefin yarın konuşamaz, yazamaz."), nullptr},
		 TEXT("Speaking at the meeting tomorrow? Not you."), TEXT("Yarın toplantıda konuşacak mısın? Sen değil.")},
		{TEXT("Forger"), TEXT("Forger"), TEXT("Sahtekar"),
		 {TEXT("Twice: rewrite a victim's last will."), TEXT("Slip fake letters into mailboxes.")},
		 {TEXT("İki kez: kurbanın vasiyetini değiştir."), TEXT("Posta kutularına sahte mektup bırak.")},
		 TEXT("A last will is a lie's first draft."), TEXT("Vasiyet, yalanın ilk taslağıdır.")},
		{TEXT("Spy"), TEXT("Spy"), TEXT("Casus"),
		 {TEXT("Backstab someone to wear their face, name and key."), TEXT("Once a day: a 90-second disguise kit.")},
		 {TEXT("Arkadan vurduğunun yüzünü, adını ve anahtarını giy."), TEXT("Günde bir kez: 90 saniyelik kılık çantası.")},
		 TEXT("Wear the face, walk the walk. Just don't say a word."), TEXT("Yüzü giy, yürüyüşü kap. Ama sakın konuşma.")},
		{TEXT("Charmer"), TEXT("Charmer"), TEXT("Baştan Çıkarıcı"),
		 {TEXT("Cancel one villager's night action."), nullptr},
		 {TEXT("Birinin gece eylemini iptal et."), nullptr},
		 TEXT("Stay in tonight, darling. The fog is cold."), TEXT("Bu gece evde kal, canım. Sis soğuk.")},
		{TEXT("Saboteur"), TEXT("Saboteur"), TEXT("Sabotajcı"),
		 {TEXT("Sabotages recharge twice as fast."), TEXT("Break bridges, jam doors, douse the lighthouse.")},
		 {TEXT("Sabotajların iki kat hızlı dolar."), TEXT("Köprü kır, kapı sıkıştır, feneri söndür.")},
		 TEXT("Bridges break. Doors stick. Lamps go out. Funny, that."), TEXT("Köprüler kırılır, kapılar sıkışır, lambalar söner. Ne tuhaf.")},
		{TEXT("Smuggler"), TEXT("Smuggler"), TEXT("Kaçakçı"),
		 {TEXT("You know the secret tunnels and carry a lockpick."), TEXT("Once: dig up a pistol for the team.")},
		 {TEXT("Gizli tünelleri bilirsin, maymuncuğun var."), TEXT("Bir kez: takıma zuladan tabanca çıkar.")},
		 TEXT("Morrowmere has two maps. The one on the wall is the boring one."), TEXT("Morrowmere'in iki haritası var. Duvardaki sıkıcı olanı.")},
		{TEXT("Ambusher"), TEXT("Ambusher"), TEXT("Pusucu"),
		 {TEXT("Lay an ambush: the first to pass by dies."), nullptr},
		 {TEXT("Pusu kur: oradan ilk geçen ölür."), nullptr},
		 TEXT("Everyone comes down the Long Ope eventually."), TEXT("Herkes eninde sonunda Uzun Ope'den iner.")},
		{TEXT("Bomber"), TEXT("Bomber"), TEXT("Bombacı"),
		 {TEXT("Plant a ticking bomb: 20-60 s fuse, 5 m blast."), nullptr},
		 {TEXT("Saatli bomba kur: 20-60 sn fitil, 5 m patlama."), nullptr},
		 TEXT("Tick. Tock. Now it's everyone's problem."), TEXT("Tik. Tak. Artık herkesin derdi.")},

		// --- Solo killers and converters ----------------------------------------------------------------------------
		{TEXT("SerialKiller"), TEXT("Serial Killer"), TEXT("Seri Katil"),
		 {TEXT("One kill every night. Armoured at night."), nullptr},
		 {TEXT("Her gece bir öldürme. Gece zırhlısın."), nullptr},
		 TEXT("I'm not impatient. I just don't wait."), TEXT("Sabırsız değilim. Sadece beklemem.")},
		{TEXT("Werewolf"), TEXT("Werewolf"), TEXT("Kurtadam"),
		 {TEXT("Full moon: become a beast that breaks any door."), TEXT("In human form the Sheriff sees nothing.")},
		 {TEXT("Dolunayda her kapıyı kıran bir canavara dönüş."), TEXT("İnsan formunda Şerif bir şey göremez.")},
		 TEXT("The moon keeps its own calendar. Mine says tonight."), TEXT("Ay kendi takvimini tutar. Benimkinde \"bu gece\" yazıyor.")},
		{TEXT("Arsonist"), TEXT("Arsonist"), TEXT("Kundakçı"),
		 {TEXT("Douse doors and houses in oil."), TEXT("One night, light them all.")},
		 {TEXT("Kapılara ve evlere yağ dök."), TEXT("Bir gece hepsini ateşle.")},
		 TEXT("Everyone wants a little warmth. I'm very generous."), TEXT("Herkes biraz sıcaklık ister. Ben çok cömerdim.")},
		{TEXT("Poisoner"), TEXT("Poisoner"), TEXT("Zehirci"),
		 {TEXT("Poison food, drink or the well."), TEXT("Victims cough, then die the next night.")},
		 {TEXT("Yemeği, içkiyi ya da kuyuyu zehirle."), TEXT("Kurban öksürür, ertesi gece ölür.")},
		 TEXT("Drink up. It's on the house. Every house."), TEXT("İç, ikramımız. Her evin ikramı.")},
		{TEXT("Vampire"), TEXT("Vampire"), TEXT("Vampir"),
		 {TEXT("Every second night, turn someone into a vampire."), nullptr},
		 {TEXT("İki gecede bir, birini vampire dönüştür."), nullptr},
		 TEXT("Waiting forever is easy. You just need company."), TEXT("Sonsuza kadar beklemek kolay. Sadece arkadaş lazım."),
		 TEXT("Turn or outlive the village until only vampires remain."), TEXT("Köyü dönüştür ya da alt et: yalnız vampirler kalsın.")},
		{TEXT("Plaguebearer"), TEXT("Plaguebearer"), TEXT("Veba Taşıyıcı"),
		 {TEXT("Stand close to someone for 5 s to infect them."), TEXT("Infect everyone and become the Apocalypse.")},
		 {TEXT("5 sn yakın durarak bulaştır."), TEXT("Herkese bulaştır ve Kıyamet ol.")},
		 TEXT("In a small village, everyone shares everything."), TEXT("Küçük köyde herkes her şeyi paylaşır.")},
		{TEXT("Drowned"), TEXT("The Drowned"), TEXT("Boğulmuş"),
		 {TEXT("Swim the water tunnels fast."), TEXT("Drag victims near the water into the sea.")},
		 {TEXT("Su tünellerinde hızlı yüz."), TEXT("Suya yakın kurbanı denize çek.")},
		 TEXT("We tried to swim past the fog. The fog swam back."), TEXT("Sisi yüzerek geçmeye çalıştık. Sis de bize doğru yüzdü.")},
		{TEXT("Doppelganger"), TEXT("Doppelgänger"), TEXT("Suret"),
		 {TEXT("Take your victim's face, name and house for good."), TEXT("Your voice stays your own.")},
		 {TEXT("Kurbanının yüzünü, adını ve evini kalıcı olarak al."), TEXT("Sesin değişmez.")},
		 TEXT("Your house, your name, your chair at supper. Thank you."), TEXT("Evin, adın, sofradaki sandalyen. Teşekkürler.")},

		// --- Wanderers (neutrals) ----------------------------------------------------------------------------------
		{TEXT("Fool"), TEXT("Fool"), TEXT("Soytarı"),
		 {TEXT("Act mad. Any other death is a loss."), TEXT("Once hanged, you haunt one of your guilty voters.")},
		 {TEXT("Deli gibi davran. Başka türlü ölmek kayıptır."), TEXT("Asılınca suçlu oy verenlerden birini perilersin.")},
		 TEXT("Hang me. Please. I insist."), TEXT("As beni. Lütfen. Israr ediyorum."),
		 TEXT("Get yourself hanged at the meeting."), TEXT("Toplantıda kendini astır.")},
		{TEXT("Executioner"), TEXT("Executioner"), TEXT("Cellat"),
		 {TEXT("You know one villager's name: your target."), TEXT("If they die another way, you become the Fool.")},
		 {TEXT("Bir köylünün adını bilirsin: hedefin."), TEXT("Başka yolla ölürse Soytarı olursun.")},
		 TEXT("Just one name. Say it louder, with me."), TEXT("Tek bir isim. Benimle birlikte, daha yüksek sesle."),
		 TEXT("Get your target hanged by the village."), TEXT("Hedefini köye astır.")},
		{TEXT("Witch"), TEXT("Witch"), TEXT("Cadı"),
		 {TEXT("Redirect someone's night action to a new target."), nullptr},
		 {TEXT("Birinin gece eylemini başka bir hedefe yönlendir."), nullptr},
		 TEXT("Your hands. My strings."), TEXT("Eller senin, ipler benim."),
		 TEXT("Survive and see the Impatient win."), TEXT("Hayatta kal ve Sabırsızların kazandığını gör.")},
		{TEXT("Survivor"), TEXT("Survivor"), TEXT("Hayatta Kalan"),
		 {TEXT("Four vests: armour for four nights."), nullptr},
		 {TEXT("Dört yelek: dört gecelik zırh."), nullptr},
		 TEXT("I'm not on anyone's side. I'm on tomorrow's."), TEXT("Kimseden yana değilim. Yarından yanayım."),
		 TEXT("Stay alive until the end."), TEXT("Sona kadar hayatta kal.")},
		{TEXT("Amnesiac"), TEXT("Amnesiac"), TEXT("Hafızasız"),
		 {TEXT("Once: remember a dead villager's role and become it."), nullptr},
		 {TEXT("Bir kez: bir ölünün rolünü hatırla ve o ol."), nullptr},
		 TEXT("I've forgotten who I am. Lend me someone."), TEXT("Kim olduğumu unuttum. Bana birini ödünç ver."),
		 TEXT("Find a role worth remembering, then win with it."), TEXT("Hatırlanmaya değer bir rol bul, onunla kazan.")},
		{TEXT("Pozzo"), TEXT("Pozzo"), TEXT("Pozzo"),
		 {TEXT("Night 1: secretly pick your Lucky."), TEXT("Stray more than 40 m apart and the rope pulls.")},
		 {TEXT("1. gece gizlice Lucky'ni seç."), TEXT("40 m'den fazla ayrılırsanız ip gerilir.")},
		 TEXT("A good servant is on a short rope. A great one never notices it."), TEXT("İyi uşak kısa iptedir. Harikası ipi hiç fark etmez."),
		 TEXT("Keep yourself and your Lucky alive to the end."), TEXT("Sen ve Lucky'n sona kadar yaşayın.")},
		{TEXT("Collector"), TEXT("Collector"), TEXT("Koleksiyoncu"),
		 {TEXT("Some of the five items sit in other people's houses."), nullptr},
		 {TEXT("Beş eşyanın bazıları başkalarının evinde."), nullptr},
		 TEXT("It isn't stealing if it was always meant for my collection."), TEXT("Koleksiyonuma yazılmışsa hırsızlık sayılmaz."),
		 TEXT("Steal five marked items into your hidden chest."), TEXT("İşaretli beş eşyayı çalıp gizli sandığına götür.")},
		{TEXT("Pirate"), TEXT("Pirate"), TEXT("Korsan"),
		 {TEXT("Twice: raid someone at night for a duel."), TEXT("Sword, pistol or musket - the winner lives.")},
		 {TEXT("İki kez: gece birine baskın yap, düello et."), TEXT("Kılıç, tabanca ya da tüfek - kazanan yaşar.")},
		 TEXT("Sword, pistol or musket? Choose. Then lose."), TEXT("Kılıç, tabanca, tüfek? Seç. Sonra kaybet."),
		 TEXT("Win two duels, then sail into the fog."), TEXT("İki düello kazan, sonra sise yelken aç.")},
	};

	const FRow* FindRow(FName RoleId)
	{
		for (const FRow& Row : Rows)
		{
			if (RoleId == FName(Row.Id))
			{
				return &Row;
			}
		}
		return nullptr;
	}

	FString TeamLine(const FKGRoleInfo& Info, bool bTr)
	{
		using C = EKGRoleCategory;
		switch (Info.Category)
		{
		case C::TownInvestigative: return bTr ? TEXT("Bekleyenler · Araştırma") : TEXT("The Waiting · Investigative");
		case C::TownSpiritual: return bTr ? TEXT("Bekleyenler · Ruhani") : TEXT("The Waiting · Spiritual");
		case C::TownProtective: return bTr ? TEXT("Bekleyenler · Koruma") : TEXT("The Waiting · Protective");
		case C::TownKilling: return bTr ? TEXT("Bekleyenler · Öldürme") : TEXT("The Waiting · Killing");
		case C::TownSupport: return bTr ? TEXT("Bekleyenler · Destek") : TEXT("The Waiting · Support");
		case C::ClockbreakerLeader:
		case C::ClockbreakerKilling:
		case C::ClockbreakerSupport: return bTr ? TEXT("Saat Kırıcılar") : TEXT("The Clockbreakers");
		case C::SoloKilling: return bTr ? TEXT("Yalnız katil") : TEXT("Solo killer");
		case C::NeutralEvil: return bTr ? TEXT("Gezginler · Kötü") : TEXT("The Wanderers · Evil");
		case C::NeutralBenign: return bTr ? TEXT("Gezginler · İyi niyetli") : TEXT("The Wanderers · Benign");
		default: return bTr ? TEXT("Gezginler · Kaos") : TEXT("The Wanderers · Chaos");
		}
	}

	FString FactionGoal(const FKGRoleInfo& Info, bool bTr)
	{
		switch (Info.Faction)
		{
		case EKGFaction::Town:
			return bTr ? TEXT("Sabırsızları bul ve köy düşmeden onları astır.")
			           : TEXT("Find the Impatient and hang them before the village falls.");
		case EKGFaction::Clockbreakers:
			return bTr ? TEXT("Bekleyenleri teker teker öldür; saat senin için dursun.")
			           : TEXT("Kill those who wait, one by one, until the clock stops for you.");
		case EKGFaction::Plague:
			return bTr ? TEXT("Herkese bulaştır, sonra ayakta kalan son kişi ol.")
			           : TEXT("Infect everyone, then be the last one standing.");
		default:
			return bTr ? TEXT("Ayakta kalan son kişi ol.") : TEXT("Be the last one standing.");
		}
	}
}

bool KGRoleCard::IsTurkish()
{
	return UKGGameUserSettings::GetActiveLanguage().StartsWith(TEXT("tr"));
}

bool KGRoleCard::HasEntry(FName RoleId)
{
	return KGRoleCardPrivate::FindRow(RoleId) != nullptr;
}

FKGRoleCardText KGRoleCard::Get(FName RoleId)
{
	return GetIn(RoleId, IsTurkish());
}

FKGRoleCardText KGRoleCard::GetIn(FName RoleId, bool bTr)
{
	FKGRoleCardText Out;
	const FKGRoleInfo* Info = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), RoleId);
	const KGRoleCardPrivate::FRow* Row = KGRoleCardPrivate::FindRow(RoleId);
	if (!Row)
	{
		// Unknown id (a role added to the catalog without card text): readable fallback instead of an empty card.
		const FString Raw = RoleId.ToString();
		for (int32 Index = 0; Index < Raw.Len(); ++Index)
		{
			if (Index > 0 && FChar::IsUpper(Raw[Index]) && !FChar::IsUpper(Raw[Index - 1]))
			{
				Out.Name.AppendChar(TEXT(' '));
			}
			Out.Name.AppendChar(Raw[Index]);
		}
		Out.Team = Info ? KGRoleCardPrivate::TeamLine(*Info, bTr) : FString();
		Out.Goal = Info ? KGRoleCardPrivate::FactionGoal(*Info, bTr) : FString();
		return Out;
	}
	Out.Name = bTr ? Row->NameTr : Row->NameEn;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const TCHAR* Line = bTr ? Row->AbilityTr[Index] : Row->AbilityEn[Index];
		if (Line)
		{
			Out.Abilities.Add(Line);
		}
	}
	Out.Flavour = bTr ? Row->FlavourTr : Row->FlavourEn;
	const TCHAR* Goal = bTr ? Row->GoalTr : Row->GoalEn;
	Out.Goal = Goal ? FString(Goal) : (Info ? KGRoleCardPrivate::FactionGoal(*Info, bTr) : FString());
	Out.Team = Info ? KGRoleCardPrivate::TeamLine(*Info, bTr) : FString();
	return Out;
}

FString KGRoleCard::AlignmentName(EKGAlignment Alignment)
{
	const bool bTr = IsTurkish();
	switch (Alignment)
	{
	case EKGAlignment::Impatient: return bTr ? TEXT("SABIRSIZ") : TEXT("IMPATIENT");
	case EKGAlignment::Neutral: return bTr ? TEXT("NÖTR") : TEXT("NEUTRAL");
	default: return bTr ? TEXT("KASABA") : TEXT("TOWN");
	}
}

FLinearColor KGRoleCard::AlignmentColor(EKGAlignment Alignment)
{
	// Same hues as the HUD (UI/KGHUD.cpp palette): village green, lifted crimson, neutral violet.
	switch (Alignment)
	{
	case EKGAlignment::Impatient: return FLinearColor::FromSRGBColor(FColor(255, 72, 94));
	case EKGAlignment::Neutral: return FLinearColor::FromSRGBColor(FColor(182, 150, 242));
	default: return FLinearColor::FromSRGBColor(FColor(110, 205, 120));
	}
}

bool KGRoleCard::IsTeamFaction(EKGFaction Faction)
{
	return Faction == EKGFaction::Clockbreakers || Faction == EKGFaction::Vampires;
}
