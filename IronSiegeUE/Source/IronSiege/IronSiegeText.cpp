#include "IronSiegeText.h"
#include "IronSiegeUserSettings.h"
#include "IronSiegeStory.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Styling/CoreStyle.h"
#include "Math/UnrealMathUtility.h"

namespace
{
// Arabic UI strings, keyed by the LOCTEXT key of the English source. Anything missing falls back
// to English, so a half-finished translation still gives a working menu.
const TMap<FString, FString>& ArabicTable()
{
	static const TMap<FString, FString> Table = {
		// ---- tabs and frame
		{ TEXT("Title"), TEXT("الإعدادات") },
		{ TEXT("Sub"), TEXT("آيرون سيج") },
		{ TEXT("TVideo"), TEXT("الفيديو") },
		{ TEXT("TAudio"), TEXT("الصوت") },
		{ TEXT("TControls"), TEXT("التحكم") },
		{ TEXT("TCamera"), TEXT("الكاميرا") },
		{ TEXT("TVoice"), TEXT("المايك والمحادثة") },
		{ TEXT("TKeys"), TEXT("الاختصارات") },
		{ TEXT("TGameplay"), TEXT("اللعب والواجهة") },
		{ TEXT("TAccess"), TEXT("سهولة الوصول") },
		{ TEXT("TProfiles"), TEXT("الملفات الشخصية") },
		{ TEXT("EscHint"), TEXT("Esc  تجاهل وإغلاق") },
		{ TEXT("ResetTab"), TEXT("إعادة ضبط هذا التبويب") },
		{ TEXT("Discard"), TEXT("تجاهل") },
		{ TEXT("Apply"), TEXT("تطبيق") },
		{ TEXT("SaveClose"), TEXT("حفظ وإغلاق") },
		{ TEXT("Saved"), TEXT("تم تطبيق الإعدادات وحفظها") },
		{ TEXT("On"), TEXT("تشغيل") },
		{ TEXT("Off"), TEXT("إيقاف") },
		{ TEXT("Shown"), TEXT("ظاهر") },
		{ TEXT("Hidden"), TEXT("مخفي") },
		{ TEXT("HiddenT"), TEXT("مخفي") },
		{ TEXT("Custom"), TEXT("مخصص") },
		{ TEXT("Unlimited"), TEXT("بلا حد") },

		// ---- video
		{ TEXT("Display"), TEXT("العرض") },
		{ TEXT("WindowMode"), TEXT("وضع النافذة") },
		{ TEXT("Fullscreen"), TEXT("ملء الشاشة") },
		{ TEXT("Borderless"), TEXT("نافذة بلا إطار") },
		{ TEXT("Windowed"), TEXT("نافذة") },
		{ TEXT("Resolution"), TEXT("دقة الشاشة") },
		{ TEXT("ResScale"), TEXT("مقياس الرسم") },
		{ TEXT("FrameCap"), TEXT("حد الإطارات") },
		{ TEXT("VSync"), TEXT("المزامنة الرأسية") },
		{ TEXT("Brightness"), TEXT("السطوع") },
		{ TEXT("Quality"), TEXT("جودة الرسوم") },
		{ TEXT("AutoDetect"), TEXT("اكتشاف أفضل إعداد لهذا الجهاز") },
		{ TEXT("RunBenchmark"), TEXT("كشف تلقائي") },
		{ TEXT("Benchmarked"), TEXT("تم الكشف: الجودة العامة {0}") },
		{ TEXT("Benchmark"), TEXT("قياس الأداء أثناء اللعب (٨ ثوانٍ)") },
		{ TEXT("RunBench"), TEXT("تشغيل القياس") },
		{ TEXT("BenchResult"), TEXT("القياس {0} إطار/ث (الهدف {1}) - ضُبطت الجودة على {2}") },
		{ TEXT("Overall"), TEXT("الإعداد العام") },
		{ TEXT("ViewDist"), TEXT("مدى الرؤية") },
		{ TEXT("AA"), TEXT("تنعيم الحواف") },
		{ TEXT("Shadows"), TEXT("الظلال") },
		{ TEXT("GI"), TEXT("الإضاءة العامة") },
		{ TEXT("Reflections"), TEXT("الانعكاسات") },
		{ TEXT("PostFx"), TEXT("المعالجة البعدية") },
		{ TEXT("Textures"), TEXT("الخامات") },
		{ TEXT("Effects"), TEXT("المؤثرات") },
		{ TEXT("Foliage"), TEXT("النباتات") },
		{ TEXT("Shading"), TEXT("التظليل") },
		{ TEXT("MotionBlur"), TEXT("ضبابية الحركة") },
		{ TEXT("VideoNote"), TEXT("وضع العرض والدقة والجودة تُطبَّق عند الضغط على «تطبيق». السطوع وضبابية الحركة تظهر فورًا.") },

		// ---- audio
		{ TEXT("Volume"), TEXT("مستوى الصوت") },
		{ TEXT("Master"), TEXT("الصوت الرئيسي") },
		{ TEXT("EffectsVol"), TEXT("الأسلحة والانفجارات") },
		{ TEXT("EngineVol"), TEXT("محركات السيارات") },
		{ TEXT("UiVol"), TEXT("الواجهة والتنبيهات") },
		{ TEXT("AudioNote"), TEXT("التغييرات تُسمع فورًا. مستوى المحادثة الصوتية في تبويب المايك.") },

		// ---- controls
		{ TEXT("Steering"), TEXT("التوجيه") },
		{ TEXT("SteerSens"), TEXT("حساسية التوجيه") },
		{ TEXT("SteerSmooth"), TEXT("نعومة التوجيه") },
		{ TEXT("Feel"), TEXT("إحساس القيادة") },
		{ TEXT("DriveStyle"), TEXT("نمط القيادة") },
		{ TEXT("Arcade"), TEXT("أركيد") },
		{ TEXT("Balanced"), TEXT("متوازن") },
		{ TEXT("Sim"), TEXT("محاكاة") },
		{ TEXT("DriveStyleNote"), TEXT("الأركيد يلتصق بالطريق ويكبح بقوة؛ المحاكاة أقل تماسكًا مع فرامل أضعف وفرملة يد أنشط للانزلاق. يُطبَّق على سيارتك فورًا.") },
		{ TEXT("Gamepad"), TEXT("يد التحكم") },
		{ TEXT("Deadzone"), TEXT("المنطقة الميتة للعصا") },
		{ TEXT("Vibration"), TEXT("قوة الاهتزاز") },
		{ TEXT("TestRumble"), TEXT("اختبار الاهتزاز") },
		{ TEXT("TestRumbleBtn"), TEXT("هزّ اليد") },
		{ TEXT("PadNote"), TEXT("الزنادان للقيادة، العصا اليسرى للتوجيه، الأزرار العلوية للإطلاق، وزر Start يفتح هذه القائمة. يمكن تغيير الأزرار من تبويب الاختصارات.") },
		{ TEXT("Weapons"), TEXT("الأسلحة") },
		{ TEXT("AutoReload"), TEXT("إعادة تذخير تلقائية عند النفاد") },
		{ TEXT("ControlsNote"), TEXT("تعيين المفاتيح في تبويب الاختصارات.") },

		// ---- camera
		{ TEXT("ChaseCam"), TEXT("كاميرا المطاردة") },
		{ TEXT("Fov"), TEXT("مجال الرؤية") },
		{ TEXT("CamDist"), TEXT("البُعد") },
		{ TEXT("CamHeight"), TEXT("الارتفاع") },
		{ TEXT("CamSmooth"), TEXT("النعومة") },
		{ TEXT("CamNote"), TEXT("التغييرات تظهر على سيارتك فورًا. بدّل رؤية المقصورة أثناء اللعب بمفتاح تبديل الكاميرا.") },

		// ---- voice
		{ TEXT("VoiceIntro"), TEXT("اللعبة فردية حاليًا: تُحفظ هذه الإعدادات للمحادثة الصوتية عند إضافة اللعب الجماعي. اختبار المايك بالأسفل يعمل بجهازك الحقيقي الآن.") },
		{ TEXT("Microphone"), TEXT("الميكروفون") },
		{ TEXT("InputDevice"), TEXT("جهاز الإدخال") },
		{ TEXT("DefaultDevice"), TEXT("الجهاز الافتراضي") },
		{ TEXT("MicGain"), TEXT("كسب الإدخال") },
		{ TEXT("MicTest"), TEXT("اختبار الميكروفون") },
		{ TEXT("MicTestToggle"), TEXT("بدء / إيقاف الاختبار") },
		{ TEXT("MicLevel"), TEXT("مستوى الإدخال") },
		{ TEXT("MicOn"), TEXT("اختبار الميكروفون يعمل - تكلّم الآن") },
		{ TEXT("MicFail"), TEXT("تعذّر فتح الميكروفون") },
		{ TEXT("VoiceChat"), TEXT("المحادثة الصوتية") },
		{ TEXT("TalkMode"), TEXT("نمط الإرسال") },
		{ TEXT("PTT"), TEXT("اضغط للتحدث") },
		{ TEXT("OpenMic"), TEXT("مايك مفتوح") },
		{ TEXT("PttKey"), TEXT("مفتاح الضغط للتحدث") },
		{ TEXT("VoiceVol"), TEXT("مستوى المحادثة") },

		// ---- shortcuts
		{ TEXT("Driving"), TEXT("القيادة والقتال") },
		{ TEXT("Unbound"), TEXT("- غير معيّن -") },
		{ TEXT("AxisKey"), TEXT("{0} (محور)") },
		{ TEXT("Bound"), TEXT("تم تعيين {0}") },
		{ TEXT("PressKey"), TEXT("اضغط مفتاحًا أو زر ماوس") },
		{ TEXT("PressPad"), TEXT("اضغط زرًا على يد التحكم") },
		{ TEXT("PressKeyHint"), TEXT("Esc للإلغاء") },
		{ TEXT("RebindNote"), TEXT("العمود الأيسر: لوحة المفاتيح والماوس. العمود الأيمن: يد التحكم. اضغط أيًّا منهما لتغييره - وتعيين زر مستخدَم ينقله إلى هنا. التوجيه والزنادان ثابتان على العصا.") },
		{ TEXT("Fixed"), TEXT("القوائم (ثابتة)") },
		{ TEXT("OpenSettings"), TEXT("فتح / إغلاق الإعدادات") },
		{ TEXT("MenuPick"), TEXT("القوائم ومتجر التطوير: الاختيار") },
		{ TEXT("MenuConfirm"), TEXT("القوائم ومتجر التطوير: التأكيد") },

		// ---- gameplay & HUD
		{ TEXT("Challenge"), TEXT("التحدي") },
		{ TEXT("Difficulty"), TEXT("الصعوبة") },
		{ TEXT("DiffNote"), TEXT("يغيّر قوة نيران الأعداء ودقتها. يسري على الأعداء الذين يظهرون من الآن.") },
		{ TEXT("Hud"), TEXT("واجهة اللعب") },
		{ TEXT("Crosshair"), TEXT("مؤشر التصويب") },
		{ TEXT("Cross"), TEXT("صليب") },
		{ TEXT("Dot"), TEXT("نقطة") },
		{ TEXT("Circle"), TEXT("دائرة") },
		{ TEXT("Minimap"), TEXT("الرادار") },
		{ TEXT("EnemyBars"), TEXT("أشرطة صحة الأعداء") },
		{ TEXT("Fps"), TEXT("عدّاد الإطارات") },

		// ---- accessibility
		{ TEXT("Language"), TEXT("اللغة") },
		{ TEXT("UiLanguage"), TEXT("لغة الواجهة") },
		{ TEXT("English"), TEXT("English") },
		{ TEXT("Readability"), TEXT("وضوح القراءة") },
		{ TEXT("HudSize"), TEXT("حجم نصوص الواجهة والعلامات") },
		{ TEXT("Colorblind"), TEXT("وضع عمى الألوان") },
		{ TEXT("CbOff"), TEXT("إيقاف") },
		{ TEXT("CbProt"), TEXT("عمى الأحمر (بروتانوبيا)") },
		{ TEXT("CbDeut"), TEXT("عمى الأخضر (ديوتيرانوبيا)") },
		{ TEXT("CbTrit"), TEXT("عمى الأزرق (تريتانوبيا)") },
		{ TEXT("CbNote"), TEXT("الأعداء وعلب الإصلاح والذخيرة مرمَّزة بالألوان؛ هذه اللوحات تنقلها إلى ألوان يسهل تمييزها.") },
		{ TEXT("Motion"), TEXT("الحركة") },
		{ TEXT("ReduceMotion"), TEXT("تقليل حركة الكاميرا") },
		{ TEXT("MotionNote"), TEXT("يبقي الكاميرا ملتصقة خلف السيارة ويخفّف الاهتزاز، وهو أفضل لمن يشعر بدوار الحركة.") },

		// ---- profiles
		{ TEXT("ActiveProfile"), TEXT("الملف النشط") },
		{ TEXT("CurrentProfile"), TEXT("الحالي") },
		{ TEXT("SaveAs"), TEXT("حفظ الإعدادات الحالية باسم") },
		{ TEXT("ProfileHint"), TEXT("اسم الملف") },
		{ TEXT("SaveProfile"), TEXT("حفظ") },
		{ TEXT("LoadProfile"), TEXT("تحميل") },
		{ TEXT("DeleteProfile"), TEXT("حذف") },
		{ TEXT("SavedProfiles"), TEXT("الملفات المحفوظة") },
		{ TEXT("NoProfiles"), TEXT("لا توجد ملفات محفوظة بعد. اضبط اللعبة كما تحب، ثم اكتب اسمًا بالأعلى واضغط حفظ.") },
		{ TEXT("ProfileSaved"), TEXT("تم حفظ الملف \"{0}\"") },
		{ TEXT("ProfileLoaded"), TEXT("تم تحميل الملف \"{0}\"") },
		{ TEXT("ProfileNote"), TEXT("الملف الشخصي يحفظ كل إعدادات هذه القائمة - الفيديو والصوت والتحكم والمفاتيح والكاميرا وسهولة الوصول - ليتبادل لاعبان على جهاز واحد إعداداتهما.") },

		// ---- HUD (Canvas strings, see IronText::Str)
		{ TEXT("HudSpeed"), TEXT("السرعة") },
		{ TEXT("HudHealth"), TEXT("الصحة") },
		{ TEXT("HudArmor"), TEXT("الدرع") },
		{ TEXT("HudMg"), TEXT("الرشاش") },
		{ TEXT("HudRockets"), TEXT("الصواريخ") },
		{ TEXT("HudKph"), TEXT("كم/س") },
		{ TEXT("HudSettingsHint"), TEXT("Esc / F1: الإعدادات") },
		{ TEXT("HudWave"), TEXT("الموجة") },
		{ TEXT("HudEnemies"), TEXT("الأعداء") },
		{ TEXT("HudKills"), TEXT("القتلى") },
		{ TEXT("HudScore"), TEXT("النقاط") },
		{ TEXT("HudReloading"), TEXT(" (إعادة تذخير)") },
		{ TEXT("HudLock"), TEXT("قفل") },
		{ TEXT("Image"), TEXT("الصورة") },
		{ TEXT("AaMethod"), TEXT("طريقة تنعيم الحواف") },
		{ TEXT("AaOff"), TEXT("إيقاف") },
		{ TEXT("AaTsr"), TEXT("TSR (الأفضل، أثقل)") },
		{ TEXT("Sharpness"), TEXT("الحدّة") },
		{ TEXT("Bloom"), TEXT("التوهّج حول الأضواء") },
		{ TEXT("Ao"), TEXT("الظلال المحيطية") },
		{ TEXT("Ssr"), TEXT("الانعكاسات") },
		{ TEXT("VolFog"), TEXT("الضباب الحجمي") },
		{ TEXT("ImageNote"), TEXT("على كرت شاشة 4 جيجابايت: TSR والانعكاسات هما الأثقل، وFXAA مع إطفاء الانعكاسات يعطيان أكبر زيادة في الإطارات.") },
		{ TEXT("AudioOptions"), TEXT("خيارات") },
		{ TEXT("MuteUnfocused"), TEXT("كتم الصوت حين تكون اللعبة في الخلفية") },
		{ TEXT("HitSounds"), TEXT("صوت تأكيد الإصابة") },
		{ TEXT("AutoFlares"), TEXT("الشراك الحرارية التلقائية") },
		{ TEXT("AutoFlaresNote"), TEXT("تطلق الشراك وحدها حين يقترب صاروخ موجّه. أسهل، لكنها قد تصرف شحنات كنت ستوقّتها أفضل.") },
		{ TEXT("DmgNumbers"), TEXT("أرقام الضرر") },
		{ TEXT("SpeedUnits"), TEXT("وحدة السرعة") },
		{ TEXT("RadarZoom"), TEXT("مدى الرادار") },
		{ TEXT("ZoomClose"), TEXT("قريب (30 م)") },
		{ TEXT("ZoomNormal"), TEXT("عادي (45 م)") },
		{ TEXT("ZoomWide"), TEXT("واسع (70 م)") },
		{ TEXT("HudOpacity"), TEXT("شفافية نصوص الواجهة") },
		{ TEXT("KRail"), TEXT("المدفع الكهرومغناطيسي") },
		{ TEXT("KTesla"), TEXT("ملف تسلا") },
		{ TEXT("KFlares"), TEXT("الشراك الحرارية (ضد الصواريخ)") },
		{ TEXT("HudFlares"), TEXT("الشراك الحرارية") },
		{ TEXT("HudRailgun"), TEXT("المدفع الكهرومغناطيسي") },
		{ TEXT("HudTesla"), TEXT("ملف تسلا") },
		{ TEXT("HudCharging"), TEXT("  (يشحن)") },
		{ TEXT("HudMissileWarn"), TEXT("صاروخ موجَّه قادم!") },
		{ TEXT("NoticeHunters"), TEXT("صيّادو صواريخ - استعمل الشراك") },
		{ TEXT("NoticeStormers"), TEXT("عاصفو تسلا - ابتعد عنهم") },
		{ TEXT("NoticeLancers"), TEXT("رامحو المدفع الكهرومغناطيسي - انعطف حين يشحنون") },
		{ TEXT("HudRailWarn"), TEXT("مدفع معادٍ يشحن!") },
		{ TEXT("HudSlamWarn"), TEXT("موجة صدمية - ابتعد!") },
		{ TEXT("HudEmpWarn"), TEXT("نبضة كهرومغناطيسية تُشحن - ابتعد!") },
		{ TEXT("HudBossPhase"), TEXT("المرحلة") },
		{ TEXT("HudBossShield"), TEXT("درع مفعّل") },
		{ TEXT("HudFuel"), TEXT("الوقود") },
		{ TEXT("HudFlameHeat"), TEXT("الفوهة") },
		{ TEXT("HudMines"), TEXT("الألغام") },
		{ TEXT("HudNitro"), TEXT("نيترو") },
		{ TEXT("HudHeat"), TEXT("حرارة") },
		{ TEXT("HudOverheat"), TEXT("سخونة زائدة") },
		{ TEXT("HudPhoto"), TEXT("نمط التصوير - P للخروج") },

		// ---- controls (shortcuts tab)
		{ TEXT("KAccel"), TEXT("التسارع") },
		{ TEXT("KBrake"), TEXT("الفرامل / الرجوع للخلف") },
		{ TEXT("KLeft"), TEXT("التوجيه لليسار") },
		{ TEXT("KRight"), TEXT("التوجيه لليمين") },
		{ TEXT("KHand"), TEXT("فرامل اليد") },
		{ TEXT("KBoost"), TEXT("دفعة النيترو") },
		{ TEXT("KMine"), TEXT("إلقاء لغم") },
		{ TEXT("KFlame"), TEXT("قاذف اللهب") },
		{ TEXT("KFire1"), TEXT("إطلاق الرشاش") },
		{ TEXT("KFire2"), TEXT("إطلاق الصواريخ") },
		{ TEXT("KRel1"), TEXT("تعبئة الرشاش") },
		{ TEXT("KRel2"), TEXT("تعبئة الصواريخ") },
		{ TEXT("KCam"), TEXT("تبديل زاوية الكاميرا") },
		{ TEXT("KPtt"), TEXT("اضغط للتحدث") },
		{ TEXT("QLow"), TEXT("منخفضة") },
		{ TEXT("QMedium"), TEXT("متوسطة") },
		{ TEXT("QHigh"), TEXT("عالية") },
		{ TEXT("QEpic"), TEXT("ملحمية") },
		{ TEXT("QCinematic"), TEXT("سينمائية") },
		{ TEXT("DiffEasy"), TEXT("سهل") },
		{ TEXT("DiffNormal"), TEXT("عادي") },
		{ TEXT("DiffHard"), TEXT("صعب") },

		// ---- main menu, car select, game over
		{ TEXT("HudBattlefield"), TEXT("ساحة المعركة") },
		{ TEXT("MapArctic"), TEXT("القطب الشمالي") },
		{ TEXT("MapDesert"), TEXT("الصحراء") },
		{ TEXT("MapCityRuins"), TEXT("أطلال المدينة") },
		{ TEXT("MapCoast"), TEXT("الساحل") },
		{ TEXT("HudChangeMap"), TEXT("F3/F4: تغيير ساحة المعركة") },
		{ TEXT("HudDeploy"), TEXT("Enter: انطلق") },
		{ TEXT("HudChooseCar"), TEXT("اختر مركبتك") },
		{ TEXT("CarSCOUT"), TEXT("الكشّافة") },
		{ TEXT("CarASSAULT"), TEXT("الهجومية") },
		{ TEXT("CarHEAVY"), TEXT("الثقيلة") },
		{ TEXT("CarARTILLERY"), TEXT("المدفعية") },
		{ TEXT("CarINTERCEPTOR"), TEXT("المعترِضة") },
		{ TEXT("CarDUNE"), TEXT("الكثبان") },
		{ TEXT("BlurbSCOUT"), TEXT("سريعة وخفيفة - اضرب واهرب.") },
		{ TEXT("BlurbASSAULT"), TEXT("متوازنة تصلح لكل المهام.") },
		{ TEXT("BlurbHEAVY"), TEXT("بطيئة ومدرّعة وضربتها قوية.") },
		{ TEXT("BlurbARTILLERY"), TEXT("إسناد - تعتمد على الصواريخ.") },
		{ TEXT("BlurbINTERCEPTOR"), TEXT("سيدان رياضية - سريعة، وأصلب مما تبدو.") },
		{ TEXT("BlurbDUNE"), TEXT("باجي للطرق الوعرة - تقفز وتنزلق وتراوغ.") },
		{ TEXT("HudTopSpeed"), TEXT("السرعة القصوى") },
		{ TEXT("HudMass"), TEXT("الوزن") },
		{ TEXT("AimAssist"), TEXT("مساعدة التصويب") },
		{ TEXT("AimNormal"), TEXT("عادية") },
		{ TEXT("AimStrong"), TEXT("قوية") },
		{ TEXT("AimNote"), TEXT("يصوّب مدفع السقف للأعلى والأسفل نحو أقرب سيارة للمؤشر، ويميل قليلًا للجانبين. «قوية» توسّع المساعدة الجانبية.") },
		{ TEXT("HudTonnes"), TEXT("طن") },
		{ TEXT("TraitSCOUT"), TEXT("رشاش سريع الإطلاق، ونيترو يمتلئ بسرعة") },
		{ TEXT("TraitASSAULT"), TEXT("أسلحة قياسية بلا نقطة ضعف") },
		{ TEXT("TraitHEAVY"), TEXT("طلقات ثقيلة، وصدمته موجعة") },
		{ TEXT("TraitARTILLERY"), TEXT("صواريخ أقوى بـ50%، وتعبئة أسرع") },
		{ TEXT("TraitINTERCEPTOR"), TEXT("رشاش بطيء السخونة: رشقات أطول") },
		{ TEXT("TraitDUNE"), TEXT("نيترو قوي، ويهبط من القفزات على عجلاته") },
		{ TEXT("HudCarKeys"), TEXT("F3/F4: تغيير     Enter: تأكيد") },
		{ TEXT("HudDestroyed"), TEXT("دُمّرت مركبتك") },
		{ TEXT("HudReachedWave"), TEXT("وصلت إلى الموجة") },
		{ TEXT("HudBest"), TEXT("الأفضل") },
		{ TEXT("HudBestLine"), TEXT("{0} نقطة  (الموجة {1}، {2} قتيل)") },
		{ TEXT("HudRedeploy"), TEXT("Enter: انطلق من جديد") },

		// ---- waves, shop, notices
		{ TEXT("HudWaveClearedShop"), TEXT("تم تطهير الموجة {0}  -  المتجر مفتوح") },
		{ TEXT("HudWaveClearedNext"), TEXT("تم تطهير الموجة {0}  -  الموجة التالية بعد {1}") },
		{ TEXT("HudCombo"), TEXT("سلسلة") },
		{ TEXT("HudShop"), TEXT("متجر التطويرات") },
		{ TEXT("HudCredits"), TEXT("الرصيد") },
		{ TEXT("HudSec"), TEXT("ث") },
		{ TEXT("HudLv"), TEXT("مستوى") },
		{ TEXT("HudMax"), TEXT("مكتمل") },
		{ TEXT("HudContinue"), TEXT("المتابعة إلى الموجة التالية") },
		{ TEXT("HudShopKeys"), TEXT("F3/F4: اختيار     Enter: شراء / متابعة") },
		{ TEXT("UpARMOR PLATING"), TEXT("تصفيح الدرع") },
		{ TEXT("UpMACHINE GUN"), TEXT("الرشاش") },
		{ TEXT("UpROCKET POD"), TEXT("حاضنة الصواريخ") },
		{ TEXT("UpENGINE TUNE"), TEXT("ضبط المحرك") },
		{ TEXT("UpMINE LAYER"), TEXT("زارع الألغام") },
		{ TEXT("UpFLAME TANK"), TEXT("خزان اللهب") },
		{ TEXT("UpRAILGUN"), TEXT("المدفع الكهرومغناطيسي") },
		{ TEXT("UpTESLA COIL"), TEXT("ملف تسلا") },
		{ TEXT("UpFIELD REPAIR"), TEXT("إصلاح ميداني") },
		{ TEXT("Up+25% max armor per level"), TEXT("+25% للدرع الأقصى لكل مستوى") },
		{ TEXT("Up+20% bullet damage per level"), TEXT("+20% لضرر الرصاص لكل مستوى") },
		{ TEXT("Up+25% rocket damage per level"), TEXT("+25% لضرر الصواريخ لكل مستوى") },
		{ TEXT("Up+15% engine power per level"), TEXT("+15% لقوة المحرك لكل مستوى") },
		{ TEXT("UpUnlocks mines, +2 spare each"), TEXT("يفتح الألغام، +2 احتياط لكل مستوى") },
		{ TEXT("UpUnlocks flamer, +25% burn each"), TEXT("يفتح قاذف اللهب، +25% حرق لكل مستوى") },
		{ TEXT("UpUnlocks railgun, +25% dmg each"), TEXT("يفتح المدفع، +25% ضرر لكل مستوى") },
		{ TEXT("UpUnlocks tesla, +25% shock each"), TEXT("يفتح ملف تسلا، +25% صعق لكل مستوى") },
		{ TEXT("UpFull health and armor now"), TEXT("صحة ودرع كاملان فورًا") },
		{ TEXT("HudBoss"), TEXT("العملاق") },
		{ TEXT("NoticeBoss"), TEXT("العملاق قادم!") },
		{ TEXT("NoticeBossPhase"), TEXT("{0}: المرحلة {1}") },
		{ TEXT("NoticeEmpHit"), TEXT("النبضة أوقفت محرّكك") },
		{ TEXT("NoticeWave"), TEXT("الموجة {0} قادمة") },
		{ TEXT("NoticeCleared"), TEXT("تم تطهير الموجة {0}   +{1}") },
		{ TEXT("NoticeLv"), TEXT("مستوى") },
		{ TEXT("NoticeDone"), TEXT("تم") },
		{ TEXT("NoticeBench"), TEXT("قياس الأداء جارٍ - قُد بشكل طبيعي") },
		{ TEXT("NoticeRecovered"), TEXT("أُعيدت السيارة إلى الطريق") },
		{ TEXT("HudMetres"), TEXT("م") },
		{ TEXT("CrateREPAIR"), TEXT("إصلاح") },
		{ TEXT("CrateAMMO"), TEXT("ذخيرة") },
		{ TEXT("CrateHealth"), TEXT("صحة") },
		{ TEXT("CrateArmor"), TEXT("درع") },
		{ TEXT("CrateRounds"), TEXT("طلقة") },
		{ TEXT("CrateRockets"), TEXT("صاروخ") },
	};
	return Table;
}

UFont* GUiFont = nullptr;
bool GUiFontChecked = false;

UFont* LoadUiFont()
{
	if (!GUiFontChecked)
	{
		GUiFontChecked = true;
		GUiFont = LoadObject<UFont>(nullptr, TEXT("/Game/IronSiege/UI/F_IronUI.F_IronUI"));
		// Nothing else holds the font, and this is a plain pointer: without rooting it the garbage
		// collector frees it about a minute in, and the next Arabic HUD line reads freed memory
		// (a pure-virtual-call crash measuring the wave status). It is needed for the whole session.
		if (GUiFont)
		{
			GUiFont->AddToRoot();
		}
	}
	return GUiFont;
}
}

namespace IronText
{
bool IsArabic()
{
	const UIronSiegeUserSettings* S = UIronSiegeUserSettings::Get();
	return S && S->Language == TEXT("ar");
}

FText Get(const TCHAR* Key, const FText& English)
{
	if (IsArabic())
	{
		if (const FString* Found = ArabicTable().Find(Key))
		{
			return FText::FromString(*Found);
		}
		// The campaign's text has its own table (IronSiegeStory.cpp).
		if (const FString* Story = IronStory::FindArabic(Key))
		{
			return FText::FromString(*Story);
		}
	}
	return English;
}

FString Name(const TCHAR* Prefix, const FString& English)
{
	return Str(*(FString(Prefix) + English), *English);
}

FString Str(const TCHAR* Key, const TCHAR* English)
{
	if (IsArabic())
	{
		if (const FString* Found = ArabicTable().Find(Key))
		{
			return *Found;
		}
		if (const FString* Story = IronStory::FindArabic(Key))
		{
			return *Story;
		}
	}
	return English;
}

FSlateFontInfo Font(int32 Size, bool bBold)
{
	if (UFont* UiFont = LoadUiFont())
	{
		FSlateFontInfo Info(UiFont, Size);
		Info.TypefaceFontName = bBold ? FName("Bold") : FName("Regular");
		return Info;
	}
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

UFont* CanvasFont(int32 SizeKind)
{
	// Only worth swapping the engine font out when Arabic is on: the engine's bitmap fonts have no
	// Arabic glyphs, but they are sharper for Latin text at HUD sizes.
	if (IsArabic())
	{
		if (UFont* UiFont = LoadUiFont())
		{
			return UiFont;
		}
	}
	return SizeKind >= 2 ? GEngine->GetLargeFont() : (SizeKind == 1 ? GEngine->GetMediumFont() : GEngine->GetSmallFont());
}

int32 CanvasFontSize(int32 SizeKind)
{
	static const int32 Sizes[] = { 13, 17, 22 };
	return Sizes[FMath::Clamp(SizeKind, 0, 2)];
}
}
