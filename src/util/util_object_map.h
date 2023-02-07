#pragma once

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <tuple>

namespace as {

template<typename K, typename V,
  typename Hash = std::hash<K>,
  typename Eq   = std::equal_to<K>>
class ObjectMap {

public:

  template<typename... Args>
  V* get(const K& key, Args&&... args) {
    { std::shared_lock lock(m_mutex);

      auto e = m_map.find(key);

      if (e != m_map.end())
        return &e->second;
    }

    std::unique_lock lock(m_mutex);

    auto e = m_map.emplace(std::piecewise_construct,
      std::tuple(key),
      std::forward_as_tuple<Args...>(args...));

    return &e.first->second;
  }

private:

  std::shared_mutex                   m_mutex;
  std::unordered_map<K, V, Hash, Eq>  m_map;

};

}
