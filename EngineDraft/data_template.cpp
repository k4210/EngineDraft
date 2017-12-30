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

	template<typename M> static bool SaveSimpleValue(std::vector<uint8>& dst, const uint8* const src, const M default_value
		, const Flag32<SaveFlags> flags)
	{
		if (default_value != GetConstRef<M>(src, 0) || flags[SaveFlags::SaveNativeDefaultValues])
		{
			const auto vec_size = dst.size();
			dst.resize(vec_size + sizeof(M));
			GetRef<M>(dst.data(), vec_size) = GetConstRef<M>(src, 0);
			return true;
		}
		return false;
	}

	static bool SaveString(std::vector<uint8>& dst, const uint8* const src, const Flag32<SaveFlags> flags)
	{
		const auto& str = GetConstRef<std::string>(src, 0);
		const uint32 len = str.size();
		const bool was_saved = (0 != len) || flags[SaveFlags::SaveNativeDefaultValues];
		if (was_saved)
		{
			const uint32 dst_offset = dst.size();
			Assert(FitsInBits(len, 16));
			dst.resize(dst_offset + sizeof(uint16) + len * sizeof(char));
			GetRef<uint16>(dst.data(), dst_offset) = static_cast<uint16>(len);
			for (uint32 i = 0; i < len; i++)
			{
				GetRef<char>(dst.data(), dst_offset + sizeof(uint16) + i * sizeof(char)) = str[i];
			}
		}
		return was_saved;
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
		const bool was_saved = (kNullObjectID != obj_id) || flags[SaveFlags::SaveNativeDefaultValues];
		if (was_saved)
		{
			const uint32 dst_offset = dst.size();
			dst.resize(dst_offset + sizeof(StructID) + sizeof(ObjectID));
			GetRef<StructID>(dst.data(), dst_offset) = obj ? obj->GetReflectionStructureID() : property_struct_id;
			GetRef<ObjectID>(dst.data(), dst_offset + sizeof(StructID)) = obj_id;

		}
		return was_saved;
	}

	static uint32 GetNativeFieldSize(const std::vector<Property>& properties, const uint32 property_index)
	{
		const auto& property = properties[property_index];
		switch (property.GetFieldType())
		{
			case MemberFieldType::Int8:		return sizeof(int8);
			case MemberFieldType::Int16:	return sizeof(int16);
			case MemberFieldType::Int32:	return sizeof(int32);
			case MemberFieldType::Int64:	return sizeof(int64);
			case MemberFieldType::UInt8:	return sizeof(uint8);
			case MemberFieldType::UInt16:	return sizeof(uint16);
			case MemberFieldType::UInt32:	return sizeof(uint32);
			case MemberFieldType::UInt64:	return sizeof(uint64);
			case MemberFieldType::Float:	return sizeof(float);
			case MemberFieldType::Double:	return sizeof(double);
			case MemberFieldType::ObjectPtr:return sizeof(Object*);

			case MemberFieldType::String:	return sizeof(std::string);
			case MemberFieldType::Vector:	return sizeof(std::vector<int32>);
			case MemberFieldType::Map:		return sizeof(std::map<int32, int32>);
			case MemberFieldType::Struct:	return Structure::GetStructure(property.GetOptionalStructID()).size_;
			case MemberFieldType::Array:	return property.GetArraySize() * GetNativeFieldSize(properties, property_index + 1);
		}
		Assert(false);
		return 0;
	}

	static void SaveLength16(std::vector<uint8>& dst, uint32 len)
	{
		const uint32 dst_offset = dst.size();
		dst.resize(dst_offset + sizeof(uint16));
		GetRef<uint16>(dst.data(), dst_offset) = static_cast<uint16>(len);
	}
};

bool serialization::DataTemplate::SaveStructure(const Structure & structure
	, const uint8 * const src
	, const uint32 nest_level
	, const Flag32<SaveFlags> flags)
{
	bool was_saved = false;
	for (uint32 property_index = 0; property_index < structure.properties_.size(); 
		property_index = NextPropertyIndexOnThisLevel(structure.properties_, property_index))
	{
		const auto& property = structure.properties_[property_index];
		Assert(EPropertyUsage::Main == property.GetPropertyUsage());
		was_saved |= SaveValue(src + property.GetFieldOffset(), structure.properties_, property_index, nest_level, 0, false, flags);
	}
	return was_saved;
}

bool serialization::DataTemplate::SaveArray(const uint8 * src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level
	, const Flag32<SaveFlags> flags)
{
	const uint32 element_size = GetNativeFieldSize(properties, property_index+1);
	const uint32 array_size = properties[property_index].GetArraySize();
	bool was_saved = false;
	for (uint32 i = 0; i < array_size; i++)
	{
		was_saved |= SaveValue(src + i * element_size, properties, property_index + 1, nest_level, i, false, flags);
	}
	return was_saved;
}

bool serialization::DataTemplate::SaveVector(const uint8 * src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level
	, const Flag32<SaveFlags> flags)
{
	bool was_saved = false;
	const auto& handler = properties[property_index + 1].GetVectorHandler();
	const uint32 element_property_index = property_index + 2;
	const uint32 num = handler.GetSize(src);

	SaveLength16(data_, num);

	for (uint32 i = 0; i < num; i++)
	{
		was_saved |= SaveValue(handler.GetElement(src, i), properties, element_property_index, nest_level, i, false, flags);
	}
	return was_saved;
}

bool serialization::DataTemplate::SaveMap(const uint8* src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level
	, const Flag32<SaveFlags> flags)
{
	bool was_saved = false;
	const auto& handler = properties[property_index + 1].GetMapHandler();
	const uint32 key_property_index = property_index + 2;
	const uint32 value_property_index = NextPropertyIndexOnThisLevel(properties, key_property_index);
	const uint32 num = handler.GetSize(src);

	SaveLength16(data_, num);

	for (uint32 i = 0; i < num; i++)
	{
		was_saved |= SaveValue(handler.GetKey(src, i),	properties, key_property_index,		nest_level, i, true, flags);
		was_saved |= SaveValue(handler.GetValue(src, i),	properties, value_property_index,	nest_level, i, false, flags);
	}
	return was_saved;
}

bool serialization::DataTemplate::SaveValue(const uint8 * src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level
	, const uint32 element_index
	, const bool is_key
	, const Flag32<SaveFlags> flags)
{
	const auto& property = properties[property_index];
	Assert(property.GetPropertyUsage() == EPropertyUsage::Main || property.GetPropertyUsage() == EPropertyUsage::SubType);
	tags_.emplace_back(Tag(
#if EDITOR
		property.GetPropertyID(), 
#endif		
		property_index, data_.size(), nest_level, element_index, is_key ? 1 : 0));
	bool was_saved = false;
	switch (property.GetFieldType())
	{
		case MemberFieldType::Int8:		was_saved = SaveSimpleValue<int8	>(data_, src, 0, flags);						break;
		case MemberFieldType::Int16:	was_saved = SaveSimpleValue<int16	>(data_, src, 0, flags);						break;
		case MemberFieldType::Int32:	was_saved = SaveSimpleValue<int32	>(data_, src, 0, flags);						break;
		case MemberFieldType::Int64:	was_saved = SaveSimpleValue<int64	>(data_, src, 0, flags);						break;
		case MemberFieldType::UInt8:	was_saved = SaveSimpleValue<uint8	>(data_, src, 0, flags);						break;
		case MemberFieldType::UInt16:	was_saved = SaveSimpleValue<uint16	>(data_, src, 0, flags);						break;
		case MemberFieldType::UInt32:	was_saved = SaveSimpleValue<uint32	>(data_, src, 0, flags);						break;
		case MemberFieldType::UInt64:	was_saved = SaveSimpleValue<uint64	>(data_, src, 0, flags);						break;
		case MemberFieldType::Float:	was_saved = SaveSimpleValue<float	>(data_, src, 0.0f, flags);						break;
		case MemberFieldType::Double:	was_saved = SaveSimpleValue<double	>(data_, src, 0.0, flags);						break;
		case MemberFieldType::String:	was_saved = SaveString(data_, src, flags);											break;
		case MemberFieldType::ObjectPtr:was_saved = SaveObject(data_, src, property.GetOptionalStructID(), flags);			break;
		case MemberFieldType::Array:	was_saved = SaveArray(src, properties, property_index, nest_level + 1, flags);		break;
		case MemberFieldType::Vector:	was_saved = SaveVector(src, properties, property_index, nest_level + 1, flags);	break;
		case MemberFieldType::Map:		was_saved = SaveMap(src, properties, property_index, nest_level + 1, flags);		break;
		case MemberFieldType::Struct:
			const Structure& structure = Structure::GetStructure(property.GetOptionalStructID());
			Assert(structure.RepresentNonObjectStructure());
			was_saved = SaveStructure(structure, src, nest_level + 1, flags);
			break;
	}
	if (!was_saved)
	{
		tags_.pop_back();
	}
	return was_saved;
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
	SaveStructure(structure, reinterpret_cast<const uint8*>(obj), 0, flags);
	Assert(tags_.empty() == data_.empty());
}

uint32 serialization::DataTemplate::LoadArray(const Structure& structure, uint8* dst, const Tag tag, uint32 tag_index) const
{
	const auto& property = structure.properties_[tag.property_index_];
	const uint32 element_size = GetNativeFieldSize(structure.properties_, tag.property_index_ + 1);
	while (tag_index < tags_.size())
	{
		const Tag inner_tag = tags_[tag_index];
		const bool within_size = inner_tag.element_index_ < property.GetArraySize();
		Assert(inner_tag.element_index_ < property.GetArraySize());
		const bool expected_property_idx = inner_tag.property_index_ == tag.property_index_ + 1;
		const bool expected_nest_idx = inner_tag.nest_level_ == tag.nest_level_ + 1;
		Assert(expected_property_idx == expected_nest_idx);
		if (!expected_property_idx || !expected_nest_idx || !within_size)
			break;

		tag_index = LoadValue(structure, dst + inner_tag.element_index_ * element_size, tag_index);
	}
	return tag_index;
}

uint32 serialization::DataTemplate::LoadVector(const Structure& structure, uint8* dst, const Tag tag, uint32 tag_index) const
{
	const auto& handler = structure.properties_[tag.property_index_ + 1].GetVectorHandler();
	const uint32 size = GetConstRef<uint16>(data_.data(), tag.byte_offset_);
	handler.SetSize(dst, size);
	while (tag_index < tags_.size())
	{
		const Tag inner_tag = tags_[tag_index];
		const bool expected_property_idx = inner_tag.property_index_ == tag.property_index_ + 2;
		const bool expected_nest_idx = inner_tag.nest_level_ == tag.nest_level_ + 1;
		Assert(expected_property_idx == expected_nest_idx);
		if (!expected_property_idx || !expected_nest_idx)
			break;
		tag_index = LoadValue(structure, handler.GetElement(dst, inner_tag.element_index_), tag_index);
	}
	return tag_index;
}

uint32 serialization::DataTemplate::LoadMap(const Structure& structure, uint8* dst, const Tag tag, uint32 tag_index) const
{
	const auto& handler = structure.properties_[tag.property_index_ + 1].GetMapHandler();
	const uint32 key_property_index = tag.property_index_ + 2;
	const uint32 value_property_index = NextPropertyIndexOnThisLevel(structure.properties_, key_property_index);
	const uint32 map_size = GetConstRef<uint16>(data_.data(), tag.byte_offset_); //number of keys

	std::vector<uint8> temp_key_memory;
	for (uint32 idx = 0; idx < map_size; idx++)
	{
		Assert(tag_index < tags_.size());
		//assume SaveNativeDefaultValues
		//assume proper order
		{
			const Tag key_tag = tags_[tag_index];
			Assert(key_tag.nest_level_ == tag.nest_level_ + 1);
			Assert(key_tag.is_key_);
			Assert(key_tag.property_index_ == key_property_index);
			Assert(key_tag.element_index_ == idx);
			handler.InitializeKeyMemory(temp_key_memory);
			tag_index = LoadValue(structure, temp_key_memory.data(), tag_index);
			Assert(tag_index < tags_.size());
		}

		{
			const Tag value_tag = tags_[tag_index];
			Assert(value_tag.nest_level_ == tag.nest_level_ + 1);
			Assert(!value_tag.is_key_);
			Assert(value_tag.property_index_ == value_property_index);
			Assert(value_tag.element_index_ == idx);
			uint8* value_ptr = handler.Add(dst, temp_key_memory);
			tag_index = LoadValue(structure, value_ptr, tag_index);
		}
	}
	return tag_index;
}

uint32 serialization::DataTemplate::LoadValue(const Structure& structure, uint8* dst, uint32 tag_index) const
{
	const Tag tag = tags_[tag_index];
	const auto& property = structure.properties_[tag.property_index_];
	tag_index++;
	switch (property.GetFieldType())
	{
		case MemberFieldType::Int8:		LoadSimpleValue<uint8>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::Int16:	LoadSimpleValue<int16>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::Int32:	LoadSimpleValue<int32>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::Int64:	LoadSimpleValue<int64>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::UInt8:	LoadSimpleValue<uint8>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::UInt16:	LoadSimpleValue<uint16>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::UInt32:	LoadSimpleValue<uint32>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::UInt64:	LoadSimpleValue<uint64>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::Float:	LoadSimpleValue<float>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::Double:	LoadSimpleValue<double>		(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::String:	LoadSimpleValue<std::string>(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::ObjectPtr:LoadSimpleValue<Object*>	(dst, data_.data(), tag.byte_offset_);	break;
		case MemberFieldType::Array:	tag_index = LoadArray		(structure, dst, tag, tag_index);		break;
		case MemberFieldType::Vector:	tag_index = LoadVector		(structure, dst, tag, tag_index);		break;
		case MemberFieldType::Map:		tag_index = LoadMap			(structure, dst, tag, tag_index);		break;
		case MemberFieldType::Struct:	tag_index = LoadStructure(
								Structure::GetStructure(property.GetOptionalStructID()), dst, tag_index);	break;
	}
	return tag_index;
}

uint32 serialization::DataTemplate::LoadStructure(const Structure& structure, uint8* dst, uint32 tag_index) const
{
	if (tag_index < tags_.size())
	{
		const Tag first_tag = tags_[tag_index];
		while (tag_index < tags_.size())
		{
			const Tag tag = tags_[tag_index];
			Assert(tag.nest_level_ <= first_tag.nest_level_);
			if (tag.nest_level_ != first_tag.nest_level_ || tag.element_index_ != first_tag.element_index_ || tag.is_key_ != first_tag.is_key_)
				break;
			const auto& property = structure.properties_[tag.property_index_];
			tag_index = LoadValue(structure, dst + property.GetFieldOffset(), tag_index);
		}
	}
	return tag_index;
}

void serialization::DataTemplate::Load(Object* obj) const
{
	Assert(nullptr != obj);
	Assert(kWrongID != structure_id_);
	Assert(obj->GetReflectionStructureID() == structure_id_);
	const auto& structure = Structure::GetStructure(structure_id_);
	Assert(structure.RepresentsObjectClass());
	LoadStructure(structure, reinterpret_cast<uint8*>(obj), 0);
}

void serialization::DataTemplate::CloneFrom(const DataTemplate& src)
{
	Assert(tags_.empty() && data_.empty());
	Assert(kWrongID == structure_id_);

	Assert(!src.tags_.empty() && !src.data_.empty());
	Assert(kWrongID != src.structure_id_);

	*this = src;
}

void serialization::DataTemplate::Diff(const DataTemplate&)
{
	Assert(false);
}

void serialization::DataTemplate::Merge(const DataTemplate&)
{
	Assert(false);
}
