// SPDX-License-Identifier: MIT
#pragma once
#include <cstdlib>
#include <expected>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>
enum PropertyType { kPropertyTypeInteger, kPropertyTypeString };
struct Property {
    std::string name;
    std::variant<int, std::string> data;
    Property(const char* n, PropertyType) : name(n), data(std::string()) {}
    Property(const char* n, PropertyType, int minimum, int) : name(n), data(minimum) {}
    Property(const char* n, PropertyType, int value, int, int) : name(n), data(value) {}
    template <class T>
    const T& value() const {
        return std::get<T>(data);
    }
};
struct PropertyList {
    std::vector<Property> properties;
    PropertyList() = default;
    PropertyList(std::initializer_list<Property> list) : properties(list) {}
    const Property& operator[](const char* name) const {
        for (auto& p : properties)
            if (p.name == name)
                return p;
        std::abort();
    }
    template <class T>
    void Set(const char* name, T value) {
        for (auto& p : properties)
            if (p.name == name) {
                p.data = std::move(value);
                return;
            }
        std::abort();
    }
};
using ToolResult = std::expected<std::string, std::string>;
struct McpServer {
    struct Tool {
        std::string name;
        PropertyList defaults;
        std::function<ToolResult(const PropertyList&)> callback;
    };
    std::vector<Tool> tools;
    void AddTool(const char* name, const char*, PropertyList defaults,
                 std::function<ToolResult(const PropertyList&)> callback) {
        tools.push_back({name, std::move(defaults), std::move(callback)});
    }
};
