#pragma once
#include "reflection.h"

namespace Serialization
{
	using namespace reflection;
	class DataTemplate
	{
		struct Tag 
		{
			uint32 property_index_	: 8; // in structure
			uint32 byte_offset_		: 12; // in byte data
			uint32 nest_level_		: 4;
			uint32 element_index_	: 7;
			uint32 is_key_			: 1;

			Tag() : property_index_(0), byte_offset_(0), nest_level_(0), element_index_(0){}

			Tag(uint32 property_index, uint32 byte_offset, uint32 nest_level, uint32 element_index, uint32 is_key = 0)
				: property_index_(property_index), byte_offset_(byte_offset)
				, nest_level_(nest_level), element_index_(element_index), is_key_(is_key)
			{
				Assert(FitsInBits(property_index, 8));
				Assert(FitsInBits(byte_offset, 12));
				Assert(FitsInBits(nest_level, 4));
				Assert(FitsInBits(element_index, 7));
				Assert(FitsInBits(is_key, 1));
			}
		};

		std::vector<Tag> tags_;
		std::vector<uint8> data_;
		StructID structure_id_ = kWrongID;
	
		bool SaveStructure(const Structure& structure, const uint8* const src, const uint32 nest_level);
		bool SaveArray(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level);
		bool SaveContainer(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level);
		bool SaveMap(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level);
		bool SaveElement(const uint8* src, const std::vector<Property>& properties, const uint32 property_index, const uint32 nest_level
			, const uint32 element_index = 0, const bool is_key = false);

	public:
		void Save(const Object* obj);

		void Load(Object*);
	};
}