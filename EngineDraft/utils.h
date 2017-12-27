#pragma once
#include <assert.h>
#include "basic_types.h"

// Code format: max line length - 110, max function len - 

#define DEBUG_ONLY(x) x
#define Assert assert
#if !defined(UNREFERENCED_PARAMETER)
#define UNREFERENCED_PARAMETER(x) ((void)x)
#endif

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define UNIQUE_NAME(n) CONCATENATE(n, __COUNTER__)


template<typename E>
struct Flag32
{
	static_assert(std::is_enum<E>::value, "An Enum type is required");
	static_assert(sizeof(E) <= sizeof(uint32), "The Enum type is too big");

private:
	uint32 data = 0;

public:
	bool Get(E v) const
	{
		return 0 != (data & static_cast<uint32>(v));
	}

	void Add(E v)
	{
		data |= static_cast<uint32>(v);
	}

	template <class... T>
	void Add(E head, T... tail)
	{
		Add(head);
		Add(tail...);
	}

	Flag32 operator|(Flag32 other) const
	{
		Flag32 new_flag(*this);
		new_flag.data |= other.data;
		return new_flag;
	}

	void Remove(E v)
	{
		data &= ~static_cast<uint32>(v);
	}

	void Reset()
	{
		data = 0;
	}

	uint32 GetRawData() const
	{
		return data;
	}

public:
	Flag32() = default;

	Flag32(uint32 raw_data) : data(raw_data) {}

	template <class... T>
	Flag32(T... tail) : Flag32()
	{
		Add(tail...);
	}
};

template<typename E>
inline Flag32<E> operator|(E a, E b) { return Flag32<E>(a, b); }

//Type Detection templates
template<typename T> struct is_vector : public std::false_type {};
template<typename T, typename A> struct is_vector<std::vector<T, A>> : public std::true_type {};

template<typename T> struct is_map : public std::false_type {};
template<typename K, typename T, typename P, typename A> struct is_map<std::map<K, T, P, A>> 
	: public std::true_type {};

template<typename T> struct is_std_array : public std::false_type {};
template < class T, size_t N > struct is_std_array<std::array<T, N>> : public std::true_type {};

template<class T> struct array_element {};
template<class T, size_t N> struct array_element<T[N]> { using type = T; };
template<class T, size_t N> struct array_element<std::array<T, N>> { using type = T; };

template<class T> struct array_length {};
template<class T, size_t N> struct array_length<T[N]> { static constexpr size_t value = N; };
template<class T, size_t N> struct array_length<std::array<T, N>> { static constexpr size_t value = N; };

constexpr bool FitsInBits(const uint32 value, const uint32 num_bits)
{
	const uint32 mask = ~(0xFFFFFFFF << num_bits);
	return value == (mask & value);
}