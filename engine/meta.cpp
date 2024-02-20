/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Copyright (C) 2023-2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <https://www.gnu.org/licenses/>
 */

#include "core.hpp"
#include "meta.hpp"
#include <cctype>
#include <cmath>
#include <exception>
#include <stdexcept>

#include "base/defer.hpp"

//implementation
#include "external/robin_map.h"
namespace pb {

  using MetaValue = std::variant<double, std::string>;
	// types of MetaValue
	enum MetaTypes {
		NUM,
		STR,
		OBJ
	};

namespace impl {
	using Mmap = tsl::robin_map<MetaKey, MetaValue, std::hash<MetaKey>, std::equal_to<MetaKey>, std::allocator<std::pair<MetaKey, MetaValue>>, false>;
  // Value for associative metadata map (see below)
  struct MetaMap : public Mmap {
    using Base = Mmap;
    using Base::Base;

    // assumes key is validated already. May return NULLPTR
    const MetaValue* get_variant(MetaKey k) const {
      auto v = find(k);
      if (v != end()) {
        return &v->second;
      };
      return nullptr;
    }; 
  };

  const MetaValue NIL = std::string();
  const MetaKey NILK = HString();

  bool is_nil(const MetaValue &src) {
    return src.index() == 1 && std::get<1>(src).size() == 0;
  }

  bool is_nil(const MetaKey &src) {
    return src.index() == 1 && std::get<1>(src).size() == 0;
  }

  bool is_str_number(const std::string& ref, double& res) {
    res = 0;
    if (isdigit(ref[0])) {
      const char* a = ref.c_str();
      char* b = nullptr;
      double v = pb::strtod(a, &b);
      res = v;

      while (isspace(*b)) b++;
      if (*b == '\0' && (std::isnormal(v) || v == 0.0)) {
        return true;
      }; // valid number
    }
    return false; // also in case of Nan, Inf
  }

  // utility : values to valid to MetaKey
  bool validateKey(MetaKey& src) {
    // check number
    if (src.index() == 0) {
      double& ref = std::get<0>(src);
      if (std::isnormal(ref) || ref == 0.0) return true; // number
      return false; // bad numeric value
    }
    
    // check if empty string
    HString& ref = std::get<1>(src);
    if (ref.size() == 0) return false; // empty string
    
    // check if string is a valid number
    double res;
    if (is_str_number(ref, res)) {
      src = MetaKey(res);
      return true; // successful cast
    }

    // string is OK
    return true;
  };

  std::string num2str(double d) {
    double v = std::trunc(d);
    if (v == d) 
      return pb::format_r("%li", (int32_t)v);
    return pb::format_r("%f", d);
  }
  
  std::string to_str(const MetaValue& v) {
    try {
      if (v.index() == 0) {
        return num2str(std::get<0>(v));
      }
      return std::get<1>(v);
    } catch(...) {
      return std::string(); // oom  
    }
  }

  std::string to_str(const MetaKey& v) {
    try {
      if (v.index() == 0) {
        return num2str(std::get<0>(v));
      }
      return std::get<1>(v);
    } catch(...) {
      return std::string(); // oom  
    }
  }

  MetaValue to_val(const std::string& src) {
    if (src.size() == 0) return NIL; // empty
    
    double res;
    if (is_str_number(src, res)) { // cast to number if string is a number!
      return MetaValue(res); // to number 
    }

    return MetaValue(src); // keep original string
  }
  
  // for numbers
  MetaValue to_val(const double src) {
    if (std::isnormal(src) || src == 0.0) return MetaValue(src);
    return NIL; // NULL
  }


};

Metadata::Metadata() noexcept {
  map = nullptr;
}

Metadata::Metadata(Metadata&& src) noexcept {
  map = src.map;
  src.map = nullptr;
}

Metadata& Metadata::operator=(Metadata&& src) noexcept {
  if (this != &src) {
    if (map) delete map;
    map = src.map;
    src.map = nullptr;
  }
  return *this;
}

Metadata::~Metadata() {
  if (map) delete map;
}

size_t Metadata::size() const noexcept {
  return map ? map->size() : 0;
} 

// we check 
bool Metadata::has_value(const MetaKey& key) const {
  MetaKey k = key;
  return get_type(k) >= 0; 
}

// type of a VALUE
int Metadata::get_type(MetaKey& k) const {
  if (map && impl::validateKey(k)) {
    auto v = map->find(k);
    if (v != map->end()) {
      size_t t = v->second.index(); // VALUE
      return (int)t; // both can be converted back to string
    };
  };
  return -1; // bad key or empty map
}
/** check for type + has */
bool Metadata::is_string (const MetaKey& key) const {
  MetaKey k = key;
  return get_type(k) >= 0; // number and string are convertable to string! 
}

/** may attempt casting :) */
bool Metadata::is_number(const MetaKey& key) const {
  MetaKey k = key;
  return get_type(k) == 0; // number
}

/** return string value or emplaces it (and casts). Returns empty string on OOM or empty! */
std::string Metadata::get_string(const MetaKey& key) const {
  MetaKey k = key;
  if (map && impl::validateKey(k)) {
    const MetaValue* v = map->get_variant(k);
    if (v) {
      return impl::to_str(*v);
    }
  }
  return std::string();
}

int32_t     Metadata::get_integer(const MetaKey& key) const {
  MetaKey k = key;
  if (map && impl::validateKey(k)) {
    const MetaValue* v = map->get_variant(k);
    if (v && v->index() == 0) {
      return std::get<0>(*v);
    }
  }
  return 0; // default
}

double      Metadata::get_number(const MetaKey& key) const {
  MetaKey k = key;
  if (map && impl::validateKey(k)) {
    const MetaValue* v = map->get_variant(k);
    if (v && v->index() == 0) {
      return std::get<0>(*v);
    }
  }
  return 0.0; // default
}

/** removes value from the map */
void Metadata::unset(const MetaKey& key) {
  MetaKey k = key;
  if (map && impl::validateKey(k)) {
    map->erase(k);
  }
}

/** sets value */ 
void Metadata::set(const MetaKey& key, const std::string& value) {
  MetaKey k = key;
  if (impl::validateKey(k)) {
    if (!map) map = new impl::MetaMap();
    MetaValue v = impl::to_val(value);
    if (!impl::is_nil(v)) // ignore emty strings
      map->insert_or_assign(k, v);
    else map->erase(k); // else remove key
  }
}

void Metadata::set(const MetaKey& key, const int32_t value) {
  MetaKey k = key;
  if (impl::validateKey(k)) {
    if (!map) map = new impl::MetaMap();
    MetaValue v = impl::to_val(value);
    if (!impl::is_nil(v)) // ignore invalid numbers
      map->insert_or_assign(k, v);
    else map->erase(k); // else remove key
  }
}

void Metadata::set(const MetaKey& key, const double value) {
  MetaKey k = key;
  if (impl::validateKey(k)) {
    if (!map) map = new impl::MetaMap();
    MetaValue v = impl::to_val(value);
    if (!impl::is_nil(v)) // ignore invalid numbers
      map->insert_or_assign(k, v);
    else map->erase(k); // else remove key
  }
}

Metadata Metadata::copy() {
  Metadata dst;
  if (!map) return dst; // empty map 
  
  // copy stuff
  dst.map = new impl::MetaMap;
  dst.map->insert(map->begin(), map->end());
  return dst; // done
}

void Metadata::clear() {
  // don't delete, because may be refilled
  if (map) map->clear();
} 

bool Metadata::next(MetaKey& key) {
  if (!map) { // nothing
    key = impl::NILK;
    return false;
  }

  // get next key
  {
  MetaKey k = key;
  if (impl::validateKey(k)) {
    auto v = map->find(k);
    if (v != map->end()) v++; // next
    if (v != map->end()) { // found
      key = v->first;
      return true; // another one
    };
    // unlikely to hit there, so we can let us do extra checks for is_nil!
  };
  }
  // key does not exist

  // is nil -> start of the iteration?
  if (impl::is_nil(key)) {
    auto i = map->begin();
    if (i != map->end()) {
      key = i->first; // get "next" key
      return true;
    }
  }
  
  // not found or end of iteration or empty map!
  key = impl::NILK;
  return false;
};
};
/**
 * THE BEAST
 * 
 * JSON
 *
 * I hate "modern c++ json porsers" because of importance of object representation.
 * We can do that on the fly. Also, we don't use deep ass distionary-in-dictionary thing at all.
 */

#include <ios>
#include <iostream>
#include <iomanip>
#include <cctype>

namespace pb {
namespace impl {
  
  // uTF8 string out
  void jsonify_string(std::ostream& dst, const std::string& src) {
    dst << '"' ;
    for (char c : src) {
      if (c == '"' || c == '\\') { // escape
        dst << '\\' << c;
      } else if(iscntrl(c)) { // special
        dst << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        dst << std::dec << std::setw(0);
      } else dst << c;
    }
    dst << '"';
  }
  
  struct Parser {
    std::istream& src;
    int c = ' '; // current char;
    public:
    Parser(std::istream& src) : src(src) {}

    char next() {
      c = src.get();
      if (c == EOF) std::runtime_error("unexpected EOF"); 
      return c;
    }

    void skip_space() {
      if (!isspace(c)) return; // we already jave good char
      while (isspace(next())) {} // skip :) 
    }

    char escape() {
      switch (next()) {
        case '\\' : return '\\';
        case '\n' : return '\n'; 
        case 'u' : {
          char v[5] = {0};
          for (int i = 0; i < 4; i++) {
            if (!isdigit(next())) throw std::runtime_error("Invalid UTF8 escape!");
          }
          int v2 = std::strtod(v, nullptr);
          if (v2 > '\x1F') throw std::runtime_error("UTF escapes not support values > than 255! :D. I am lazy");
          return char(v2);
        };
        default: throw std::runtime_error("Invalid escape");
      }
    }
    
    // we found " char!
    std::string parse_str() {
      std::string v;
      while (next() != '"') {
        if (c == '\\') {
          v += escape(); 
        } else {
          v += c;
        }
      }
      next(); // after string
      return v; // done
    }

    double parse_num() {
      char tmp[64] = {0};
      int i = 0;
      // put curr digit
      tmp[i++] = c;

      while (next(), (isdigit(c) || c == 'e' || c == 'E' || c == '.') && i < 63) {
        tmp[i++] = c;  
      }
      
      char* p = nullptr;
      double v = pb::strtod(tmp, &p);
      if (*p != '\0') throw std::runtime_error("Bad number parsed!"); 
      return v;
    }

    void execute(Metadata& meta) {
      while ((c = src.get()) != EOF && isspace(c)) {}
      if (c == EOF) return; // no data 
      if (c != '{') throw std::runtime_error("only json dictionary is supported and only once!");
      next();
      if (c == '}') return; // end early

      while (true) {
        skip_space();
          
        // parse key
        if (c != '"') // required to be a string!
          throw std::runtime_error("only string may be a key!");
        MetaKey k = HString(parse_str());
        skip_space();
        if (c != ':') std::runtime_error(": separator betwwen key and value required!");
        next();
        skip_space();
        
        // parse value and set pair into metamap
        if (isdigit(c) || c == '.' || c == 'e' || c == 'E') meta.set(k, parse_num());
        else if (c == '"') meta.set(k, parse_str());
        else std::runtime_error("invalid value!");
          
        // check for end or separator
        skip_space();
        if (c == ',') next(); // skip separator
        else if (c == '}') return;
        else throw std::runtime_error("separator , or end of dictionary } required!");
      }
      // unreachable
      };

      // end

    };
    
  };


bool Metadata::deserialize(std::istream& src) {
  impl::Parser p(src);
  try {
    p.execute(*this);
    return true;
  } catch (std::exception& e) {
    std::cerr << "Metadata::deserialize() : " << e.what() << std::endl;
    return false;
  }
}

// hard one... :skull:
void Metadata::serialize(std::ostream& dst, bool exclude_defaults) {
  dst << '{';
  
  if (!map) {
    dst << '}';
    return; // end
  }

  std::ios_base::fmtflags f(dst.flags() );
  auto _ = pb::defer([&dst, f]() {
    dst.flags(f);
  });
  
  for (auto i = map->begin(); i != map->end();) {
    
    const MetaValue& v = i->second;
    if (exclude_defaults && v.index() == 0 && std::get<0>(v) == 0.0) { // skip to save disk space 
      i++;
      continue; 
    }

    // key
    impl::jsonify_string(dst, impl::to_str(i->first)); // to string all keys 
    dst << ':';

    if (v.index() == 0) { // number
      dst << impl::num2str(std::get<0>(v));
    } else {
      impl::jsonify_string(dst, impl::to_str(v)); 
    }
    
    // next
    i++;
    if (i != map->end()) dst << ',';
  }

  dst << '}';

}


};
