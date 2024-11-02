// json-util.h
// Some utilities on top of json.hpp.

#ifndef JSON_UTIL_H
#define JSON_UTIL_H


// Save a field where converting to JSON only requires invoking one of
// the `JSON` constructors.
#define SAVE_KEY_FIELD_CTOR(name) \
  obj[#name] = json::JSON(m_##name) /* user ; */


// Load `field` from `key`.  `expr` is an expression that refers to
// `data`, the JSON object at `key`.
#define LOAD_FIELD(key, field, expr)      \
  if (obj.hasKey(key)) {                  \
    json::JSON const &data = obj.at(key); \
    field = (expr);                       \
  }

// For when the key and field names are related in the usual way.
#define LOAD_KEY_FIELD(name, expr) \
  LOAD_FIELD(#name, m_##name, expr)


#endif // JSON_UTIL_H
