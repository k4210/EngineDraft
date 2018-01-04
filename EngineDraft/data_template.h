#pragma once
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
		StructID struct_id_ = kWrongID;		// redundant
		PropertyID property_id_ = kWrongID;	// redundant

		uint32 byte_offset_ : 16;
		uint32 element_index_ : 8;
		uint32 nest_level_ : 7;
		uint32 is_key_ : 1;

		uint32 type_ : 5;					// redundant
		uint32 sub_property_offset_ : 5;	// redundant
		uint32 property_index_ : 14;		

		//8 bits left - flags, etc..

	public:
		uint32				GetDataOffset()			const { return byte_offset_; }
		uint32				GetElementIndex()		const { return element_index_; }
		uint32				GetNestLevel()			const { return nest_level_; }
		bool				IsKey()					const { return 0 != is_key_; }
		PropertyIndex		GetPropertyIndex()		const { return property_index_; }

		// only needed to refresh after layout was changed:
		StructID			GetStructID()			const { return struct_id_; }
		PropertyID			GetPropertyID()			const { return property_id_; }
		MemberFieldType		GetFieldType()			const { return static_cast<MemberFieldType>(type_); }
		SubPropertyOffset	GetSubPropertyOffset()	const { return sub_property_offset_; }

		Tag(StructID struct_id, PropertyID property_id, PropertyIndex property_index
			, SubPropertyOffset sub_property_offset, MemberFieldType type
			, uint32 byte_offset, uint32 nest_level, uint32 element_index, uint32 is_key)
			: struct_id_(struct_id)
			, property_id_(property_id)
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
		SaveNativeDefaultValues = 1 << 0,
	};

	struct DataTemplate
	{
		std::vector<Tag> tags_;
		std::vector<uint8> data_;
		StructID structure_id_ = kWrongID;

		void Save(const Object* obj, const Flag32<SaveFlags> flags);
		void Load(Object* obj) const;

		void CloneFrom(const DataTemplate& src);
		void Diff(const DataTemplate& lower_dt); //substract lower_dt
		void Merge(const DataTemplate& higher_dt); // Add higher_dt

		void RefreshAfterLayoutChanged(const StructID struct_id);

	private:
		bool SaveStructure(	const uint8* const src, const Structure& structure,										const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveArray(		const uint8* const src, const Structure& structure, const PropertyIndex property_index,	const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveVector(	const uint8* const src, const Structure& structure, const PropertyIndex property_index,	const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveMap(		const uint8* const src, const Structure& structure, const PropertyIndex property_index,	const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveValue(		const uint8* const src, const Structure& structure, const PropertyIndex property_index,	const uint32 nest_level, const Flag32<SaveFlags> flags
			, const uint32 element_index = 0, const bool is_key = false);

		uint32 LoadStructure(const Structure& structure, uint8* dst, uint32 tag_index) const;
		uint32 LoadValue(const Structure& structure, uint8* dst, uint32 tag_index) const;
		uint32 LoadArray(const Structure& structure, uint8* dst, const Tag array_tag, uint32 tag_index) const;
		uint32 LoadVector(const Structure& structure, uint8* dst, const Tag vector_tag, uint32 tag_index) const;
		uint32 LoadMap(const Structure& structure, uint8* dst, const Tag map_tag, uint32 tag_index) const;
	};
}