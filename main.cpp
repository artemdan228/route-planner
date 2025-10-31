#include <iostream>
#include "cpr/cpr.h"
#include <sstream>
#include "jsonParser.h"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <iomanip>

using json = nlohmann::json;

std::unordered_map<std::string, nlohmann::json> routeCache;

struct ApiRequest {
    std::string from;
    std::string to;
    std::string date_from;
    std::string date_to;
    std::string apikey;
};

std::string generateCacheKey(const std::string& from, const std::string& to, const std::string& date_from, const std::string& date_to) {
    return from + "_" + to + "_" + date_from + "_" + date_to;
}

std::string DateParser(const std::string& inputDate) {
    std::istringstream iss(inputDate);
    int day, month, year;
    char point1, point2;

    if(iss >> day >> point1 >> month >> point2 >> year && point1 == '.' && point2 == '.') {
        std::ostringstream oss;
        oss << year << "-"
            << (month < 10 ? "0" : "") << month << "-"
            << (day < 10 ? "0" : "") << day;
        return oss.str();
    }

    return "";
}

std::string getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d");
    return oss.str();
}

void clearOldCache(std::unordered_map<std::string, nlohmann::json>& cache) {
    std::string currentDate = getCurrentDate();

    for (auto it = cache.begin(); it != cache.end();) {
        size_t pos = it->first.find(currentDate);
        if (pos == std::string::npos) {
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}

std::string find_city_yandex_code(const json& data, const std::string& city_name) {
    for (const auto& country : data["countries"]) {
        for (const auto& region : country["regions"]) {
            for (const auto& settlement : region["settlements"]) {
                std::string settlement_name = settlement["title"];
                if (settlement_name == city_name) {
                    if (settlement.contains("codes") && settlement["codes"].contains("yandex_code")) {
                        return settlement["codes"]["yandex_code"].get<std::string>();
                    }
                    return "Код города не найден";
                }
            }
        }
    }
    return "Город не найден";
}

void loadCacheFromFile(std::unordered_map<std::string, nlohmann::json>& cache, const std::string& filename) {
    std::ifstream inputFile(filename);
    if (inputFile.is_open()) {
        nlohmann::json cacheJson;
        inputFile >> cacheJson;
        cache = cacheJson.get<std::unordered_map<std::string, nlohmann::json>>();
        inputFile.close();
    } else {
        std::cerr << "Не удалось открыть файл для чтения кэша!" << std::endl;
    }
}

void saveCacheToFile(const std::unordered_map<std::string, nlohmann::json>& cache, const std::string& filename) {
    std::ofstream outputFile(filename);
    if (outputFile.is_open()) {
        nlohmann::json cacheJson = cache;
        outputFile << cacheJson.dump(4);
        outputFile.close();
    } else {
        std::cerr << "Не удалось открыть файл для записи кэша!" << std::endl;
    }
}

bool isValidDate(const std::string& date) {
    return date.size() == 10 && date[2] == '.' && date[5] == '.';
}

cpr::Response getCityData() {
    cpr::Response response = cpr::Get(cpr::Url{"https://api.rasp.yandex.net/v3.0/stations_list/?apikey=YOUR_API_KEY&lang=ru_RU&format=json"});
    if (response.status_code != 200) {
        std::cerr << "Ошибка запроса к API. Код ошибки: " << response.status_code << std::endl;
        return cpr::Response();
    }
    return response;
}

int main() {
    clearOldCache(routeCache);
    loadCacheFromFile(routeCache, "cache.json");

    std::cout << "Введите город отправления:" << "\n";
    std::string cityes_from;
    std::cin >> cityes_from;

    std::cout << "Введите город назначения:" << "\n";
    std::string cityes_to;
    std::cin >> cityes_to;

    std::string inputDate_from, inputDate_to;
    std::cout << "Введите дату поездки туда (дд.мм.гггг):" << "\n";
    std::cin >> inputDate_from;
    if (!isValidDate(inputDate_from)) {
        std::cerr << "Неправильный формат даты!" << std::endl;
        return 1;
    }

    std::cout << "Введите дату поездки обратно (дд.мм.гггг):" << "\n";
    std::cin >> inputDate_to;
    if (!isValidDate(inputDate_to)) {
        std::cerr << "Неправильный формат даты!" << std::endl;
        return 1;
    }

    cpr::Response r_cityes = getCityData();
    if (r_cityes.status_code != 200) {
        return 1;
    }

    json data_city = json::parse(r_cityes.text);

    std::string code_from = find_city_yandex_code(data_city, cityes_from);
    std::string code_to = find_city_yandex_code(data_city, cityes_to);

    if (code_from == "Город не найден" || code_to == "Город не найден") {
        std::cerr << "Один из городов не найден!" << std::endl;
        return 1;
    }

    ApiRequest request;
    request.from = code_from;
    request.to = code_to;
    request.date_from = DateParser(inputDate_from);
    request.date_to = DateParser(inputDate_to);
    request.apikey = "YOUR_API_KEY"; // Замените на ваш реальный API ключ

    std::string cacheKey_from = generateCacheKey(request.from, request.to, request.date_from, request.date_to);
    std::string cacheKey_to = generateCacheKey(request.to, request.from, request.date_to, request.date_from);

    nlohmann::json jsonData_from;
    if (routeCache.find(cacheKey_from) != routeCache.end()) {
        jsonData_from = routeCache[cacheKey_from];
    } else {
        std::string url_from = "https://api.rasp.yandex.net/v3.0/search/"
                               "?from=" + request.from +
                               "&to=" + request.to +
                               "&format=json"
                               "&apikey=" + request.apikey +
                               "&date=" + request.date_from +
                               "&transfers=true";

        cpr::Response r_from = cpr::Get(cpr::Url{url_from});

        if (r_from.status_code == 200) {
            jsonData_from = nlohmann::json::parse(r_from.text);
            routeCache[cacheKey_from] = jsonData_from;
        } else {
            std::cerr << "Ошибка запроса! Код: " << r_from.status_code << "\n";
            return 1;
        }
    }

    nlohmann::json jsonData_to;
    if (routeCache.find(cacheKey_to) != routeCache.end()) {
        jsonData_to = routeCache[cacheKey_to];
    } else {
        std::string url_to = "https://api.rasp.yandex.net/v3.0/search/"
                             "?from=" + request.to +
                             "&to=" + request.from +
                             "&format=json"
                             "&apikey=" + request.apikey +
                             "&date=" + request.date_to +
                             "&transfers=true";

        cpr::Response r_to = cpr::Get(cpr::Url{url_to});

        if (r_to.status_code == 200) {
            jsonData_to = nlohmann::json::parse(r_to.text);
            routeCache[cacheKey_to] = jsonData_to;
        } else {
            std::cerr << "Ошибка запроса! Код: " << r_to.status_code << "\n";
            return 1;
        }
    }

    std::cout << "====================================" << "\n";
    std::cout << cityes_from << " ⇄ " << cityes_to << "\n";
    std::cout << "====================================" << "\n";
    std::cout << "Маршруты туда:" << "\n";

    RouteParser routeParser_from(jsonData_from);
    routeParser_from.parse();
    routeParser_from.printRoutes();

    std::cout << "Маршруты обратно:" << "\n";
    RouteParser routeParser_to(jsonData_to);
    routeParser_to.parse();
    routeParser_to.printRoutes();

    saveCacheToFile(routeCache, "cache.json");

    return 0;
}
