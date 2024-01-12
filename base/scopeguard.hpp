/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Guaranteed execution of costructor and destructor, for RAII
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
#include <functional>


namespace pb {

template <class F>
class ScopeGuard {
	private:
	std::function<F> call;
	public:
	ScopeGuard() = delete;
	ScopeGuard(const ScopeGuard &) = default;
	ScopeGuard(ScopeGuard &&) = default;
	ScopeGuard &operator=(const ScopeGuard &) = default;
	ScopeGuard &operator=(ScopeGuard &&) = default;
	public:
	void invoke() {
		if (call) call();
		dismiss();
	}
	void dismiss() {
		call = std::function<F>();
	}
	operator bool() {return call;}
	public:
	ScopeGuard(const std::function<F> &src) noexcept(std::is_nothrow_copy_constructible<F>::value) : call(src) {}
	~ScopeGuard() {invoke();}
};

template <>
class ScopeGuard<void> {
	public:
	ScopeGuard(const ScopeGuard &) = default;
	ScopeGuard(ScopeGuard &&) = default;
	ScopeGuard &operator=(const ScopeGuard &) = default;
	ScopeGuard &operator=(ScopeGuard &&) = default;
	public:
	void invoke() {}
	void dismiss() {}
	operator bool() {return false;}
	public:
	ScopeGuard() noexcept {}
};

};