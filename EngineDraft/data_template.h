#pragma once
#include "utils.h"
#include "reflection.h"

namespace serialization
{
	using namespace reflection;

	struct Tag
	{
		EDITOR_ONLY(PropertyID property_id_ = kWrongID);
		uint32 property_index_ : 8; // in structure
		uint32 byte_offset_ : 12; // in byte data
		uint32 nest_level_ : 4;
		uint32 element_index_ : 7;
		uint32 is_key_ : 1;

		Tag() : property_index_(0), byte_offset_(0), nest_level_(0), element_index_(0), is_key_(0) {}

		Tag(
#if EDITOR
			PropertyID property_id,
#endif			
			uint32 property_index, uint32 byte_offset, uint32 nest_level, uint32 element_index, uint32 is_key = 0)
			: 
#if EDITOR
			property_id_(property_id),
#endif					
			property_index_(property_index), byte_offset_(byte_offset)
			, nest_level_(nest_level), element_index_(element_index), is_key_(is_key)
		{
			Assert(FitsInBits(property_index, 8));
			Assert(FitsInBits(byte_offset, 12));
			Assert(FitsInBits(nest_level, 4));
			Assert(FitsInBits(element_index, 7));
			Assert(FitsInBits(is_key, 1));
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

#if EDITOR
		void RefreshAfterLayoutChanged() {}
#endif

	private:
		bool SaveStructure(const Structure& structure, const uint8* const src, const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveArray(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveVector(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveMap(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level, const Flag32<SaveFlags> flags);
		bool SaveValue(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level
			, const uint32 element_index, const bool is_key, const Flag32<SaveFlags> flags);

		uint32 LoadStructure(const Structure& structure, uint8* dst, uint32 tag_index) const;
		uint32 LoadValue(const Structure& structure, uint8* dst, uint32 tag_index) const;
		uint32 LoadArray(const Structure& structure, uint8* dst, const Tag array_tag, uint32 tag_index) const;
		uint32 LoadVector(const Structure& structure, uint8* dst, const Tag array_tag, uint32 tag_index) const;
		uint32 LoadMap(const Structure& structure, uint8* dst, const Tag array_tag, uint32 tag_index) const;
	};
}