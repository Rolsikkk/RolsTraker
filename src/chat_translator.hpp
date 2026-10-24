#pragma once

#include <string>
#include <vector>
#include <regex>
#include <algorithm>

struct TranslationRule {
    std::string pattern;
    std::string replacement;
};

inline std::string toLowerAscii(std::string s) {
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');
    }
    return s;
}

// Translates English gaming callouts, tactical phrases and slang into Russian
inline std::string translateGamingChat(const std::string& text) {
    if (text.empty()) return "";

    std::string lower = toLowerAscii(text);

    // Common phrases / full matches first
    static const std::vector<std::pair<std::string, std::string>> PHRASE_DICT = {
        {"drop me vandal", "скинь мне вандал"},
        {"drop me phantom", "скинь мне фантом"},
        {"drop me operator", "скинь мне оператор / авп"},
        {"drop me op", "скинь мне оп / авп"},
        {"drop me sheriff", "скинь мне шериф"},
        {"drop me gun", "скинь мне оружие"},
        {"drop me please", "скинь мне оружие, пожалуйста"},
        {"drop me pls", "скинь мне, пожалуйста"},
        {"buy me please", "купи мне, пожалуйста"},
        {"buy me pls", "купи мне, пожалуйста"},
        {"buy me", "купи мне оружие"},
        {"need gun", "нужно оружие / дроп"},
        {"they are rushing a", "они бегут / рашат А!"},
        {"they are rushing b", "они бегут / рашат Б!"},
        {"they are rushing c", "они бегут / рашат С!"},
        {"they are rushing mid", "они рашат мид!"},
        {"rush a", "рашим А"},
        {"rush b", "рашим Б"},
        {"rush c", "рашим С"},
        {"rush mid", "рашим мид"},
        {"all a", "все на А"},
        {"all b", "все на Б"},
        {"all c", "все на С"},
        {"all mid", "все на миду"},
        {"go a", "идём на А"},
        {"go b", "идём на Б"},
        {"go c", "идём на С"},
        {"go mid", "идём мид"},
        {"rotate to a", "ротируемся на А"},
        {"rotate to b", "ротируемся на Б"},
        {"rotate a", "перетягиваемся на А"},
        {"rotate b", "перетягиваемся на Б"},
        {"rotate", "перетягивайтесь / ротация"},
        {"dont push", "не пушьте! стойте"},
        {"don't push", "не пушьте! стойте"},
        {"stop pushing", "перестаньте пушить!"},
        {"dont peek", "не пикайте! ждите"},
        {"don't peek", "не пикайте! ждите"},
        {"play time", "играйте по времени!"},
        {"play post plant", "играем пост-плант"},
        {"defuse defuse", "дифьюзьте бомбу!"},
        {"defuse please", "дифьюзь, пожалуйста"},
        {"defuse pls", "дифьюзьте"},
        {"he is defusing", "он дифьюзит!"},
        {"they are defusing", "они дифьюзят!"},
        {"fake a", "фейк А"},
        {"fake b", "фейк Б"},
        {"he is one shot", "он на один выстрел / лоу!"},
        {"he is 1 shot", "он ваншотный!"},
        {"one shot", "ваншот / лоу хп"},
        {"one hp", "1 хп / очень лоу"},
        {"1 hp", "1 хп!"},
        {"low hp", "мало здоровья (лоу)"},
        {"nice try", "хорошая попытка (NT)"},
        {"well played", "отлично сыграно (WP)"},
        {"good job", "хорошая работа / красавцы"},
        {"good luck have fun", "удачи и приятной игры"},
        {"have fun", "приятной игры"},
        {"dont save", "не сейви!"},
        {"please save", "сейви, пожалуйста"},
        {"save save", "сейвите оружие"},
        {"save gun", "сейви ган"},
        {"what are you doing", "что вы делаете?"},
        {"my bad", "моя вина / сори"},
        {"no worries", "без проблем / всё норм"},
        {"dont worry", "не переживай, всё норм"},
        {"thank you", "спасибо"},
        {"thanks", "спасибо"},
        {"thx", "спасибо"},
        {"ty", "спасибо"},
        {"np", "не за что / без проблем"},
        {"gl hf", "удачи в игре"},
        {"glhf", "удачи в игре"},
        {"gg wp", "отличная игра"},
        {"ggwp", "отличная игра"},
        {"gg", "хорошая игра (GG)"},
        {"nt", "хорошая попытка (NT)"},
        {"wp", "отлично сыграно (WP)"},
        {"ns", "отличный выстрел! (Nice Shot)"},
        {"gh", "хорошая половина матча (Good Half)"},
        {"eco round", "эко раунд (копим деньги)"},
        {"full buy", "полный закуп"},
        {"force buy", "форс бай (закупаемся)"},
        {"half buy", "полузакуп"}
    };

    for (const auto& [phrase, transl] : PHRASE_DICT) {
        if (lower == phrase) return transl;
    }

    // Word-by-word / sub-pattern translation
    static const std::vector<std::pair<std::regex, std::string>> WORD_RULES = {
        {std::regex(R"(\b(defuse|defusing)\b)", std::regex::icase), "дифьюз"},
        {std::regex(R"(\b(plant|planting)\b)", std::regex::icase), "ставит спайк"},
        {std::regex(R"(\b(rotate|rotating)\b)", std::regex::icase), "ротация"},
        {std::regex(R"(\b(push|pushing)\b)", std::regex::icase), "пушат"},
        {std::regex(R"(\b(rush|rushing)\b)", std::regex::icase), "рашат"},
        {std::regex(R"(\b(peek|peeking)\b)", std::regex::icase), "пикает"},
        {std::regex(R"(\b(save|saving)\b)", std::regex::icase), "сейв"},
        {std::regex(R"(\b(drop)\b)", std::regex::icase), "дропни"},
        {std::regex(R"(\b(buy)\b)", std::regex::icase), "купи"},
        {std::regex(R"(\b(flank|flanking)\b)", std::regex::icase), "обходят в спину"},
        {std::regex(R"(\b(behind)\b)", std::regex::icase), "сзади / спина"},
        {std::regex(R"(\b(heaven)\b)", std::regex::icase), "хэвен / балкон"},
        {std::regex(R"(\b(hell)\b)", std::regex::icase), "под балконом (хелл)"},
        {std::regex(R"(\b(ct)\b)", std::regex::icase), "КТ / база защиты"},
        {std::regex(R"(\b(t spawn)\b)", std::regex::icase), "база атаки"},
        {std::regex(R"(\b(short)\b)", std::regex::icase), "шорт"},
        {std::regex(R"(\b(long)\b)", std::regex::icase), "лонг"},
        {std::regex(R"(\b(hookah)\b)", std::regex::icase), "хука"},
        {std::regex(R"(\b(lamps|ujam)\b)", std::regex::icase), "лампы"},
        {std::regex(R"(\b(shower)\b)", std::regex::icase), "душевые / баня"},
        {std::regex(R"(\b(garage)\b)", std::regex::icase), "гараж"},
        {std::regex(R"(\b(elbow)\b)", std::regex::icase), "элбоу / локоть"},
        {std::regex(R"(\b(link)\b)", std::regex::icase), "линк / проход"},
        {std::regex(R"(\b(cuba|cubby)\b)", std::regex::icase), "карман / нычка"},
        {std::regex(R"(\b(smoke|smoking)\b)", std::regex::icase), "смок / дым"},
        {std::regex(R"(\b(flash|flashing)\b)", std::regex::icase), "флешка / ослепил"},
        {std::regex(R"(\b(heal me)\b)", std::regex::icase), "полечи меня"},
        {std::regex(R"(\b(rez me|revive me)\b)", std::regex::icase), "воскреси меня"},
        {std::regex(R"(\b(res)\b)", std::regex::icase), "воскреси"},
        {std::regex(R"(\b(ult|ultimate)\b)", std::regex::icase), "ульта"},
        {std::regex(R"(\b(careful|care)\b)", std::regex::icase), "осторожно"},
        {std::regex(R"(\b(watch out)\b)", std::regex::icase), "внимание / осторожно"},
        {std::regex(R"(\b(one hit|1 hit)\b)", std::regex::icase), "на один удар (лоу)"},
        {std::regex(R"(\b(last one)\b)", std::regex::icase), "последний враг"},
        {std::regex(R"(\b(last player)\b)", std::regex::icase), "последний игрок"},
        {std::regex(R"(\b(afk)\b)", std::regex::icase), "отошел (АФК)"},
        {std::regex(R"(\b(cheater|hacker)\b)", std::regex::icase), "читер"},
        {std::regex(R"(\b(smurf)\b)", std::regex::icase), "смурф"}
    };

    std::string result = text;
    bool anyReplaced = false;
    for (const auto& [re, repl] : WORD_RULES) {
        if (std::regex_search(result, re)) {
            result = std::regex_replace(result, re, repl);
            anyReplaced = true;
        }
    }

    if (anyReplaced) return result;
    return "";
}


inline std::string toLowerUtf8(const std::string& s) {
    std::string res;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') {
            res.push_back((char)(c + ('a' - 'A')));
        } else if (c == 0xD0 && i + 1 < s.size()) {
            unsigned char c2 = (unsigned char)s[i + 1];
            if (c2 >= 0x90 && c2 <= 0x9F) {
                res.push_back((char)0xD0);
                res.push_back((char)(c2 + 0x20));
                i++;
            } else if (c2 >= 0xA0 && c2 <= 0xAF) {
                res.push_back((char)0xD1);
                res.push_back((char)(c2 - 0x20));
                i++;
            } else if (c2 == 0x81) {
                res.push_back((char)0xD1);
                res.push_back((char)0x91);
                i++;
            } else {
                res.push_back((char)c);
            }
        } else if (c == 0xD1 && i + 1 < s.size()) {
            res.push_back((char)c);
            res.push_back(s[i + 1]);
            i++;
        } else {
            res.push_back((char)c);
        }
    }
    return res;
}

struct FilterResult {
    std::string text;
    bool wasCensored = false;
    bool isHardBanned = false;
    std::string reason = "";
};

inline FilterResult filterProfanityAndSlurs(const std::string& input) {
    FilterResult res;
    res.text = input;

    // Hard banned words (Riot automated permanent ban triggers)
    static const std::vector<std::string> HARD_BANS = {
        "nigger", "nigga", "faggot", "fag", "chink", "retard", "kys", "kill yourself", "убейся", "негр", "чурка", "хач", "пидорас"
    };

    std::string lower = toLowerUtf8(input);

    for (const auto& bad : HARD_BANS) {
        if (lower.find(bad) != std::string::npos) {
            res.isHardBanned = true;
            res.reason = "Содержит запрещенные слова (высокий риск бана Riot Games!)";
            return res;
        }
    }

    // Swear words to censor with ***
    static const std::vector<std::string> SWEAR_WORDS = {
        "хуй", "хуя", "хуе", "хуи", "хую", "хер", "пизд", "ебат", "ебан", "ебу", "ёб", "еб", "бля", "блят", "сука", "суч", "гандон", "гондон", "мудак", "мудил", "чмо", "даун", "шлюх", "уеб", "залуп", "дроч", "тварь",
        "fuck", "fucking", "fucked", "fucker", "shit", "bitch", "cunt", "asshole", "bastard", "dick", "pussy", "idiot"
    };

    for (const auto& w : SWEAR_WORDS) {
        size_t pos = 0;
        while ((pos = lower.find(w, pos)) != std::string::npos) {
            size_t wLen = w.length();
            std::string stars(3, '*');
            res.text.replace(pos, wLen, stars);
            lower.replace(pos, wLen, stars);
            res.wasCensored = true;
            pos += 3;
        }
    }

    return res;
}

inline std::string translateRussianToEnglish(const std::string& text) {
    if (text.empty()) return text;
    std::string lower = toLowerUtf8(text);

    static const std::vector<std::pair<std::string, std::string>> RU_TO_EN_PHRASES = {
        {"го а", "rush A"},
        {"го на а", "rush A"},
        {"рашим а", "rush A"},
        {"пойдем на а", "go A"},
        {"го б", "rush B"},
        {"го на б", "rush B"},
        {"рашим б", "rush B"},
        {"пойдем на б", "go B"},
        {"го с", "rush C"},
        {"го на с", "rush C"},
        {"рашим с", "rush C"},
        {"пойдем на с", "go C"},
        {"го мид", "go mid"},
        {"рашим мид", "rush mid"},
        {"все а", "all A"},
        {"все на а", "all A"},
        {"все б", "all B"},
        {"все на б", "all B"},
        {"все с", "all C"},
        {"все на с", "all C"},
        {"все мид", "all mid"},
        {"все на миду", "all mid"},
        {"дропни вандал", "drop vandal please"},
        {"скинь вандал", "drop vandal pls"},
        {"дай вандал", "give vandal please"},
        {"дропни фантом", "drop phantom please"},
        {"скинь фантом", "drop phantom pls"},
        {"дай фантом", "give phantom please"},
        {"дропни оп", "drop op please"},
        {"дропни оружие", "drop weapon please"},
        {"дай оружие", "drop weapon please"},
        {"дай дроп", "drop please"},
        {"скинь дроп", "drop please"},
        {"купи мне", "buy for me please"},
        {"купи", "buy please"},
        {"не пикай", "don't peek, play safe"},
        {"не пикайте", "don't peek, play time"},
        {"не пушь", "don't push, stay safe"},
        {"не пушьте", "don't push"},
        {"сейви", "save weapon"},
        {"сейв", "save"},
        {"сохраняй", "save your gun"},
        {"пленти", "plant the spike"},
        {"ставьте", "plant the spike"},
        {"ставь спайк", "plant spike"},
        {"дифьюзь", "defuse the spike"},
        {"дефь", "defuse"},
        {"он ванхп", "he is 1hp"},
        {"он лоу", "he is low hp"},
        {"он ваншот", "he is one shot"},
        {"дай инфу", "give info please"},
        {"где они", "where are they?"},
        {"хорош", "nice shot / gj!"},
        {"красава", "gj nice shot!"},
        {"красавчик", "well played!"},
        {"гж", "gj"},
        {"спасибо", "thanks"},
        {"спс", "thx"},
        {"сори", "sorry my bad"},
        {"извини", "sorry my bad"},
        {"не переживай", "no worries, all good"},
        {"все норм", "all good"},
        {"удачи", "gl hf"},
        {"эко", "eco round"},
        {"копим", "save credits / eco"},
        {"форс", "force buy"},
        {"форсим", "force buy"},
        {"полный закуп", "full buy"},
        {"дай смок", "smoke please"},
        {"смокни", "smoke please"},
        {"дай флешку", "flash please"},
        {"ослепи", "flash please"},
        {"полечи", "heal me please"},
        {"дай хил", "heal me pls"},
        {"воскреси", "res please"},
        {"дай рез", "res me pls"},
        {"ульта готова", "ult ready"},
        {"в спину", "flank behind!"},
        {"спина", "behind us!"},
        {"обходят", "they are flanking!"},
        {"они на а", "they are on A"},
        {"они на б", "they are on B"},
        {"они на с", "they are on C"},
        {"они на миду", "they are on mid"},
        {"он на хэвене", "he is in heaven"},
        {"он на спавне", "he is in spawn / CT"},
        {"он кт", "he is CT"},
        {"фейк", "fake site"},
        {"тихо", "play quiet, walk"}
    };

    for (const auto& [ru, en] : RU_TO_EN_PHRASES) {
        if (lower == ru) return en;
    }

    // Word replacements for partial translation
    static const std::vector<std::pair<std::string, std::string>> WORD_DICT = {
        {"вандал", "vandal"},
        {"фантом", "phantom"},
        {"оператор", "operator"},
        {"спайк", "spike"},
        {"дифьюз", "defuse"},
        {"хэвен", "heaven"},
        {"хелл", "hell"},
        {"мид", "mid"},
        {"шорт", "short"},
        {"лонг", "long"},
        {"гараж", "garage"},
        {"баня", "shower"},
        {"нычка", "cubby"},
        {"смок", "smoke"},
        {"флешка", "flash"},
        {"ульта", "ult"},
        {"раш", "rush"},
        {"пуш", "push"},
        {"пик", "peek"},
        {"сейв", "save"}
    };

    std::string result = text;
    for (const auto& [ruWord, enWord] : WORD_DICT) {
        size_t p = 0;
        std::string lRes = toLowerUtf8(result);
        while ((p = lRes.find(ruWord, p)) != std::string::npos) {
            result.replace(p, ruWord.length(), enWord);
            lRes = toLowerUtf8(result);
            p += enWord.length();
        }
    }

    return result;
}
