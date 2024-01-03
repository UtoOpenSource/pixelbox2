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
#include "base.hpp"
#include "spinlock.hpp"

namespace pb {

	// reference counter for composition.
	// you should call delete or destructor+memory_free when release() returns false!
	// uses std::recursive_mutex by default
	template <class Mutex = SpinLock>
	class RefCounter {
		private:
		uint32_t refcnt = 0;
		protected: 
		Mutex mutex;
		public:
		void aqquire() {
			auto ll = std::unique_lock<Mutex>(mutex);
			refcnt++;
		}
		bool release() {
			auto ll = std::unique_lock<Mutex>(mutex);
			if (!refcnt) throw "bad reference count!";
			refcnt--;
			if (!refcnt) return false;
			return true;
		}
	};

	// basic shared object
	class Shared : public Abstract {
		protected:
		RefCounter<std::mutex> refcnt;
		public:
		void aqquire() {
			refcnt.aqquire();
		}
		void release() {
			if (refcnt.release() == false) {
				delete this; // hehe
			}
		}
		virtual ~Shared() = 0;
	};

	template <typename T, typename U>
	concept derived_from = std::is_base_of_v<U, T>;

	// GIGABLOBA
	template <derived_from<Shared> T = Shared>
	class SharedPtr {
		public:
		// We can let us see internals to make things simpler, 
		// and since we alredy can fuckup Shared refgerence counter using
		// it's methods, it's not reeally makes any sence to hide theese methods... yep
		T* _ptr = nullptr;
		void _rel() {
			if (_ptr) _ptr->release();
			_ptr = nullptr;
		}
		void _aqq() {
			if (_ptr) _ptr->aqquire();
		}
		public:
		SharedPtr() = default;

		template < derived_from<Shared> U>
		SharedPtr(U* src) {
			_ptr = src;
			_aqq();
		}

		template < derived_from<Shared> U>
		SharedPtr& operator=(const SharedPtr<U>& src) {
			if ((void*)this != (void*)&src) {
				_rel();
				_ptr = dynamic_cast<T*>(src._ptr);
				_aqq();
			}
			return *this;
		}

		template < derived_from<Shared> U>
		SharedPtr(const SharedPtr<U>& src) {
			*this = src;
		}

		template < derived_from<Shared> U>
		SharedPtr(SharedPtr<U>&& src) {
			*this = src;
			src._rel();
		}

		T* get() const {return _ptr;}
		T* operator*() const {return _ptr;}
		T* operator->() const {return _ptr;}
		void take_care() {
			_ptr = nullptr;
		}
	};

};


