#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <random>
#include <deque>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <pqxx/pqxx>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <windows.h>

class ServerClient {
    private:
    httplib::Client client;
    
    bool sendPostRequest(const std::string& endpoint, const nlohmann::json& body) {
        if (!client.is_valid()) {
            std::cerr << "Error: client invalid!" << std::endl;
            return false;
        }
        httplib::Result result = client.Post(endpoint, body.dump(), "application/json");
        return result && result->status == 200;
    }

    public:
    std::string token;
    bool isAuthorized;

    ServerClient() : client("https://localhost:8080") {
        client.set_default_headers({{"Content-Type", "application/json"}});
        client.set_ca_cert_path("server-cert.pem");
    }

    bool isTokenValid() {
        if (token.empty()) {
            std::cerr << "Token is empty.\n";
            isAuthorized = false;
            return false;
        }
        httplib::Headers headers = {{"Authorization", "Bearer " + token}};
        httplib::Result result = client.Get("/leaderboard", headers);
        if (!result || result->status != 200) {
            std::cerr << "Token validation failed. Server response: " << (result ? result->body : "No response") << "\n";
            isAuthorized = false;
            token.clear();
            return false;
        }
        isAuthorized = true;
        return true;
    }

    bool registerUser(const std::string& username, const std::string& password) {
        return sendPostRequest("/register", {{"username", username}, {"password", password}});
    }

    bool loginUser(const std::string& username, const std::string& password) {
        nlohmann::json requestBody = {
            {"username", username},
            {"password", password}
        };
        httplib::Result result = client.Post("/login", 
            {{"Content-Type", "application/json"}}, 
            requestBody.dump(), 
            "application/json");
            if (!result) {
                std::cerr << "Error: no server return." << std::endl;
                return false;
            }
            if (result->status != 200) {
                std::cerr << "Authorisation error. Code: " << result->status << std::endl;
                return false;
            }
            try {
                nlohmann::json response = nlohmann::json::parse(result->body);
                if (response.contains("token")) {
                    token = response["token"];
                    std::cout << "Login success. Token: " << token << std::endl;
                    isTokenValid();
                    return true;
                } else {
                    std::cerr << "Error: No token return!" << std::endl;
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
                return false;
            }
        return sendPostRequest("/login", {{"username", username}, {"password", password}});
    }

    bool updateUserHighScore(const int& newScore) {
        if (!isTokenValid()) {
            std::cerr << "Error! Token expired.\n";
            return false;
        }
        nlohmann::json requestBody = {{"score", newScore}};
        httplib::Result result = client.Post("/update_user_score", {{"Authorization", "Bearer " + token}}, requestBody.dump(), "application/json");
        return result && result->status == 200;
    }

    std::vector<std::pair<std::string, int>> fetchLeaderboard() {
        if (!isTokenValid()) {
            std::cerr << "Error. Token expired. Please login.\n";
            return {};
        }
        httplib::Headers headers = {{"Authorization", "Bearer " + token}};
        httplib::Result result = client.Get("/leaderboard", headers);
        if (!result || result->status != 200) {
            std::cerr << "Failed to get leaderboard: " << (result ? std::to_string(result->status) : "No return.") << "\n";
            return {};
        }
        try {
            nlohmann::json jsonResponse = nlohmann::json::parse(result->body);
            std::vector<std::pair<std::string, int>> leaderboard;
            for (const auto& entry : jsonResponse) {
                std::string username = entry["username"];
                int score = entry["highscore"];
                if (score == 0) continue;
                leaderboard.emplace_back(username, score);
            }
            std::sort(leaderboard.begin(), leaderboard.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
            return leaderboard;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse JSON: " << e.what() << '\n';
            return {};
        }
    }
};

class TextInput {
    private:
    public:
    
    sf::Text textLogin;
    sf::Text textPass;
    TextInput(sf::Font& font) {
        textLogin.setFont(font);
        textLogin.setFillColor(sf::Color(31, 32, 52));
        textLogin.setCharacterSize(75);
        textLogin.setPosition(770, 430);
        textLogin.setLetterSpacing(0.3);
        textPass.setFont(font);
        textPass.setFillColor(sf::Color(31, 32, 52));
        textPass.setCharacterSize(75);
        textPass.setPosition(770, 555);
        textPass.setLetterSpacing(0.3);
    }

    bool text2Login(std::string& inputText){
        textLogin.setString(inputText);
        return true;
    }

    bool text2Pass(std::string& inputText){
        textPass.setString(inputText);
        return true;
    }
};



void closeWindow(sf::RenderWindow& window){ 
    window.close();
}

class ConfigManager {
    public:
    ServerClient& serverClient;
    ConfigManager(ServerClient& serverClient) : serverClient{serverClient} {}

    void loadSettings(int& musicVolume, int& musicSliderInt, bool& isMusic, int& soundVolume, int& soundSliderInt, bool& isSound, float& moveInterval, int& choseItem){
        std::ifstream file("cfg.txt");
        std::string line;
        if (file.is_open()){
            while (std::getline(file, line)){
                if (line.starts_with        ("musicVolume="))       musicVolume = std::stoi(line.substr(12));
                else if (line.starts_with   ("musicSliderInt="))    musicSliderInt = std::stoi(line.substr(15));
                else if (line.starts_with   ("isMusic="))           isMusic = std::stoi(line.substr(8));
                else if (line.starts_with   ("soundVolume="))       soundVolume = std::stoi(line.substr(12));
                else if (line.starts_with   ("soundSliderInt="))    soundSliderInt = std::stoi(line.substr(15));
                else if (line.starts_with   ("isSound="))           isSound = std::stoi(line.substr(8));
                else if (line.starts_with   ("moveInterval="))      moveInterval = std::stof(line.substr(13));
                else if (line.starts_with   ("choseItem="))         choseItem = std::stoi(line.substr(10));
                else if (line.starts_with   ("token="))             serverClient.token = line.substr(6);
            }
            file.close();
        }
    }

    void saveSettings(int& musicVolume, int& musicSliderInt, bool& isMusic, int& soundVolume, int& soundSliderInt, bool& isSound, float& moveInterval, int& choseItem){
        std::ofstream file("cfg.txt");
        if (file.is_open()){
            file << "musicVolume=" <<       musicVolume << "\n";
            file << "musicSliderInt=" <<    musicSliderInt << "\n";
            file << "isMusic=" <<           isMusic << "\n";
            file << "soundVolume=" <<       soundVolume << "\n";
            file << "soundSliderInt=" <<    soundSliderInt << "\n";
            file << "isSound=" <<           isSound << "\n";
            file << "moveInterval=" <<      moveInterval << "\n";
            file << "choseItem=" <<         choseItem << "\n";
            file << "token=" <<             serverClient.token << "\n";
            file.close();
        }
    }
};

class AudioManager {
    public:
    int soundVolumeI, musicVolumeI;
    sf::Music musicChillGuy, soundUIClick, soundFoodPop;

    AudioManager(){
        musicChillGuy.openFromFile("assets/audio/ChillGuyTheme.ogg");
        soundUIClick.openFromFile("assets/audio/ClickStereo.ogg");
        soundFoodPop.openFromFile("assets/audio/SnesPop.ogg");
        musicChillGuy.setVolume(0);
    }

    void playMusic(){
        musicChillGuy.play();
        musicChillGuy.setLoop(true);
    }

    void playSoundUIClick(){
        soundUIClick.stop();
        soundUIClick.play();
    }

    void playSoundFoodPop(){
        soundFoodPop.play();
    }

    void soundUpdate(bool& isSound, int& soundVolume){
        if (isSound) {
            soundFoodPop.setVolume(soundVolume);
            soundUIClick.setVolume(soundVolume);
        } else {
            soundFoodPop.setVolume(0);
            soundUIClick.setVolume(0);
        }
    }

    void musicUpdate(bool& isMusic, int& musicVolume){
        if (isMusic) musicChillGuy.setVolume(musicVolume);
        else musicChillGuy.setVolume(0);
    }
};

class UserInterface {    
    public:
    std::array<sf::Sprite,10> digitSprites;
    std::array<sf::Sprite,16> blockSprites;
    std::array<sf::Texture,3> preGameTimerTextures;
    sf::Font font;
    sf::Texture background, backgroundm, backgroundmblur, backgroundAYS, textBACK, textBACKpressed, textgoals, textgoalscontain, textquit, textquitcontain, textsetup, textsetupcontain, textstart, textstartcontain, noback, nobackdark, nored, nowhite, yesback, yesbackdark, yesgreen, yeswhite, exit, snakeback, score, zero, one, two, three, four, five, six, seven, eight, nine, locked, inGameSettingsBACK, inGameSettingsBACKpressed, inGameSettingsFRONT, inGameSettingsFRONTcont, block0, block1, block2, block3, block4, block5, block6, block7, block8, block9, block10, block11, block12, block13, block14, block15, CLS, INF, ARC, CLSwhite, INFwhite, ARCwhite, selectmodeBACK, selectsmodesBACK, selectsmodesBACKpressed, textselectmode, selectmodeESC, selectmodeESCcont, selectmodeESCBACK, selectmodeESCBACKpressed, chose0, chose1, chose2, music, musicCont, musicOFF, musicON, sound, soundCont, soundOFF, soundON0, soundON1, soundON2, speedBACK0, speedBACK0pressed, speedBACK1, speedBACK1pressed, speedBACK2, speedBACK2pressed, speedTEXT0, speedTEXT0cont, speedTEXT1, speedTEXT1cont, speedTEXT2, speedTEXT2cont, setupTextBACK, setupTextBACKpressed, textBACKSFXMSC, textBACKSFXMSCpressed, setupback, setupbackCont, backgroundg, resume, resumeCont, GREENbackground, BLUEbackground, PURPLEbackground, REDbackground, ORANGEbackground, YELLOWbackground, BLUEsnakeHead, BLUEsnakeBody, PURPLEsnakeHead, PURPLEsnakeBody, REDsnakeHead, REDsnakeBody, ORANGEsnakeHead, ORANGEsnakeBody, YELLOWsnakeHead, YELLOWsnakeBody, null, textagain, textagaincontain, ARChole, foodextra, wasted, youwon, oneBIGwhite, twoBIGwhite, threeBIGwhite, loginback, loginbackpressed, loginfront, loginfrontcont, logoutback, logoutbackpressed, logoutfront, logoutfrontcont, logreg, textloginBACK, textloginBACKpressed, textlogin, textlogincont, textsignin, textsignincont, logregESC, logregESCcont, logregESCback, logregESCbackpressed, logregback2, textlog, textpass, logregbackno, logregbacknopressed, logregbackyes, logregbackyespressed, reg, regcont, log, logcont;
    sf::Sprite backgroundSprite, backgroundmSprite, backgroundmblurSprite, backgroundAYSSprite, textBACKSprite1, textBACKSprite2, textBACKSprite3, textBACKSprite4, textBACKpressedSprite1, textBACKpressedSprite2, textBACKpressedSprite3, textBACKpressedSprite4, textgoalsSprite, textgoalscontainSprite, textquitSprite1, textquitSprite2, textquitcontainSprite1, textquitcontainSprite2, textsetupSprite, textsetupcontainSprite, textstartSprite, textstartcontainSprite, nobackSprite, nobackdarkSprite, noredSprite1, noredSprite2, nowhiteSprite1, nowhiteSprite2, yesbackSprite, yesbackdarkSprite, yesgreenSprite, yeswhiteSprite, exitSprite, snakebackSprite, scoreSprite, zeroSprite, oneSprite, twoSprite, threeSprite, fourSprite, fiveSprite, sixSprite, sevenSprite, eightSprite, nineSprite, lockedSprite, inGameSettingsBACKSprite, inGameSettingsBACKpressedSprite, inGameSettingsFRONTSprite, inGameSettingsFRONTcontSprite, block0Sprite, block1Sprite, block2Sprite, block3Sprite, block4Sprite, block5Sprite, block6Sprite, block7Sprite, block8Sprite, block9Sprite, block10Sprite, block11Sprite, block12Sprite, block13Sprite, block14Sprite, block15Sprite, CLSSprite, INFSprite, ARCSprite, CLSwhiteSprite, INFwhiteSprite, ARCwhiteSprite, selectmodeBACKSprite, selectsmodesBACK1Sprite, selectsmodesBACK2Sprite, selectsmodesBACK3Sprite, selectsmodesBACK1pressedSprite, selectmodeESCSprite, selectsmodesBACK2pressedSprite, selectsmodesBACK3pressedSprite, textselectmodeSprite, selectmodeESCcontSprite, selectmodeESCBACKSprite, selectmodeESCBACKpressedSprite, chose0Sprite, chose1Sprite, chose2Sprite, musicSprite, musicContSprite, musicOFFSprite, musicONSprite, soundSprite, soundContSprite, soundOFFSprite, soundON0Sprite, soundON1Sprite, soundON2Sprite, speedBACK0Sprite, speedBACK0pressedSprite, speedBACK1Sprite, speedBACK1pressedSprite, speedBACK2Sprite, speedBACK2pressedSprite, speedTEXT0Sprite, speedTEXT0contSprite, speedTEXT1Sprite, speedTEXT1contSprite, speedTEXT2Sprite, speedTEXT2contSprite, setupTextBACKSprite, setupTextBACKpressedSprite, textBACKSFXMSC0Sprite, textBACKSFXMSC1Sprite, textBACKSFXMSC0pressedSprite, textBACKSFXMSC1pressedSprite, setupbackSprite, setupbackContSprite, backgroundgSprite, resumeSprite , resumeContSprite, GREENbackgroundSprite, BLUEbackgroundSprite, PURPLEbackgroundSprite, REDbackgroundSprite, ORANGEbackgroundSprite, YELLOWbackgroundSprite, BLUEsnakeHeadSprite, BLUEsnakeBodySprite, PURPLEsnakeHeadSprite, PURPLEsnakeBodySprite, REDsnakeHeadSprite, REDsnakeBodySprite, ORANGEsnakeHeadSprite, ORANGEsnakeBodySprite, YELLOWsnakeHeadSprite, YELLOWsnakeBodySprite, nullSprite, textagainSprite, textagaincontainSprite, ARCholeSprite1, ARCholeSprite2, foodextraSprite, wastedSprite, youwonSprite, oneBIGwhiteSprite, twoBIGwhiteSprite, threeBIGwhiteSprite, loginbackSprite, loginbackpressedSprite, loginfrontSprite, loginfrontcontSprite, logoutbackSprite, logoutbackpressedSprite, logoutfrontSprite, logoutfrontcontSprite, logregSprite, textloginBACKSprite, textloginBACKpressedSprite, textloginSprite, textlogincontSprite, textsigninSprite, textsignincontSprite, logregESCSprite, logregESCcontSprite, logregESCbackSprite, logregESCbackpressedSprite, logregback2Sprite, textlogSprite, textpassSprite, logregbacknoSprite, logregbacknopressedSprite, logregbackyesSprite, logregbackyespressedSprite, regSprite, regcontSprite, logSprite, logcontSprite;
    int releasedItem, containItem, pressedItem, inGameContain, inGamePressed, inGameReleased, logregReleasedItem;
    bool isGamePaused;

    UserInterface() : releasedItem{0}, containItem {0}, pressedItem{0}, inGameContain{0}, inGamePressed{0}, inGameReleased{0}, logregReleasedItem{0}, isGamePaused{false}{
        Texture2Sprite(background, backgroundSprite, "assets/sprites/background.png", 0, 0);
        Texture2Sprite(backgroundm, backgroundmSprite, "assets/sprites/backgroundm.png", 720, 260);
        Texture2Sprite(backgroundmblur, backgroundmblurSprite, "assets/sprites/backgroundmblur.png", 735, 275);
        Texture2Sprite(backgroundAYS, backgroundAYSSprite, "assets/sprites/backgroundAYS.png", 680, 405);
        Texture2Sprite(textBACK, textBACKSprite1, "assets/sprites/textBACK.png", 750, 300);
        Texture2Sprite(textBACK, textBACKSprite2, "assets/sprites/textBACK.png", 750, 425);
        Texture2Sprite(textBACK, textBACKSprite3, "assets/sprites/textBACK.png", 750, 550);
        Texture2Sprite(textBACK, textBACKSprite4, "assets/sprites/textBACK.png", 750, 675);
        Texture2Sprite(textBACKpressed, textBACKpressedSprite1, "assets/sprites/textBACKpressed.png", 750, 300);
        Texture2Sprite(textBACKpressed, textBACKpressedSprite2, "assets/sprites/textBACKpressed.png", 750, 425);
        Texture2Sprite(textBACKpressed, textBACKpressedSprite3, "assets/sprites/textBACKpressed.png", 750, 550);
        Texture2Sprite(textBACKpressed, textBACKpressedSprite4, "assets/sprites/textBACKpressed.png", 750, 675);
        Texture2Sprite(textgoals, textgoalsSprite, "assets/sprites/textgoals.png", 878, 572);
        Texture2Sprite(textgoalscontain, textgoalscontainSprite, "assets/sprites/textgoalscontain.png", 878, 572);
        Texture2Sprite(textquit, textquitSprite1, "assets/sprites/textquit.png", 856, 697);
        Texture2Sprite(textquit, textquitSprite2, "assets/sprites/textquit.png", 856, 572);
        Texture2Sprite(textquitcontain, textquitcontainSprite1, "assets/sprites/textquitcontain.png", 856, 697);
        Texture2Sprite(textquitcontain, textquitcontainSprite2, "assets/sprites/textquitcontain.png", 856, 572);
        Texture2Sprite(textsetup, textsetupSprite, "assets/sprites/textsetup.png", 817, 447);
        Texture2Sprite(textsetupcontain, textsetupcontainSprite, "assets/sprites/textsetupcontain.png", 817, 447);
        Texture2Sprite(textstart, textstartSprite, "assets/sprites/textstart.png", 817, 322);
        Texture2Sprite(textstartcontain, textstartcontainSprite, "assets/sprites/textstartcontain.png", 817, 322);
        Texture2Sprite(noback, nobackSprite, "assets/sprites/noback.png", 970, 550);
        Texture2Sprite(nobackdark, nobackdarkSprite, "assets/sprites/nobackdark.png", 970, 550);
        Texture2Sprite(nored, noredSprite1, "assets/sprites/nored.png", 1042, 572);
        Texture2Sprite(nored, noredSprite2, "assets/sprites/nored.png", 787, 728);
        Texture2Sprite(nowhite, nowhiteSprite1, "assets/sprites/nowhite.png", 1042, 572);
        Texture2Sprite(nowhite, nowhiteSprite2, "assets/sprites/nowhite.png", 787, 728);
        Texture2Sprite(yesback, yesbackSprite, "assets/sprites/yesback.png", 700, 550);
        Texture2Sprite(yesbackdark, yesbackdarkSprite, "assets/sprites/yesbackdark.png", 700, 550);
        Texture2Sprite(yesgreen, yesgreenSprite, "assets/sprites/yesgreen.png", 743, 572);
        Texture2Sprite(yeswhite, yeswhiteSprite, "assets/sprites/yeswhite.png", 743, 572);
        Texture2Sprite(exit, exitSprite, "assets/sprites/exit.png", 825, 447);
        Texture2Sprite(snakeback, snakebackSprite, "assets/sprites/snakeback.png", 90, 82);
        Texture2Sprite(score, scoreSprite, "assets/sprites/score.png", 120, 112);
        Texture2Sprite(zero, zeroSprite, "assets/sprites/0zero.png");
        Texture2Sprite(one, oneSprite, "assets/sprites/1one.png");
        Texture2Sprite(two, twoSprite, "assets/sprites/2two.png");
        Texture2Sprite(three, threeSprite, "assets/sprites/3three.png");
        Texture2Sprite(four, fourSprite, "assets/sprites/4four.png");
        Texture2Sprite(five, fiveSprite, "assets/sprites/5five.png");
        Texture2Sprite(six, sixSprite, "assets/sprites/6six.png");
        Texture2Sprite(seven, sevenSprite, "assets/sprites/7seven.png");
        Texture2Sprite(eight, eightSprite, "assets/sprites/8eight.png");
        Texture2Sprite(nine, nineSprite, "assets/sprites/9nine.png");
        Texture2Sprite(inGameSettingsBACK, inGameSettingsBACKSprite, "assets/sprites/inGameSettings/inGameSettingsBACK.png", 1740, 113);
        Texture2Sprite(inGameSettingsBACKpressed, inGameSettingsBACKpressedSprite, "assets/sprites/inGameSettings/inGameSettingsBACKpressed.png", 1740, 113);
        Texture2Sprite(inGameSettingsFRONT, inGameSettingsFRONTSprite, "assets/sprites/inGameSettings/inGameSettingsFRONT.png", 1748, 121);
        Texture2Sprite(inGameSettingsFRONTcont, inGameSettingsFRONTcontSprite, "assets/sprites/inGameSettings/inGameSettingsFRONTcont.png", 1748, 121);
        Texture2Sprite(locked, lockedSprite, "assets/sprites/inGameSettings/locked.png", 1756, 125);
        Texture2Sprite(block0, block0Sprite, "assets/sprites/inGameSettings/block0.png", 1744, 117);
        Texture2Sprite(block1, block1Sprite, "assets/sprites/inGameSettings/block1.png", 1744, 117);
        Texture2Sprite(block2, block2Sprite, "assets/sprites/inGameSettings/block2.png", 1744, 117);
        Texture2Sprite(block3, block3Sprite, "assets/sprites/inGameSettings/block3.png", 1744, 117);
        Texture2Sprite(block4, block4Sprite, "assets/sprites/inGameSettings/block4.png", 1744, 117);
        Texture2Sprite(block5, block5Sprite, "assets/sprites/inGameSettings/block5.png", 1744, 117);
        Texture2Sprite(block6, block6Sprite, "assets/sprites/inGameSettings/block6.png", 1744, 117);
        Texture2Sprite(block7, block7Sprite, "assets/sprites/inGameSettings/block7.png", 1744, 117);
        Texture2Sprite(block8, block8Sprite, "assets/sprites/inGameSettings/block8.png", 1744, 117);
        Texture2Sprite(block9, block9Sprite, "assets/sprites/inGameSettings/block9.png", 1744, 117);
        Texture2Sprite(block10, block10Sprite, "assets/sprites/inGameSettings/block10.png", 1744, 117);
        Texture2Sprite(block11, block11Sprite, "assets/sprites/inGameSettings/block11.png", 1744, 117);
        Texture2Sprite(block12, block12Sprite, "assets/sprites/inGameSettings/block12.png", 1744, 117);
        Texture2Sprite(block13, block13Sprite, "assets/sprites/inGameSettings/block13.png", 1744, 117);
        Texture2Sprite(block14, block14Sprite, "assets/sprites/inGameSettings/block14.png", 1744, 117);
        Texture2Sprite(block15, block15Sprite, "assets/sprites/inGameSettings/block15.png", 1744, 117);
        Texture2Sprite(CLS, CLSSprite, "assets/sprites/CLS.png", 567, 572);
        Texture2Sprite(CLSwhite, CLSwhiteSprite, "assets/sprites/CLSwhite.png", 567, 572);
        Texture2Sprite(INF, INFSprite, "assets/sprites/INF.png", 886, 572);
        Texture2Sprite(INFwhite, INFwhiteSprite, "assets/sprites/INFwhite.png", 886, 572);
        Texture2Sprite(ARC, ARCSprite, "assets/sprites/ARC.png", 1187, 572);
        Texture2Sprite(ARCwhite, ARCwhiteSprite, "assets/sprites/ARCwhite.png", 1187, 572);
        Texture2Sprite(selectmodeBACK, selectmodeBACKSprite, "assets/sprites/selectmodeBACK.png", 480, 395);
        Texture2Sprite(selectsmodesBACK, selectsmodesBACK1Sprite, "assets/sprites/selectsmodesBACK.png", 510, 550);
        Texture2Sprite(selectsmodesBACK, selectsmodesBACK2Sprite, "assets/sprites/selectsmodesBACK.png", 820, 550);
        Texture2Sprite(selectsmodesBACK, selectsmodesBACK3Sprite, "assets/sprites/selectsmodesBACK.png", 1130, 550);
        Texture2Sprite(selectsmodesBACKpressed, selectsmodesBACK1pressedSprite, "assets/sprites/selectsmodesBACKpressed.png", 510, 550);
        Texture2Sprite(selectsmodesBACKpressed, selectsmodesBACK2pressedSprite, "assets/sprites/selectsmodesBACKpressed.png", 820, 550);
        Texture2Sprite(selectsmodesBACKpressed, selectsmodesBACK3pressedSprite, "assets/sprites/selectsmodesBACKpressed.png", 1130, 550);
        Texture2Sprite(textselectmode, textselectmodeSprite, "assets/sprites/textselectmode.png", 638, 440);
        Texture2Sprite(selectmodeESCBACK, selectmodeESCBACKSprite, "assets/sprites/selectmodeESCBACK.png", 490, 405);
        Texture2Sprite(selectmodeESCBACKpressed, selectmodeESCBACKpressedSprite, "assets/sprites/selectmodeESCBACKpressed.png", 490, 405);
        Texture2Sprite(selectmodeESC, selectmodeESCSprite, "assets/sprites/selectmodeESC.png", 494, 409);
        Texture2Sprite(selectmodeESCcont, selectmodeESCcontSprite, "assets/sprites/selectmodeESCcont.png", 494, 409);
        Texture2Sprite(chose0, chose0Sprite, "assets/sprites/Setup/chose0.png", 750, 550);
        Texture2Sprite(chose1, chose1Sprite, "assets/sprites/Setup/chose1.png", 890, 550);
        Texture2Sprite(chose2, chose2Sprite, "assets/sprites/Setup/chose2.png", 1030, 550);
        Texture2Sprite(music, musicSprite, "assets/sprites/Setup/music.png", 772, 322);
        Texture2Sprite(musicCont, musicContSprite, "assets/sprites/Setup/musicCont.png", 772, 322);
        Texture2Sprite(musicOFF, musicOFFSprite, "assets/sprites/Setup/musicOFF.png", 1089, 308);
        Texture2Sprite(musicON, musicONSprite, "assets/sprites/Setup/musicON.png", 1089, 308);
        Texture2Sprite(sound, soundSprite, "assets/sprites/Setup/sound.png", 772, 447);
        Texture2Sprite(soundCont, soundContSprite, "assets/sprites/Setup/soundCont.png", 772, 447);
        Texture2Sprite(soundOFF, soundOFFSprite, "assets/sprites/Setup/soundOFF.png", 1089, 438);
        Texture2Sprite(soundON0, soundON0Sprite, "assets/sprites/Setup/soundON0.png", 1089, 438);
        Texture2Sprite(soundON1, soundON1Sprite, "assets/sprites/Setup/soundON1.png", 1089, 438);
        Texture2Sprite(soundON2, soundON2Sprite, "assets/sprites/Setup/soundON2.png", 1089, 438);
        Texture2Sprite(speedBACK0, speedBACK0Sprite, "assets/sprites/Setup/speedBACK0.png", 750, 550);
        Texture2Sprite(speedBACK0pressed, speedBACK0pressedSprite, "assets/sprites/Setup/speedBACK0pressed.png", 750, 550);
        Texture2Sprite(speedBACK1, speedBACK1Sprite, "assets/sprites/Setup/speedBACK1.png", 890, 550);
        Texture2Sprite(speedBACK1pressed, speedBACK1pressedSprite, "assets/sprites/Setup/speedBACK1pressed.png", 890, 550);
        Texture2Sprite(speedBACK2, speedBACK2Sprite, "assets/sprites/Setup/speedBACK2.png", 1030, 550);
        Texture2Sprite(speedBACK2pressed, speedBACK2pressedSprite, "assets/sprites/Setup/speedBACK2pressed.png", 1030, 550);
        Texture2Sprite(speedTEXT0, speedTEXT0Sprite, "assets/sprites/Setup/speedTEXT0.png", 823, 574);
        Texture2Sprite(speedTEXT0cont, speedTEXT0contSprite, "assets/sprites/Setup/speedTEXT0cont.png", 823, 574);
        Texture2Sprite(speedTEXT1, speedTEXT1Sprite, "assets/sprites/Setup/speedTEXT1.png", 890, 574);
        Texture2Sprite(speedTEXT1cont, speedTEXT1contSprite, "assets/sprites/Setup/speedTEXT1cont.png", 890, 574);
        Texture2Sprite(speedTEXT2, speedTEXT2Sprite, "assets/sprites/Setup/speedTEXT2.png", 1030, 574);
        Texture2Sprite(speedTEXT2cont, speedTEXT2contSprite, "assets/sprites/Setup/speedTEXT2cont.png", 1030, 574);
        Texture2Sprite(setupTextBACK, setupTextBACKSprite, "assets/sprites/Setup/textBACK.png", 750, 675);
        Texture2Sprite(setupTextBACKpressed, setupTextBACKpressedSprite, "assets/sprites/Setup/textBACKpressed.png", 750, 675);
        Texture2Sprite(textBACKSFXMSC, textBACKSFXMSC0Sprite, "assets/sprites/Setup/textBACKSFXMSC.png", 750, 300);
        Texture2Sprite(textBACKSFXMSC, textBACKSFXMSC1Sprite, "assets/sprites/Setup/textBACKSFXMSC.png", 750, 425);
        Texture2Sprite(textBACKSFXMSCpressed, textBACKSFXMSC0pressedSprite, "assets/sprites/Setup/textBACKSFXMSCpressed.png", 750, 300);
        Texture2Sprite(textBACKSFXMSCpressed, textBACKSFXMSC1pressedSprite, "assets/sprites/Setup/textBACKSFXMSCpressed.png", 750, 425);
        Texture2Sprite(setupback, setupbackSprite, "assets/sprites/Setup/setupback.png", 849, 699);
        Texture2Sprite(setupbackCont, setupbackContSprite, "assets/sprites/Setup/setupbackCont.png", 849, 699);
        Texture2Sprite(backgroundg, backgroundgSprite, "assets/sprites/backgroundg.png", 720, 260);
        Texture2Sprite(resume, resumeSprite, "assets/sprites/resume.png", 778, 322);
        Texture2Sprite(resumeCont, resumeContSprite, "assets/sprites/resumeCont.png", 778, 322);
        Texture2Sprite(GREENbackground, GREENbackgroundSprite, "assets/sprites/INFMode/1GREENbackground.png");
        Texture2Sprite(BLUEbackground, BLUEbackgroundSprite, "assets/sprites/INFMode/2BLUEbackground.png");
        Texture2Sprite(PURPLEbackground, PURPLEbackgroundSprite, "assets/sprites/INFMode/3PURPLEbackground.png");
        Texture2Sprite(REDbackground, REDbackgroundSprite, "assets/sprites/INFMode/4REDbackground.png");
        Texture2Sprite(ORANGEbackground, ORANGEbackgroundSprite, "assets/sprites/INFMode/5ORANGEbackground.png");
        Texture2Sprite(YELLOWbackground, YELLOWbackgroundSprite, "assets/sprites/INFMode/6YELLOWbackground.png");
        Texture2Sprite(BLUEsnakeHead, BLUEsnakeHeadSprite, "assets/sprites/INFMode/BLUEsnakeHead.png");
        Texture2Sprite(BLUEsnakeBody, BLUEsnakeBodySprite, "assets/sprites/INFMode/BLUEsnakeBody.png");
        Texture2Sprite(PURPLEsnakeHead, PURPLEsnakeHeadSprite, "assets/sprites/INFMode/PURPLEsnakeHead.png");
        Texture2Sprite(PURPLEsnakeBody, PURPLEsnakeBodySprite, "assets/sprites/INFMode/PURPLEsnakeBody.png");
        Texture2Sprite(REDsnakeHead, REDsnakeHeadSprite, "assets/sprites/INFMode/REDsnakeHead.png");
        Texture2Sprite(REDsnakeBody, REDsnakeBodySprite, "assets/sprites/INFMode/REDsnakeBody.png");
        Texture2Sprite(ORANGEsnakeHead, ORANGEsnakeHeadSprite, "assets/sprites/INFMode/ORANGEsnakeHead.png");
        Texture2Sprite(ORANGEsnakeBody, ORANGEsnakeBodySprite, "assets/sprites/INFMode/ORANGEsnakeBody.png");
        Texture2Sprite(YELLOWsnakeHead, YELLOWsnakeHeadSprite, "assets/sprites/INFMode/YELLOWsnakeHead.png");
        Texture2Sprite(YELLOWsnakeBody, YELLOWsnakeBodySprite, "assets/sprites/INFMode/YELLOWsnakeBody.png");
        Texture2Sprite(null, nullSprite, "assets/sprites/null.png");
        Texture2Sprite(textagain, textagainSprite, "assets/sprites/textagain.png", 825, 447);
        Texture2Sprite(textagaincontain, textagaincontainSprite, "assets/sprites/textagaincontain.png", 825, 447);
        Texture2Sprite(ARChole, ARCholeSprite1, "assets/sprites/ARChole.png");
        Texture2Sprite(ARChole, ARCholeSprite2, "assets/sprites/ARChole.png");
        Texture2Sprite(foodextra, foodextraSprite, "assets/sprites/foodextra.png");
        Texture2Sprite(wasted, wastedSprite, "assets/sprites/wasted.png", 777, 322);
        Texture2Sprite(youwon, youwonSprite, "assets/sprites/wasted.png", 759, 322);
        Texture2Sprite(oneBIGwhite, oneBIGwhiteSprite, "assets/sprites/1oneBIGwhite.png");
        Texture2Sprite(twoBIGwhite, twoBIGwhiteSprite, "assets/sprites/2twoBIGwhite.png");
        Texture2Sprite(threeBIGwhite, threeBIGwhiteSprite, "assets/sprites/3threeBIGwhite.png");
        Texture2Sprite(loginback, loginbackSprite, "assets/sprites/loginback.png", 1230, 550);
        Texture2Sprite(loginbackpressed, loginbackpressedSprite, "assets/sprites/loginbackpressed.png", 1230, 550);
        Texture2Sprite(loginfront, loginfrontSprite, "assets/sprites/loginfront.png", 1230, 550);
        Texture2Sprite(loginfrontcont, loginfrontcontSprite, "assets/sprites/loginfrontcont.png", 1230, 550);
        Texture2Sprite(logoutback, logoutbackSprite, "assets/sprites/logoutback.png", 1230, 550);
        Texture2Sprite(logoutbackpressed, logoutbackpressedSprite, "assets/sprites/logoutbackpressed.png", 1230, 550);
        Texture2Sprite(logoutfront, logoutfrontSprite, "assets/sprites/logoutfront.png", 1230, 550);
        Texture2Sprite(logoutfrontcont, logoutfrontcontSprite, "assets/sprites/logoutfrontcont.png", 1230, 550);
        Texture2Sprite(logreg, logregSprite, "assets/sprites/logreg.png", 720, 385);
        Texture2Sprite(textlogin, textloginSprite, "assets/sprites/textlogin.png", 865, 447);
        Texture2Sprite(textlogincont, textlogincontSprite, "assets/sprites/textlogincont.png", 865, 447);
        Texture2Sprite(textsignin, textsigninSprite, "assets/sprites/textsignin.png", 786, 572);
        Texture2Sprite(textsignincont, textsignincontSprite, "assets/sprites/textsignincont.png", 786, 572);
        Texture2Sprite(logregESC, logregESCSprite, "assets/sprites/logregESC.png", 754, 429);
        Texture2Sprite(logregESCcont, logregESCcontSprite, "assets/sprites/logregESCcont.png", 754, 429);
        Texture2Sprite(logregESCback, logregESCbackSprite, "assets/sprites/logregESCBACK.png", 750, 424);
        Texture2Sprite(logregESCbackpressed, logregESCbackpressedSprite, "assets/sprites/logregESCBACKpressed.png", 750, 425);
        Texture2Sprite(textloginBACK, textloginBACKSprite, "assets/sprites/textloginBACK.png", 830, 425);
        Texture2Sprite(textloginBACKpressed, textloginBACKpressedSprite, "assets/sprites/textloginBACKpressed.png", 830, 425);
        Texture2Sprite(logregback2, logregback2Sprite, "assets/sprites/logregback2.png", 720, 385);
        Texture2Sprite(textlog, textlogSprite, "assets/sprites/textlog.png", 834, 442);
        Texture2Sprite(textpass, textpassSprite, "assets/sprites/textpass.png", 847, 584);
        Texture2Sprite(logregbackno, logregbacknoSprite, "assets/sprites/logregbackno.png", 720, 707);
        Texture2Sprite(logregbacknopressed, logregbacknopressedSprite, "assets/sprites/logregbacknopressed.png", 720, 707);
        Texture2Sprite(logregbackyes, logregbackyesSprite, "assets/sprites/logregbackyes.png", 960, 707);
        Texture2Sprite(logregbackyespressed, logregbackyespressedSprite, "assets/sprites/logregbackyespressed.png", 960, 707);
        Texture2Sprite(reg, regSprite, "assets/sprites/reg.png", 997, 728);
        Texture2Sprite(regcont, regcontSprite, "assets/sprites/regcont.png", 997, 728);
        Texture2Sprite(log, logSprite, "assets/sprites/log.png", 997, 728);
        Texture2Sprite(logcont, logcontSprite, "assets/sprites/logcont.png", 997, 728);

        digitSprites = {
            zeroSprite, oneSprite, twoSprite, threeSprite, fourSprite, fiveSprite, sixSprite, sevenSprite, eightSprite, nineSprite
        };

        blockSprites = {
            block0Sprite, block1Sprite, block2Sprite, block3Sprite, block4Sprite, block5Sprite, block6Sprite, block7Sprite, block8Sprite, block9Sprite, block10Sprite, block11Sprite, block12Sprite, block13Sprite, block14Sprite, block15Sprite
        };

        preGameTimerTextures = {
            oneBIGwhite, twoBIGwhite, threeBIGwhite
        };
    }

    void Texture2Sprite(sf::Texture& texture, sf::Sprite& sprite, std::string str, int posx = 1921, int posy = 1081){
        texture.loadFromFile(str);
        sprite.setTexture(texture);
        sprite.setPosition(posx, posy);
    }
};

class SnakeGame {
    public:
    AudioManager& cAudioManager;
    UserInterface& cUserInterface;
    ConfigManager& cConfigManager;
    ServerClient& serverClient;
    int randX1, randY1, randX2, randY2, randX3, randY3, gameScore, tempGameScore, tempX, tempBounds, snakeInt, backgroundInt, gameOverScore, foodInt;
    bool isCLSModeStarted, isINFModeStarted, isARCModeStarted, isFoodEaten, isSnakeGrowing, isGameStarted, isNextLevel, youWon, youLose, isGameRestarted, isHoleSpawned, isPreGameTimer;
    sf::Texture food, snakeHead, snakeBodyTexture;
    sf::Sprite foodSprite, snakeHeadSprite, snakeBodySprite, snakeBackgroundSprite;
    std::deque<sf::Vector2i> snakeBody;
    sf::Vector2i mSdirection, direction, snakeHeadPos, newSnakePos, foodPos, newHeadPos, holePos1, holePos2;
    std::vector<int> digitsGS;
    sf::Clock moveSnakeClock, preGameClock;
    float elapsedTime, moveInterval, preGameElapsed, oneFloat, twoFloat, threeFloat, preGameTimerSpeed, deltaTime;
    std::array<sf::Texture,6> snakeHeadTextures, snakeBodyTextures, snakeBackgroundTextures;
    sf::IntRect playArea, tempHoleBounds1, tempHoleBounds2, tempFoodBounds, tempSnakeHeadBounds;
    std::random_device seedGen;
    std::mt19937 genX1, genY1, genX2, genY2, genX3, genY3;
    std::uniform_int_distribution<int> distX1, distY1, distX2, distY2, distX3, distY3;

    SnakeGame(UserInterface& UserInterface, AudioManager& AudioManager, ConfigManager& ConfigManager, ServerClient& serverClient) : cAudioManager{AudioManager}, cUserInterface{UserInterface}, cConfigManager{ConfigManager}, serverClient{serverClient}, gameScore{1}, snakeInt{0}, backgroundInt{5}, foodInt{0}, isCLSModeStarted{false}, isINFModeStarted{false}, isARCModeStarted{false}, isFoodEaten{false}, isSnakeGrowing{false}, isGameStarted{false}, isNextLevel{false}, youWon{false}, youLose{false}, isGameRestarted{true}, mSdirection{1, 0}, direction{1, 0}, elapsedTime{0.0f}, preGameElapsed{0.f}, oneFloat{0.f}, twoFloat{0.f}, threeFloat{0.f}, preGameTimerSpeed{1416.f}, playArea{120, 208, 1680, 760}, genX1{seedGen()}, genY1{seedGen()}, genX2{seedGen()}, genY2{seedGen()}, genX3{seedGen()}, genY3{seedGen()}, distX1{0, 41}, distY1{0, 18}, distX2{0, 35}, distY2{0, 12}, distX3{0, 35}, distY3{0, 12} {
        food.loadFromFile("assets/sprites/food.png");
        snakeHead.loadFromFile("assets/sprites/snakeHead.png");
        snakeBodyTexture.loadFromFile("assets/sprites/snakeBody.png");
        foodSprite.setTexture(food);
        spawnFood();
        snakeHeadTextures = {
            snakeHead, cUserInterface.BLUEsnakeHead, cUserInterface.PURPLEsnakeHead, cUserInterface.REDsnakeHead, cUserInterface.ORANGEsnakeHead, cUserInterface.YELLOWsnakeHead
        };
        snakeBodyTextures = {
            snakeBodyTexture, cUserInterface.BLUEsnakeBody, cUserInterface.PURPLEsnakeBody, cUserInterface.REDsnakeBody, cUserInterface.ORANGEsnakeBody, cUserInterface.YELLOWsnakeBody
        };
        snakeBackgroundTextures = {
            cUserInterface.GREENbackground, cUserInterface.BLUEbackground, cUserInterface.PURPLEbackground, cUserInterface.REDbackground, cUserInterface.ORANGEbackground, cUserInterface.YELLOWbackground
        };
        snakeHeadPos.x = 20, snakeHeadPos.y = 9;
        snakeHeadSprite.setTexture(snakeHeadTextures[snakeInt]);
        snakeBodySprite.setTexture(snakeBodyTextures[snakeInt]);
        snakeHeadSprite.setPosition((snakeHeadPos.x*40)+120, (snakeHeadPos.y*40)+208);
        snakeBody.push_back(snakeHeadPos);
        snakeBackgroundSprite.setTexture(cUserInterface.null);
        snakeBackgroundSprite.setPosition(122, 210);
    }

    bool holeCollision(){
        tempHoleBounds1 = sf::IntRect((holePos1.x * 40) + 120, (holePos1.y * 40) + 208, 240, 240);
        tempHoleBounds2 = sf::IntRect((holePos2.x * 40) + 120, (holePos2.y * 40) + 208, 240, 240);
        return  tempHoleBounds1.intersects(tempHoleBounds2) ||
                tempHoleBounds1.intersects(sf::IntRect(920, 568, 40, 40)) ||
                tempHoleBounds2.intersects(sf::IntRect(920, 568, 40, 40));
    }

    void spawnHoles(){
        if (!isHoleSpawned && isARCModeStarted){
            do {
                randX2 = distX2(genX2);
                randY2 = distY2(genY2);
                randX3 = distX3(genX3);
                randY3 = distY3(genY3);
                holePos1 = sf::Vector2i(randX2, randY2);
                holePos2 = sf::Vector2i(randX3, randY3);
            } while (holeCollision() || holePos1 == snakeBody.front() || holePos2 == snakeBody.front());
            cUserInterface.ARCholeSprite1.setPosition((holePos1.x * 40) + 120, (holePos1.y * 40) + 208);
            cUserInterface.ARCholeSprite2.setPosition((holePos2.x * 40) + 120, (holePos2.y * 40) + 208);
            isHoleSpawned = true;
        }
    }

    void gameOver(){
        gameOverScore = gameScore;
        std::cout << "Game Over! Score: " << gameOverScore << std::endl;
        if (isINFModeStarted && serverClient.isAuthorized) {
            std::cout << "Updating high score...\n";
            if (!serverClient.updateUserHighScore(gameOverScore)) std::cerr << "Failed to update score.\n";
            else std::cout << "Score updated successfully.\n";
        } else std::cerr << "Score not updated: either not in INF mode or not authorized.\n";
    }

    bool snakeBodyCollision(){
        if (std::find(snakeBody.begin() + 1, snakeBody.end(), newSnakePos) != snakeBody.end() && !isNextLevel && snakeBody.size() > 4){
            youLose = true;
            return true;
        }
        return false;
    }

    void moveSnake(){
        elapsedTime += moveSnakeClock.restart().asSeconds();
        if (elapsedTime >= moveInterval || direction != mSdirection){
            elapsedTime = 0.0f;
            mSdirection = direction;
            tempSnakeHeadBounds = sf::IntRect((snakeBody.front().x + mSdirection.x) * 40 + 120, (snakeBody.front().y + mSdirection.y) * 40 + 208, 40, 40);
            if (isARCModeStarted && !playArea.contains(sf::Vector2i(tempSnakeHeadBounds.left, tempSnakeHeadBounds.top))){
                if (mSdirection == sf::Vector2i(1, 0)) newSnakePos =        sf::Vector2i(0, snakeBody.front().y);
                else if (mSdirection == sf::Vector2i(-1, 0)) newSnakePos =  sf::Vector2i(41, snakeBody.front().y);
                else if (mSdirection == sf::Vector2i(0, 1)) newSnakePos =   sf::Vector2i(snakeBody.front().x, 0);
                else if (mSdirection == sf::Vector2i(0, -1)) newSnakePos =  sf::Vector2i(snakeBody.front().x, 18);
            } else if (isARCModeStarted && (tempSnakeHeadBounds.intersects(tempHoleBounds1) || tempSnakeHeadBounds.intersects(tempHoleBounds2))) youLose = true;
            else if (!isARCModeStarted && !playArea.contains(sf::Vector2i(tempSnakeHeadBounds.left, tempSnakeHeadBounds.top))) youLose = true;
            else {
                newSnakePos = snakeBody.front() + mSdirection;
            }
            if (!youLose){
                sf::Vector2i tempBack = snakeBody.back();
                if (!isSnakeGrowing) snakeBody.pop_back();
                else isSnakeGrowing = false;
                snakeBodyCollision();
                if (youLose && !isSnakeGrowing) snakeBody.push_back(tempBack);
            }
            if (!youLose) snakeBody.push_front(newSnakePos);
        }
    }

    void snakeGrow(){
        if (snakeBody.front() == foodPos){
            isFoodEaten = true;
            if (isARCModeStarted){
                foodInt++;
                if (foodInt > 5){
                    foodInt = 0;
                    gameScore += 5;
                    for (int i = 0; i < 4; i++) snakeBody.push_back(snakeBody.back());
                } else gameScore++;
            } else gameScore++;
            if (isINFModeStarted && gameScore % 798 == 0 && gameScore != 0) nextLevel();
            else {
                isSnakeGrowing = true;
                isNextLevel = false;
            }
            cAudioManager.playSoundFoodPop();
        }
    }

    bool foodCollision(){
        tempFoodBounds = sf::IntRect((foodPos.x * 40) + 120, (foodPos.y * 40) + 208, 40, 40);
        return  ((tempFoodBounds.intersects(tempHoleBounds1) || tempFoodBounds.intersects(tempHoleBounds2)) && isARCModeStarted) || 
                std::find(snakeBody.begin(), snakeBody.end(), foodPos) != snakeBody.end();
    }

    void spawnFood(){
        do {
            randX1 = distX1(genX1);
            randY1 = distY1(genY1);
            foodPos = sf::Vector2i(randX1, randY1);
        } while (foodCollision());
        isFoodEaten = false;
        foodSprite.setPosition(120 + (40 * foodPos.x), 208 + (40 * foodPos.y));
    }

    void nextLevel(){
        isNextLevel = true;
        snakeInt++, backgroundInt++;
        if (snakeInt > 5) snakeInt = 0;
        if (backgroundInt > 5) backgroundInt = 0;
        snakeHeadSprite.setTexture(snakeHeadTextures[snakeInt]);
        snakeBodySprite.setTexture(snakeBodyTextures[snakeInt]);
        snakeBackgroundSprite.setTexture(snakeBackgroundTextures[backgroundInt], true);
        snakeBody.clear();
        newHeadPos = {
            static_cast<int>((foodSprite.getPosition().x - 120) / 40),
            static_cast<int>((foodSprite.getPosition().y - 208) / 40)
        };
        snakeBody.push_back(newHeadPos);
    }

    void gameUpdate(bool& isGamePaused){
        if (isGameStarted && !isGamePaused && !youLose && !youWon && !isPreGameTimer){
            snakeGrow();
            moveSnake();
            if (foodInt == 5){
                foodSprite.setTexture(cUserInterface.foodextra);
            } else foodSprite.setTexture(food);
            if (isFoodEaten) spawnFood();
            if (isCLSModeStarted && gameScore == 798){
                youWon = true;
            } else if (isARCModeStarted && gameScore == 999){
                youWon = true;
            }
            mSdirectionFunc();
        } else if (youLose && !gameOverScore){
            gameOver();
        }
    }

    void mSdirectionFunc(){
        if (mSdirection == sf::Vector2i(1, 0)){
            snakeHeadSprite.setRotation(0);
            snakeHeadSprite.setPosition((snakeBody.front().x*40)+120, (snakeBody.front().y*40)+208);
        } else if (mSdirection == sf::Vector2i(-1, 0)){
            snakeHeadSprite.setRotation(-180);
            snakeHeadSprite.setPosition((snakeBody.front().x*40)+160, (snakeBody.front().y*40)+248);
        } else if (mSdirection == sf::Vector2i(0, 1)){
            snakeHeadSprite.setRotation(90);
            snakeHeadSprite.setPosition((snakeBody.front().x*40)+160, (snakeBody.front().y*40)+208);
        } else if (mSdirection == sf::Vector2i(0, -1)){
            snakeHeadSprite.setRotation(-90);
            snakeHeadSprite.setPosition((snakeBody.front().x*40)+120, (snakeBody.front().y*40)+248);
        }
    }

    void restartGame(){
        if (isGameRestarted){
            snakeBody.clear();
            snakeBody.push_back(snakeHeadPos);
            snakeHeadSprite.setPosition(920, 568);
            gameScore = 1, elapsedTime = 0.f, snakeInt = 0, backgroundInt = 0, foodInt = 0, oneFloat = 1081.f, twoFloat = 1081.f, threeFloat = 1081.f, preGameElapsed = 0.f, deltaTime = 0.f, gameOverScore = 0;
            mSdirection = sf::Vector2i(1, 0), direction = sf::Vector2i(1, 0);
            isNextLevel = false, isFoodEaten = false, isSnakeGrowing = false, youWon = false, youLose = false, isFoodEaten = true, isHoleSpawned = false,
            isGameRestarted = false;
            spawnHoles();
            spawnFood();
            mSdirectionFunc();
            preGameClock.restart();
            foodSprite.setTexture(food);
        }
    }

    void convertScoreToImage(sf::RenderWindow& window){
        tempGameScore = gameScore;
        while(tempGameScore > 0){
            digitsGS.push_back(tempGameScore % 10);
            tempGameScore /= 10;
        }
        std::reverse(digitsGS.begin(), digitsGS.end());
        for (std::size_t i = 0; i < digitsGS.size(); i++){
            cUserInterface.digitSprites[digitsGS[i]].setPosition(450 + (tempBounds + 17 * i), 112);
            window.draw(cUserInterface.digitSprites[digitsGS[i]]);
            tempBounds += cUserInterface.digitSprites[digitsGS[i]].getGlobalBounds().width;
        }
        tempBounds = 0;
        digitsGS.clear();
    }

    float easyInOut(float t){
        return 0.5f * (1 - cos(t * std::numbers::pi));
    }

    void preGameTimer(sf::RenderWindow& window){
        if (isPreGameTimer){
            if (!cUserInterface.isGamePaused){
                deltaTime = preGameClock.restart().asSeconds();
                preGameElapsed += deltaTime;
                if (preGameElapsed < 1.f){
                    threeFloat -= deltaTime * preGameTimerSpeed;
                    cUserInterface.threeBIGwhiteSprite.setPosition(840, threeFloat);
                    window.draw(cUserInterface.threeBIGwhiteSprite);
                } else if (preGameElapsed < 2.f){ 
                    twoFloat -= deltaTime * preGameTimerSpeed;
                    cUserInterface.twoBIGwhiteSprite.setPosition(840, twoFloat);
                    window.draw(cUserInterface.twoBIGwhiteSprite);
                } else if (preGameElapsed < 3.f){
                    oneFloat -= deltaTime * preGameTimerSpeed;
                    cUserInterface.oneBIGwhiteSprite.setPosition(840, oneFloat);
                    window.draw(cUserInterface.oneBIGwhiteSprite);
                } else if (preGameElapsed >= 3.f){
                    preGameElapsed = 0.f, oneFloat = 1081.f, twoFloat = 1081.f, threeFloat = 1081.f;
                    isPreGameTimer = false;
                    preGameClock.restart();
                }
            } else preGameClock.restart();   
        }
    }
};

class InputManager {
    public:
    bool isLeaderboardLoaded;
    std::vector<std::pair<std::string, int>> leaderboard;
    SnakeGame& cSnakeGame;
    AudioManager& cAudioManager;
    TextInput& textInput;
    ServerClient& serverClient;
    int setupContainItem, setupPressedItem, choseItem, soundItem, wlContainItem, wlPressedItem, logregContainItem, logregPressedItem;
    sf::Cursor handCursor, defaultCursor, textCursor;
    bool cursorSet, isMusic, isSound, wasGameUnpaused, isTextLActive, isTextRActive, isSent, logoutTriggered;
    std::string inputLText, inputRText;
    sf::Vector2i mousePos;
    sf::Vector2f mouseFloatPos;
    sf::Event fakeEvent;

    InputManager(SnakeGame& SnakeGame, AudioManager& AudioManager, TextInput& textInput, ServerClient& serverClient) : cSnakeGame{SnakeGame}, cAudioManager{AudioManager}, textInput(textInput), serverClient{serverClient}, choseItem{1}, isMusic{true}, isSound{true}, wasGameUnpaused{false}, isTextLActive{false}, isTextRActive{false}, isSent{false}, logoutTriggered{false}, isLeaderboardLoaded{false} {
        fakeEvent.type = sf::Event::MouseButtonPressed;
        fakeEvent.mouseButton.button = sf::Mouse::Right;
        handCursor.loadFromSystem(sf::Cursor::Hand);
        defaultCursor.loadFromSystem(sf::Cursor::Arrow);
        textCursor.loadFromSystem(sf::Cursor::Text);
    }

    void loadLeaderboard(){
        if (!isLeaderboardLoaded) {
            leaderboard = serverClient.fetchLeaderboard();
            isLeaderboardLoaded = true;
        }
    }

    void pressedKeys(sf::RenderWindow& window, sf::Event& event, UserInterface& cUserInterface){
        cursorSet = false;
        cUserInterface.pressedItem = 0, cUserInterface.containItem = 0, cUserInterface.inGameContain = 0, cUserInterface.inGamePressed = 0, setupContainItem = 0, setupPressedItem = 0, wlContainItem = 0, wlPressedItem = 0, logregContainItem = 0, logregPressedItem = 0;
        mousePos = sf::Mouse::getPosition(window);
        mouseFloatPos = sf::Vector2f(mousePos.x, mousePos.y);
        if (cUserInterface.releasedItem == 0){
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                cUserInterface.pressedItem = 4;
                cUserInterface.containItem = 4;
            } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                cAudioManager.playSoundUIClick();
                cUserInterface.releasedItem = 4;
            } else if (cUserInterface.textBACKSprite1.getGlobalBounds().contains(mouseFloatPos)){
                processMouseInput(event, window, &cUserInterface.containItem, 1, &cUserInterface.pressedItem, 1, &cUserInterface.releasedItem, 1, nullptr); //STARTmouse
            } else if (cUserInterface.textBACKSprite2.getGlobalBounds().contains(mouseFloatPos)){
                processMouseInput(event, window, &cUserInterface.containItem, 2, &cUserInterface.pressedItem, 2, &cUserInterface.releasedItem, 2, nullptr); //SETUPmouse
            } else if (cUserInterface.textBACKSprite3.getGlobalBounds().contains(mouseFloatPos)){
                if (processMouseInput(event, window, &cUserInterface.containItem, 3, &cUserInterface.pressedItem, 3, &cUserInterface.releasedItem, 3, nullptr)) {
                    loadLeaderboard();
                } //GOALSmouse
            } else if (cUserInterface.textBACKSprite4.getGlobalBounds().contains(mouseFloatPos)){
                processMouseInput(event, window, &cUserInterface.containItem, 4, &cUserInterface.pressedItem, 4, &cUserInterface.releasedItem, 4, nullptr); //QUITmouse
            } else if (cUserInterface.loginbackSprite.getGlobalBounds().contains(mouseFloatPos)){
                window.setMouseCursor(handCursor);
                cursorSet = true;
                cUserInterface.containItem = 6;
                if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
                    cUserInterface.pressedItem = 6;
                } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                    cAudioManager.playSoundUIClick();
                    cursorSet = false;
                    if (serverClient.isAuthorized) {
                        serverClient.isAuthorized = false;
                        serverClient.token.clear();
                        event = fakeEvent;
                        logoutTriggered = true;
                    } else if (!logoutTriggered) cUserInterface.releasedItem = 6;
                }
            }
        } else if (cUserInterface.releasedItem == 1){ //SELECT MODE
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                cUserInterface.containItem = 10;
                cUserInterface.pressedItem = 10;
            } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                cAudioManager.playSoundUIClick();
                cUserInterface.releasedItem = 0;
            } else if (cUserInterface.selectsmodesBACK1Sprite.getGlobalBounds().contains(mouseFloatPos)){
                if (processMouseInput(event, window, &cUserInterface.containItem, 7, &cUserInterface.pressedItem, 7, &cUserInterface.releasedItem, 5, nullptr)){
                    cSnakeGame.isCLSModeStarted = true, cSnakeGame.isGameRestarted = true, cSnakeGame.isPreGameTimer = true;
                    cSnakeGame.restartGame();
                }
            } else if (cUserInterface.selectsmodesBACK2Sprite.getGlobalBounds().contains(mouseFloatPos)){
                if (processMouseInput(event, window, &cUserInterface.containItem, 8, &cUserInterface.pressedItem, 8, &cUserInterface.releasedItem, 5, nullptr)){
                    cSnakeGame.isINFModeStarted = true, cSnakeGame.isGameRestarted = true, cSnakeGame.isPreGameTimer = true;
                    cSnakeGame.restartGame();
                }
            } else if (cUserInterface.selectsmodesBACK3Sprite.getGlobalBounds().contains(mouseFloatPos)){
                if (processMouseInput(event, window, &cUserInterface.containItem, 9, &cUserInterface.pressedItem, 9, &cUserInterface.releasedItem, 5, nullptr)){
                    cSnakeGame.isARCModeStarted = true, cSnakeGame.isGameRestarted = true, cSnakeGame.isPreGameTimer = true;
                    cSnakeGame.restartGame();
                }
            } else if (cUserInterface.selectmodeESCBACKSprite.getGlobalBounds().contains(mouseFloatPos)){
                processMouseInput(event, window, &cUserInterface.containItem, 10, &cUserInterface.pressedItem, 10, &cUserInterface.releasedItem, 0, nullptr);
            } else if (!cUserInterface.selectmodeBACKSprite.getGlobalBounds().contains(mouseFloatPos)){
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) cUserInterface.releasedItem = 0;
            }
        } else if (cUserInterface.releasedItem == 2){ //SETUP
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                setupContainItem = 1;
                setupPressedItem = 1;
            } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                cAudioManager.playSoundUIClick();
                cUserInterface.releasedItem = 0;
            } else if (cUserInterface.setupTextBACKSprite.getGlobalBounds().contains(mouseFloatPos)){
                processMouseInput(event, window, &setupContainItem, 1, &setupPressedItem, 1, &cUserInterface.releasedItem, 0, nullptr);
            } else if (cUserInterface.speedBACK0Sprite.getGlobalBounds().contains(mouseFloatPos)){
                if (processMouseInput(event, window, &setupContainItem, 2, &setupPressedItem, 2, &choseItem, 1, nullptr)) cSnakeGame.moveInterval = 0.35;
            } else if (cUserInterface.speedBACK1Sprite.getGlobalBounds().contains(mouseFloatPos)){
                if (processMouseInput(event, window, &setupContainItem, 3, &setupPressedItem, 3, &choseItem, 2, nullptr)) cSnakeGame.moveInterval = 0.24;
            } else if (cUserInterface.speedBACK2Sprite.getGlobalBounds().contains(mouseFloatPos)){
                if (processMouseInput(event, window, &setupContainItem, 4, &setupPressedItem, 4, &choseItem, 3, nullptr)) cSnakeGame.moveInterval = 0.13;
            } else if (cUserInterface.musicONSprite.getGlobalBounds().contains(mouseFloatPos)){
                window.setMouseCursor(handCursor);
                cursorSet = true;
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left){
                    cAudioManager.playSoundUIClick();
                    isMusic = !isMusic;
                }
            } else if (cUserInterface.soundOFFSprite.getGlobalBounds().contains(mouseFloatPos)){
                window.setMouseCursor(handCursor);
                cursorSet = true;
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left){
                    if (!isSound) cAudioManager.playSoundUIClick();
                    isSound = !isSound;
                }
            }
        } else if (cUserInterface.releasedItem == 3){ //GOALS
            if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                cAudioManager.playSoundUIClick();
                cUserInterface.releasedItem = 0;
                isLeaderboardLoaded = false;
            }
        } else if (cUserInterface.releasedItem == 4){ //QUIT
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                cUserInterface.containItem = 6;
                cUserInterface.pressedItem = 6;
            } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                cAudioManager.playSoundUIClick();
                cUserInterface.releasedItem = 0;
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Enter)){
                cUserInterface.containItem = 5;
                cUserInterface.pressedItem = 5;
            } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Enter){
                closeWindow(window);
            } else if (cUserInterface.yesbackSprite.getGlobalBounds().contains(mouseFloatPos)){
                processMouseInput(event, window, &cUserInterface.containItem, 5, &cUserInterface.pressedItem, 5, nullptr, 0, closeWindow);
            } else if (cUserInterface.nobackSprite.getGlobalBounds().contains(mouseFloatPos)){
                processMouseInput(event, window, &cUserInterface.containItem, 6, &cUserInterface.pressedItem, 6, &cUserInterface.releasedItem, 0, nullptr);
            } else if (!cUserInterface.backgroundAYSSprite.getGlobalBounds().contains(mouseFloatPos)){
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) cUserInterface.releasedItem = 0;
            }
        } else if (cUserInterface.releasedItem == 5){
            cSnakeGame.isFoodEaten = false;
            cSnakeGame.isGameStarted = true;
            if (cUserInterface.inGameReleased == 0 && !cSnakeGame.youWon && !cSnakeGame.youLose){
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape) && !wasGameUnpaused){
                    cUserInterface.inGameContain = 1;
                    cUserInterface.inGamePressed = 1;
                } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape && !wasGameUnpaused){
                    cAudioManager.playSoundUIClick();
                    cUserInterface.isGamePaused = true;
                    cUserInterface.inGameReleased = 1;
                }
                if (cUserInterface.inGameSettingsBACKSprite.getGlobalBounds().contains(mouseFloatPos) && !wasGameUnpaused){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    cUserInterface.inGameContain = 1;
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
                        cUserInterface.inGamePressed = 1;
                    } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        cursorSet = false;
                        cUserInterface.isGamePaused = true;
                        cUserInterface.inGameReleased = 1;
                    }
                }
            } else if (cUserInterface.inGameReleased == 1){
                if (cUserInterface.textBACKSprite1.getGlobalBounds().contains(mouseFloatPos)){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    cUserInterface.inGameContain = 2;
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
                        cUserInterface.inGamePressed = 2;
                    } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        cUserInterface.inGamePressed = 0;
                        cUserInterface.inGameContain = 0;
                        cAudioManager.playSoundUIClick();
                        cursorSet = false;
                        wasGameUnpaused = true;
                        cUserInterface.isGamePaused = false;
                        cUserInterface.inGameReleased = 0;
                    }
                } else if (cUserInterface.textBACKSprite2.getGlobalBounds().contains(mouseFloatPos)){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    cUserInterface.inGameContain = 3;
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
                        cUserInterface.inGamePressed = 3;
                    } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        cAudioManager.playSoundUIClick();
                        cursorSet = false;
                        cUserInterface.inGameReleased = 2;
                    }
                } else if (cUserInterface.textBACKSprite3.getGlobalBounds().contains(mouseFloatPos)){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    cUserInterface.inGameContain = 4;
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
                        cUserInterface.inGamePressed = 4;
                    } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        cAudioManager.playSoundUIClick();
                        cSnakeGame.isGameStarted = false;
                        cUserInterface.isGamePaused = false;
                        cUserInterface.releasedItem = 0;
                        cUserInterface.inGameReleased = 0;
                        cursorSet = false;
                    }
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                    cUserInterface.inGameContain = 2;
                    cUserInterface.inGamePressed = 2;
                } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                    cUserInterface.inGamePressed = 0;
                    cUserInterface.inGameContain = 0;
                    cAudioManager.playSoundUIClick();
                    cUserInterface.isGamePaused = false;
                    cUserInterface.inGameReleased = 0;
                    wasGameUnpaused = true;
                }
            } else if(cUserInterface.inGameReleased == 2){
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                    setupContainItem = 1;
                    setupPressedItem = 1;
                } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                    cAudioManager.playSoundUIClick();
                    cUserInterface.inGameReleased = 1;
                } else if (cUserInterface.setupTextBACKSprite.getGlobalBounds().contains(mouseFloatPos)){
                    processMouseInput(event, window, &setupContainItem, 1, &setupPressedItem, 1, &cUserInterface.inGameReleased, 1, nullptr);
                } else if (cUserInterface.speedBACK0Sprite.getGlobalBounds().contains(mouseFloatPos)){
                    if (processMouseInput(event, window, &setupContainItem, 2, &setupPressedItem, 2, &choseItem, 1, nullptr)) cSnakeGame.moveInterval = 0.35;
                } else if (cUserInterface.speedBACK1Sprite.getGlobalBounds().contains(mouseFloatPos)){
                    if (processMouseInput(event, window, &setupContainItem, 3, &setupPressedItem, 3, &choseItem, 2, nullptr)) cSnakeGame.moveInterval = 0.24;
                } else if (cUserInterface.speedBACK2Sprite.getGlobalBounds().contains(mouseFloatPos)){
                    if (processMouseInput(event, window, &setupContainItem, 4, &setupPressedItem, 4, &choseItem, 3, nullptr)) cSnakeGame.moveInterval = 0.13;
                } else if (cUserInterface.musicONSprite.getGlobalBounds().contains(mouseFloatPos)){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left){
                        cAudioManager.playSoundUIClick();
                        isMusic = !isMusic;
                    }
                } else if (cUserInterface.soundOFFSprite.getGlobalBounds().contains(mouseFloatPos)){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left){
                        if (!isSound) cAudioManager.playSoundUIClick();
                        isSound = !isSound;
                    }
                }
            } else if (cSnakeGame.youWon || cSnakeGame.youLose){
                if (cUserInterface.textBACKSprite2.getGlobalBounds().contains(mouseFloatPos)){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    wlContainItem = 1;
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
                        wlPressedItem = 1;
                    } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        cAudioManager.playSoundUIClick();
                        cursorSet = false, cSnakeGame.isGameRestarted = true, cSnakeGame.isPreGameTimer = true;
                        cSnakeGame.restartGame();
                    }
                } else if (cUserInterface.textBACKSprite3.getGlobalBounds().contains(mouseFloatPos)){
                    window.setMouseCursor(handCursor);
                    cursorSet = true;
                    wlContainItem = 2;
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
                        wlPressedItem = 2;
                    } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        cAudioManager.playSoundUIClick();
                        cursorSet = false, cSnakeGame.isGameStarted = false, cSnakeGame.isCLSModeStarted = false, cSnakeGame.isINFModeStarted = false, cSnakeGame.isARCModeStarted = false, cSnakeGame.isGameRestarted = true;
                        cUserInterface.releasedItem = 0;
                        cSnakeGame.restartGame();
                    }
                }
            }
        } else if (cUserInterface.releasedItem == 6){
            if (cUserInterface.logregReleasedItem == 0){
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                    logregContainItem = 3, logregPressedItem = 3;
                } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                    cAudioManager.playSoundUIClick();
                    cUserInterface.releasedItem = 0;
                }
                if (cUserInterface.textloginBACKSprite.getGlobalBounds().contains(mouseFloatPos)){
                    processMouseInput(event, window, &logregContainItem, 1, &logregPressedItem, 1, &cUserInterface.logregReleasedItem, 1);
                } else if (cUserInterface.textsigninSprite.getGlobalBounds().contains(mouseFloatPos)){
                    processMouseInput(event, window, &logregContainItem, 2, &logregPressedItem, 2, &cUserInterface.logregReleasedItem, 2);
                } else if (cUserInterface.logregESCbackSprite.getGlobalBounds().contains(mouseFloatPos)){
                    processMouseInput(event, window, &logregContainItem, 3, &logregPressedItem, 3, &cUserInterface.releasedItem, 0);
                }
            } else if (cUserInterface.logregReleasedItem == 1) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
                    
                } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                    cAudioManager.playSoundUIClick();
                    cUserInterface.logregReleasedItem = 0;
                }
                if (cUserInterface.logregbacknoSprite.getGlobalBounds().contains(mouseFloatPos)){
                    if (processMouseInput(event, window, &logregContainItem, 4, &logregPressedItem, 4, &cUserInterface.logregReleasedItem, 0)) {
                        inputLText.clear();
                        inputRText.clear();
                    }
                } else if (cUserInterface.logregbackyesSprite.getGlobalBounds().contains(mouseFloatPos)){
                    if (processMouseInput(event, window, &logregContainItem, 5, &logregPressedItem, 5, &cUserInterface.releasedItem, 0)) {
                        serverClient.loginUser(inputLText, inputRText);
                        inputLText.clear();
                        inputRText.clear();
                        cUserInterface.logregReleasedItem = 0;
                    }
                } else if (cUserInterface.textBACKSprite2.getGlobalBounds().contains(mouseFloatPos)){
                    cursorSet = true;
                    window.setMouseCursor(textCursor);
                    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        isTextLActive = true;
                        isTextRActive = false;
                    }
                } else if(cUserInterface.textBACKSprite3.getGlobalBounds().contains(mouseFloatPos)){
                    cursorSet = true;
                    window.setMouseCursor(textCursor);
                    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        isTextRActive = true;
                        isTextLActive = false;
                    }
                } else if (!cUserInterface.textBACKSprite2.getGlobalBounds().contains(mouseFloatPos) && !cUserInterface.textBACKSprite3.getGlobalBounds().contains(mouseFloatPos) && event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                    isTextLActive = false, isTextRActive = false;
                } else {
                    cursorSet = false;
                }
                if (isTextLActive) {
                    username_password(event, inputLText);
                    textInput.text2Login(inputLText);
                } else if (isTextRActive) {
                    username_password(event, inputRText);
                    textInput.text2Pass(inputRText);
                }
            } else if (cUserInterface.logregReleasedItem == 2) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)){
        
                } else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape){
                    cAudioManager.playSoundUIClick();
                    cUserInterface.logregReleasedItem = 0;
                }
                if (cUserInterface.logregbacknoSprite.getGlobalBounds().contains(mouseFloatPos)){
                    if (processMouseInput(event, window, &logregContainItem, 4, &logregPressedItem, 4, &cUserInterface.logregReleasedItem, 0)) {
                        inputLText.clear();
                        inputRText.clear();
                        textInput.text2Pass(inputRText);
                        textInput.text2Pass(inputLText);
                        isTextLActive = false, isTextRActive = false;
                    }
                } else if (cUserInterface.logregbackyesSprite.getGlobalBounds().contains(mouseFloatPos)){
                    if (processMouseInput(event, window, &logregContainItem, 6, &logregPressedItem, 6, &cUserInterface.releasedItem, 0)) {
                        serverClient.registerUser(inputLText, inputRText);
                        inputLText.clear();
                        inputRText.clear();
                        textInput.text2Pass(inputRText);
                        textInput.text2Pass(inputLText);
                        isTextLActive = false, isTextRActive = false;
                        cUserInterface.logregReleasedItem = 0;
                    }
                } else if (cUserInterface.textBACKSprite2.getGlobalBounds().contains(mouseFloatPos)){
                    cursorSet = true;
                    window.setMouseCursor(textCursor);
                    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        isTextLActive = true;
                        isTextRActive = false;
                    }
                } else if(cUserInterface.textBACKSprite3.getGlobalBounds().contains(mouseFloatPos)){
                    cursorSet = true;
                    window.setMouseCursor(textCursor);
                    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                        isTextRActive = true;
                        isTextLActive = false;
                    }
                } else if (!cUserInterface.textBACKSprite2.getGlobalBounds().contains(mouseFloatPos) && !cUserInterface.textBACKSprite3.getGlobalBounds().contains(mouseFloatPos) && event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
                    isTextLActive = false, isTextRActive = false;
                } else {
                    cursorSet = false;
                }
                if (isTextLActive) {
                    username_password(event, inputLText);
                    textInput.text2Login(inputLText);
                } else if (isTextRActive) {
                    username_password(event, inputRText);
                    textInput.text2Pass(inputRText);
                }
            }
        }
        if (event.type == sf::Event::MouseMoved || event.type == sf::Event::KeyPressed) logoutTriggered = false;
        if (!cursorSet) window.setMouseCursor(defaultCursor);
        if (cSnakeGame.isGameStarted && !cUserInterface.isGamePaused){
            if (event.type == sf::Event::KeyPressed){
                if (event.key.code == sf::Keyboard::W && cSnakeGame.mSdirection != sf::Vector2i(0, 1)) cSnakeGame.direction = sf::Vector2i(0, -1);
                else if (event.key.code == sf::Keyboard::S && cSnakeGame.mSdirection != sf::Vector2i(0, -1)) cSnakeGame.direction = sf::Vector2i(0, 1);
                else if (event.key.code == sf::Keyboard::A && cSnakeGame.mSdirection != sf::Vector2i(1, 0)) cSnakeGame.direction = sf::Vector2i(-1, 0);
                else if (event.key.code == sf::Keyboard::D && cSnakeGame.mSdirection != sf::Vector2i(-1, 0)) cSnakeGame.direction = sf::Vector2i(1, 0);
            }
        }
    }
    
    bool processMouseInput(sf::Event& event, sf::RenderWindow& window, int* var1 = nullptr, int int1 = 0, int* var2 = nullptr, int int2 = 0, int* var3 = nullptr, int int3 = 0, void (*closeWindow)(sf::RenderWindow&) = nullptr){
        window.setMouseCursor(handCursor);
        cursorSet = true;
        if (var1 != nullptr) *var1 = int1;
        if (sf::Mouse::isButtonPressed(sf::Mouse::Left)){
            if (var2 != nullptr) *var2 = int2;
        } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left){
            cAudioManager.playSoundUIClick();
            cursorSet = false;
            if (var3 != nullptr) *var3 = int3;
            if (closeWindow != nullptr) closeWindow(window);
            return true;
        }
        return false;
    }

    void pollEventFunc(sf::RenderWindow& window, sf::Event& event, UserInterface& cUserInterface){
        while (window.pollEvent(event)){
            if (event.type == sf::Event::Closed){
                window.close();
            }
            pressedKeys(window, event, cUserInterface);
        }
    }

    void username_password(sf::Event& event, std::string& inputText){
        if ((isTextLActive || isTextRActive) && event.type == sf::Event::TextEntered) {
            if (event.text.unicode == 8 && !inputText.empty()) {
                inputText.pop_back();
            } else if (event.text.unicode >= 32 && event.text.unicode <= 126) {
                inputText += static_cast<char>(event.text.unicode);
            }
        }
    }
};

class Draw {
    private:
    public:
    ServerClient& serverClient;
    UserInterface& cUserInterface;
    SnakeGame& cSnakeGame;
    InputManager& cInputManager;
    AudioManager& cAudioManager;
    ConfigManager& cConfigManager;
    TextInput& textInput;
    int soundSlider, musicSlider, setupMousePosX, soundSliderInt, musicSliderInt;
    float soundVolumeF, musicVolumeF, elapsedTime1;
    bool isMusicSlider, isSoundSlider;
    sf::Sprite snakeTempBodySprite;
    sf::Clock unpauseClock;
    sf::Font& font;

    Draw(ServerClient& serverClient, UserInterface& UserInterface, SnakeGame& SnakeGame, InputManager& InputManager, AudioManager& AudioManager, ConfigManager& ConfigManager, TextInput& textInput, sf::Font& font) : serverClient{serverClient},cUserInterface(UserInterface), cSnakeGame(SnakeGame), cInputManager(InputManager), cAudioManager(AudioManager), cConfigManager{ConfigManager}, textInput{textInput}, soundSlider{1082}, musicSlider{1082}, elapsedTime1{0.f}, isMusicSlider{false}, isSoundSlider{false}, font{font} {
        cUserInterface.textBACKSFXMSC0pressedSprite.setTextureRect(sf::IntRect(0, 0, musicSliderInt, 105));
        cUserInterface.textBACKSFXMSC1pressedSprite.setTextureRect(sf::IntRect(0, 0, soundSliderInt, 105));
    }

    void windowDraw(sf::RenderWindow& window, sf::Event& event){
        window.draw(cUserInterface.backgroundSprite);
        if ((!cSnakeGame.isGameStarted || cSnakeGame.youLose || cSnakeGame.youWon) && cInputManager.wasGameUnpaused) {
            elapsedTime1 = 0.f;
            cInputManager.wasGameUnpaused = false;
            unpauseClock.restart();
        }
        if (cUserInterface.releasedItem == 0){
            window.draw(cUserInterface.backgroundmSprite);
            if (cUserInterface.pressedItem == 1){
                window.draw(cUserInterface.textBACKpressedSprite1);window.draw(cUserInterface.textBACKSprite2);window.draw(cUserInterface.textBACKSprite3);window.draw(cUserInterface.textBACKSprite4);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutbackSprite);
                else window.draw(cUserInterface.loginbackSprite);
            } else if (cUserInterface.pressedItem == 2){
                window.draw(cUserInterface.textBACKpressedSprite2);window.draw(cUserInterface.textBACKSprite1);window.draw(cUserInterface.textBACKSprite3);window.draw(cUserInterface.textBACKSprite4);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutbackSprite);
                else window.draw(cUserInterface.loginbackSprite);
            } else if (cUserInterface.pressedItem == 3){
                window.draw(cUserInterface.textBACKpressedSprite3);window.draw(cUserInterface.textBACKSprite1);window.draw(cUserInterface.textBACKSprite2);window.draw(cUserInterface.textBACKSprite4);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutbackSprite);
                else window.draw(cUserInterface.loginbackSprite);
            } else if (cUserInterface.pressedItem == 4){
                window.draw(cUserInterface.textBACKpressedSprite4);window.draw(cUserInterface.textBACKSprite1);window.draw(cUserInterface.textBACKSprite2);window.draw(cUserInterface.textBACKSprite3);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutbackSprite);
                else window.draw(cUserInterface.loginbackSprite);
            } else if (cUserInterface.pressedItem == 6){
                window.draw(cUserInterface.textBACKSprite1);window.draw(cUserInterface.textBACKSprite2);window.draw(cUserInterface.textBACKSprite3);window.draw(cUserInterface.textBACKSprite4);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutbackpressedSprite);
                else window.draw(cUserInterface.loginbackpressedSprite);
            } else {
                window.draw(cUserInterface.textBACKSprite1);
                window.draw(cUserInterface.textBACKSprite2);
                window.draw(cUserInterface.textBACKSprite3);
                window.draw(cUserInterface.textBACKSprite4);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutbackSprite);
                else window.draw(cUserInterface.loginbackSprite);
                
            }
            if (cUserInterface.containItem == 1){
                window.draw(cUserInterface.textstartcontainSprite);window.draw(cUserInterface.textsetupSprite);window.draw(cUserInterface.textgoalsSprite);window.draw(cUserInterface.textquitSprite1);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutfrontSprite);
                else window.draw(cUserInterface.loginfrontSprite);
            } else if (cUserInterface.containItem == 2){
                window.draw(cUserInterface.textsetupcontainSprite);window.draw(cUserInterface.textstartSprite);window.draw(cUserInterface.textgoalsSprite);window.draw(cUserInterface.textquitSprite1);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutfrontSprite);
                else window.draw(cUserInterface.loginfrontSprite);
            } else if (cUserInterface.containItem == 3){
                window.draw(cUserInterface.textgoalscontainSprite);window.draw(cUserInterface.textsetupSprite);window.draw(cUserInterface.textstartSprite);window.draw(cUserInterface.textquitSprite1);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutfrontSprite);
                else window.draw(cUserInterface.loginfrontSprite);
            } else if (cUserInterface.containItem == 4){
                window.draw(cUserInterface.textquitcontainSprite1);window.draw(cUserInterface.textstartSprite);window.draw(cUserInterface.textsetupSprite);window.draw(cUserInterface.textgoalsSprite);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutfrontSprite);
                else window.draw(cUserInterface.loginfrontSprite);
            } else if (cUserInterface.containItem == 6){
                window.draw(cUserInterface.textstartSprite);window.draw(cUserInterface.textsetupSprite);window.draw(cUserInterface.textgoalsSprite);window.draw(cUserInterface.textquitSprite1);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutfrontcontSprite);
                else window.draw(cUserInterface.loginfrontcontSprite);
            } else {
                window.draw(cUserInterface.textstartSprite);
                window.draw(cUserInterface.textsetupSprite);
                window.draw(cUserInterface.textgoalsSprite);
                window.draw(cUserInterface.textquitSprite1);
                if (serverClient.isAuthorized) window.draw(cUserInterface.logoutfrontSprite);
                else window.draw(cUserInterface.loginfrontSprite);
            }
        } else if (cUserInterface.releasedItem == 1){
            window.draw(cUserInterface.backgroundmSprite);
            window.draw(cUserInterface.textBACKpressedSprite1);
            window.draw(cUserInterface.textstartcontainSprite);
            window.draw(cUserInterface.textBACKSprite4);
            window.draw(cUserInterface.textquitSprite1);
            window.draw(cUserInterface.selectmodeBACKSprite);
            window.draw(cUserInterface.textselectmodeSprite);
            if (cUserInterface.pressedItem == 7){
                window.draw(cUserInterface.selectsmodesBACK1pressedSprite);window.draw(cUserInterface.selectsmodesBACK2Sprite);window.draw(cUserInterface.selectsmodesBACK3Sprite);window.draw(cUserInterface.selectmodeESCBACKSprite);
            } else if (cUserInterface.pressedItem == 8){
                window.draw(cUserInterface.selectsmodesBACK2pressedSprite);window.draw(cUserInterface.selectsmodesBACK1Sprite);window.draw(cUserInterface.selectsmodesBACK3Sprite);window.draw(cUserInterface.selectmodeESCBACKSprite);
            } else if (cUserInterface.pressedItem == 9){
                window.draw(cUserInterface.selectsmodesBACK3pressedSprite);window.draw(cUserInterface.selectsmodesBACK1Sprite);window.draw(cUserInterface.selectsmodesBACK2Sprite);window.draw(cUserInterface.selectmodeESCBACKSprite);
            } else if (cUserInterface.pressedItem == 10){
                window.draw(cUserInterface.selectsmodesBACK3Sprite);window.draw(cUserInterface.selectsmodesBACK1Sprite);window.draw(cUserInterface.selectsmodesBACK2Sprite);window.draw(cUserInterface.selectmodeESCBACKpressedSprite);
            } else {
                window.draw(cUserInterface.selectsmodesBACK1Sprite);window.draw(cUserInterface.selectsmodesBACK2Sprite);window.draw(cUserInterface.selectsmodesBACK3Sprite);window.draw(cUserInterface.selectmodeESCBACKSprite);
            }
            if (cUserInterface.containItem == 7){
                window.draw(cUserInterface.CLSwhiteSprite);window.draw(cUserInterface.INFSprite);window.draw(cUserInterface.ARCSprite);window.draw(cUserInterface.selectmodeESCSprite);
            } else if (cUserInterface.containItem == 8){
                window.draw(cUserInterface.CLSSprite);window.draw(cUserInterface.INFwhiteSprite);window.draw(cUserInterface.ARCSprite);window.draw(cUserInterface.selectmodeESCSprite);
            } else if (cUserInterface.containItem == 9){
                window.draw(cUserInterface.CLSSprite);window.draw(cUserInterface.INFSprite);window.draw(cUserInterface.ARCwhiteSprite);window.draw(cUserInterface.selectmodeESCSprite);
            } else if (cUserInterface.containItem == 10){
                window.draw(cUserInterface.CLSSprite);window.draw(cUserInterface.INFSprite);window.draw(cUserInterface.ARCSprite);window.draw(cUserInterface.selectmodeESCcontSprite);
            } else {
                window.draw(cUserInterface.CLSSprite);window.draw(cUserInterface.INFSprite);window.draw(cUserInterface.ARCSprite);window.draw(cUserInterface.selectmodeESCSprite);
            }
        } else if (cUserInterface.releasedItem == 2){
            setup(window, event);
        } else if (cUserInterface.releasedItem == 3){
            drawLeaderboard(window, font, cInputManager.leaderboard);
        } else if (cUserInterface.releasedItem == 4){
            window.draw(cUserInterface.backgroundmSprite);
            window.draw(cUserInterface.textBACKSprite1);
            window.draw(cUserInterface.textBACKpressedSprite4);
            window.draw(cUserInterface.textquitcontainSprite1);
            window.draw(cUserInterface.textstartSprite);
            if (serverClient.isAuthorized) {
                window.draw(cUserInterface.logoutbackSprite);
                window.draw(cUserInterface.logoutfrontSprite);
            } else {
                window.draw(cUserInterface.loginbackSprite);
                window.draw(cUserInterface.loginfrontSprite);
            }
            window.draw(cUserInterface.backgroundAYSSprite);
            window.draw(cUserInterface.exitSprite);
            if (cUserInterface.pressedItem == 5){
                window.draw(cUserInterface.yesbackdarkSprite);window.draw(cUserInterface.nobackSprite);
            } else if (cUserInterface.pressedItem == 6){
                window.draw(cUserInterface.nobackdarkSprite);window.draw(cUserInterface.yesbackSprite);
            } else {
                window.draw(cUserInterface.yesbackSprite);window.draw(cUserInterface.nobackSprite);
            }
            if (cUserInterface.containItem == 5){
                window.draw(cUserInterface.yeswhiteSprite);window.draw(cUserInterface.noredSprite1);
            } else if (cUserInterface.containItem == 6){
                window.draw(cUserInterface.nowhiteSprite1);window.draw(cUserInterface.yesgreenSprite);
            } else {
                window.draw(cUserInterface.yesgreenSprite);
                window.draw(cUserInterface.noredSprite1);
            }
        } else if (cUserInterface.releasedItem == 5){
            window.draw(cUserInterface.snakebackSprite);
            window.draw(cSnakeGame.snakeBackgroundSprite);
            if (cSnakeGame.isARCModeStarted){
                window.draw(cUserInterface.ARCholeSprite1);
                window.draw(cUserInterface.ARCholeSprite2);
            }
            window.draw(cSnakeGame.foodSprite);
            for (std::size_t i = 1; i < size(cSnakeGame.snakeBody); i++){
                snakeTempBodySprite = cSnakeGame.snakeBodySprite;
                snakeTempBodySprite.setPosition((cSnakeGame.snakeBody[i].x*40)+120, (cSnakeGame.snakeBody[i].y*40)+208);
                window.draw(snakeTempBodySprite);
            }
            window.draw(cSnakeGame.snakeHeadSprite);
            window.draw(cUserInterface.scoreSprite);
            cSnakeGame.preGameTimer(window);
            cSnakeGame.convertScoreToImage(window);
            if (cInputManager.wasGameUnpaused){
                elapsedTime1 += unpauseClock.restart().asSeconds();
                if (elapsedTime1 >= 16.f){
                    elapsedTime1 = 0.f;
                    cInputManager.wasGameUnpaused = false;
                }
            } else unpauseClock.restart();
            if (cUserInterface.inGameReleased == 0){
                if (cUserInterface.inGamePressed == 0){
                    window.draw(cUserInterface.inGameSettingsBACKSprite);
                } else if (cUserInterface.inGamePressed == 1){
                    window.draw(cUserInterface.inGameSettingsBACKpressedSprite);
                }
                if (cUserInterface.inGameContain == 0){
                    window.draw(cUserInterface.inGameSettingsFRONTSprite);
                } else if(cUserInterface.inGameContain == 1){
                    window.draw(cUserInterface.inGameSettingsFRONTcontSprite);
                }
                if (cInputManager.wasGameUnpaused){
                    window.draw(cUserInterface.lockedSprite);
                    window.draw(cUserInterface.blockSprites[static_cast<int>(elapsedTime1)]);
                }
            } else if (cUserInterface.isGamePaused){
                window.draw(cUserInterface.inGameSettingsBACKpressedSprite);
                window.draw(cUserInterface.inGameSettingsFRONTcontSprite);
                if (cUserInterface.inGameReleased == 1){
                    window.draw(cUserInterface.backgroundgSprite);
                    if (cUserInterface.inGamePressed == 2){
                        window.draw(cUserInterface.textBACKpressedSprite1);window.draw(cUserInterface.textBACKSprite2);window.draw(cUserInterface.textBACKSprite3);
                    } else if(cUserInterface.inGamePressed == 3){
                        window.draw(cUserInterface.textBACKpressedSprite2);window.draw(cUserInterface.textBACKSprite1);window.draw(cUserInterface.textBACKSprite3);
                    } else if(cUserInterface.inGamePressed == 4){
                        window.draw(cUserInterface.textBACKpressedSprite3);window.draw(cUserInterface.textBACKSprite1);window.draw(cUserInterface.textBACKSprite2);
                    } else {
                        window.draw(cUserInterface.textBACKSprite1);window.draw(cUserInterface.textBACKSprite2);window.draw(cUserInterface.textBACKSprite3);
                    }

                    if (cUserInterface.inGameContain == 2){
                        window.draw(cUserInterface.resumeContSprite);window.draw(cUserInterface.textsetupSprite);window.draw(cUserInterface.textquitSprite2);
                    } else if(cUserInterface.inGameContain == 3){
                        window.draw(cUserInterface.resumeSprite);window.draw(cUserInterface.textsetupcontainSprite);window.draw(cUserInterface.textquitSprite2);
                    } else if(cUserInterface.inGameContain == 4){
                        window.draw(cUserInterface.resumeSprite);window.draw(cUserInterface.textsetupSprite);window.draw(cUserInterface.textquitcontainSprite2);
                    } else {
                        window.draw(cUserInterface.resumeSprite);window.draw(cUserInterface.textsetupSprite);window.draw(cUserInterface.textquitSprite2);
                    }
                } else if (cUserInterface.inGameReleased == 2){
                    setup(window, event);
                }
            }
            if (cSnakeGame.youLose){
                window.draw(cUserInterface.backgroundgSprite);
                window.draw(cUserInterface.wastedSprite);
                wlDraw(window);
            } else if (cSnakeGame.youWon){
                window.draw(cUserInterface.backgroundgSprite);
                window.draw(cUserInterface.youwonSprite);
                wlDraw(window);
            }
        } else if (cUserInterface.releasedItem == 6){
            if (cUserInterface.logregReleasedItem == 0){
                window.draw(cUserInterface.logregSprite);
                if (cInputManager.logregPressedItem == 1){
                    window.draw(cUserInterface.textloginBACKpressedSprite);window.draw(cUserInterface.textBACKSprite3);window.draw(cUserInterface.logregESCbackSprite);
                } else if (cInputManager.logregPressedItem == 2){
                    window.draw(cUserInterface.textloginBACKSprite);window.draw(cUserInterface.textBACKpressedSprite3);window.draw(cUserInterface.logregESCbackSprite);
                } else if (cInputManager.logregPressedItem == 3){
                    window.draw(cUserInterface.textloginBACKSprite);window.draw(cUserInterface.textBACKSprite3);window.draw(cUserInterface.logregESCbackpressedSprite);
                } else {
                    window.draw(cUserInterface.textloginBACKSprite);window.draw(cUserInterface.textBACKSprite3);window.draw(cUserInterface.logregESCbackSprite);
                }
                if (cInputManager.logregContainItem == 1){
                    window.draw(cUserInterface.textlogincontSprite);window.draw(cUserInterface.textsigninSprite);window.draw(cUserInterface.logregESCSprite);
                } else if (cInputManager.logregContainItem == 2){
                    window.draw(cUserInterface.textloginSprite);window.draw(cUserInterface.textsignincontSprite);window.draw(cUserInterface.logregESCSprite);
                } else if (cInputManager.logregContainItem == 3){
                    window.draw(cUserInterface.textloginSprite);window.draw(cUserInterface.textsigninSprite);window.draw(cUserInterface.logregESCcontSprite);
                } else {
                    window.draw(cUserInterface.textloginSprite);window.draw(cUserInterface.textsigninSprite);window.draw(cUserInterface.logregESCSprite);
                }
            } else if (cUserInterface.logregReleasedItem == 1){
                window.draw(cUserInterface.logregback2Sprite);
                window.draw(cUserInterface.textBACKSprite2);
                window.draw(cUserInterface.textBACKSprite3);
                if (cInputManager.inputLText.empty()) window.draw(cUserInterface.textlogSprite);
                if (cInputManager.inputRText.empty()) window.draw(cUserInterface.textpassSprite);
                if (cInputManager.logregPressedItem == 4) {
                    window.draw(cUserInterface.logregbacknopressedSprite);
                    window.draw(cUserInterface.logregbackyesSprite);
                } else if (cInputManager.logregPressedItem == 5) {
                    window.draw(cUserInterface.logregbacknoSprite);
                    window.draw(cUserInterface.logregbackyespressedSprite);
                } else {
                    window.draw(cUserInterface.logregbacknoSprite);
                    window.draw(cUserInterface.logregbackyesSprite);
                }
                if (cInputManager.logregContainItem == 4) {
                    window.draw(cUserInterface.nowhiteSprite2);
                    window.draw(cUserInterface.logSprite);
                } else if (cInputManager.logregContainItem == 5) {
                    window.draw(cUserInterface.logcontSprite);
                    window.draw(cUserInterface.noredSprite2);
                } else {
                    window.draw(cUserInterface.logSprite);
                    window.draw(cUserInterface.noredSprite2);
                }
                if (cInputManager.inputLText.empty()) window.draw(cUserInterface.textlogSprite);
                else window.draw(textInput.textLogin);
                if (cInputManager.inputRText.empty()) window.draw(cUserInterface.textpassSprite);
                else window.draw(textInput.textPass);
            } else if (cUserInterface.logregReleasedItem == 2){
                window.draw(cUserInterface.logregback2Sprite);
                window.draw(cUserInterface.textBACKSprite2);
                window.draw(cUserInterface.textBACKSprite3);
                if (cInputManager.inputLText.empty()) window.draw(cUserInterface.textlogSprite);
                else window.draw(textInput.textLogin);
                if (cInputManager.inputRText.empty()) window.draw(cUserInterface.textpassSprite);
                else window.draw(textInput.textPass);
                if (cInputManager.logregPressedItem == 4) {
                    window.draw(cUserInterface.logregbacknopressedSprite);
                    window.draw(cUserInterface.logregbackyesSprite);
                } else if (cInputManager.logregPressedItem == 6) {
                    window.draw(cUserInterface.logregbacknoSprite);
                    window.draw(cUserInterface.logregbackyespressedSprite);
                } else {
                    window.draw(cUserInterface.logregbacknoSprite);
                    window.draw(cUserInterface.logregbackyesSprite);
                }
                if (cInputManager.logregContainItem == 4) {
                    window.draw(cUserInterface.nowhiteSprite2);
                    window.draw(cUserInterface.regSprite);
                } else if (cInputManager.logregContainItem == 6) {
                    window.draw(cUserInterface.regcontSprite);
                    window.draw(cUserInterface.noredSprite2);
                } else {
                    window.draw(cUserInterface.regSprite);
                    window.draw(cUserInterface.noredSprite2);
                }
            }
        }
    }

    void wlDraw(sf::RenderWindow& window){
        if (cInputManager.wlPressedItem == 1){
            window.draw(cUserInterface.textBACKpressedSprite2); window.draw(cUserInterface.textBACKSprite3);
        } else if (cInputManager.wlPressedItem == 2){
            window.draw(cUserInterface.textBACKSprite2); window.draw(cUserInterface.textBACKpressedSprite3);
        } else if (cInputManager.wlPressedItem == 0) {
            window.draw(cUserInterface.textBACKSprite2); window.draw(cUserInterface.textBACKSprite3);
        }

        if (cInputManager.wlContainItem == 1){ 
            window.draw(cUserInterface.textagaincontainSprite); window.draw(cUserInterface.textquitSprite2);
        } else if (cInputManager.wlContainItem == 2){ 
            window.draw(cUserInterface.textagainSprite); window.draw(cUserInterface.textquitcontainSprite2);
        } else if (cInputManager.wlContainItem == 0){ 
            window.draw(cUserInterface.textagainSprite); window.draw(cUserInterface.textquitSprite2);
        }
    }

    void setup(sf::RenderWindow& window, sf::Event& event){
        setupMousePosX = cInputManager.mousePos.x;
        window.draw(cUserInterface.backgroundmSprite);
        cInputManager.cursorSet = false;
        if (setupMousePosX < 750){ //SLIDERS BEGIN
            if (isSoundSlider) {
                cInputManager.isSound = false;
                soundSliderInt = 0;
            }
            else if (isMusicSlider) {
                cInputManager.isMusic = false;
                musicSliderInt = 0;
            }
            setupMousePosX = 750;
        } else if (setupMousePosX > 1082) setupMousePosX = 1082;
        if (setupMousePosX > 750 && setupMousePosX <= 1082){
            if (isSoundSlider) cInputManager.isSound = true;
            else if (isMusicSlider) cInputManager.isMusic = true;
        }
        window.draw(cUserInterface.textBACKSFXMSC0Sprite);
        window.draw(cUserInterface.textBACKSFXMSC1Sprite);
        if (cUserInterface.textBACKSFXMSC0Sprite.getGlobalBounds().contains(cInputManager.mouseFloatPos)){
            cInputManager.cursorSet = true;
            window.setMouseCursor(cInputManager.handCursor);
            cInputManager.setupContainItem = 5;
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && !isSoundSlider && !isMusicSlider) {
                isMusicSlider = true;
            }
        } else if (cUserInterface.textBACKSFXMSC1Sprite.getGlobalBounds().contains(cInputManager.mouseFloatPos)){
            cInputManager.cursorSet = true;
            window.setMouseCursor(cInputManager.handCursor);
            cInputManager.setupContainItem = 6;
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && !isSoundSlider && !isMusicSlider) {
                isSoundSlider = true;
            }
        } 
        if (!sf::Mouse::isButtonPressed(sf::Mouse::Left)){
            if (isMusicSlider || isSoundSlider){
                cAudioManager.playSoundUIClick();
                isMusicSlider = false;
                isSoundSlider = false;
            }
        }
        if (isMusicSlider && !isSoundSlider && sf::Mouse::isButtonPressed(sf::Mouse::Left)){
            musicSliderInt = setupMousePosX - 750;
            musicVolumeF = ((setupMousePosX-750)/332.f)*100;
            cAudioManager.musicVolumeI = static_cast<int>(std::round(musicVolumeF));
        } else if (isSoundSlider && !isMusicSlider && sf::Mouse::isButtonPressed(sf::Mouse::Left)){
            soundSliderInt = setupMousePosX - 750;
            soundVolumeF = ((setupMousePosX-750)/332.f)*100;
            cAudioManager.soundVolumeI = static_cast<int>(std::round(soundVolumeF));
        }
        cUserInterface.textBACKSFXMSC0pressedSprite.setTextureRect(sf::IntRect(0, 0, musicSliderInt, 105));
        cUserInterface.textBACKSFXMSC1pressedSprite.setTextureRect(sf::IntRect(0, 0, soundSliderInt, 105));
        window.draw(cUserInterface.textBACKSFXMSC0pressedSprite);
        window.draw(cUserInterface.textBACKSFXMSC1pressedSprite);
        if (isSoundSlider) soundSlider = setupMousePosX;
        else if (isMusicSlider) musicSlider = setupMousePosX;
        if (cInputManager.isMusic) window.draw(cUserInterface.musicONSprite);
        else if (!cInputManager.isMusic || musicSlider == 750) window.draw(cUserInterface.musicOFFSprite);
        if (cInputManager.isSound && soundSlider > 971 && soundSlider <= 1082) window.draw(cUserInterface.soundON2Sprite);
        else if (cInputManager.isSound && soundSlider > 861 && soundSlider <= 971) window.draw(cUserInterface.soundON1Sprite);
        else if (cInputManager.isSound && soundSlider > 750 && soundSlider <= 861) window.draw(cUserInterface.soundON0Sprite);
        else if (!cInputManager.isSound || soundSlider == 750) window.draw(cUserInterface.soundOFFSprite); //SLIDERS END
        if (cInputManager.setupPressedItem == 1){
            window.draw(cUserInterface.setupTextBACKpressedSprite);window.draw(cUserInterface.speedBACK0Sprite);window.draw(cUserInterface.speedBACK1Sprite);window.draw(cUserInterface.speedBACK2Sprite);
        } else if(cInputManager.setupPressedItem == 2){
            window.draw(cUserInterface.speedBACK0pressedSprite);window.draw(cUserInterface.setupTextBACKSprite);window.draw(cUserInterface.speedBACK1Sprite);window.draw(cUserInterface.speedBACK2Sprite);
        } else if(cInputManager.setupPressedItem == 3){
            window.draw(cUserInterface.speedBACK1pressedSprite);window.draw(cUserInterface.setupTextBACKSprite);window.draw(cUserInterface.speedBACK0Sprite);window.draw(cUserInterface.speedBACK2Sprite);
        } else if(cInputManager.setupPressedItem == 4){
            window.draw(cUserInterface.speedBACK2pressedSprite);window.draw(cUserInterface.setupTextBACKSprite);window.draw(cUserInterface.speedBACK0Sprite);window.draw(cUserInterface.speedBACK1Sprite);
        } else {
            window.draw(cUserInterface.setupTextBACKSprite);window.draw(cUserInterface.speedBACK0Sprite);window.draw(cUserInterface.speedBACK1Sprite);window.draw(cUserInterface.speedBACK2Sprite);
        }
        if (cInputManager.choseItem == 1) {
            window.draw(cUserInterface.speedBACK0pressedSprite);
            window.draw(cUserInterface.chose0Sprite);
        } else if (cInputManager.choseItem == 2) {
            window.draw(cUserInterface.speedBACK1pressedSprite);
            window.draw(cUserInterface.chose1Sprite);
        } else if (cInputManager.choseItem == 3) {
            window.draw(cUserInterface.speedBACK2pressedSprite);
            window.draw(cUserInterface.chose2Sprite);
        }
        if (cInputManager.setupContainItem == 1){
            window.draw(cUserInterface.setupbackContSprite);window.draw(cUserInterface.speedTEXT0Sprite);window.draw(cUserInterface.speedTEXT1Sprite);window.draw(cUserInterface.speedTEXT2Sprite);window.draw(cUserInterface.musicSprite);window.draw(cUserInterface.soundSprite);
        } else if(cInputManager.setupContainItem == 2){
            window.draw(cUserInterface.speedTEXT0contSprite);window.draw(cUserInterface.speedTEXT1Sprite);window.draw(cUserInterface.speedTEXT2Sprite);window.draw(cUserInterface.setupbackSprite);window.draw(cUserInterface.musicSprite);window.draw(cUserInterface.soundSprite);
        } else if(cInputManager.setupContainItem == 3){
            window.draw(cUserInterface.speedTEXT1contSprite);window.draw(cUserInterface.speedTEXT0Sprite);window.draw(cUserInterface.speedTEXT2Sprite);window.draw(cUserInterface.setupbackSprite);window.draw(cUserInterface.musicSprite);window.draw(cUserInterface.soundSprite);
        } else if(cInputManager.setupContainItem == 4){
            window.draw(cUserInterface.speedTEXT2contSprite);window.draw(cUserInterface.speedTEXT0Sprite);window.draw(cUserInterface.speedTEXT1Sprite);window.draw(cUserInterface.setupbackSprite);window.draw(cUserInterface.musicSprite);window.draw(cUserInterface.soundSprite);
        } else if(cInputManager.setupContainItem == 5){
            window.draw(cUserInterface.musicContSprite);window.draw(cUserInterface.setupbackSprite);window.draw(cUserInterface.speedTEXT0Sprite);window.draw(cUserInterface.speedTEXT1Sprite);window.draw(cUserInterface.speedTEXT2Sprite);window.draw(cUserInterface.soundSprite);
        } else if(cInputManager.setupContainItem == 6){
            window.draw(cUserInterface.soundContSprite);window.draw(cUserInterface.setupbackSprite);window.draw(cUserInterface.speedTEXT0Sprite);window.draw(cUserInterface.speedTEXT1Sprite);window.draw(cUserInterface.speedTEXT2Sprite);window.draw(cUserInterface.musicSprite);
        } else {
            window.draw(cUserInterface.setupbackSprite);window.draw(cUserInterface.speedTEXT0Sprite);window.draw(cUserInterface.speedTEXT1Sprite);window.draw(cUserInterface.speedTEXT2Sprite);window.draw(cUserInterface.musicSprite);window.draw(cUserInterface.soundSprite);
        }
        cAudioManager.musicUpdate(cInputManager.isMusic, cAudioManager.musicVolumeI);
        cAudioManager.soundUpdate(cInputManager.isSound, cAudioManager.soundVolumeI);
        cConfigManager.saveSettings(cAudioManager.musicVolumeI, musicSliderInt, cInputManager.isMusic, cAudioManager.soundVolumeI, soundSliderInt, cInputManager.isSound, cSnakeGame.moveInterval, cInputManager.choseItem);
    }

    void drawLeaderboard(sf::RenderWindow& window, sf::Font& font, const std::vector<std::pair<std::string, int>>& leaderboard) {
        sf::Text text;
        text.setFont(font);
        text.setCharacterSize(24);
        float startY = 50;
        for (size_t i = 0; i < leaderboard.size(); i++) {
            text.setString(std::to_string(i + 1) + ". " + leaderboard[i].first + " - " + std::to_string(leaderboard[i].second));
            text.setPosition(100, startY + i * 30);
            if (i == 0) text.setFillColor(sf::Color::Yellow);
            else if (i == 1) text.setFillColor(sf::Color::Cyan);
            else if (i == 2) text.setFillColor(sf::Color::Magenta);
            else text.setFillColor(sf::Color::White);
            window.draw(text);
        }
    }
};

class Game {
    public:
    void gameWindow (){
        sf::Font font;
        if (!font.loadFromFile("assets/font/8bitOperatorPlus8-Regular.ttf")) std::cerr << "Failed to load font!\n";
        ServerClient serverClient;
        TextInput textInput(font);
        ConfigManager cConfigManager(serverClient);
        AudioManager cAudioManager;
        UserInterface cUserInterface;
        SnakeGame cSnakeGame(cUserInterface, cAudioManager, cConfigManager, serverClient);
        InputManager cInputManager(cSnakeGame, cAudioManager, textInput, serverClient);
        Draw cDraw(serverClient, cUserInterface, cSnakeGame, cInputManager, cAudioManager, cConfigManager, textInput, font);
        cConfigManager.loadSettings(cAudioManager.musicVolumeI, cDraw.musicSliderInt, cInputManager.isMusic, cAudioManager.soundVolumeI, cDraw.soundSliderInt, cInputManager.isSound, cSnakeGame.moveInterval, cInputManager.choseItem);
        cAudioManager.soundUpdate(cInputManager.isSound, cAudioManager.soundVolumeI);
        cAudioManager.musicUpdate(cInputManager.isMusic, cAudioManager.musicVolumeI);
        serverClient.isTokenValid();
        int window_width = 1920;
        int window_height = 1080;
        sf::RenderWindow window(sf::VideoMode(window_width, window_height), "Snake", sf::Style::Default);
        cAudioManager.playMusic();
        sf::Event event;
        window.setFramerateLimit(144);
        while (window.isOpen()){
            cSnakeGame.gameUpdate(cUserInterface.isGamePaused);
            cInputManager.pollEventFunc(window, event, cUserInterface);
            window.clear();
            cDraw.windowDraw(window, event);
            window.display();
        }
    }
};

int main(){
    SetDllDirectoryA("libs");
    Game Game;
    Game.gameWindow();
    return 0;
}