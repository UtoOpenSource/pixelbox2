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

#pragma once
#include "base/base.hpp"
#include "base/string.hpp"
#include "engine/core.hpp"
#include <variant>

namespace pb {


namespace impl {
/*
 * PRIVATE implementation of METADATA!
 * Metadata methods are just a proxies to the methods of this object!
 */
  struct MetaMap;

/*
 * Basic associative array for Chunks!
 */
  struct ChunkMap;
};

struct MetaKey : public std::variant<double, pb::HString> {
  using Base = std::variant<double, pb::HString>;
  using Base::Base;

  MetaKey(int i) : Base((double)i) {}
  MetaKey(uint32_t i) : Base((double)i) {}
  MetaKey(const char* c) : Base(HString(c)) {}
};

};

template<>
struct std::hash<pb::MetaKey> {
  size_t operator()(const pb::MetaKey& key) const noexcept {
    std::hash<pb::MetaKey::Base> hasher;
    return hasher(pb::MetaKey::Base(key));
  }
};

namespace pb {

/**
 * Every Object that relates to the game itself : Chunk, Entyty,
 * Player, etc. Always has special storage for any amount of key=value
 * paired data - METADATA.
 *
 * Altrough name suggests taht this is a minor thing, sometimes 
 * metadata plays a key role in some systems. It allows code to be
 * dynamic and less depended on exact object structures!
 *
 * All mods are primary use ONLY metadata. 
 * We don't make this shared to prevent recursion.
 * @warning USED AS COMPOSITION! metadata filed MUST be public! 
 *
 * @warning string keys that can be represented as numbers will be casted to a number! 
 * @warning empty string keys will return default/empty values!
 * @warning values with empty key will not be set!
 * @warning NAN, INF and other invalid key/values will not be set!
 * default values are : "", 0 and 0.0
 * default values are excluded from a dump
 */
	class Metadata : public Moveable {
		private:
    impl::MetaMap *map = nullptr; // dynamicly allocated! 
    int get_type(MetaKey& k) const; 
		public:
    Metadata() noexcept;
    Metadata(Metadata&&) noexcept;
    Metadata& operator=(Metadata&&) noexcept;
    ~Metadata();
    public:
    /** returns count of items in map */
    size_t size() const noexcept;

    /** check if map has this key */
    bool has_value(const MetaKey& key) const;
    
    /** check for type + has */
    bool is_string (const MetaKey& key) const;
    /** may attempt casting :) */
    bool is_number (const MetaKey& key) const;

    /** return string value or emplaces it (and casts). Returns empty string on OOM or empty! */
    std::string get_string(const MetaKey& key) const;
    int32_t     get_integer(const MetaKey& key) const;
    double      get_number(const MetaKey& key) const;
    
    /** removes value from the map */
    void unset(const MetaKey& key);
    
    /** sets value */ 
    void set(const MetaKey& key, const std::string& value);
    void set(const MetaKey& key, const int32_t value);
    void set(const MetaKey& key, const double value);

    Metadata copy(); // explicit copy
    void clear(); // clear all content  
    
    // iterate over map
    // overwrites key on return. Returns false and ampty key at end
    // pass empty string as first value 
    bool next(MetaKey& key);

    public: // extra
    /** save map content into stringstream (JSON-LIKE)
     * exclude_defaults should be set when saving to disk to save space.
     * but when sending over a net - must be setted to false!
     */
    void serialize(std::ostream&, bool exclude_defaults=true); 
    bool deserialize(std::istream&); // load map content from stringstream (JSON-LIKE)
	};

	/**
	 * To make difference between Game-related Objects and just a metadata, every Game-related
	 * must inherit from MetaClass, not Metadata!
	 */
  class MetaClass : public Shared<MetaClass> {
    public:
    Metadata metadata;
  };

};
