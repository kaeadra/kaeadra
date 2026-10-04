#include "IronSiegeStory.h"
#include "CrewRules.h"
#include "IronSiegeText.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace
{
const TCHAR* GCampaignSection = TEXT("IronSiege.Campaign");

// Arabic for the campaign: missions, radio lines, the crew and the campaign screens, keyed like the
// English in MissionRules.h / CrewRules.h. Anything missing falls back to English.
const TMap<FString, FString>& ArabicTable()
{
	static const TMap<FString, FString> Table = {
		// ---- Mission 1
		{ TEXT("M1Name"), TEXT("الطلعة الأولى") },
		{ TEXT("M1Brief0"), TEXT("طوّق «فيلق الصدأ» مدينة مرسى الحرّة بالحديد. لا شيء يدخل ولا شيء يخرج.") },
		{ TEXT("M1Brief1"), TEXT("أنت أحدث سائق في «كاسري الحصار». اليوم نعرف إن كنت تجيد القيادة وإطلاق النار معًا.") },
		{ TEXT("M1Brief2"), TEXT("اذهب إلى نقطة التجمّع، ثم طهّر دورية الفيلق التي تحوم حول مخزن وقودنا.") },
		{ TEXT("M1Obj0"), TEXT("اذهب إلى نقطة التجمّع") },
		{ TEXT("M1S0L0"), TEXT("من القيادة إلى كاسر الحصار. هل سخن المحرّك؟ اتبع العلامة إلى نقطة التجمّع.") },
		{ TEXT("M1Obj1"), TEXT("دمّر دورية الفيلق") },
		{ TEXT("M1S1L0"), TEXT("أهداف معادية! عربات الفيلق تقترب منك. أطلق النار بحرّية.") },
		{ TEXT("M1Out0"), TEXT("الدورية صارت خردة. ليست سيئة لطلعة أولى. عُد إلى القاعدة يا كاسر الحصار.") },

		// ---- Mission 2
		{ TEXT("M2Name"), TEXT("خطوط الوقود") },
		{ TEXT("M2Brief0"), TEXT("مدرّعات الفيلق تشرب الوقود بالصهاريج، وهم يخزّنونه في ثلاثة مستودعات في هذه الصحراء.") },
		{ TEXT("M2Brief1"), TEXT("أحرق الثلاثة. توقّع حرّاسًا، وقوّة ردّ حين يرون الدخان.") },
		{ TEXT("M2Brief2"), TEXT("عتاد جديد لهذه المهمة: زارع ألغام. ألقِ الألغام خلفك حين يطاردونك.") },
		{ TEXT("M2Obj0"), TEXT("دمّر مستودعات الوقود") },
		{ TEXT("M2S0L0"), TEXT("حدّدنا ثلاثة مستودعات. الصواريخ هي الأنجع ضد تلك الخزّانات.") },
		{ TEXT("M2Obj1"), TEXT("دمّر قوّة الردّ") },
		{ TEXT("M2S1L0"), TEXT("رأوا الدخان. المغيرون قادمون، وهم سريعون. استعمل ألغامك.") },
		{ TEXT("M2Out0"), TEXT("ثلاثة أعمدة دخان في الأفق. دبّاباتهم ستبيت عطشى الليلة.") },

		// ---- Mission 3
		{ TEXT("M3Name"), TEXT("قافلة الميناء") },
		{ TEXT("M3Brief0"), TEXT("شاحنتان محمّلتان بالدواء تنتظران على الرصيف. المدينة تحتاجهما الليلة.") },
		{ TEXT("M3Brief1"), TEXT("الفيلق يسيطر على ساحة الحاويات. رافق الشاحنتين حولها إلى البوّابة.") },
		{ TEXT("M3Brief2"), TEXT("ابقَ قريبًا: سيهاجمون الشاحنتين لا أنت. يجب أن تصل شاحنة واحدة على الأقل.") },
		{ TEXT("M3Obj0"), TEXT("رافق شاحنات الإمداد إلى البوّابة") },
		{ TEXT("M3S0L0"), TEXT("القافلة تتحرّك. أبعِد الفيلق عن تلك الشاحنات.") },
		{ TEXT("M3Obj1"), TEXT("اقضِ على المطاردين") },
		{ TEXT("M3S1L0"), TEXT("الشاحنات عبرت البوّابة. استدر الآن وأجهز على من يطاردونها.") },
		{ TEXT("M3Out0"), TEXT("الدواء وصل إلى المدينة. أنقذتَ للتوّ أرواحًا أكثر مما ستحصي يومًا.") },

		// ---- Mission 4
		{ TEXT("M4Name"), TEXT("صمت الأثير") },
		{ TEXT("M4Brief0"), TEXT("ثلاثة أجهزة تشويش للفيلق تُغرق كل تردّدات الميناء. نحن صمٌّ وعميان.") },
		{ TEXT("M4Brief1"), TEXT("استولِ على كل موقع تشويش واثبُت فيه حتى يعمل اتصالنا. بعدها سيهاجمون محطّة الترحيل.") },
		{ TEXT("M4Brief2"), TEXT("احمِ المحطّة حتى يكتمل الاتصال. زوّدناك بقاذف لهب: مفيد للقتال القريب حول المواقع.") },
		{ TEXT("M4Obj0"), TEXT("استولِ على مواقع التشويش") },
		{ TEXT("M4S0L0"), TEXT("قف داخل الحلقة وأبقِها خالية. الاستيلاء يتوقّف ما دام العدو ينازعك عليها.") },
		{ TEXT("M4Obj1"), TEXT("دافع عن محطّة الترحيل") },
		{ TEXT("M4S1L0"), TEXT("بدأ الاتصال. تسعون ثانية. إنهم يتّجهون مباشرة نحو المحطّة!") },
		{ TEXT("M4Out0"), TEXT("عادت الإشارة. أسمع المدينة كلها... وأسمع نداءً للفيلق تمنّيت ألّا أسمعه ثانية: الغراب.") },

		// ---- Mission 5
		{ TEXT("M5Name"), TEXT("شارعًا شارعًا") },
		{ TEXT("M5Brief0"), TEXT("الحيّ الخارجي لمرسى. رتل الإجلاء عالق على بُعد شارعين من خط الفيلق.") },
		{ TEXT("M5Brief1"), TEXT("قُد إلى التقاطع واثبُت فيه ريثما ينسحب الرتل. صيّادو الصواريخ في الشوارع: انتبه للإنذار واستعمل الشراك الحرارية.") },
		{ TEXT("M5Brief2"), TEXT("اصمد دقيقتين ونصفًا. لا تطارد أحدًا. اصمد.") },
		{ TEXT("M5Obj0"), TEXT("اذهب إلى تقاطع الإجلاء") },
		{ TEXT("M5S0L0"), TEXT("اسلك الجادّة. الحطام ومصائد الدبّابات في كل مكان: أبقِ مسارًا مفتوحًا.") },
		{ TEXT("M5Obj1"), TEXT("اصمد حتى يخرج الرتل") },
		{ TEXT("M5S1L0"), TEXT("الرتل يتحرّك. كل ما لديهم قادم عبر تلك الشوارع. اصمد!") },
		{ TEXT("M5S1L1"), TEXT("إذن أنت كاسر الحصار الجديد. سأتذكّر صوت محرّكك.") },
		{ TEXT("M5Out0"), TEXT("خرج الرتل. ذلك الصوت على اللاسلكي كان الغراب. في المرّة القادمة لن تكتفي بالكلام.") },

		// ---- Mission 6
		{ TEXT("M6Name"), TEXT("الغراب") },
		{ TEXT("M6Brief0"), TEXT("الغراب هي قنّاصة المدفع الكهرومغناطيسي لدى الفيلق. خسرنا أحد عشر سائقًا بسببها.") },
		{ TEXT("M6Brief1"), TEXT("تصطاد برفقة حرس. جرّدها من حرسها أولًا، وعندها ستأتيك بنفسها.") },
		{ TEXT("M6Brief2"), TEXT("مدفعها يشحن قبل أن يطلق، وستسمعه. انعطف لحظة سماع الصفير. ركّبنا لك مدفعًا كهرومغناطيسيًا أيضًا.") },
		{ TEXT("M6Obj0"), TEXT("دمّر حرس الغراب") },
		{ TEXT("M6S0L0"), TEXT("هيّا إذن. لنرَ ما يقدر عليه مدلَّل هانا الجديد.") },
		{ TEXT("M6Obj1"), TEXT("اهزم الغراب") },
		{ TEXT("M6S1L0"), TEXT("تلك هي: سيدان سوداء بسكّتين. لا تقُد في خط مستقيم!") },
		{ TEXT("M6S1L1"), TEXT("اثبُت مكانك. لن تؤلمك إلا مرّة واحدة.") },
		{ TEXT("M6Out0"), TEXT("...ليس سيّئًا. قل لهانا إنني لم أحبّ الجنرال يومًا على أي حال.") },
		{ TEXT("M6Out1"), TEXT("سقطت الغراب. الفيلق خسر للتوّ أمضى نصاله.") },

		// ---- Mission 7
		{ TEXT("M7Name"), TEXT("الأثر البارد") },
		{ TEXT("M7Brief0"), TEXT("بعد سقوط الغراب، يسحب الجنرال فارغا مولّدات دروعه شمالًا عبر الجليد.") },
		{ TEXT("M7Brief1"), TEXT("قافلة تحمل وحدات التحكّم. أوقف الشاحنات قبل أن تغادر البحيرة. نحتمل إفلات واحدة لا أكثر.") },
		{ TEXT("M7Brief2"), TEXT("ثم أعمِ أبراج راداره حتى لا يرى هجومنا الأخير. زوّدناك بملفّ تسلا.") },
		{ TEXT("M7Obj0"), TEXT("أوقف قافلة الفيلق") },
		{ TEXT("M7S0L0"), TEXT("القافلة على الجليد متّجهة إلى البوّابة الشمالية. لا تدعها تغادر البحيرة!") },
		{ TEXT("M7Obj1"), TEXT("دمّر أبراج الرادار") },
		{ TEXT("M7S1L0"), TEXT("الوحدات بأيدينا. والآن الرادار: برجان.") },
		{ TEXT("M7Out0"), TEXT("صار أعمى، ودروعه لنا لنحطّمها. دفعة أخيرة يا كاسر الحصار.") },

		// ---- Mission 8
		{ TEXT("M8Name"), TEXT("كسر الحصار") },
		{ TEXT("M8Brief0"), TEXT("هذه هي النهاية. قلعة قيادة فارغا تقف على الجليد خلف ثلاثة مولّدات دروع.") },
		{ TEXT("M8Brief1"), TEXT("حطّم المولّدات، واصمد أمام آخر احتياطه، وعندها سيخرج البارون الحديدي بنفسه.") },
		{ TEXT("M8Brief2"), TEXT("كل ما نملكه مركّب على سيارتك. أنهِ الحصار.") },
		{ TEXT("M8Obj0"), TEXT("دمّر مولّدات الدروع") },
		{ TEXT("M8S0L0"), TEXT("سيارة واحدة؟ هانا ترسل إليّ طفلًا ليموت على جليدي.") },
		{ TEXT("M8Obj1"), TEXT("اصمد أمام الهجوم المضاد") },
		{ TEXT("M8S1L0"), TEXT("سقطت الدروع! احتياطه يندفع نحوك. اصمد!") },
		{ TEXT("M8Obj2"), TEXT("دمّر البارون الحديدي") },
		{ TEXT("M8S2L0"), TEXT("كفى. سأسحقك بنفسي.") },
		{ TEXT("M8S2L1"), TEXT("ذلك هو العملاق. اضربه بكل ما لديك!") },
		{ TEXT("M8Out0"), TEXT("مستحيل... حصاري...") },
		{ TEXT("M8Out1"), TEXT("انتهى الأمر. الطريق إلى مرسى مفتوح. أهلًا بعودتك يا كاسر الحصار.") },

		{ TEXT("BossRaven"), TEXT("الغراب") },
		{ TEXT("BossBaron"), TEXT("البارون الحديدي") },

		// ---- The crew
		{ TEXT("DriverRinName"), TEXT("رين كوروساوا") },
		{ TEXT("DriverRinBlurb"), TEXT("متسابقة شوارع صارت كشّافة. سريعة، صاخبة، ولا تضغط الفرامل أولًا.") },
		{ TEXT("DriverRinPerk"), TEXT("النيترو يمتلئ أسرع بـ30%") },
		{ TEXT("DriverRinAbility"), TEXT("نيترو كامل لا ينفد ومحرّك أقوى لمدّة 4 ثوانٍ") },
		{ TEXT("DriverLaylaName"), TEXT("ليلى حدّاد") },
		{ TEXT("DriverLaylaBlurb"), TEXT("رامية من ميليشيا الميناء. هادئة، دقيقة، صبورة.") },
		{ TEXT("DriverLaylaPerk"), TEXT("سبطانة الرشاش تسخن أبطأ بـ20%") },
		{ TEXT("DriverLaylaAbility"), TEXT("بلا تشتّت ولا سخونة و+50% لضرر الرشاش لمدّة 5 ثوانٍ") },
		{ TEXT("DriverKenjiName"), TEXT("كينجي موري") },
		{ TEXT("DriverKenjiBlurb"), TEXT("سائق إنقاذ سابق. يضع سيارته بينك وبين النار.") },
		{ TEXT("DriverKenjiPerk"), TEXT("+15% للدرع الأقصى") },
		{ TEXT("DriverKenjiAbility"), TEXT("يتلقّى ربع الضرر لمدّة 6 ثوانٍ ويستعيد 30% من الدرع") },
		{ TEXT("DriverYukiName"), TEXT("يوكي شيراني") },
		{ TEXT("DriverYukiBlurb"), TEXT("مهندسة نابغة. تُصلح السيارة وهي تسير.") },
		{ TEXT("DriverYukiPerk"), TEXT("صناديق الإمداد تعطي 50% أكثر") },
		{ TEXT("DriverYukiAbility"), TEXT("تستعيد 40% من الصحة و25% من الدرع فورًا") },
		{ TEXT("DriverZaraName"), TEXT("زارا النور") },
		{ TEXT("DriverZaraBlurb"), TEXT("خبيرة متفجّرات. تؤمن أن لكل مشكلة نصف قطر انفجار.") },
		{ TEXT("DriverZaraPerk"), TEXT("تعبئة الصواريخ أسرع بـ25%") },
		{ TEXT("DriverZaraAbility"), TEXT("تملأ الحاضنة؛ الصواريخ تنطلق بضعف السرعة و+50% ضرر لمدّة 6 ثوانٍ") },
		{ TEXT("DriverOmarName"), TEXT("عمر ناصر") },
		{ TEXT("DriverOmarBlurb"), TEXT("مخترق إشارات. تصمت أجهزة الفيلق حين يبتسم.") },
		{ TEXT("DriverOmarPerk"), TEXT("الشراك الحرارية تُشحن أسرع بـ40%") },
		{ TEXT("DriverOmarAbility"), TEXT("تطفئ محرّكات الأعداء ضمن 30 م لمدّة 3 ثوانٍ وتُفجّر الصواريخ") },
		{ TEXT("AbilityOVERDRIVE"), TEXT("الدفع الأقصى") },
		{ TEXT("AbilityDEADEYE"), TEXT("عين الصقر") },
		{ TEXT("AbilityIRON WALL"), TEXT("الجدار الحديدي") },
		{ TEXT("AbilityFIELD REPAIR"), TEXT("إصلاح ميداني") },
		{ TEXT("AbilityBARRAGE"), TEXT("الوابل") },
		{ TEXT("AbilityEMP PULSE"), TEXT("نبضة كهرومغناطيسية") },
		{ TEXT("SpeakerCOMET"), TEXT("المذنَّب") },
		{ TEXT("SpeakerFALCON"), TEXT("الصقر") },
		{ TEXT("SpeakerBULWARK"), TEXT("الحصن") },
		{ TEXT("SpeakerFROST"), TEXT("الصقيع") },
		{ TEXT("SpeakerEMBER"), TEXT("الجمرة") },
		{ TEXT("SpeakerGHOST"), TEXT("الشبح") },
		{ TEXT("SpeakerOVERWATCH"), TEXT("القيادة") },
		{ TEXT("SpeakerRAVEN"), TEXT("الغراب") },
		{ TEXT("SpeakerGENERAL VARGA"), TEXT("الجنرال فارغا") },

		// ---- Barks
		{ TEXT("BarkRinDeploy"), TEXT("المذنَّب تنطلق. حاولوا اللحاق بي!") },
		{ TEXT("BarkRinKill"), TEXT("بطيء جدًّا!") },
		{ TEXT("BarkRinStreak"), TEXT("صفّوهم لي وسأسقطهم واحدًا واحدًا!") },
		{ TEXT("BarkRinHurt"), TEXT("تبًّا، السيارة تتلقّى الضربات! تماسكي!") },
		{ TEXT("BarkRinAbility"), TEXT("الدفع الأقصى! كلوا غباري!") },
		{ TEXT("BarkRinBoss"), TEXT("واحد كبير. جيّد، كنت بدأت أملّ.") },
		{ TEXT("BarkRinVictory"), TEXT("الأولى عند خط النهاية. دائمًا.") },
		{ TEXT("BarkRinDefeat"), TEXT("لا... أنا لا أخسر سباقًا أبدًا...") },
		{ TEXT("BarkLaylaDeploy"), TEXT("الصقر في موقعها. الريح ثابتة.") },
		{ TEXT("BarkLaylaKill"), TEXT("سقط الهدف.") },
		{ TEXT("BarkLaylaStreak"), TEXT("نفَس واحد، طلقة واحدة. مرّة أخرى.") },
		{ TEXT("BarkLaylaHurt"), TEXT("الدرع ينهار. أحتاج إلى مسافة.") },
		{ TEXT("BarkLaylaAbility"), TEXT("عين الصقر. لا يتحرّك أحد.") },
		{ TEXT("BarkLaylaBoss"), TEXT("هدف كبير. إصابته أسهل.") },
		{ TEXT("BarkLaylaVictory"), TEXT("عمل نظيف. الميناء ينام الليلة.") },
		{ TEXT("BarkLaylaDefeat"), TEXT("أخطأتُ... الطلقة التي كانت تهمّ.") },
		{ TEXT("BarkKenjiDeploy"), TEXT("الحصن هنا. قفوا خلفي.") },
		{ TEXT("BarkKenjiKill"), TEXT("هذا لن يؤذي أحدًا بعد الآن.") },
		{ TEXT("BarkKenjiStreak"), TEXT("تعالوا. عندي درع يكفي الجميع!") },
		{ TEXT("BarkKenjiHurt"), TEXT("الصفائح تتشقّق... ليس بعد!") },
		{ TEXT("BarkKenjiAbility"), TEXT("الجدار الحديدي! لن تمرّوا!") },
		{ TEXT("BarkKenjiBoss"), TEXT("هذا جدار يستحقّ أن يُهدم.") },
		{ TEXT("BarkKenjiVictory"), TEXT("الكل ما زال يتنفّس؟ إذن هو يوم جيّد.") },
		{ TEXT("BarkKenjiDefeat"), TEXT("آسف... لم أستطع الصمود.") },
		{ TEXT("BarkYukiDeploy"), TEXT("الصقيع متّصلة. كل الأنظمة خضراء... تقريبًا.") },
		{ TEXT("BarkYukiKill"), TEXT("محرّكهم كان سيّئ الضبط على أي حال.") },
		{ TEXT("BarkYukiStreak"), TEXT("خردة وخردة وخردة أخرى. قطع غيار!") },
		{ TEXT("BarkYukiHurt"), TEXT("ذاك كان ناقل حركتي الجيّد! توقّفوا!") },
		{ TEXT("BarkYukiAbility"), TEXT("ترقيع أثناء السير. لا تجرّبوا هذا في البيت.") },
		{ TEXT("BarkYukiBoss"), TEXT("من لحَم ذلك الشيء لا ذوق له.") },
		{ TEXT("BarkYukiVictory"), TEXT("انتهت المهمة ولم يسقط شيء. رقم قياسي!") },
		{ TEXT("BarkYukiDefeat"), TEXT("أستطيع إصلاح... لا. هذه لا أستطيع إصلاحها.") },
		{ TEXT("BarkZaraDeploy"), TEXT("الجمرة جاهزة. ليقُل أحدكم «بوم».") },
		{ TEXT("BarkZaraKill"), TEXT("كرة نار بديعة!") },
		{ TEXT("BarkZaraStreak"), TEXT("حطب أكثر للنار!") },
		{ TEXT("BarkZaraHurt"), TEXT("حذارِ! هنا صواريخ!") },
		{ TEXT("BarkZaraAbility"), TEXT("الوابل! لوّنوا السماء!") },
		{ TEXT("BarkZaraBoss"), TEXT("آه، هذا سيترك حفرة جميلة.") },
		{ TEXT("BarkZaraVictory"), TEXT("دخان في الأفق. منظري المفضّل.") },
		{ TEXT("BarkZaraDefeat"), TEXT("هه... على الأقل كان انفجارًا كبيرًا...") },
		{ TEXT("BarkOmarDeploy"), TEXT("الشبح في الشبكة. لا يروننا.") },
		{ TEXT("BarkOmarKill"), TEXT("انقطعت الإشارة. نهائيًّا.") },
		{ TEXT("BarkOmarStreak"), TEXT("قناتهم كلها تصرخ.") },
		{ TEXT("BarkOmarHurt"), TEXT("وجدوني. ما كان يُفترض أن يحدث هذا.") },
		{ TEXT("BarkOmarAbility"), TEXT("النبضة انطلقت. أطفئوا الأنوار جميعًا.") },
		{ TEXT("BarkOmarBoss"), TEXT("ذلك الشيء جدرانه النارية أكثر من عقله.") },
		{ TEXT("BarkOmarVictory"), TEXT("وأنا لم أكن هنا قط.") },
		{ TEXT("BarkOmarDefeat"), TEXT("انقطع... الاتصال...") },

		// ---- Mission notices and markers
		{ TEXT("MisMission"), TEXT("المهمة") },
		{ TEXT("MisNew"), TEXT("هدف جديد") },
		{ TEXT("MisObjDone"), TEXT("اكتمل الهدف") },
		{ TEXT("MisTruckHome"), TEXT("شاحنة عبرت البوّابة") },
		{ TEXT("MisTruckLost"), TEXT("خسرنا شاحنة") },
		{ TEXT("MisTruckStopped"), TEXT("أُوقفت شاحنة") },
		{ TEXT("MisTruckAway"), TEXT("أفلتت شاحنة") },
		{ TEXT("MisSiteTaken"), TEXT("تم الاستيلاء على موقع") },
		{ TEXT("MisTargetDown"), TEXT("دُمّر الهدف") },
		{ TEXT("MisRelayLost"), TEXT("ضاعت المحطّة") },
		{ TEXT("MisRelayHalf"), TEXT("المحطّة بنصف قوّتها") },
		{ TEXT("MisTruckHalf"), TEXT("شاحنة متضرّرة بشدّة") },
		{ TEXT("MarkContested"), TEXT("متنازَع عليه") },
		{ TEXT("MarkGo"), TEXT("اذهب إلى هنا") },
		{ TEXT("MarkCapture"), TEXT("استولِ") },
		{ TEXT("MarkTarget"), TEXT("هدف") },
		{ TEXT("MarkDefend"), TEXT("دافع") },
		{ TEXT("MarkEscort"), TEXT("رافق") },
		{ TEXT("MarkGate"), TEXT("البوّابة") },

		// ---- Campaign screens
		{ TEXT("MenuCampaign"), TEXT("الحملة") },
		{ TEXT("MenuCampaignSub"), TEXT("اكسر حصار مرسى: ثماني مهمات قصصية") },
		{ TEXT("MenuSurvival"), TEXT("البقاء") },
		{ TEXT("MenuSurvivalSub"), TEXT("موجات بلا نهاية، متجر التطويرات، وأفضل نتيجة لك") },
		{ TEXT("MenuProgress"), TEXT("المهمات {0}/{1}     النجوم {2}/{3}") },
		{ TEXT("MenuBest"), TEXT("الأفضل: {0} نقطة، الموجة {1}") },
		{ TEXT("MenuKeysTop"), TEXT("F3/F4: اختيار     Enter: تأكيد     Esc / F1: الإعدادات") },
		{ TEXT("MenuKeys"), TEXT("F3/F4: اختيار     Enter: تأكيد     F2: رجوع") },
		{ TEXT("MenuKeysShort"), TEXT("F3/F4: اختيار     Enter: تأكيد") },
		{ TEXT("MenuMissions"), TEXT("اختر مهمة") },
		{ TEXT("MenuLocked"), TEXT("مقفلة") },
		{ TEXT("MenuLockedHint"), TEXT("أكمل المهمة التي قبلها لفتح هذه.") },
		{ TEXT("MenuPar"), TEXT("الوقت المستهدف") },
		{ TEXT("MenuStars"), TEXT("النجوم") },
		{ TEXT("MenuStarBonus"), TEXT("عند 8 نجوم: تصفيح درع   عند 16: ترقية الرشاش والصواريخ") },
		{ TEXT("MenuBriefing"), TEXT("الإحاطة") },
		{ TEXT("MenuObjectives"), TEXT("الأهداف") },
		{ TEXT("MenuIssued"), TEXT("العتاد المصروف لهذه المهمة") },
		{ TEXT("MenuStock"), TEXT("رشاش وصواريخ") },
		{ TEXT("MenuContinue"), TEXT("Enter: متابعة     F2: رجوع") },
		{ TEXT("MenuDriver"), TEXT("اختر سائقك") },
		{ TEXT("MenuPerk"), TEXT("الميزة") },
		{ TEXT("MenuAbility"), TEXT("القدرة") },
		{ TEXT("MenuDriverLocked"), TEXT("ينضمّ بعد المهمة {0}") },
		{ TEXT("MenuCooldown"), TEXT("إعادة الشحن {0} ث") },
		{ TEXT("MisComplete"), TEXT("اكتملت المهمة") },
		{ TEXT("MisFailed"), TEXT("فشلت المهمة") },
		{ TEXT("MisTime"), TEXT("الوقت") },
		{ TEXT("MisHealth"), TEXT("صحة السيارة") },
		{ TEXT("MisLost"), TEXT("خسائر (شاحنات / محطّة)") },
		{ TEXT("MisStarDone"), TEXT("إكمال المهمة") },
		{ TEXT("MisStarTime"), TEXT("ضمن الوقت المستهدف") },
		{ TEXT("MisStarCare"), TEXT("صحة 50% فأكثر وبلا خسائر") },
		{ TEXT("MisNewBest"), TEXT("رقم قياسي جديد") },
		{ TEXT("MisNewDriver"), TEXT("سائق جديد") },
		{ TEXT("MisNextIssue"), TEXT("يُصرف في المهمة التالية") },
		{ TEXT("MisNext"), TEXT("المهمة التالية") },
		{ TEXT("MisRetry"), TEXT("إعادة المحاولة") },
		{ TEXT("MisMenu"), TEXT("القائمة الرئيسية") },
		{ TEXT("MisCampaignDone"), TEXT("انكسر الحصار") },
		{ TEXT("HudAbilityReady"), TEXT("جاهزة") },
		{ TEXT("HudMenuBack"), TEXT("F2: القائمة الرئيسية") },
		{ TEXT("NoticeLeave"), TEXT("اضغط F2 مرة أخرى للعودة إلى القائمة الرئيسية") },
		{ TEXT("KAbility"), TEXT("قدرة السائق") },
	};
	return Table;
}

// Portraits read so far, by file stem ("Rin", "Rin_happy"...); null entries remember a missing
// file so the disk is asked once. Rooted: nothing else holds them, and the HUD draws them all session.
TMap<FString, UTexture2D*>& PortraitCache()
{
	static TMap<FString, UTexture2D*> Cache;
	return Cache;
}

UTexture2D* LoadPortraitFile(const FString& Stem)
{
	if (UTexture2D** Found = PortraitCache().Find(Stem))
	{
		return *Found;
	}
	UTexture2D* Texture = nullptr;
	const FString Folder = FPaths::ProjectContentDir() / TEXT("IronSiege") / TEXT("Characters");
	for (const TCHAR* Extension : { TEXT(".png"), TEXT(".jpg"), TEXT(".jpeg") })
	{
		const FString Path = Folder / (Stem + Extension);
		FImage Picture;
		if (!IFileManager::Get().FileExists(*Path) || !FImageUtils::LoadImage(*Path, Picture) || Picture.SizeX < 8 || Picture.SizeY < 8)
		{
			continue;
		}
		// Square first: the middle of a wide picture, the top of a tall one (where the face is).
		Picture.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const int32 Side = FMath::Min(Picture.SizeX, Picture.SizeY);
		const int32 Left = (Picture.SizeX - Side) / 2;
		FImage Square;
		Square.Init(Side, Side, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const TArrayView64<FColor> From = Picture.AsBGRA8(), To = Square.AsBGRA8();
		for (int32 Y = 0; Y < Side; ++Y)
		{
			FMemory::Memcpy(&To[int64(Y) * Side], &From[int64(Y) * Picture.SizeX + Left], Side * sizeof(FColor));
		}
		// 512 px at most (the biggest it is ever drawn is about 340), with every mip below it: art
		// arrives at 1024 px and the radio box shows it at 132, which shimmers without them.
		const int32 Top = 512;
		Texture = UTexture2D::CreateTransient(Top, Top, PF_B8G8R8A8);
		if (!Texture)
		{
			break;
		}
		FTexturePlatformData* Data = Texture->GetPlatformData();
		for (int32 Size = Top, Level = 0; Size >= 8; Size /= 2, ++Level)
		{
			FImage Mip;
			FImageCore::ResizeImageAllocDest(Square, Mip, Size, Size, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			if (Level > 0)
			{
				Data->Mips.Add(new FTexture2DMipMap(Size, Size));
			}
			FByteBulkData& Bulk = Data->Mips[Level].BulkData;
			Bulk.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(Bulk.Realloc(int64(Size) * Size * 4), Mip.RawData.GetData(), int64(Size) * Size * 4);
			Bulk.Unlock();
		}
		Texture->SRGB = true;
		Texture->Filter = TF_Trilinear;
		Texture->NeverStream = true;
		Texture->UpdateResource();
		Texture->AddToRoot();
		break;
	}
	PortraitCache().Add(Stem, Texture);
	return Texture;
}
}

namespace IronStory
{
const FString* FindArabic(const FString& Key)
{
	return ArabicTable().Find(Key);
}

FString MissionName(int32 MissionIndex)
{
	const IronMissions::Mission& M = IronMissions::Get(MissionIndex);
	return IronText::Str(*(FString(ANSI_TO_TCHAR(M.Key)) + TEXT("Name")), ANSI_TO_TCHAR(M.Name));
}

FString SpeakerName(int32 Speaker)
{
	return IronText::Name(TEXT("Speaker"), ANSI_TO_TCHAR(IronCrew::SpeakerName(Speaker)));
}

FLinearColor SpeakerColor(int32 Speaker)
{
	static const FLinearColor Colors[IronCrew::SpeakerCount] = {
		FLinearColor(1.f, 0.42f, 0.22f),  // Rin: racing orange.
		FLinearColor(0.2f, 0.85f, 0.75f), // Layla: teal.
		FLinearColor(0.45f, 0.6f, 1.f),   // Kenji: steel blue.
		FLinearColor(0.6f, 0.92f, 1.f),   // Yuki: ice.
		FLinearColor(1.f, 0.62f, 0.15f),  // Zara: ember.
		FLinearColor(0.72f, 0.5f, 1.f),   // Omar: violet.
		FLinearColor(0.85f, 0.9f, 1.f),   // Hana: command white.
		FLinearColor(0.78f, 0.3f, 0.9f),  // Raven: purple.
		FLinearColor(1.f, 0.22f, 0.18f),  // Varga: red.
	};
	return Colors[Speaker >= 0 && Speaker < IronCrew::SpeakerCount ? Speaker : IronCrew::SpeakerHana];
}

UTexture2D* Portrait(int32 Speaker, int32 Mood)
{
	const FString Key = ANSI_TO_TCHAR(IronCrew::SpeakerKey(Speaker));
	if (Mood == IronCrew::MoodHappy || Mood == IronCrew::MoodAngry)
	{
		if (UTexture2D* Moody = LoadPortraitFile(Key + (Mood == IronCrew::MoodHappy ? TEXT("_happy") : TEXT("_angry"))))
		{
			return Moody;
		}
	}
	return LoadPortraitFile(Key);
}

IronMissions::Progress LoadProgress()
{
	IronMissions::Progress Progress;
	FString Saved;
	if (GConfig && GConfig->GetString(GCampaignSection, TEXT("Stars"), Saved, GGameIni))
	{
		TArray<FString> Parts;
		Saved.ParseIntoArray(Parts, TEXT(","));
		for (int32 i = 0; i < Parts.Num() && i < IronMissions::Count; ++i)
		{
			Progress.Best[i] = FMath::Clamp(FCString::Atoi(*Parts[i]), 0, 3);
		}
		// Missions open in order: a result with a gap before it cannot have been earned.
		for (int32 i = 1; i < IronMissions::Count; ++i)
		{
			if (Progress.Best[i - 1] <= 0)
			{
				Progress.Best[i] = 0;
			}
		}
	}
	return Progress;
}

void SaveProgress(const IronMissions::Progress& Progress)
{
	if (!GConfig)
	{
		return;
	}
	TArray<FString> Parts;
	for (int32 i = 0; i < IronMissions::Count; ++i)
	{
		Parts.Add(FString::FromInt(Progress.Best[i]));
	}
	GConfig->SetString(GCampaignSection, TEXT("Stars"), *FString::Join(Parts, TEXT(",")), GGameIni);
	GConfig->Flush(false, GGameIni);
}

void LoadLastChoice(int32& Driver, int32& Vehicle)
{
	if (GConfig)
	{
		GConfig->GetInt(GCampaignSection, TEXT("LastDriver"), Driver, GGameIni);
		GConfig->GetInt(GCampaignSection, TEXT("LastVehicle"), Vehicle, GGameIni);
	}
}

void SaveLastChoice(int32 Driver, int32 Vehicle)
{
	if (GConfig)
	{
		GConfig->SetInt(GCampaignSection, TEXT("LastDriver"), Driver, GGameIni);
		GConfig->SetInt(GCampaignSection, TEXT("LastVehicle"), Vehicle, GGameIni);
		GConfig->Flush(false, GGameIni);
	}
}
}
