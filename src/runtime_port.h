#pragma once

#include <vector>
#include <string>

#define DLL __attribute__((visibility("default")))

#define UNUSED __attribute__((unused))
#define REG_VAR static UNUSED uint32_t __Make_Object_T

#define STR_CONCAT_IMPL(x, y) x##y
#define STR_CONCAT(x, y) STR_CONCAT_IMPL(x, y)

template <typename T>
inline T* Pointer(std::vector<T>& vec) {  
  if (vec.size() == 0) {
    return NULL;
  } else {
    return &vec[0];
  }
}

template <typename T>
inline const T* Pointer(const std::vector<T>& vec) {
  if (vec.size() == 0) {
    return NULL;
  } else {
    return &vec[0];
  }
}

inline char* Pointer(std::string& str) {  
  if (str.length() == 0)
    return NULL;
  return &str[0];
}

inline const char* Pointer(const std::string& str) {
  if (str.length() == 0)
    return NULL;
  return &str[0];
}

