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
