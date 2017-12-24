#pragma once
#include <assert.h>
#include "basic_types.h"

#define DEBUG_ONLY(x) x
#define Assert assert

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