/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Binary data holder, packer and unpacker
 * Copyright (C) 2024 UtoECat
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

#include "base.hpp"
#include <cstddef>
#include <stdint.h>
#include <stddef.h>
#include "endian.hpp"

namespace pb {

	using Byte = uint8_t;

	inline size_t space_check(size_t pos, size_t count, size_t size) {
		if (pos + count > size) count = (pos <= size) ? size - pos : 0;
		return count;
	}

	/** swap bytes in place, don't goes forward, so you can read swapped bytes :) */
	inline void swapnbytes(Byte* data, size_t n) {
		for (size_t i = 0; i < n/2; i++) { // swap bytes
			Byte tmp = data[i];
			data[i] = data[n - i];
			data[n - i] = tmp;
		}
	}

	template <typename T>
	inline void swapvbytes(T& dest, size_t limit = 0) {
		swapnbytes((Byte*)&dest, sizeof(T) > limit ? limit : sizeof(T));
	}

	class BytesSized {
		protected:
		Byte* data = nullptr;
		size_t size = 0;
		public:
		BytesSized() = default;
		BytesSized(Byte* ptr, size_t sz) noexcept : data((ptr)), size(sz) {}
		BytesSized(const void* ptr, size_t sz) noexcept : BytesSized((Byte*)ptr, sz) {}; // remves const
		BytesSized(void* ptr, size_t sz) noexcept : BytesSized((Byte*)ptr, sz) {};
		BytesSized(const std::nullptr_t, size_t) noexcept : data(nullptr), size(0) {};
		public:
		inline Byte& operator[](size_t i) noexcept {return data[i];}
		inline Byte operator[](size_t i) const noexcept {return data[i];}
		inline Byte at(size_t i) const noexcept {if (i > size) return 0; else return data[i];} 
		Byte* begin() noexcept {return data;}
		Byte* end() noexcept {return data ? data + size : nullptr;}
		const Byte* begin() const noexcept {return data;}
		const Byte* end() const noexcept {return data ? data + size : nullptr;}

		operator bool() const noexcept {return begin() != end();} // check if not empty

		/** write count raw bytes from source, into this buffer at specified pos. return count of written bytes*/
		size_t awrite(const Byte* src, size_t pos, size_t count = 0) noexcept {
			if (!data || !src) return 0;
			count = space_check(pos, count, size); 

			for (size_t i = 0; i < count; i++) {
				data[pos + i] = src[i];
			}
			return count;
		}

		/** read coubnt from buffer at specified pos into dest, return count of readed bytes fills with zero all unreaded space in dest :)*/
		size_t aread(Byte* dst, size_t pos, size_t count = 0)  const noexcept {
			if (!data || !dst) return 0;
			size_t realcount = space_check(pos, count, size); 

			for (size_t i = 0; i < count; i++) {
				dst[i] = at(pos + i);
			}
			return realcount;
		}
	};

	class BytesCursor : public BytesSized {
		protected:
		size_t pos = 0;
		public:
		using BytesSized::BytesSized;
		inline void rewind() {pos = 0;}
		inline void skip(size_t i) {pos += i; if (pos > size) pos = size;}
		inline size_t seek(size_t i) {pos = i; if (pos > size) pos = size; return pos;}
		inline size_t tell() const {return pos;}
		inline size_t length() const {return size;}
		inline bool eof() {return pos >= size;}
	};

	// view on a vector/bytes of any object to perform transformations and operations
	// does not alloc/free any memory (on it's own at least...)
	class BytesView : public BytesCursor{
		public:
		using BytesCursor::BytesCursor;
		/** read N bytes into dest. return readed count. will fill non-readed with zeros */
		size_t readn(Byte* dest, size_t count) {
			size_t off = aread(dest, pos, count);
			skip(off);
			return off;
		}

		/** write N bytes from source. return written count */
		size_t writen(const Byte* dest, size_t count) {
			size_t off = awrite(dest, pos, count);
			skip(off);
			return off;
		}

		/** fill N bytes with specified byte and go on. */
		size_t filln(size_t n, Byte value=0) {
			if (pos + n < size) n = (pos <= size) ? size - pos : 0;
			for (size_t i = 0; i < n; i++) { // fill bytes
				data[pos + i] = value;
			}
			return n;
		}

		/** read value into dest. Use only on primitive types. Fills remain with zero, check return size! */
		template <typename T>
		inline size_t readv(T& dest, size_t limit = 0) {
			size_t off = readn(reinterpret_cast<Byte*>(&dest), sizeof(T) > limit ? limit : sizeof(T));
			return off;
		}

		/** write value from src. Use only on primitive types. May be incomplete, check return size */
		template <typename T>
		inline size_t writev(const T& src, size_t limit = 0) {
			size_t off = writen(reinterpret_cast<const Byte*>(&src), sizeof(T) > limit ? limit : sizeof(T)); 
			skip(off);
			return off;
		}

	  /** endian-aware variants*/
		template <typename T, bool is_little>
		inline size_t readve(T& dest, size_t limit = 0) {
			size_t off = readn(reinterpret_cast<Byte*>(&dest), sizeof(T) > limit ? limit : sizeof(T));
			if constexpr (pb::is_big_endian == is_little) if (off) swapvbytes(dest); // swap when not same
			return off;
		}

	  /** endian-aware variants. will swap event if not enough space! */
		template <typename T, bool is_little>
		inline size_t writeve(const T& src, size_t limit = 0) {
			Byte tmp[sizeof(T)] = {0};
			for (size_t i = 0; i < sizeof(T); i++) { // memcpy
				tmp[i] = reinterpret_cast<Byte*>(&src)[i];
			}
			if constexpr (pb::is_big_endian == is_little) swapnbytes(tmp, sizeof(T)); // swap when not same endianess

			size_t off = writen(tmp, sizeof(T) > limit ? limit : sizeof(T));  // write swapped data
			skip(off);
			return off;
		}
		
		/** read/write ints and so on */
		#define MAGIFUN(nbits) \
		inline bool readi##nbits(int##nbits##_t& v) {return readv(v);} \
		inline bool readu##nbits(uint##nbits##_t& v) {return readv(v);} \
		inline bool readi##nbits##le(int##nbits##_t& v) {return readve<decltype(v), true>(v);} \
		inline bool readu##nbits##le(uint##nbits##_t& v) {return readve<decltype(v), true>(v);} \
		inline bool readi##nbits##be(int##nbits##_t& v) {return readve<decltype(v), false>(v);} \
		inline bool readu##nbits##be(uint##nbits##_t& v) {return readve<decltype(v), false>(v);} \
		inline bool writei##nbits(int##nbits##_t& v) {return writev(v);} \
		inline bool writeu##nbits(uint##nbits##_t& v) {return writev(v);} \
		inline bool writei##nbits##le(int##nbits##_t& v) {return writeve<decltype(v), true>(v);} \
		inline bool writeu##nbits##le(uint##nbits##_t& v) {return writeve<decltype(v), true>(v);} \
		inline bool writei##nbits##be(int##nbits##_t& v) {return writeve<decltype(v), false>(v);} \
		inline bool writeu##nbits##be(uint##nbits##_t& v) {return writeve<decltype(v), false>(v);} 


		MAGIFUN(8);
		MAGIFUN(16);
		MAGIFUN(32);
		MAGIFUN(64);

		#undef MAGIFUN

	};

};