#ifndef QUERYMANAGER_H
#define QUERYMANAGER_H
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class QueryManager {
  public:
    uint32_t registerQuery(const std::string& name);
    [[nodiscard]] uint32_t getQueryId(const std::string& name);

    std::unordered_map<std::string, uint64_t> parseResults(const std::vector<uint64_t>& results);

    // Upper bound on distinct query ids; matches the timestamp pool capacity so
    // ids never exceed the pool (VKGS-026). Zero means unbounded.
    void setCapacity(uint32_t capacity);

    int nextId = 0;

  private:
    uint32_t capacity = 0;
    std::mutex mutex;
    std::unordered_map<std::string, uint32_t> registry;
    std::unordered_map<std::string, std::vector<uint64_t>> results;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastPrint;
};

#endif // QUERYMANAGER_H
