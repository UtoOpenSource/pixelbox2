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

#include "base/doctest.h"
#include "engine/meta.hpp"
#include "base/tests/utils.hpp"
#include "string.hpp"

TEST_CASE("metadata stability") {
  pb::Metadata m;
  REQUIRE(m.size() == 0);
  m.set(1, 1);
  REQUIRE(m.size() == 1);
  REQUIRE(m.has_value(1) == true);
}

#include <map>

TEST_CASE("metadata values") {
  pb::Metadata m;
  std::map<std::string, std::string> map = {
    {"a", "b"},
    {"c", "d"},
    {"sus", "so much"}
  };

  for (auto &[k, v] : map) {
    m.set(pb::HString(k), v);
  }

  REQUIRE(m.size() == map.size());

  for (auto &[k, v] : map) {
    REQUIRE(m.has_value(pb::HString(k)) == true);
    REQUIRE(m.is_string(pb::HString(k)));
    REQUIRE(m.get_string(pb::HString(k)) == v);
  }

  pb::Metadata m2 = m.copy();

  for (auto &[k, v] : map) {
    REQUIRE(m2.has_value(pb::HString(k)) == true);
    REQUIRE(m2.is_string(pb::HString(k)));
    REQUIRE(m2.get_string(pb::HString(k)) == v);
  }

}

TEST_CASE("metadata Value cast") {
  pb::Metadata m;
  m.set("1", "1");
  REQUIRE(m.has_value(1) == true);
  REQUIRE(m.is_number(1) == true);
  std::string res = m.get_string(1);
  REQUIRE(pb::strtod(res.c_str(), nullptr) == 1);
}

TEST_CASE("metadata unvalid values") {
  pb::Metadata m;
  std::map<std::string, std::string> map = {
    {"", "a"},
    {"a", ""}
  };

  for (auto &[k, v] : map) {
    m.set(pb::HString(k), v);
  }

  REQUIRE(m.size() == 0);

  m.set(0, 0);
  REQUIRE(m.size() > 0);

  for (auto &[k, v] : map) {
    REQUIRE(m.has_value(pb::HString(k)) == false);
    REQUIRE(m.is_string(pb::HString(k)) == false);
    REQUIRE(m.get_string(pb::HString(k)) == std::string());
  }
  
  std::string v = m.get_string(0);
  REQUIRE(pb::strtod(v.c_str(), nullptr) == 0);

}

#include <sstream>
#include <iostream>

static void fin(pb::Metadata &a, pb::Metadata &b) {
  std::stringstream strm;
  a.serialize(strm);

  std::string old = strm.str();
  std::cout << "res : " << old << std::endl;
  REQUIRE(b.deserialize(strm) == true);

  strm.str(std::string()); strm.clear();
  b.serialize(strm);
  std::string curr = strm.str();
  std::cout << "r2  : " << curr << std::endl;

  REQUIRE(old == curr);

  a.clear(); b.clear();
}

TEST_CASE("metadata serialization") {
  pb::Metadata a, b;

  fin(a, b);
  a.set("ab", "ob");

  fin(a, b);

  std::string v = "a";
  for (int i = 'a'; i < 'm'; i++) {
    v[0] = i;
    a.set(i - 'a', v);
  }
  //BRK();
  fin(a, b);
}
