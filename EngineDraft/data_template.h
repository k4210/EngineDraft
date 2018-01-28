#pragma once
#include <iostream>
#include "utils.h"
#include "reflection.h"

namespace serialization
{
	using namespace reflection;

	constexpr uint32 kSuperStructPropertyID = 0xFFFFFFFE;
	constexpr uint32 kSuperStructPropertyIndex = 0x3FFF;
	class Tag
	{
		// Redundant fields are needed to restore data after layout was changed
		PropertyID property_id_ = kWrongID;	// redundant

		uint32 byte_offset_ : 16;
		uint32 element_index_ : 8;
		uint32 nest_level_ : 7;
		uint32 is_key_ : 1;

		uint32 type_ : 5;					// redundant
		uint32 sub_property_offset_ : 5;	// redundant
		uint32 property_index_ : 14;		
		uint32 flags_ : 8;
		//8 bits left - flags, etc..

	public:
		uint32				GetDataOffset()			const { return byte_offset_; }
		uint32				GetElementIndex()		const { return element_index_; }
		uint32				GetNestLevel()			const { return nest_level_; }
		bool				IsKey()					const { return 0 != is_key_; }
		PropertyIndex		GetPropertyIndex()		const { return property_index_; }

		// only needed to refresh after layout was changed:
		//StructID			GetStructID()			const { return struct_id_; }
		PropertyID			GetPropertyID()			const { return property_id_; }
		MemberFieldType		GetFieldType()			const { return static_cast<MemberFieldType>(type_); }
		SubPropertyOffset	GetSubPropertyOffset()	const { return sub_property_offset_; }

		Tag() = default;
		Tag(PropertyID property_id, PropertyIndex property_index
			, SubPropertyOffset sub_property_offset, MemberFieldType type
			, uint32 byte_offset, uint32 nest_level, uint32 element_index, uint32 is_key)
			: property_id_(property_id)
			, byte_offset_(byte_offset)
			, element_index_(element_index)
			, nest_level_(nest_level)
			, is_key_(is_key)
			, type_(static_cast<uint8>(type))
			, sub_property_offset_(sub_property_offset)
			, property_index_(property_index)
		{
			Assert((kSuperStructPropertyID == property_id_) == (kSuperStructPropertyIndex == property_index_));
			Assert(FitsInBits(byte_offset_, 16));
			Assert(FitsInBits(element_index_, 8));
			Assert(FitsInBits(nest_level_, 7));
			Assert(FitsInBits(is_key, 1));
			Assert(FitsInBits(sub_property_offset, 5));
			Assert(FitsInBits(property_index, 14));
		}
	};

	enum SaveFlags : uint32
	{
		None = 0,
		SkipNativeDefaultValues = 1 << 0,
	};

	__interface ObjectSolver
	{
		ObjectID IdFromObject(const Object* obj);
		Object* ObjectFromId(ObjectID id);
	};

	struct DataTemplate
	{
		std::vector<Tag> tags_;
		std::vector<uint8> data_;

		StructID GetStructID() const;
		uint32 TagNum() const { return tags_.size(); }
		DataTemplate Clone() const { return *this; }

		std::string ToString() const;
		void RefreshAfterLayoutChanged(const StructID struct_id);

		//Todo: add object solver
		void SaveFromObject(const Object* obj, const Flag32<SaveFlags> flags);
		void LoadIntoObject(Object* obj) const;

		static DataTemplate Merge(const DataTemplate& lower_dt, const DataTemplate& higher_dt); // 
		static DataTemplate Diff(const DataTemplate& higher_dt, const DataTemplate& lower_dt); //= higher_dt - lower_dt
	};

	std::istream& operator>> (std::istream& is, Tag& t);
	std::ostream& operator<< (std::ostream& os, const Tag& t);
	std::istream& operator>> (std::istream& is, DataTemplate& dt);
	std::ostream& operator<< (std::ostream& os, const DataTemplate& dt);
}