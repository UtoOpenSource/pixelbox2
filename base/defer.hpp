/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Guaranteed execution of destructor, for RAII
 * Copyright (C) 2023-2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include <type_traits>
#include <utility>
#include "base/base.hpp"

namespace pb {

namespace impl {

template <class F>
class ScopeGuard : Moveable {
  private:
  F f;
  bool invoke = true;
  public:
  explicit ScopeGuard(const F& ff) : f(ff) { }
  explicit ScopeGuard(F&& ff) : f(std::move(ff)) { }
  ~ScopeGuard() { if (invoke) f(); }
  ScopeGuard(ScopeGuard&& other) noexcept : f(std::move(other.f)) {
    other.invoke = false;
  }
  ScopeGuard(const ScopeGuard&&) = delete;
};

};

// defer operator :D yay
template <class F>
[[nodiscard]] auto defer(F&& f) noexcept {
    return impl::ScopeGuard<std::decay_t<F>>{std::forward<F>(f)};
}

};
