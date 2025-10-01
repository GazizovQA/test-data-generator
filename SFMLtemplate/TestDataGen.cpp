#define _CRT_SECURE_NO_WARNINGS


#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>


#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <atomic>
#include <windows.h>
#include <locale>
#include <map>
#include <algorithm>
#include <limits>
#include <mutex>
#include <ctime>
#include <cstdio>


// ===== utf8 helpers =====
std::string utf8_encode(const std::string& str) {
    if (str.empty()) return std::string();
    int wlen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    if (wlen == 0) return std::string();
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], wlen);


    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (len == 0) return std::string();
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &out[0], len, NULL, NULL);
    return out;
}


std::wstring utf8_to_wstring(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (wlen == 0) return std::wstring();
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wlen);
    return wstr;
}


// ===== Data Structures =====
struct PersonalData {
    std::string name;
    std::string email;
    std::string phone;
    std::string birthDate;
    std::string address;
};


struct FinancialData {
    std::string cardNumber;
    std::string cvv;
    std::string expiryDate;
};


struct InvalidData {
    std::string sqlInjection;
    std::string xss;
    std::string boundaryTest;
};


enum class DataType { PERSONAL, FINANCIAL, INVALID, ALL };
enum class ExportFormat { JSON, CSV, API, NONE };
enum class LanguageMode { RUSSIAN, ENGLISH, MIXED };
enum class QualityMode { VALID, INVALID, MIXED };


std::atomic<float> g_progress{ 0.0f };
std::atomic<bool> g_generating{ false };
std::mutex g_statusMutex;
std::string g_statusMessage = "Ready to work";
std::atomic<bool> g_statusUpdated{ false };
std::atomic<int> g_preset{ 0 };


class Button {
private:
    sf::RectangleShape shape;
    sf::Text text;
    sf::Color defaultColor;
    sf::Color hoverColor;
    sf::Color clickColor;
    bool isHovered;
    bool isClicked;

public:
    Button(const std::string& btnText, const sf::Font& font, const sf::Vector2f& size) {
        shape.setSize(size);
        shape.setFillColor(sf::Color(70, 130, 180));
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color(50, 100, 150));

        text.setString(btnText);
        text.setFont(font);
        text.setCharacterSize(16);
        text.setFillColor(sf::Color::White);

        defaultColor = sf::Color(70, 130, 180);
        hoverColor = sf::Color(100, 160, 210);
        clickColor = sf::Color(50, 100, 150);
        isHovered = false;
        isClicked = false;

        
        sf::FloatRect textRect = text.getLocalBounds();
        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    }

    void setPosition(const sf::Vector2f& position) {
        shape.setPosition(position);
        sf::Vector2f center = position + shape.getSize() / 2.0f;
        text.setPosition(center);
    }

    void draw(sf::RenderWindow& window) {
        window.draw(shape);
        window.draw(text);
    }

    void update(const sf::RenderWindow& window) {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::FloatRect bounds = shape.getGlobalBounds();

        bool wasHovered = isHovered;
        isHovered = bounds.contains(mousePos);

        if (isHovered && !wasHovered) {
            shape.setFillColor(hoverColor);
        }
        else if (!isHovered && wasHovered) {
            shape.setFillColor(defaultColor);
        }
    }

    bool isMouseOver(const sf::RenderWindow& window) const {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        return shape.getGlobalBounds().contains(mousePos);
    }

    bool handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            if (isMouseOver(window)) {
                shape.setFillColor(clickColor);
                isClicked = true;
                return true;
            }
        }
        else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
            if (isClicked) {
                shape.setFillColor(isMouseOver(window) ? hoverColor : defaultColor);
                isClicked = false;
                if (isMouseOver(window)) {
                    return true;
                }
            }
        }
        return false;
    }

    void setEnabled(bool enabled) {
        if (enabled) {
            shape.setFillColor(defaultColor);
        }
        else {
            shape.setFillColor(sf::Color(150, 150, 150));
        }
    }
};


class Dropdown {
private:
    sf::RectangleShape mainBox;
    sf::Text mainText;
    std::vector<sf::Text> optionTexts;
    std::vector<std::string> options;
    bool isOpen;
    int selectedIndex;
    sf::Font font;

    sf::RectangleShape overlay;

public:
    Dropdown(const std::vector<std::string>& opts, const sf::Font& fnt, const sf::Vector2f& size)
        : options(opts), font(fnt), isOpen(false), selectedIndex(0) {

        mainBox.setSize(size);
        mainBox.setFillColor(sf::Color::White);
        mainBox.setOutlineThickness(1);
        mainBox.setOutlineColor(sf::Color::Black);

        mainText.setFont(font);
        mainText.setCharacterSize(14);
        mainText.setFillColor(sf::Color::Black);
        if (!options.empty()) mainText.setString(options[0]);
        else mainText.setString("");

        for (const auto& option : options) {
            sf::Text text;
            text.setFont(font);
            text.setCharacterSize(14);
            text.setFillColor(sf::Color::Black);
            text.setString(option);
            optionTexts.push_back(text);
        }

        overlay.setFillColor(sf::Color(0, 0, 0, 110));
    }

    void setPosition(const sf::Vector2f& position) {
        mainBox.setPosition(position);

        
        sf::FloatRect textRect = mainText.getLocalBounds();
        mainText.setPosition(position.x + 10, position.y + (mainBox.getSize().y - textRect.height) / 2 - 4);

        
        for (size_t i = 0; i < optionTexts.size(); ++i) {
            optionTexts[i].setPosition(position.x + 10, position.y + mainBox.getSize().y + i * mainBox.getSize().y + 6);
        }
    }

    void draw(sf::RenderWindow& window) {
        
        window.draw(mainBox);
        window.draw(mainText);

        if (isOpen) {
            
            overlay.setSize(static_cast<sf::Vector2f>(window.getSize()));
            overlay.setPosition(0, 0);
            window.draw(overlay);

            
            for (size_t i = 0; i < optionTexts.size(); ++i) {
                sf::RectangleShape optionBg(mainBox.getSize());
                optionBg.setPosition(mainBox.getPosition().x,
                    mainBox.getPosition().y + mainBox.getSize().y * (i + 1));
                optionBg.setOutlineThickness(1);
                optionBg.setOutlineColor(sf::Color::Black);

                if (isOptionHovered(window, i)) {
                    optionBg.setFillColor(sf::Color(200, 220, 255));
                }
                else {
                    optionBg.setFillColor(sf::Color(240, 240, 240));
                }

                window.draw(optionBg);
                window.draw(optionTexts[i]);
            }
        }
    }

    void update(const sf::RenderWindow& window) {
        
    }

    bool handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            if (mainBox.getGlobalBounds().contains(mousePos)) {
                isOpen = !isOpen;
                return true;
            }

            if (isOpen) {
               
                for (size_t i = 0; i < optionTexts.size(); ++i) {
                    sf::FloatRect optionRect(mainBox.getPosition().x,
                        mainBox.getPosition().y + (i + 1) * mainBox.getSize().y,
                        mainBox.getSize().x,
                        mainBox.getSize().y);
                    if (optionRect.contains(mousePos)) {
                        selectedIndex = static_cast<int>(i);
                        mainText.setString(options[i]);
                        isOpen = false;
                        return true;
                    }
                }
                
                sf::FloatRect fullRect(mainBox.getPosition().x,
                    mainBox.getPosition().y,
                    mainBox.getSize().x,
                    mainBox.getSize().y * (1 + optionTexts.size()));
                if (!fullRect.contains(mousePos)) {
                    isOpen = false;
                }
            }
        }
        return false;
    }

    int getSelectedIndex() const { return selectedIndex; }
    std::string getSelectedText() const { return options[selectedIndex]; }
    bool isOpened() const { return isOpen; }

private:
    bool isOptionHovered(const sf::RenderWindow& window, int index) const {
        if (!isOpen) return false;
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::FloatRect optionRect(mainBox.getPosition().x,
            mainBox.getPosition().y + (index + 1) * mainBox.getSize().y,
            mainBox.getSize().x,
            mainBox.getSize().y);
        return optionRect.contains(mousePos);
    }
};


class TextBox {
private:
    sf::RectangleShape box;
    sf::Text text;
    sf::Text label;
    std::string inputText;
    bool isSelected;
    int maxLength;

public:
    TextBox(const std::string& lbl, const sf::Font& font, const sf::Vector2f& size)
        : isSelected(false), maxLength(4) {

        box.setSize(size);
        box.setFillColor(sf::Color::White);
        box.setOutlineThickness(1);
        box.setOutlineColor(sf::Color::Black);

        label.setString(lbl);
        label.setFont(font);
        label.setCharacterSize(14);
        label.setFillColor(sf::Color::Black);

        text.setFont(font);
        text.setCharacterSize(14);
        text.setFillColor(sf::Color::Black);
        text.setString("10");
        inputText = "10";
    }

    void setPosition(const sf::Vector2f& position) {
        label.setPosition(position.x, position.y - 25);
        box.setPosition(position);
        text.setPosition(position.x + 5, position.y + 5);
    }

    void draw(sf::RenderWindow& window) {
        window.draw(label);
        window.draw(box);
        text.setString(inputText);
        window.draw(text);

        if (isSelected) {
            static sf::Clock cursorClock;
            if (cursorClock.getElapsedTime().asSeconds() < 0.5f) {
                sf::RectangleShape cursor(sf::Vector2f(2, text.getLocalBounds().height));
                cursor.setFillColor(sf::Color::Black);
                cursor.setPosition(text.getPosition().x + text.getLocalBounds().width, text.getPosition().y);
                window.draw(cursor);
            }
            else if (cursorClock.getElapsedTime().asSeconds() > 1.0f) {
                cursorClock.restart();
            }
        }
    }

    void update(const sf::RenderWindow& window) {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        if (box.getGlobalBounds().contains(mousePos)) {
            box.setOutlineColor(sf::Color::Blue);
        }
        else if (!isSelected) {
            box.setOutlineColor(sf::Color::Black);
        }
    }

    bool handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            isSelected = box.getGlobalBounds().contains(mousePos);
            if (isSelected) {
                box.setOutlineColor(sf::Color::Blue);
                return true;
            }
            else {
                box.setOutlineColor(sf::Color::Black);
            }
        }

        if (isSelected && event.type == sf::Event::TextEntered) {
            if (event.text.unicode == '\b') {
                if (!inputText.empty()) inputText.pop_back();
            }
            else if (event.text.unicode >= '0' && event.text.unicode <= '9') {
                if (inputText.length() < static_cast<size_t>(maxLength)) {
                    inputText += static_cast<char>(event.text.unicode);
                    try {
                        if (std::stoi(inputText) > 1000) inputText = "1000";
                    }
                    catch (...) {}
                }
            }
            return true;
        }

        return false;
    }

    int getValue() const {
        try {
            return std::stoi(inputText);
        }
        catch (...) {
            return 10;
        }
    }
};


class ProgressBar {
private:
    sf::RectangleShape background;
    sf::RectangleShape fill;
    sf::Text text;
    float progress;

public:
    ProgressBar(const sf::Font& font, const sf::Vector2f& size) : progress(0.0f) {
        background.setSize(size);
        background.setFillColor(sf::Color(200, 200, 200));
        background.setOutlineThickness(1);
        background.setOutlineColor(sf::Color::Black);

        fill.setSize(sf::Vector2f(0, size.y));
        fill.setFillColor(sf::Color(50, 150, 50));

        text.setFont(font);
        text.setCharacterSize(14);
        text.setFillColor(sf::Color::Black);
        text.setString("Ready");
    }

    void setPosition(const sf::Vector2f& position) {
        background.setPosition(position);
        fill.setPosition(position);

        sf::FloatRect textRect = text.getLocalBounds();
        text.setPosition(position.x + background.getSize().x + 10,
            position.y + (background.getSize().y - textRect.height) / 2);
    }

    void draw(sf::RenderWindow& window) {
        window.draw(background);
        window.draw(fill);
        window.draw(text);
    }

    void setProgress(float p) {
        progress = std::max(0.0f, std::min(1.0f, p));
        fill.setSize(sf::Vector2f(background.getSize().x * progress, background.getSize().y));
        int percentage = static_cast<int>(progress * 100);
        text.setString("Progress: " + std::to_string(percentage) + "%");
    }

    void setText(const std::string& newText) {
        text.setString(newText);
    }
};


class DataGenerator {
private:
    std::mt19937 gen;

    std::vector<std::string> russianFirstNames = { "Иван","Алексей","Сергей","Дмитрий","Михаил","Андрей","Александр","Евгений","Владимир","Николай" };
    std::vector<std::string> russianLastNames = { "Иванов","Петров","Сидоров","Смирнов","Кузнецов","Попов","Васильев","Федоров","Морозов","Волков" };

    std::vector<std::string> englishFirstNames = { "John","Michael","David","James","Robert","William","Thomas","Christopher","Daniel","Matthew" };
    std::vector<std::string> englishLastNames = { "Smith","Johnson","Williams","Brown","Jones","Miller","Davis","Garcia","Rodriguez","Wilson" };

    std::vector<std::string> domains = { "gmail.com","yahoo.com","hotmail.com","outlook.com","yandex.ru","mail.ru" };

    std::vector<std::string> russianCities = { "Москва","Санкт-Петербург","Новосибирск","Екатеринбург","Казань","Нижний Новгород","Челябинск","Самара" };
    std::vector<std::string> englishCities = { "New York","Los Angeles","Chicago","Houston","Phoenix","Philadelphia","San Antonio","San Diego" };

    std::vector<std::string> russianStreets = { "Ленина","Пушкина","Советская","Школьная","Садовая","Молодёжная","Гагарина","Мира" };
    std::vector<std::string> englishStreets = { "Main","Oak","Maple","Park","Washington","Lake","Hill","Cedar" };

    std::vector<std::string> sqlInjections = {
        "' OR '1'='1","' UNION SELECT * FROM users--","'; DROP TABLE users--","' OR 1=1--","admin'--",
        "' OR 'x'='x","' OR 'a'='a'--","') OR ('1'='1--"
    };

    std::vector<std::string> xssVectors = {
        "<script>alert('XSS')</script>","<img src=x onerror=alert('XSS')>","<svg onload=alert('XSS')>",
        "javascript:alert('XSS')","<body onload=alert('XSS')>","<iframe src=\"javascript:alert('XSS')\">",
        "<input onfocus=alert('XSS') autofocus>"
    };

    int presetMode = 0; 

    std::string transliterate(const std::string& text) {
        std::map<std::string, std::string> t = {
            {"а","a"},{"б","b"},{"в","v"},{"г","g"},{"д","d"},{"е","e"},{"ё","e"},
            {"ж","zh"},{"з","z"},{"и","i"},{"й","y"},{"к","k"},{"л","l"},{"м","m"},
            {"н","n"},{"о","o"},{"п","p"},{"р","r"},{"с","s"},{"т","t"},{"у","u"},
            {"ф","f"},{"х","kh"},{"ц","ts"},{"ч","ch"},{"ш","sh"},{"щ","shch"},
            {"ы","y"},{"э","e"},{"ю","yu"},{"я","ya"},
            {"А","A"},{"Б","B"},{"В","V"},{"Г","G"},{"Д","D"},{"Е","E"},{"Ё","E"},
            {"Ж","Zh"},{"З","Z"},{"И","I"},{"Й","Y"},{"К","K"},{"Л","L"},{"М","M"},
            {"Н","N"},{"О","O"},{"П","P"},{"Р","R"},{"С","S"},{"Т","T"},{"У","U"},
            {"Ф","F"},{"Х","Kh"},{"Ц","Ts"},{"Ч","Ch"},{"Ш","Sh"},{"Щ","Shch"},
            {"Ы","Y"},{"Э","E"},{"Ю","Yu"},{"Я","Ya"}
        };
        std::string res;
        for (auto& c : text) {
            std::string one(1, c);
            res += (t.count(one) ? t[one] : one);
        }
        return res;
    }

    std::string generateRandomString(int length) {
        const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()_+-=[]{}|;:,.<>? ";
        std::string result;
        for (int i = 0; i < length; ++i) result += chars[randomInt(0, static_cast<int>(chars.size()) - 1)];
        return result;
    }

public:
    DataGenerator(unsigned int seed = static_cast<unsigned int>(std::time(nullptr))) : gen(seed) {}

    void setPreset(int p) { presetMode = p; }

    int randomInt(int min, int max) {
        std::uniform_int_distribution<> d(min, max);
        return d(gen);
    }

    PersonalData generatePersonalData(LanguageMode mode, bool valid = true) {
        if (valid) return generateValidPersonal(mode);
        else return generateInvalidPersonal(mode);
    }

    PersonalData generateValidPersonal(LanguageMode mode) {
        PersonalData data;
        bool useRussian;
        if (mode == LanguageMode::RUSSIAN) useRussian = true;
        else if (mode == LanguageMode::ENGLISH) useRussian = false;
        else useRussian = (randomInt(0, 1) == 0);

        std::string firstName, lastName;
        if (useRussian) {
            firstName = russianFirstNames[randomInt(0, (int)russianFirstNames.size() - 1)];
            lastName = russianLastNames[randomInt(0, (int)russianLastNames.size() - 1)];
            data.name = lastName + " " + firstName;
            std::string city = russianCities[randomInt(0, (int)russianCities.size() - 1)];
            std::string street = russianStreets[randomInt(0, (int)russianStreets.size() - 1)];
            int house = randomInt(1, 200), apt = randomInt(1, 300);
            data.address = "г. " + city + ", ул. " + street + ", д. " + std::to_string(house) + ", кв. " + std::to_string(apt);
        }
        else {
            firstName = englishFirstNames[randomInt(0, (int)englishFirstNames.size() - 1)];
            lastName = englishLastNames[randomInt(0, (int)englishLastNames.size() - 1)];
            data.name = firstName + " " + lastName;
            std::string city = englishCities[randomInt(0, (int)englishCities.size() - 1)];
            std::string street = englishStreets[randomInt(0, (int)englishStreets.size() - 1)];
            int house = randomInt(1, 200);
            data.address = city + ", " + street + " St., " + std::to_string(house);
        }

        // email
        std::string domain = domains[randomInt(0, (int)domains.size() - 1)];
        int emailType = randomInt(0, 4);
        std::string emailName;
        if (useRussian) {
            std::string f = transliterate(firstName), l = transliterate(lastName);
            std::transform(f.begin(), f.end(), f.begin(), ::tolower);
            std::transform(l.begin(), l.end(), l.begin(), ::tolower);
            switch (emailType) {
            case 0: emailName = f + "." + l; break;
            case 1: emailName = f.substr(0, 1) + l; break;
            case 2: emailName = f + std::to_string(randomInt(1, 999)); break;
            case 3: emailName = l + "." + f; break;
            default: emailName = f + "_" + l + std::to_string(randomInt(10, 99));
            }
        }
        else {
            std::string f = firstName, l = lastName;
            std::transform(f.begin(), f.end(), f.begin(), ::tolower);
            std::transform(l.begin(), l.end(), l.begin(), ::tolower);
            switch (emailType) {
            case 0: emailName = f + "." + l; break;
            case 1: emailName = f.substr(0, 1) + l; break;
            case 2: emailName = f + std::to_string(randomInt(1, 999)); break;
            case 3: emailName = l + "." + f; break;
            default: emailName = f + "_" + l + std::to_string(randomInt(10, 99));
            }
        }
        data.email = emailName + "@" + domain;

        // phone
        if (useRussian) data.phone = "+7" + std::to_string(9000000000ULL + randomInt(0, 9999999));
        else data.phone = "+1" + std::to_string(2000000000ULL + randomInt(0, 999999999));

        // birth
        int year = 2024 - randomInt(18, 65), month = randomInt(1, 12), day = randomInt(1, 28);
        auto two = [](int v) { std::string s = std::to_string(v); if (s.size() < 2) s = "0" + s; return s; };
        data.birthDate = two(day) + "." + two(month) + "." + std::to_string(year);

        return data;

    }

    PersonalData generateInvalidPersonal(LanguageMode mode) {
        PersonalData data;

        // name
        int nameType = randomInt(0, 4);
        if (nameType == 0) data.name = generateRandomString(randomInt(1, 3));
        else if (nameType == 1) data.name = generateRandomString(randomInt(20, 80));
        else if (nameType == 2) data.name = "12345";
        else if (nameType == 3) data.name = "!!!@@@";
        else data.name = "";

        // email invalid variants
        int eType = randomInt(0, 5);
        switch (eType) {
        case 0: data.email = "plainaddress"; break;
        case 1: data.email = "@missinglocal.com"; break;
        case 2: data.email = "missingatsign.com"; break;
        case 3: data.email = "local@domain@domain.com"; break;
        case 4: data.email = "name with spaces@domain.com"; break;
        default: data.email = generateRandomString(30); break;
        }

        // phone wrong formats
        int pType = randomInt(0, 4);
        if (pType == 0) data.phone = "abcdefg";
        else if (pType == 1) data.phone = "123";
        else if (pType == 2) data.phone = "+700000000000000000000";
        else if (pType == 3) data.phone = "(555) 555-5555 ext 999999";
        else data.phone = "";

        // birthDate invalid
        int bType = randomInt(0, 4);
        if (bType == 0) data.birthDate = "32.13.abcd";
        else if (bType == 1) data.birthDate = "99/99/9999";
        else if (bType == 2) data.birthDate = "2000-02-30";
        else if (bType == 3) data.birthDate = "";
        else data.birthDate = generateRandomString(10);

        // address
        int aType = randomInt(0, 3);
        if (aType == 0) data.address = "";
        else if (aType == 1) data.address = generateRandomString(500);
        else data.address = "!@#$%^&*()_+";

        return data;
    }

    FinancialData generateFinancialData(bool valid = true) {
        if (valid) return generateValidFinancial();
        else return generateInvalidFinancial();
    }

    FinancialData generateValidFinancial() {

        FinancialData d;
 
        std::string card;
        for (int i = 0; i < 4; ++i) {
            int part = randomInt(0, 9999);
            char buf[5];
            snprintf(buf, sizeof(buf), "%04d", part);
            if (i > 0) card += " ";
            card += buf;
        }
        d.cardNumber = card;

        d.cvv = std::to_string(randomInt(100, 999));
        int month = randomInt(1, 12), year = 24 + randomInt(1, 5);
        std::ostringstream e; e << std::setw(2) << std::setfill('0') << month << "/" << year;
        d.expiryDate = e.str();
        return d;
    }

    FinancialData generateInvalidFinancial() {
        FinancialData d;
        int t = randomInt(0, 4);
        if (t == 0) d.cardNumber = generateRandomString(randomInt(1, 10)); 
        else if (t == 1) {
            
            std::ostringstream card; card << generateRandomString(19);
            d.cardNumber = card.str();
        }
        else if (t == 2) d.cardNumber = "";
        else d.cardNumber = "0000 0000 0000 0000"; 

        int cvt = randomInt(0, 3);
        if (cvt == 0) d.cvv = std::to_string(randomInt(0, 9)); 
        else if (cvt == 1) d.cvv = generateRandomString(5); 
        else d.cvv = "";

        int et = randomInt(0, 4);
        if (et == 0) d.expiryDate = "13/99";
        else if (et == 1) d.expiryDate = "00/00";
        else if (et == 2) d.expiryDate = "abcd";
        else d.expiryDate = "";
        return d;
    }

    InvalidData generateInvalidData() {
        InvalidData data;

        
        if (presetMode == 1) { 
            data.sqlInjection = sqlInjections[randomInt(0, (int)sqlInjections.size() - 1)];
            data.xss = "";
            data.boundaryTest = generateRandomString(2000);
            return data;
        }
        if (presetMode == 2) { 
            data.sqlInjection = "";
            data.xss = xssVectors[randomInt(0, (int)xssVectors.size() - 1)];
            data.boundaryTest = generateRandomString(200);
            return data;
        }
        if (presetMode == 3) { 
            data.sqlInjection = "";
            data.xss = "";
            data.boundaryTest = generateRandomString(5000);
            return data;
        }

        data.sqlInjection = sqlInjections[randomInt(0, (int)sqlInjections.size() - 1)];
        data.xss = xssVectors[randomInt(0, (int)xssVectors.size() - 1)];
        int choice = randomInt(0, 5);
        switch (choice) {
        case 0:
            data.boundaryTest = generateRandomString(1000);
            break;
        case 1:
            data.boundaryTest = std::to_string(std::numeric_limits<long long>::max());
            break;
        case 2:
            data.boundaryTest = "---" + generateRandomString(50) + "---";
            break;
        case 3:
            data.boundaryTest = "";
            break;
        case 4:
            data.boundaryTest = std::string("\x00\xFF", 2) + generateRandomString(10);
            break;
        default:
            data.boundaryTest = "NULL";
            break;
        }
        return data;
    }
};


std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (unsigned char ch : s) {
        switch (ch) {
        case '\"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if (ch < 0x20) {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)ch << std::dec;
            }
            else {
                o << ch;
            }
        }
    }
    return o.str();
}

std::string escapeCsv(const std::string& s) {
    std::string res = s;
    size_t pos = 0;
    while ((pos = res.find('\"', pos)) != std::string::npos) {
        res.replace(pos, 1, "\"\"");
        pos += 2;
    }
    return res;
}

bool exportToJson(const std::vector<PersonalData>& personalData,
    const std::vector<FinancialData>& financialData,
    const std::vector<InvalidData>& invalidData,
    const std::string& filename) {

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    // BOM и JSON
    file << "\xEF\xBB\xBF";
    file << "{\n";

    // personal
    file << "  \"personal_data\": [\n";
    for (size_t i = 0; i < personalData.size(); ++i) {
        file << "    {\n";
        file << "      \"name\": \"" << escapeJson(utf8_encode(personalData[i].name)) << "\",\n";
        file << "      \"email\": \"" << escapeJson(utf8_encode(personalData[i].email)) << "\",\n";
        file << "      \"phone\": \"" << escapeJson(utf8_encode(personalData[i].phone)) << "\",\n";
        file << "      \"birth_date\": \"" << escapeJson(utf8_encode(personalData[i].birthDate)) << "\",\n";
        file << "      \"address\": \"" << escapeJson(utf8_encode(personalData[i].address)) << "\"\n";
        file << "    }" << (i + 1 < personalData.size() ? "," : "") << "\n";
    }
    file << "  ],\n";

    // financial
    file << "  \"financial_data\": [\n";
    for (size_t i = 0; i < financialData.size(); ++i) {
        file << "    {\n";
        file << "      \"card_number\": \"" << escapeJson(utf8_encode(financialData[i].cardNumber)) << "\",\n";
        file << "      \"cvv\": \"" << escapeJson(utf8_encode(financialData[i].cvv)) << "\",\n";
        file << "      \"expiry_date\": \"" << escapeJson(utf8_encode(financialData[i].expiryDate)) << "\"\n";
        file << "    }" << (i + 1 < financialData.size() ? "," : "") << "\n";
    }
    file << "  ],\n";

    // invalid
    file << "  \"invalid_data\": [\n";
    for (size_t i = 0; i < invalidData.size(); ++i) {
        file << "    {\n";
        file << "      \"sql_injection\": \"" << escapeJson(utf8_encode(invalidData[i].sqlInjection)) << "\",\n";
        file << "      \"xss\": \"" << escapeJson(utf8_encode(invalidData[i].xss)) << "\",\n";
        file << "      \"boundary_test\": \"" << escapeJson(utf8_encode(invalidData[i].boundaryTest)) << "\"\n";
        file << "    }" << (i + 1 < invalidData.size() ? "," : "") << "\n";
    }
    file << "  ]\n";

    file << "}\n";
    file.close();
    return true;
}

bool exportToCsv(const std::vector<PersonalData>& personalData,
    const std::vector<FinancialData>& financialData,
    const std::vector<InvalidData>& invalidData,
    const std::string& filename) {

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    file << "\xEF\xBB\xBF";

    // Personal
    file << "Personal Data\n";
    file << "Name,Email,Phone,Birth Date,Address\n";
    for (const auto& d : personalData) {
        file << "\"" << escapeCsv(utf8_encode(d.name)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.email)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.phone)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.birthDate)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.address)) << "\"\n";
    }
    file << "\n";

    // Financial
    file << "Financial Data\n";
    file << "Card Number,CVV,Expiry Date\n";
    for (const auto& d : financialData) {
        file << "\"" << escapeCsv(utf8_encode(d.cardNumber)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.cvv)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.expiryDate)) << "\"\n";
    }
    file << "\n";

    // Invalid
    file << "Invalid Data\n";
    file << "SQL Injection,XSS,Boundary Test\n";
    for (const auto& d : invalidData) {
        file << "\"" << escapeCsv(utf8_encode(d.sqlInjection)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.xss)) << "\","
            << "\"" << escapeCsv(utf8_encode(d.boundaryTest)) << "\"\n";
    }

    file.close();
    return true;
}

bool exportToApiJson(const std::vector<PersonalData>& personalData,
    const std::vector<FinancialData>& financialData,
    const std::vector<InvalidData>& invalidData,
    const std::string& filename) {

    
    std::ofstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;
    f << "\xEF\xBB\xBF";
    f << "[\n";
    for (size_t i = 0; i < personalData.size(); ++i) {
        f << "  { \"name\": \"" << escapeJson(utf8_encode(personalData[i].name))
            << "\", \"email\": \"" << escapeJson(utf8_encode(personalData[i].email)) << "\" }";
        if (i + 1 < personalData.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
    return true;
}


bool copyToClipboard(const std::string& text) {
    std::string utf8 = utf8_encode(text);
    std::wstring w = utf8_to_wstring(utf8);
    if (w.empty()) return false;

    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();
    SIZE_T sizeInBytes = (w.size() + 1) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
    if (!hGlobal) {
        CloseClipboard();
        return false;
    }
    wchar_t* pGlobal = static_cast<wchar_t*>(GlobalLock(hGlobal));
    memcpy(pGlobal, w.c_str(), sizeInBytes);
    GlobalUnlock(hGlobal);
    SetClipboardData(CF_UNICODETEXT, hGlobal);
    CloseClipboard();
    return true;
}


std::string getTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}


int runCliMode(int argc, char* argv[]) {
    DataType type = DataType::ALL;
    LanguageMode lang = LanguageMode::MIXED;
    QualityMode qual = QualityMode::MIXED;
    ExportFormat format = ExportFormat::JSON;
    int count = 10;
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    int preset = 0; 

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--type" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "personal") type = DataType::PERSONAL;
            else if (v == "financial") type = DataType::FINANCIAL;
            else if (v == "invalid") type = DataType::INVALID;
            else type = DataType::ALL;
        }
        else if (arg == "--count" && i + 1 < argc) count = std::stoi(argv[++i]);
        else if (arg == "--mode" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "valid") qual = QualityMode::VALID;
            else if (v == "invalid") qual = QualityMode::INVALID;
            else qual = QualityMode::MIXED;
        }
        else if (arg == "--lang" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "ru") lang = LanguageMode::RUSSIAN;
            else if (v == "en") lang = LanguageMode::ENGLISH;
            else lang = LanguageMode::MIXED;
        }
        else if (arg == "--format" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "json") format = ExportFormat::JSON;
            else if (v == "csv") format = ExportFormat::CSV;
            else if (v == "api") format = ExportFormat::API;
            else format = ExportFormat::NONE;
        }
        else if (arg == "--seed" && i + 1 < argc) seed = static_cast<unsigned int>(std::stoul(argv[++i]));
        else if (arg == "--preset" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "sql") preset = 1;
            else if (v == "xss") preset = 2;
            else if (v == "boundary") preset = 3;
            else preset = 0;
        }
    }

    if (count < 1) count = 1;
    if (count > 1000000) count = 1000000; 

    DataGenerator gen(seed);
    gen.setPreset(preset);

    std::vector<PersonalData> personal;
    std::vector<FinancialData> financial;
    std::vector<InvalidData> invalid;

    for (int i = 0; i < count; ++i) {
        bool makeValid;
        if (qual == QualityMode::VALID) makeValid = true;
        else if (qual == QualityMode::INVALID) makeValid = false;
        else makeValid = (gen.randomInt(0, 1) == 0);

        if (type == DataType::PERSONAL || type == DataType::ALL) personal.push_back(gen.generatePersonalData(lang, makeValid));
        if (type == DataType::FINANCIAL || type == DataType::ALL) financial.push_back(gen.generateFinancialData(makeValid));
        if (type == DataType::INVALID || type == DataType::ALL) invalid.push_back(gen.generateInvalidData());
    }

    std::string filename = "test_data_" + getTimestamp();
    bool ok = false;
    switch (format) {
    case ExportFormat::JSON:
        filename += ".json";
        ok = exportToJson(personal, financial, invalid, filename);
        break;
    case ExportFormat::CSV:
        filename += ".csv";
        ok = exportToCsv(personal, financial, invalid, filename);
        break;
    case ExportFormat::API:
        filename += "_api.json";
        ok = exportToApiJson(personal, financial, invalid, filename);
        break;
    default:
        ok = false;
        break;
    }

    if (ok) std::cout << "Generated " << count << " records -> " << filename << std::endl;
    else std::cout << "Generation failed or export disabled." << std::endl;

    return ok ? 0 : 1;
}


int runGuiMode() {
    try { std::locale::global(std::locale("")); }
    catch (...) {}

    sf::RenderWindow window(sf::VideoMode(760, 560), "Test Data Generator", sf::Style::Close);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
            std::cerr << "Cannot load font arial.ttf\n";
            return -1;
        }
    }

    
    sf::Text title("Test Data Generator", font, 24);
    title.setFillColor(sf::Color::Black);
    title.setPosition(220, 20);

    
    std::vector<std::string> dataTypes = { "Personal Data", "Financial Data", "Invalid Data", "All Data" };
    Dropdown dataTypeDropdown(dataTypes, font, sf::Vector2f(200, 30));
    dataTypeDropdown.setPosition(sf::Vector2f(50, 80));

    std::vector<std::string> exportFormats = { "JSON", "CSV", "No File" };
    Dropdown exportFormatDropdown(exportFormats, font, sf::Vector2f(200, 30));
    exportFormatDropdown.setPosition(sf::Vector2f(50, 140));

    
    std::vector<std::string> languages = { "Russian", "English", "Mixed" };
    Dropdown languageDropdown(languages, font, sf::Vector2f(200, 30));
    languageDropdown.setPosition(sf::Vector2f(300, 80));

    
    std::vector<std::string> qualities = { "Valid", "Invalid", "Mixed" };
    Dropdown qualityDropdown(qualities, font, sf::Vector2f(200, 30));
    qualityDropdown.setPosition(sf::Vector2f(300, 200));

    
    std::vector<std::string> presets = { "None", "SQL-heavy", "XSS-heavy", "Boundary-heavy" };
    Dropdown presetDropdown(presets, font, sf::Vector2f(200, 30));
    presetDropdown.setPosition(sf::Vector2f(50, 200));

    TextBox countTextBox("Records count (1-1000):", font, sf::Vector2f(200, 30));
    countTextBox.setPosition(sf::Vector2f(50, 260));

    Button generateButton("Generate Data", font, sf::Vector2f(200, 40));
    generateButton.setPosition(sf::Vector2f(50, 320));

    Button copyButton("Copy to Clipboard", font, sf::Vector2f(200, 40));
    copyButton.setPosition(sf::Vector2f(300, 320));
    copyButton.setEnabled(false);

    ProgressBar progressBar(font, sf::Vector2f(420, 20));
    progressBar.setPosition(sf::Vector2f(50, 440));

    sf::Text statusText("Ready to work", font, 16);
    statusText.setFillColor(sf::Color::Black);
    statusText.setPosition(50, 480);

    DataGenerator generator;
    std::vector<PersonalData> personalData;
    std::vector<FinancialData> financialData;
    std::vector<InvalidData> invalidData;
    std::atomic<bool> generating(false);

    // Главный цикл
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();

            // Обработка событий
            dataTypeDropdown.handleEvent(event, window);
            exportFormatDropdown.handleEvent(event, window);
            languageDropdown.handleEvent(event, window); 
            qualityDropdown.handleEvent(event, window); 
            countTextBox.handleEvent(event, window);

            int dtIndex = dataTypeDropdown.getSelectedIndex();
            bool showPreset = (dtIndex == static_cast<int>(DataType::INVALID) || dtIndex == static_cast<int>(DataType::ALL));
            if (showPreset) presetDropdown.handleEvent(event, window);


            if (generateButton.handleEvent(event, window) && !generating) {
                int count = countTextBox.getValue();
                if (count < 1 || count > 1000) {
                    statusText.setString("Error: count must be from 1 to 1000");
                    statusText.setFillColor(sf::Color::Red);
                }
                else {
                    generating = true;
                    copyButton.setEnabled(false);
                    progressBar.setProgress(0.0f);
                    statusText.setString("Generating data...");
                    statusText.setFillColor(sf::Color::Blue);

                    std::thread([&, count]() {
                        DataType dataType = static_cast<DataType>(dataTypeDropdown.getSelectedIndex());
                        ExportFormat exportFormat = static_cast<ExportFormat>(exportFormatDropdown.getSelectedIndex());
                        LanguageMode langMode = static_cast<LanguageMode>(languageDropdown.getSelectedIndex());
                        QualityMode qualityMode = static_cast<QualityMode>(qualityDropdown.getSelectedIndex());
                        int presetModeLocal = presetDropdown.getSelectedIndex(); // 0..3

                        personalData.clear();
                        financialData.clear();
                        invalidData.clear();

                        DataGenerator localGen(static_cast<unsigned int>(std::time(nullptr)));
                        localGen.setPreset(presetModeLocal);

                        for (int i = 0; i < count; ++i) {
                            
                            bool makeValid = true;
                            if (qualityMode == QualityMode::VALID) makeValid = true;
                            else if (qualityMode == QualityMode::INVALID) makeValid = false;
                            else makeValid = (localGen.randomInt(0, 1) == 0);

                            if (dataType == DataType::PERSONAL || dataType == DataType::ALL) {
                                personalData.push_back(localGen.generatePersonalData(langMode, makeValid));
                            }
                            if (dataType == DataType::FINANCIAL || dataType == DataType::ALL) {
                                financialData.push_back(localGen.generateFinancialData(makeValid));
                            }
                            if (dataType == DataType::INVALID || dataType == DataType::ALL) {
                                
                                invalidData.push_back(localGen.generateInvalidData());
                            }

                            float progress = static_cast<float>(i + 1) / static_cast<float>(count);
                     
                            g_progress.store(progress, std::memory_order_relaxed);


                            if ((i % 10) == 0) {
                                std::lock_guard<std::mutex> lock(g_statusMutex);
                                g_statusMessage = "Generating data... " + std::to_string(static_cast<int>(progress * 100)) + "%";
                                g_statusUpdated.store(true, std::memory_order_relaxed);
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds(8));
                        }

                        std::string filename;
                        if (exportFormat != ExportFormat::NONE) {
                            std::string timestamp = getTimestamp();
                            if (exportFormat == ExportFormat::JSON) {
                                filename = "test_data_" + timestamp + ".json";
                                exportToJson(personalData, financialData, invalidData, filename);
                            }
                            else if (exportFormat == ExportFormat::CSV) {
                                filename = "test_data_" + timestamp + ".csv";
                                exportToCsv(personalData, financialData, invalidData, filename);
                            }
                            else if (exportFormat == ExportFormat::API) {
                                filename = "test_data_" + timestamp + "_api.json";
                                exportToApiJson(personalData, financialData, invalidData, filename);
                            }
                            std::lock_guard<std::mutex> lock(g_statusMutex);
                            g_statusMessage = "Done! File: " + filename;
                            g_statusUpdated.store(true, std::memory_order_relaxed);
                        }
                        else {
                            std::lock_guard<std::mutex> lock(g_statusMutex);
                            g_statusMessage = "Generation completed!";
                            g_statusUpdated.store(true, std::memory_order_relaxed);
                        }

                        g_generating.store(false, std::memory_order_release);
                        generating = false;
                        }).detach();
                }
            }

            if (copyButton.handleEvent(event, window) && !generating) {
                std::string dataToCopy;
                if (!personalData.empty()) {
                    dataToCopy += "=== PERSONAL DATA ===\n";
                    for (const auto& d : personalData) {
                        dataToCopy += "Name: " + d.name + "\n";
                        dataToCopy += "Email: " + d.email + "\n";
                        dataToCopy += "Phone: " + d.phone + "\n";
                        dataToCopy += "Birth Date: " + d.birthDate + "\n";
                        dataToCopy += "Address: " + d.address + "\n\n";
                    }
                }
                if (!financialData.empty()) {
                    dataToCopy += "=== FINANCIAL DATA ===\n";
                    for (const auto& d : financialData) {
                        dataToCopy += "Card Number: " + d.cardNumber + "\n";
                        dataToCopy += "CVV: " + d.cvv + "\n";
                        dataToCopy += "Expiry Date: " + d.expiryDate + "\n\n";
                    }
                }
                if (!invalidData.empty()) {
                    dataToCopy += "=== INVALID DATA ===\n";
                    for (const auto& d : invalidData) {
                        dataToCopy += "SQL Injection: " + d.sqlInjection + "\n";
                        dataToCopy += "XSS: " + d.xss + "\n";
                        dataToCopy += "Boundary Test: " + d.boundaryTest + "\n\n";
                    }
                }

                if (copyToClipboard(dataToCopy)) {
                    statusText.setString("Data copied to clipboard!");
                    statusText.setFillColor(sf::Color::Green);
                }
                else {
                    statusText.setString("Error copying to clipboard!");
                    statusText.setFillColor(sf::Color::Red);
                }
            }
        }

        
        float progressNow = g_progress.load(std::memory_order_relaxed);
        progressBar.setProgress(progressNow);

        if (g_statusUpdated.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(g_statusMutex);
            statusText.setString(g_statusMessage);
            if (g_generating.load(std::memory_order_relaxed)) {
                statusText.setFillColor(sf::Color::Blue);
            }
            else {
                statusText.setFillColor(sf::Color::Green);
            }
            g_statusUpdated.store(false, std::memory_order_relaxed);
        }

       
        generateButton.update(window);
        copyButton.update(window);
        dataTypeDropdown.update(window);
        exportFormatDropdown.update(window);
        languageDropdown.update(window);
        qualityDropdown.update(window);
        countTextBox.update(window);

        int dtIndex = dataTypeDropdown.getSelectedIndex();
        bool showPreset = (dtIndex == static_cast<int>(DataType::INVALID) || dtIndex == static_cast<int>(DataType::ALL));
        if (showPreset) presetDropdown.update(window);


        
        bool hasData = !personalData.empty() || !financialData.empty() || !invalidData.empty();
        copyButton.setEnabled(!generating && hasData);

        // Отрисовка
        window.clear(sf::Color::White);

        window.draw(title);
        countTextBox.draw(window);
        generateButton.draw(window);
        copyButton.draw(window);
        progressBar.draw(window);
        window.draw(statusText);

        exportFormatDropdown.draw(window);
        dataTypeDropdown.draw(window);
        languageDropdown.draw(window);
        qualityDropdown.draw(window);
        
        if (showPreset) presetDropdown.draw(window);


        window.display();
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        return runCliMode(argc, argv);
    }
    else {
        return runGuiMode();
    }
}
