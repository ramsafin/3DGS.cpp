#include "QueryManager.h"

#include <stdexcept>

void QueryManager::setCapacity(uint32_t capacity) {
    std::lock_guard<std::mutex> lock(mutex);
    this->capacity = capacity;
}

uint32_t QueryManager::registerQuery(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!registry.contains(name)) {
        if (capacity != 0 && static_cast<uint32_t>(nextId) >= capacity) {
            throw std::runtime_error("QueryManager exceeded timestamp query pool capacity");
        }
        registry[name] = nextId++;
    }
    return registry[name];
}

uint32_t QueryManager::getQueryId(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (registry.contains(name)) {
        return registry.at(name);
    }
    return 0;
}

std::unordered_map<std::string, uint64_t> QueryManager::parseResults(const std::vector<uint64_t>& results) {
    std::lock_guard<std::mutex> lock(mutex);
    std::unordered_map<std::string, uint64_t> resultsMap;
    for (auto& [name, id] : registry) {
        if (name.ends_with("_start")) {
            auto endName = name.substr(0, name.size() - 5) + "end";
            if (registry.contains(endName)) {
                auto start = results[id];
                auto end = results[registry[endName]];
                auto diff = end - start;
                auto truncated = name.substr(0, name.size() - 6);
                resultsMap[truncated] = diff;
            }
        }
    }
    return resultsMap;
}
