#include "data_template.h"
#include <sstream>
#include <iomanip>

namespace
{
	using namespace reflection;
	using namespace serialization;
	template<typename M> static M& GetRef(uint8* data, const uint32 offset)
	{
		uint8* const ptr = data + offset;
		return *reinterpret_cast<std::remove_cv<M>::type*>(ptr);
	}

	template<typename M> static const M& GetConstRef(const uint8* const data, const uint32 offset)
	{
		const uint8* const ptr = data + offset;
		return *reinterpret_cast<const std::remove_cv<M>::type*>(ptr);
	}

	template<typename M> static void LoadSimpleValue(uint8* const dst, const uint8* const src, const uint32 src_offset)
	{
		GetRef<M>(dst, 0) = GetConstRef<M>(src, src_offset);
	}

	template<> void LoadSimpleValue<Object*>(uint8* const dst, const uint8* const src, const uint32 src_offset)
	{
		const StructID struct_id = GetConstRef<StructID>(src, src_offset);
		const auto& structure = Structure::GetStructure(struct_id);
		const ObjectID object_id = GetConstRef<ObjectID>(src, src_offset + sizeof(StructID));
		Object* obj = structure.obj_from_id(object_id);
		GetRef<Object*>(dst, 0) = obj;
	}

	template<> void LoadSimpleValue<std::string>(uint8* const dst, const uint8* const src, const uint32 src_offset)
	{
		const uint16 len = GetConstRef<uint16>(src, src_offset);
		const char* c_str = &GetConstRef<char>(src, src_offset + sizeof(uint16));
		GetRef<std::string>(dst, 0) = std::string(c_str, len);
	}
};

namespace save
{
	using namespace serialization;

	template<typename M> static bool SaveSimpleValue(std::vector<uint8>& dst, const uint8* const src, const M default_value
		, const Flag32<SaveFlags> flags)
	{
		if (default_value == GetConstRef<M>(src, 0) && flags[SaveFlags::SkipNativeDefaultValues])
			return false;

		const auto vec_size = dst.size();
		dst.resize(vec_size + sizeof(M));
		GetRef<M>(dst.data(), vec_size) = GetConstRef<M>(src, 0);
		return true;
	}

	static bool SaveString(std::vector<uint8>& dst, const uint8* const src, const Flag32<SaveFlags> flags)
	{
		const auto& str = GetConstRef<std::string>(src, 0);
		const uint32 len = str.size();
		if ((0 == len) && flags[SaveFlags::SkipNativeDefaultValues])
			return false;

		const uint32 dst_offset = dst.size();
		Assert(FitsInBits(len, 16));
		dst.resize(dst_offset + sizeof(uint16) + len * sizeof(char));
		GetRef<uint16>(dst.data(), dst_offset) = static_cast<uint16>(len);
		for (uint32 i = 0; i < len; i++)
		{
			GetRef<char>(dst.data(), dst_offset + sizeof(uint16) + i * sizeof(char)) = str[i];
		}
		return true;
	}

	static bool SaveObject(std::vector<uint8>& dst, const uint8* const src, const StructID property_struct_id, const Flag32<SaveFlags> flags)
	{
		ObjectID obj_id = kNullObjectID;
		const Object* obj = GetConstRef<Object*>(src, 0);
		if (obj)
		{
			const auto& structure = Structure::GetStructure(obj->GetReflectionStructureID());
			Assert(structure.RepresentsObjectClass());
			obj_id = structure.get_obj_id(obj);
		}

		if ((kNullObjectID == obj_id) && flags[SaveFlags::SkipNativeDefaultValues])
			return false;

		const uint32 dst_offset = dst.size();
		dst.resize(dst_offset + sizeof(StructID) + sizeof(ObjectID));
		GetRef<StructID>(dst.data(), dst_offset) = obj ? obj->GetReflectionStructureID() : property_struct_id;
		GetRef<ObjectID>(dst.data(), dst_offset + sizeof(StructID)) = obj_id;
		return true;
	}

	static bool SaveLength16(std::vector<uint8>& dst, uint32 len, const Flag32<SaveFlags> flags)
	{
		if ((0 == len) && flags[SaveFlags::SkipNativeDefaultValues])
			return false;

		const uint32 dst_offset = dst.size();
		dst.resize(dst_offset + sizeof(uint16));
		GetRef<uint16>(dst.data(), dst_offset) = static_cast<uint16>(len);
		return true;
	}

	bool SaveValue(const uint8* const src, DataTemplate& dst, const Structure& structure
		, const PropertyIndex property_index, const uint32 nest_level, const Flag32<SaveFlags> flags
		, const uint32 element_index = 0, const bool is_key = 0);
	bool SaveStructure(const uint8* const src, DataTemplate& dst
		, const Structure& structure, const uint32 nest_level, const Flag32<SaveFlags> flags)
	{
		bool was_saved = false;
		if (structure.super_id_ != kWrongID)
		{
			dst.tags_.emplace_back(Tag(structure.id_, kSuperStructPropertyID, kSuperStructPropertyIndex, 0, MemberFieldType::Struct
				, dst.data_.size(), nest_level, 0, 0));
			was_saved = SaveStructure(src, dst, Structure::GetStructure(structure.super_id_), nest_level + 1, flags);
			if (!was_saved)
			{
				dst.tags_.pop_back();
			}
		}

		for (uint32 property_index = 0; property_index < structure.GetNumberOfProperties();
			property_index = structure.NextPropertyIndexOnThisLevel(property_index))
		{
			const auto& property = structure.GetProperty(property_index);
			Assert(EPropertyUsage::Main == property.GetPropertyUsage());
			was_saved |= SaveValue(src + property.GetFieldOffset(), dst, structure, property_index, nest_level, flags);
		}
		return was_saved;
	}

	bool SaveArray(const uint8* const src, DataTemplate& dst, const Structure& structure
		, const PropertyIndex property_index, const uint32 nest_level, const Flag32<SaveFlags> flags)
	{
		const PropertyIndex element_property_index = structure.GetSubPropertyIndex(property_index, ESubType::Array_Element);
		const uint32 element_size = structure.GetNativeFieldSize(element_property_index);
		const uint32 array_size = structure.GetProperty(property_index).GetArraySize();
		bool was_saved = false;
		for (uint32 i = 0; i < array_size; i++)
		{
			was_saved |= SaveValue(src + i * element_size, dst, structure, element_property_index, nest_level, flags, i);
		}
		return was_saved;
	}

	bool SaveVector(const uint8* const src, DataTemplate& dst, const Structure& structure
		, const PropertyIndex property_index, const uint32 nest_level, const Flag32<SaveFlags> flags)
	{
		const auto& handler = structure.GetHandlerProperty(property_index).GetVectorHandler();
		const PropertyIndex element_property_index = structure.GetSubPropertyIndex(property_index, ESubType::Vector_Element);
		const uint32 num = handler.GetSize(src);
		bool was_saved = SaveLength16(dst.data_, num, flags);
		for (uint32 i = 0; i < num; i++)
		{
			was_saved |= SaveValue(handler.GetElement(src, i), dst, structure, element_property_index, nest_level, flags, i);
		}
		return was_saved;
	}

	bool SaveMap(const uint8* const src, DataTemplate& dst, const Structure& structure
		, const PropertyIndex property_index, const uint32 nest_level, const Flag32<SaveFlags> flags)
	{
		const auto& handler = structure.GetHandlerProperty(property_index).GetMapHandler();
		const PropertyIndex key_property_index = structure.GetSubPropertyIndex(property_index, ESubType::Key);
		const PropertyIndex value_property_index = structure.GetSubPropertyIndex(property_index, ESubType::Map_Value);
		const uint32 num = handler.GetSize(src);
		bool was_saved = SaveLength16(dst.data_, num, flags);
		const Flag32<SaveFlags> key_flags = Flag32<SaveFlags>::Remove(flags, SaveFlags::SkipNativeDefaultValues);
		for (uint32 i = 0; i < num; i++)
		{
			was_saved |= SaveValue(handler.GetKey(src, i), dst, structure, key_property_index, nest_level, key_flags, i, true);
			was_saved |= SaveValue(handler.GetValue(src, i), dst, structure, value_property_index, nest_level, flags, i, false);
		}
		return was_saved;
	}

	bool SaveValue(const uint8* const src, DataTemplate& dst, const Structure& structure
		, const PropertyIndex property_index, const uint32 nest_level, const Flag32<SaveFlags> flags
		, const uint32 element_index, const bool is_key)
	{
		const auto& property = structure.GetProperty(property_index);
		Assert(property.GetPropertyUsage() == EPropertyUsage::Main || property.GetPropertyUsage() == EPropertyUsage::SubType);
		const PropertyIndex main_property_idx = structure.GetMainPropertyIndex(property.GetPropertyID());
		Assert(main_property_idx != kWrongID);
		dst.tags_.emplace_back(Tag(structure.id_, property.GetPropertyID(), property_index, (property_index - main_property_idx),
			property.GetFieldType(), dst.data_.size(), nest_level, element_index, is_key ? 1 : 0));
		bool was_saved = false;
		switch (property.GetFieldType())
		{
		case MemberFieldType::Int8:		was_saved = SaveSimpleValue<int8	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::Int16:	was_saved = SaveSimpleValue<int16	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::Int32:	was_saved = SaveSimpleValue<int32	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::Int64:	was_saved = SaveSimpleValue<int64	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::UInt8:	was_saved = SaveSimpleValue<uint8	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::UInt16:	was_saved = SaveSimpleValue<uint16	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::UInt32:	was_saved = SaveSimpleValue<uint32	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::UInt64:	was_saved = SaveSimpleValue<uint64	>(dst.data_, src, 0, flags);						break;
		case MemberFieldType::Float:	was_saved = SaveSimpleValue<float	>(dst.data_, src, 0.0f, flags);						break;
		case MemberFieldType::Double:	was_saved = SaveSimpleValue<double	>(dst.data_, src, 0.0, flags);						break;
		case MemberFieldType::String:	was_saved = SaveString(dst.data_, src, flags);											break;
		case MemberFieldType::ObjectPtr:was_saved = SaveObject(dst.data_, src, property.GetOptionalStructID(), flags);			break;
		case MemberFieldType::Array:	was_saved = SaveArray(src, dst, structure, property_index, nest_level + 1, flags);		break;
		case MemberFieldType::Vector:	was_saved = SaveVector(src, dst, structure, property_index, nest_level + 1, flags);		break;
		case MemberFieldType::Map:		was_saved = SaveMap(src, dst, structure, property_index, nest_level + 1, flags);		break;
		case MemberFieldType::Struct:
			const Structure& inner_structure = Structure::GetStructure(property.GetOptionalStructID());
			Assert(inner_structure.RepresentNonObjectStructure());
			was_saved = SaveStructure(src, dst, inner_structure, nest_level + 1, flags);
			break;
		}
		if (!was_saved)
		{
			dst.tags_.pop_back();
		}
		return was_saved;
	}
}

void serialization::DataTemplate::Save(const Object * obj, const Flag32<SaveFlags> flags)
{
	Assert(nullptr != obj);
	Assert(kWrongID == structure_id_); //uninitialized
	structure_id_ = obj->GetReflectionStructureID();
	Assert(kWrongID != structure_id_);
	const auto& structure = Structure::GetStructure(structure_id_);
	Assert(structure.RepresentsObjectClass());
	Assert(tags_.empty() && data_.empty());
	save::SaveStructure(reinterpret_cast<const uint8*>(obj), *this, structure, 0, flags);
	Assert(tags_.empty() == data_.empty());
}

#pragma endregion

namespace load
{
	using namespace serialization;

	uint32 LoadValue(const DataTemplate& src, uint8* dst, const Structure& structure, uint32 tag_index);
	uint32 LoadArray(const DataTemplate& src, uint8* dst, const Structure& structure, const Tag tag, uint32 tag_index) 
	{
		const auto& property = structure.GetProperty(tag.GetPropertyIndex());
		const PropertyIndex element_property_index = structure.GetSubPropertyIndex(tag.GetPropertyIndex(), ESubType::Array_Element);
		const uint32 element_size = structure.GetNativeFieldSize(element_property_index);
		while (tag_index < src.tags_.size())
		{
			const Tag inner_tag = src.tags_[tag_index];
			const bool within_size = inner_tag.GetElementIndex() < property.GetArraySize();
			Assert(within_size);
			const bool expected_property_idx = inner_tag.GetPropertyIndex() == element_property_index;
			const bool expected_nest_idx = inner_tag.GetNestLevel() == tag.GetNestLevel() + 1;
			Assert(expected_property_idx == expected_nest_idx);
			if (!expected_property_idx || !expected_nest_idx || !within_size)
				break;

			tag_index = LoadValue(src, dst + inner_tag.GetElementIndex() * element_size, structure, tag_index);
		}
		return tag_index;
	}

	uint32 LoadVector(const DataTemplate& src, uint8* dst, const Structure& structure, const Tag tag, uint32 tag_index) 
	{
		const auto& handler = structure.GetHandlerProperty(tag.GetPropertyIndex()).GetVectorHandler();
		const PropertyIndex element_property_index = structure.GetSubPropertyIndex(tag.GetPropertyIndex(), ESubType::Vector_Element);
		const uint32 size = GetConstRef<uint16>(src.data_.data(), tag.GetDataOffset());
		handler.SetSize(dst, size);
		while (tag_index < src.tags_.size())
		{
			const Tag inner_tag = src.tags_[tag_index];
			const bool expected_property_idx = inner_tag.GetPropertyIndex() == element_property_index;
			const bool expected_nest_idx = inner_tag.GetNestLevel() == (tag.GetNestLevel() + 1);
			Assert(expected_property_idx == expected_nest_idx);
			if (!expected_property_idx || !expected_nest_idx)
				break;
			Assert(inner_tag.GetElementIndex() < size);
			tag_index = LoadValue(src, handler.GetElement(dst, inner_tag.GetElementIndex()), structure, tag_index);
		}
		return tag_index;
	}

	uint32 LoadMap(const DataTemplate& src, uint8* dst, const Structure& structure, const Tag tag, uint32 tag_index) 
	{
		const auto& handler = structure.GetHandlerProperty(tag.GetPropertyIndex()).GetMapHandler();
		const PropertyIndex key_property_index = structure.GetSubPropertyIndex(tag.GetPropertyIndex(), ESubType::Key);
		const PropertyIndex value_property_index = structure.GetSubPropertyIndex(tag.GetPropertyIndex(), ESubType::Map_Value);
		const uint32 map_size = GetConstRef<uint16>(src.data_.data(), tag.GetDataOffset()); //number of keys

		std::vector<uint8> temp_key_memory;
		for (uint32 idx = 0; idx < map_size; idx++)
		{
			//assume proper order
			Assert(tag_index < src.tags_.size());
			{
				//assume !SkipNativeDefaultValues, full key data
				const Tag key_tag = src.tags_[tag_index];
				Assert(key_tag.GetNestLevel() == (tag.GetNestLevel() + 1));
				Assert(key_tag.IsKey());
				Assert(key_tag.GetPropertyIndex() == key_property_index);
				Assert(key_tag.GetElementIndex() == idx);
				handler.InitializeKeyMemory(temp_key_memory);
				tag_index = LoadValue(src, temp_key_memory.data(), structure, tag_index);
			}

			uint8* value_ptr = handler.Add(dst, temp_key_memory);
			if (tag_index < src.tags_.size())
			{
				const Tag value_tag = src.tags_[tag_index];
				const bool expected_nest_lvl = value_tag.GetNestLevel() == (tag.GetNestLevel() + 1);
				const bool expected_property_idx = value_tag.GetPropertyIndex() == value_property_index;
				const bool proper_value = expected_nest_lvl && expected_property_idx && !value_tag.IsKey();
				Assert(proper_value == expected_nest_lvl && proper_value == expected_property_idx);
				if (proper_value)
				{
					Assert(value_tag.GetElementIndex() == idx);
					tag_index = LoadValue(src, value_ptr, structure, tag_index);
				}
			}
		}
		return tag_index;
	}

	uint32 LoadStructure(const DataTemplate& src, uint8* dst, const Structure& structure, uint32 tag_index) 
	{
		if (tag_index >= src.tags_.size())
			return tag_index;

		const Tag first_tag = src.tags_[tag_index];
		do
		{
			const Tag tag = src.tags_[tag_index];
			Assert(tag.GetNestLevel() <= first_tag.GetNestLevel());
			if (tag.GetNestLevel() != first_tag.GetNestLevel() || tag.GetElementIndex() != first_tag.GetElementIndex()
				|| tag.IsKey() != first_tag.IsKey())
				break;
			if (kSuperStructPropertyIndex == tag.GetPropertyIndex())
			{
				tag_index++;
				const auto* super_struct = structure.TryGetSuperStructure();
				Assert(nullptr != super_struct);
				tag_index = LoadStructure(src, dst, *super_struct, tag_index);
			}
			else
			{
				const auto& property = structure.GetProperty(tag.GetPropertyIndex());
				tag_index = LoadValue(src, dst + property.GetFieldOffset(), structure, tag_index);
			}
		} while (tag_index < src.tags_.size());
		return tag_index;
	}

	uint32 LoadValue(const DataTemplate& src, uint8* dst, const Structure& structure, uint32 tag_index)
	{
		const Tag tag = src.tags_[tag_index];
		const auto& property = structure.GetProperty(tag.GetPropertyIndex());
		Assert(property.GetPropertyUsage() != EPropertyUsage::Handler);
		tag_index++;
		switch (property.GetFieldType())
		{
		case MemberFieldType::Int8:		LoadSimpleValue<uint8>(dst, src.data_.data(), tag.GetDataOffset());			break;
		case MemberFieldType::Int16:	LoadSimpleValue<int16>(dst, src.data_.data(), tag.GetDataOffset());			break;
		case MemberFieldType::Int32:	LoadSimpleValue<int32>(dst, src.data_.data(), tag.GetDataOffset());			break;
		case MemberFieldType::Int64:	LoadSimpleValue<int64>(dst, src.data_.data(), tag.GetDataOffset());			break;
		case MemberFieldType::UInt8:	LoadSimpleValue<uint8>(dst, src.data_.data(), tag.GetDataOffset());			break;
		case MemberFieldType::UInt16:	LoadSimpleValue<uint16>(dst, src.data_.data(), tag.GetDataOffset());		break;
		case MemberFieldType::UInt32:	LoadSimpleValue<uint32>(dst, src.data_.data(), tag.GetDataOffset());		break;
		case MemberFieldType::UInt64:	LoadSimpleValue<uint64>(dst, src.data_.data(), tag.GetDataOffset());		break;
		case MemberFieldType::Float:	LoadSimpleValue<float>(dst, src.data_.data(), tag.GetDataOffset());			break;
		case MemberFieldType::Double:	LoadSimpleValue<double>(dst, src.data_.data(), tag.GetDataOffset());		break;
		case MemberFieldType::String:	LoadSimpleValue<std::string>(dst, src.data_.data(), tag.GetDataOffset());	break;
		case MemberFieldType::ObjectPtr:LoadSimpleValue<Object*>(dst, src.data_.data(), tag.GetDataOffset());		break;
		case MemberFieldType::Array:	tag_index = LoadArray(src, dst, structure, tag, tag_index);					break;
		case MemberFieldType::Vector:	tag_index = LoadVector(src, dst, structure, tag, tag_index);				break;
		case MemberFieldType::Map:		tag_index = LoadMap(src, dst, structure, tag, tag_index);					break;
		case MemberFieldType::Struct:	tag_index = LoadStructure(src, dst,
			Structure::GetStructure(property.GetOptionalStructID()), tag_index);									break;
		}
		return tag_index;
	}
}
void serialization::DataTemplate::Load(Object* obj) const
{
	Assert(nullptr != obj);
	Assert(kWrongID != structure_id_);
	const auto& structure = Structure::GetStructure(structure_id_);
	Assert(Structure::GetStructure(obj->GetReflectionStructureID()).IsBasedOn(structure_id_));
	Assert(structure.RepresentsObjectClass());
	load::LoadStructure(*this, reinterpret_cast<uint8*>(obj), structure, 0);
}

#pragma endregion

namespace layout_changed
{
	using namespace serialization;

	uint32 LoadValue(DataTemplate& dst, const Structure& structure, const DataTemplate& src, uint32 tag_index);
	uint32 LoadStructure(DataTemplate& dst, const Structure& structure, const DataTemplate& src, uint32 tag_index);

	template<typename M> static void LoadSimpleValue(DataTemplate& dst, const uint8* const src, const uint32 src_offset)
	{
		const auto vec_size = dst.data_.size();
		dst.data_.resize(vec_size + sizeof(M));
		GetRef<M>(dst.data_.data(), vec_size) = GetConstRef<M>(src, src_offset);
	}

	template<> void LoadSimpleValue<Object*>(DataTemplate& dst, const uint8* const src, const uint32 src_offset)
	{
		const uint32 dst_offset = dst.data_.size();
		dst.data_.resize(dst_offset + sizeof(StructID) + sizeof(ObjectID));

		GetRef<StructID>(dst.data_.data(), dst_offset) = GetConstRef<StructID>(src, src_offset);
		GetRef<ObjectID>(dst.data_.data(), dst_offset + sizeof(StructID)) = GetConstRef<ObjectID>(src, src_offset + sizeof(StructID));
	}

	template<> void LoadSimpleValue<std::string>(DataTemplate& dst, const uint8* const src, const uint32 src_offset)
	{
		const uint16 len = GetConstRef<uint16>(src, src_offset);
		const char* c_str = &GetConstRef<char>(src, src_offset + sizeof(uint16));
		const uint32 dst_offset = dst.data_.size();
		dst.data_.resize(dst_offset + sizeof(uint16) + len * sizeof(char));
		GetRef<uint16>(dst.data_.data(), dst_offset) = static_cast<uint16>(len);
		for (uint32 i = 0; i < len; i++)
		{
			GetRef<char>(dst.data_.data(), dst_offset + sizeof(uint16) + i * sizeof(char)) = c_str[i];
		}
	}

	uint32 LoadArray(DataTemplate& dst, const Structure& structure, const DataTemplate& src, const PropertyIndex main_property_index, const Tag tag, uint32 tag_index)
	{
		const auto& property = structure.GetProperty(main_property_index + tag.GetSubPropertyOffset());
		const PropertyIndex element_property_index = structure.GetSubPropertyIndex(main_property_index + tag.GetSubPropertyOffset(), ESubType::Array_Element);
		const SubPropertyOffset element_property_offset = element_property_index - main_property_index;

		while (tag_index < src.TagNum())
		{
			const Tag inner_tag = src.tags_[tag_index];
			if (inner_tag.GetNestLevel() <= tag.GetNestLevel())
				break;
			if (inner_tag.GetNestLevel() > (tag.GetNestLevel() + 1))
			{
				tag_index++;
				continue;
			}

			const bool expected_property = (inner_tag.GetPropertyID() == property.GetPropertyID()) &&
				(element_property_offset == inner_tag.GetSubPropertyOffset());
			if (!expected_property)
				break;
			const bool within_size = inner_tag.GetElementIndex() < property.GetArraySize();
			if (!within_size)
			{
				tag_index++;
				continue;
			}
			tag_index = LoadValue(dst, structure, src, tag_index);
		}
		return tag_index;
	}

	uint32 LoadVector(DataTemplate& dst, const Structure& structure, const DataTemplate& src, const PropertyIndex main_property_index, const Tag tag, uint32 tag_index)
	{
		const auto& property = structure.GetProperty(main_property_index + tag.GetSubPropertyOffset());
		const PropertyIndex element_property_index = structure.GetSubPropertyIndex(main_property_index + tag.GetSubPropertyOffset(), ESubType::Vector_Element);
		const SubPropertyOffset element_property_offset = element_property_index - main_property_index;
		const uint32 size = GetConstRef<uint16>(src.data_.data(), tag.GetDataOffset());
		save::SaveLength16(dst.data_, size, Flag32<SaveFlags>());

		while (tag_index < src.TagNum())
		{
			const Tag inner_tag = src.tags_[tag_index];
			if (inner_tag.GetNestLevel() <= tag.GetNestLevel())
				break;
			if (inner_tag.GetNestLevel() >(tag.GetNestLevel() + 1))
			{
				tag_index++;
				continue;
			}

			const bool expected_property = (inner_tag.GetPropertyID() == property.GetPropertyID()) &&
				(element_property_offset == inner_tag.GetSubPropertyOffset());
			if (!expected_property)
				break;

			const bool within_size = inner_tag.GetElementIndex() < size;
			if (!within_size)
			{
				tag_index++;
				continue;
			}

			tag_index = LoadValue(dst, structure, src, tag_index);
		}
		return tag_index;
	}

	uint32 LoadMap(DataTemplate& dst, const Structure& structure, const DataTemplate& src, const PropertyIndex main_property_index, const Tag tag, uint32 tag_index)
	{
		const PropertyIndex map_property_index = main_property_index + tag.GetSubPropertyOffset();
		const auto& property = structure.GetProperty(map_property_index);
		const PropertyIndex key_property_index = structure.GetSubPropertyIndex(map_property_index, ESubType::Key);
		const SubPropertyOffset key_property_offset = key_property_index - main_property_index;
		const PropertyIndex value_property_index = structure.GetSubPropertyIndex(map_property_index, ESubType::Map_Value);
		const SubPropertyOffset value_property_offset = value_property_index - main_property_index;
		const uint32 map_size = GetConstRef<uint16>(src.data_.data(), tag.GetDataOffset()); //number of keys
		save::SaveLength16(dst.data_, map_size, Flag32<SaveFlags>());
		while (tag_index < src.TagNum())
		{
			const Tag inner_tag = src.tags_[tag_index];
			if (inner_tag.GetNestLevel() <= tag.GetNestLevel())
				break;

			if (inner_tag.GetNestLevel() >(tag.GetNestLevel() + 1))
			{
				tag_index++;
				continue;
			}

			const bool expected_main_property = inner_tag.GetPropertyID() == property.GetPropertyID();
			if (!expected_main_property)
				break;

			const bool expected_property_offset = inner_tag.GetSubPropertyOffset() == (inner_tag.IsKey() ? key_property_offset : value_property_offset);
			if (!expected_property_offset)
				break;

			const bool within_size = inner_tag.GetElementIndex() < map_size;
			if (!within_size)
			{
				tag_index++;
				continue;
			}
			
			tag_index = LoadValue(dst, structure, src, tag_index);
		}
		return tag_index;
	}

	uint32 LoadValue(DataTemplate& dst, const Structure& structure, const DataTemplate& src, uint32 tag_index)
	{
		const Tag tag = src.tags_[tag_index];
		tag_index++;
		const PropertyIndex main_property_idx = structure.GetMainPropertyIndex(tag.GetPropertyID());
		if (kWrongID == main_property_idx) {
			ErrorStream() << "layout_changed::LoadValue cannot find property by ID\n";
			return tag_index;
		}
		const PropertyIndex property_idx = main_property_idx + tag.GetSubPropertyOffset();
		if (property_idx >= structure.GetNumberOfProperties()) {
			ErrorStream() << "layout_changed::LoadValue property index out of scope\n";
			return tag_index;
		}
		const auto& property = structure.GetProperty(property_idx);
		if (EPropertyUsage::Handler == property.GetPropertyUsage()) {
			ErrorStream() << "layout_changed::LoadValue unexpected handler property\n";
			return tag_index;
		}
		if (tag.GetFieldType() != property.GetFieldType()) {
			ErrorStream() << "layout_changed::LoadValue " << property.GetName() << " different type\n";
			return tag_index;
		}
		const uint32 saved_data = dst.data_.size();
		dst.tags_.emplace_back(Tag(structure.id_, property.GetPropertyID(), property_idx, (property_idx - main_property_idx),
			property.GetFieldType(), saved_data, tag.GetNestLevel(), tag.GetElementIndex(), tag.IsKey() ? 1 : 0));
		switch (property.GetFieldType())
		{
			case MemberFieldType::Int8:		LoadSimpleValue<uint8>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::Int16:	LoadSimpleValue<int16>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::Int32:	LoadSimpleValue<int32>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::Int64:	LoadSimpleValue<int64>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::UInt8:	LoadSimpleValue<uint8>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::UInt16:	LoadSimpleValue<uint16>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::UInt32:	LoadSimpleValue<uint32>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::UInt64:	LoadSimpleValue<uint64>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::Float:	LoadSimpleValue<float>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::Double:	LoadSimpleValue<double>		(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::String:	LoadSimpleValue<std::string>(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::ObjectPtr:LoadSimpleValue<Object*>	(dst, src.data_.data(), tag.GetDataOffset());	break;
			case MemberFieldType::Array:	tag_index = LoadArray		(dst, structure, src, main_property_idx, tag, tag_index);	break;
			case MemberFieldType::Vector:	tag_index = LoadVector		(dst, structure, src, main_property_idx, tag, tag_index);	break;
			case MemberFieldType::Map:		tag_index = LoadMap			(dst, structure, src, main_property_idx, tag, tag_index);	break;
			case MemberFieldType::Struct:	tag_index = LoadStructure(dst,
				Structure::GetStructure(property.GetOptionalStructID()), src, tag_index);	break;
		}
		if (saved_data == dst.data_.size())
		{
			dst.tags_.pop_back();
		}
		return tag_index;
	}

	uint32 LoadStructure(DataTemplate& dst, const Structure& structure, const DataTemplate& src, uint32 tag_index)
	{
		if (tag_index < src.TagNum())
		{
			const Tag first_tag = src.tags_[tag_index];
			while (tag_index < src.TagNum())
			{
				const Tag tag = src.tags_[tag_index];
				if (tag.GetNestLevel() == first_tag.GetNestLevel() && tag.GetElementIndex() == first_tag.GetElementIndex() && tag.IsKey() == first_tag.IsKey()
					&& structure.id_ == tag.GetStructID())
				{
					if (kSuperStructPropertyID == tag.GetPropertyID())
					{
						tag_index++;
						const Structure* super_struct = structure.TryGetSuperStructure();
						if (super_struct)
						{
							const uint32 saved_data = dst.data_.size();
							dst.tags_.emplace_back(Tag(structure.id_, kSuperStructPropertyID, kSuperStructPropertyIndex, 0, MemberFieldType::Struct
								, saved_data, tag.GetNestLevel(), 0, 0));
							tag_index = LoadStructure(dst, *super_struct, src, tag_index);
							if (saved_data == dst.data_.size())
							{
								dst.tags_.pop_back();
							}
						}
						else
						{
							ErrorStream() << "Error layout_changed::LoadStructure - unknown super struct \n";
						}
					}
					else
					{
						tag_index = LoadValue(dst, structure, src, tag_index);
					}
				}
				else if (tag.GetNestLevel() < first_tag.GetNestLevel())
				{
					break;
				}
				else
				{
					if (structure.id_ != tag.GetStructID())
					{
						const Structure* actual_struct = Structure::TryGetStructure(tag.GetStructID());
						ErrorStream() << "Error layout_changed::LoadStructure - expected structure: '" << structure.GetName() << "' actual structure: '"
							<< (actual_struct ? actual_struct->GetName().c_str() : "unknown") << '\n';
					}
					tag_index++;
				}
			}
		}
		return tag_index;
	}
};

void serialization::DataTemplate::RefreshAfterLayoutChanged(const StructID struct_id)
{
	Assert(kWrongID != struct_id);
	structure_id_ = kWrongID;

	const auto& structure = Structure::GetStructure(struct_id);
	Assert(structure.RepresentsObjectClass() && structure.Validate());

	DataTemplate refreshed_dt;
	refreshed_dt.structure_id_ = struct_id;

	layout_changed::LoadStructure(refreshed_dt, structure, *this, 0);

	*this = refreshed_dt;
}

DataTemplate serialization::DataTemplate::Clone() const
{
	Assert(!tags_.empty() && !data_.empty());
	Assert(kWrongID != structure_id_);
	return *this;
}

namespace dt_operation
{
	using namespace serialization;

	bool TagsEqual(const Tag a, const Tag b)
	{
		Assert(a.GetStructID() == b.GetStructID());

		const bool are_equal =
			a.GetPropertyIndex() == b.GetPropertyIndex() &&
			a.GetElementIndex() == b.GetElementIndex() &&
			a.IsKey() == b.IsKey();
		if (are_equal)
		{
			Assert(a.GetFieldType() == b.GetFieldType());
			Assert(a.GetPropertyID() == b.GetPropertyID());
			Assert(a.GetSubPropertyOffset() == b.GetSubPropertyOffset());
		}
		return are_equal;
	}

	//returns if a goes before b
	bool IsTagFirst(const Tag a, const Tag b)
	{
		Assert(a.GetStructID() == b.GetStructID());

		if (a.GetPropertyIndex() == b.GetPropertyIndex())
		{
			if (a.GetElementIndex() < b.GetElementIndex())
				return true;
			if ((a.GetElementIndex() == b.GetElementIndex()) && a.IsKey() && !b.IsKey())
				return true;

			return false;
		}
		if (a.GetPropertyIndex() == kSuperStructPropertyIndex)
			return true;
		if (b.GetPropertyIndex() == kSuperStructPropertyIndex)
			return false;
		return a.GetPropertyIndex() < b.GetPropertyIndex();
	}

	void CopySingleTagUnchecked(DataTemplate& dst, const DataTemplate& src, const uint32 tag_index, const uint32 nest_lvl_offset)
	{
		const Tag tag = src.tags_[tag_index];
		dst.tags_.emplace_back(Tag(tag.GetStructID(), tag.GetPropertyID(), tag.GetPropertyIndex(), tag.GetSubPropertyOffset(),
			tag.GetFieldType(), dst.data_.size(), tag.GetNestLevel() + nest_lvl_offset, tag.GetElementIndex(), tag.IsKey() ? 1 : 0));

		const uint32 src_data_chunk_end = (src.TagNum() > (tag_index + 1)) ? src.tags_[tag_index + 1].GetDataOffset() : src.data_.size();
		std::copy(src.data_.begin() + tag.GetDataOffset(), src.data_.begin() + src_data_chunk_end, std::back_inserter(dst.data_));
	}

	bool CopyNestedTags(DataTemplate& dst, const DataTemplate& src, uint32& tag_index, const uint32 nest_lvl_offset)
	{
		if (src.TagNum() <= tag_index)
			return false;
		const uint32 min_nest_level = src.tags_[tag_index].GetNestLevel();
		do
		{
			CopySingleTagUnchecked(dst, src, tag_index, nest_lvl_offset);
			tag_index++;
		} while ((tag_index < src.TagNum()) && (src.tags_[tag_index].GetNestLevel() > min_nest_level));
		return true;
	}

	void SkipNestedTags(const DataTemplate& src, uint32& tag_index)
	{
		if (src.TagNum() <= tag_index)
			return;
		const uint32 min_nest_level = src.tags_[tag_index].GetNestLevel();
		do {
			tag_index++;
		} while ((tag_index < src.TagNum()) && (src.tags_[tag_index].GetNestLevel() > min_nest_level));
	}

	enum class EDataTemplateOperation
	{
		Merge,
		Diff
	};

	struct ProcessContext
	{
		DataTemplate& dst;
		const DataTemplate& lower_dt;
		const DataTemplate& higher_dt;
		uint32& lower_tag_index; 
		uint32& higher_tag_index;
		const uint32 nest_lvl_offset;
		const EDataTemplateOperation op;

		ProcessContext(DataTemplate& in_dst, const DataTemplate& in_lower_dt, const DataTemplate& in_higher_dt, uint32& in_lower_tag_index, uint32& in_higher_tag_index
			, const uint32 in_nest_lvl_offset, const EDataTemplateOperation in_op)
			: dst(in_dst)
			, lower_dt(in_lower_dt)
			, higher_dt(in_higher_dt)
			, lower_tag_index(in_lower_tag_index)
			, higher_tag_index(in_higher_tag_index)
			, nest_lvl_offset(in_nest_lvl_offset)
			, op(in_op)
		{}

		bool AnyReachedEnd() const
		{
			return lower_tag_index >= lower_dt.TagNum() || higher_tag_index >= higher_dt.TagNum();
		}

		Tag GetHighTag() const
		{
			return higher_dt.tags_[higher_tag_index];
		}

		Tag GetLowTag() const
		{
			return lower_dt.tags_[lower_tag_index];
		}
	};

	bool ProcessInner(ProcessContext& ctx, const Structure& structure, const uint32 max_size);

	template<typename M> bool AreSimpleValuesEqual(const uint8* const a, const uint8* const b)
	{
		return GetConstRef<M>(a, 0) == GetConstRef<M>(b, 0);
	}

	template<> bool AreSimpleValuesEqual<Object*>(const uint8* const a, const uint8* const b)
	{
		const auto obj_a = GetConstRef<ObjectID>(a, sizeof(StructID));
		const auto obj_b = GetConstRef<ObjectID>(b, sizeof(StructID));
		return obj_a == obj_b;
	}

	template<> bool AreSimpleValuesEqual<std::string>(const uint8* const a, const uint8* const b)
	{
		if (GetConstRef<uint16>(a, 0) != GetConstRef<uint16>(b, 0))
			return false;
		const uint16 len = GetConstRef<uint16>(a, 0);
		for (uint32 i = 0; i < len; i++)
		{
			if (GetConstRef<char>(a, sizeof(uint16) + i * sizeof(char)) != GetConstRef<char>(b, sizeof(uint16) + i * sizeof(char)))
				return false;
		}
		return true;
	}

	template<typename M> bool ProcessSimpleValue(ProcessContext& ctx, const uint32 low_offser, const uint32 high_offset)
	{
		const bool save_high = (EDataTemplateOperation::Merge == ctx.op)
			|| ((EDataTemplateOperation::Diff == ctx.op) 
				&& !AreSimpleValuesEqual<M>(ctx.higher_dt.data_.data() + high_offset, ctx.lower_dt.data_.data() + low_offser));
		if (save_high)
		{
			layout_changed::LoadSimpleValue<M>(ctx.dst, ctx.higher_dt.data_.data(), high_offset);
		}
		return save_high;
	}

	bool ProcessValue(ProcessContext& ctx, const Structure& structure)
	{
		if (ctx.AnyReachedEnd())
			return false;
		const Tag high_tag = ctx.GetHighTag();
		const Tag low_tag = ctx.GetLowTag();
		ctx.higher_tag_index++;
		ctx.lower_tag_index++;
		Assert(TagsEqual(low_tag, high_tag));
		const auto& property = structure.GetProperty(high_tag.GetPropertyIndex());
		ctx.dst.tags_.emplace_back(Tag(structure.id_, property.GetPropertyID(), high_tag.GetPropertyIndex(), high_tag.GetSubPropertyOffset(),
			property.GetFieldType(), ctx.dst.data_.size(), high_tag.GetNestLevel(), high_tag.GetElementIndex(), high_tag.IsKey() ? 1 : 0));
		bool was_saved = false;
		switch (property.GetFieldType())
		{
			case MemberFieldType::Int8:		was_saved = ProcessSimpleValue<uint8>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::Int16:	was_saved = ProcessSimpleValue<int16>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::Int32:	was_saved = ProcessSimpleValue<int32>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::Int64:	was_saved = ProcessSimpleValue<int64>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::UInt8:	was_saved = ProcessSimpleValue<uint8>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::UInt16:	was_saved = ProcessSimpleValue<uint16>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::UInt32:	was_saved = ProcessSimpleValue<uint32>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::UInt64:	was_saved = ProcessSimpleValue<uint64>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::Float:	was_saved = ProcessSimpleValue<float>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::Double:	was_saved = ProcessSimpleValue<double>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::String:	was_saved = ProcessSimpleValue<std::string>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::ObjectPtr:was_saved = ProcessSimpleValue<Object*>(ctx, low_tag.GetDataOffset(), high_tag.GetDataOffset());	break;
			case MemberFieldType::Array:	was_saved = ProcessInner(ctx, structure, property.GetArraySize()); break;
			case MemberFieldType::Vector:
			case MemberFieldType::Map:
			{
				const uint32 size = GetConstRef<uint16>(ctx.higher_dt.data_.data(), high_tag.GetDataOffset());
				const uint32 data_size = ctx.dst.data_.size();
				save::SaveLength16(ctx.dst.data_, size, SaveFlags::None);
				was_saved = ProcessInner(ctx, structure, size);
				if (!was_saved)
				{
					ctx.dst.data_.resize(data_size);
				}
				break;
			} 
			case MemberFieldType::Struct:	was_saved = ProcessInner(ctx, Structure::GetStructure(property.GetOptionalStructID()), 1);	break;
		}
		if (!was_saved)
		{
			ctx.dst.tags_.pop_back();
		}
		return was_saved;
	}

	bool ProcessInner(ProcessContext& ctx, const Structure& structure, const uint32 max_size)
	{
		if (ctx.AnyReachedEnd())
			return false;
		Assert(ctx.GetLowTag().GetStructID() == ctx.GetHighTag().GetStructID());
		Assert(ctx.GetLowTag().GetStructID() == structure.id_);
		const uint32 max_nest_lvl = std::max(ctx.GetLowTag().GetNestLevel() + ctx.nest_lvl_offset, ctx.GetHighTag().GetNestLevel());
		bool was_saved = false;
		do
		{
			const Tag higher_tag = ctx.higher_dt.tags_[ctx.higher_tag_index];
			const Tag lower_tag = ctx.lower_dt.tags_[ctx.lower_tag_index];
			const bool lower_in_struct = (lower_tag.GetNestLevel() + ctx.nest_lvl_offset == max_nest_lvl) && (lower_tag.GetStructID() == structure.id_);
			const bool higher_in_struct = (higher_tag.GetNestLevel() == (max_nest_lvl)) && (higher_tag.GetStructID() == structure.id_);
			if (lower_in_struct && higher_in_struct && TagsEqual(lower_tag, higher_tag))
			{
				if (kSuperStructPropertyID == higher_tag.GetPropertyID())
				{
					ctx.higher_tag_index++;
					ctx.lower_tag_index++;
					const Structure* super_struct = structure.TryGetSuperStructure();
					Assert(super_struct);
					ctx.dst.tags_.emplace_back(Tag(structure.id_, kSuperStructPropertyID, kSuperStructPropertyIndex, 0, MemberFieldType::Struct
						, ctx.dst.data_.size(), higher_tag.GetNestLevel(), 0, 0));
					const bool was_super_struct_safe = ProcessInner(ctx, *super_struct, 1);
					if (!was_super_struct_safe)
					{
						ctx.dst.tags_.pop_back();
					}
					was_saved |= was_super_struct_safe;
				}
				else
				{
					was_saved |= ProcessValue(ctx, structure);
				}
			}
			else if (lower_in_struct && (!higher_in_struct || IsTagFirst(lower_tag, higher_tag)))
			{
				if ((EDataTemplateOperation::Diff == ctx.op) || (lower_tag.GetElementIndex() >= max_size))
				{
					SkipNestedTags(ctx.lower_dt, ctx.lower_tag_index);
				}
				else
				{
					was_saved |= CopyNestedTags(ctx.dst, ctx.lower_dt, ctx.lower_tag_index, ctx.nest_lvl_offset);
				}
			}
			else if(higher_in_struct && (!lower_in_struct || IsTagFirst(higher_tag, lower_tag)))
			{
				was_saved |= CopyNestedTags(ctx.dst, ctx.higher_dt, ctx.higher_tag_index, 0);
			}
			else
			{
				Assert(!higher_in_struct && !lower_in_struct);
				break;
			}
		} while (!ctx.AnyReachedEnd());
		return was_saved;
	}

	DataTemplate Process(const DataTemplate& lower_dt, const DataTemplate& higher_dt, const EDataTemplateOperation op)
	{
		Assert(kWrongID != lower_dt.structure_id_);
		Assert(kWrongID != higher_dt.structure_id_);
		Assert(Structure::GetStructure(higher_dt.structure_id_).IsBasedOn(lower_dt.structure_id_));

		DataTemplate dst;
		dst.structure_id_ = higher_dt.structure_id_;
		dst.tags_.reserve(std::max(higher_dt.TagNum(), lower_dt.TagNum()));
		dst.data_.reserve(std::max(higher_dt.data_.size(), lower_dt.data_.size()));

		uint32 lower_tag_index = 0, higher_tag_index = 0;
		uint32 nest_lvl_offset = 0;
		if (lower_tag_index < lower_dt.TagNum() && higher_tag_index < higher_dt.TagNum())
		{
			const StructID lower_struct_id = lower_dt.structure_id_;
			StructID higher_struct_id = higher_dt.structure_id_;
			while (higher_struct_id != lower_struct_id)
			{
				Assert(kWrongID != higher_struct_id);
				const Tag super_struct_high = higher_dt.tags_[higher_tag_index];
				Assert(super_struct_high.GetPropertyID() == kSuperStructPropertyID);
				Assert(super_struct_high.GetPropertyIndex() == kSuperStructPropertyIndex);
				Assert(super_struct_high.GetStructID() == higher_struct_id);
				Assert(super_struct_high.GetNestLevel() == nest_lvl_offset);
				higher_struct_id = Structure::GetStructure(higher_struct_id).super_id_;

				dst.tags_.emplace_back(Tag(higher_struct_id, kSuperStructPropertyID, kSuperStructPropertyIndex, 0, MemberFieldType::Struct
					, dst.data_.size(), nest_lvl_offset, 0, 0));

				higher_tag_index++;
				nest_lvl_offset++;
			}
			ProcessContext ctx(dst, lower_dt, higher_dt, lower_tag_index, higher_tag_index, nest_lvl_offset, op);
			ProcessInner(ctx, Structure::GetStructure(lower_struct_id), 1);
		}
		if (op == EDataTemplateOperation::Merge)
		{
			for (; lower_tag_index < lower_dt.TagNum(); lower_tag_index++)
			{
				CopySingleTagUnchecked(dst, lower_dt, lower_tag_index, nest_lvl_offset);
			}
		}
		for (; higher_tag_index < higher_dt.TagNum(); higher_tag_index++)
		{
			CopySingleTagUnchecked(dst, higher_dt, higher_tag_index, 0);
		}

		return dst;
	}
}

DataTemplate serialization::DataTemplate::Merge(const DataTemplate& lower_dt, const DataTemplate& higher_dt)
{
	return dt_operation::Process(lower_dt, higher_dt, dt_operation::EDataTemplateOperation::Merge);
}

DataTemplate serialization::DataTemplate::Diff(const DataTemplate& higher_dt, const DataTemplate& lower_dt)
{
	return dt_operation::Process(lower_dt, higher_dt, dt_operation::EDataTemplateOperation::Diff);
}