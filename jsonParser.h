#ifndef ROUTE_PARSER_H
#define ROUTE_PARSER_H

#include <string>
#include <nlohmann/json.hpp>
#include <vector>

struct RouteSegment {
    std::string from_city;
    std::string to_city;
    std::string from_station;
    std::string to_station;
    std::string date;
    std::vector<std::string> transfers_titles;
    std::vector<std::string> numbers;
    std::vector<std::string> transport_types;
    // std::vector<std::string> vehicle;
    std::vector<std::string> company;
    std::vector<std::string> route_main;
    std::vector<std::string> dep_time_route;
    std::vector<std::string> arr_time_route;
    std::string departure_time;
    std::string arrival_time;
    bool hasTransfers;
};

class RouteParser {
public:
    explicit RouteParser(const nlohmann::json& jsonData);
    void parse();
    void printRoutes() const;
    std::string convertISO(const std::string& iso_date) const;
    std::string formatDateTime(const std::string& isoDateTime) const;

private:
    nlohmann::json data;
    std::vector<RouteSegment> routes;
};

#endif



