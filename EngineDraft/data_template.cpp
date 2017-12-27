#include "data_template.h"

namespace
{
	using namespace reflection;
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

	template<typename M> static bool SaveSimpleValue(std::vector<uint8>& dst, const uint8* const src, const M default_value)
	{
		if (default_value != GetConstRef<M>(src, 0))
		{
			const auto vec_size = dst.size();
			dst.resize(vec_size + sizeof(M));
			GetRef<M>(dst.data(), vec_size) = GetConstRef<M>(src, 0);
			return true;
		}
		return false;
	}

	static bool SaveString(std::vector<uint8>& dst, const uint8* const src)
	{
		const auto& str = GetConstRef<std::string>(src, 0);
		const uint32 len = str.size();
		const bool was_saved = 0 != len;
		if (was_saved)
		{
			const uint32 dst_offset = dst.size();
			Assert(FitsInBits(len, 16));
			dst.resize(dst_offset + sizeof(uint16) + len * sizeof(char));
			GetRef<uint16>(dst.data(), 0) = static_cast<uint16>(len);
			for (uint32 i = 0; i < len; i++)
			{
				GetRef<char>(dst.data(), dst_offset + sizeof(uint16) + i * sizeof(char)) = str[i];
			}
		}
		return was_saved;
	}

	static bool SaveObject(std::vector<uint8>& dst, const uint8* const src)
	{
		ObjectID obj_id = kNullObjectID;
		const Object* obj = GetConstRef<Object*>(src, 0);
		if (obj)
		{
			const auto& structure = Structure::GetStructure(obj->GetReflectionStructureID());
			Assert(structure.RepresentsObjectClass());
			obj_id = structure.get_obj_id(obj);
		}
		const bool was_saved = kNullObjectID != obj_id;
		if (was_saved)
		{
			const uint32 dst_offset = dst.size();
			dst.resize(dst_offset + sizeof(ObjectID));
			GetRef<ObjectID>(dst.data(), dst_offset) = obj_id;
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
			case MemberFieldType::Vector:return sizeof(std::vector<int32>);
			case MemberFieldType::Map:		return sizeof(std::map<int32, int32>);
			case MemberFieldType::Struct:	
				return Structure::GetStructure(property.GetOptionalStructID()).size_;
			case MemberFieldType::Array:	
				return property.GetArraySize() * GetNativeFieldSize(properties, property_index + 1);
		}
		Assert(false);
		return 0;
	}
};

bool Serialization::DataTemplate::SaveStructure(const Structure & structure
	, const uint8 * const src
	, const uint32 nest_level)
{
	bool was_saved = false;
	for (uint32 property_index = 0; property_index < structure.properties_.size(); 
		property_index = NextPropertyIndexOnThisLevel(structure.properties_, property_index))
	{
		const auto& property = structure.properties_[property_index];
		Assert(EPropertyUsage::Main == property.GetPropertyUsage());
		was_saved |= SaveElement(src + property.GetFieldOffset(), structure.properties_, property_index, nest_level);
	}
	return was_saved;
}

bool Serialization::DataTemplate::SaveArray(const uint8 * src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level)
{
	const uint32 element_size = GetNativeFieldSize(properties, property_index+1);
	const uint32 array_size = properties[property_index].GetArraySize();
	bool was_saved = false;
	for (uint32 i = 0; i < array_size; i++)
	{
		was_saved |= SaveElement(src + i * element_size, properties, property_index + 1, nest_level, i);
	}
	return was_saved;
}

bool Serialization::DataTemplate::SaveContainer(const uint8 * src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level)
{
	bool was_saved = false;
	const auto& handler = properties[property_index + 1].GetHandler();
	const uint32 element_property_index = property_index + 2;
	const uint32 num = handler.GetNumElements(src);
	for (uint32 i = 0; i < num; i++)
	{
		was_saved |= SaveElement(handler.GetElement(src, i), properties, element_property_index, nest_level, i);
	}
	return was_saved;
}

bool Serialization::DataTemplate::SaveMap(const uint8* src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level)
{
	bool was_saved = false;
	const auto& handler = properties[property_index + 1].GetHandler();
	const uint32 key_property_index = property_index + 2;
	const uint32 value_property_index = NextPropertyIndexOnThisLevel(properties, key_property_index);
	const uint32 num = handler.GetNumElements(src);
	for (uint32 i = 0; i < num; i++)
	{
		was_saved |= SaveElement(handler.GetKey(src, i),	properties, key_property_index,		nest_level, i, true);
		was_saved |= SaveElement(handler.GetValue(src, i),	properties, value_property_index,	nest_level, i); 
	}
	return was_saved;
}

bool Serialization::DataTemplate::SaveElement(const uint8 * src
	, const std::vector<Property>& properties
	, const uint32 property_index
	, const uint32 nest_level
	, const uint32 element_index
	, const bool is_key)
{
	const auto& property = properties[property_index];
	Assert(property.GetPropertyUsage() == EPropertyUsage::Main || property.GetPropertyUsage() == EPropertyUsage::SubType);
	tags_.emplace_back(Tag(property_index, data_.size(), nest_level, element_index, is_key ? 1 : 0));
	bool was_saved = false;
	switch (property.GetFieldType())
	{
		case MemberFieldType::Int8:		was_saved = SaveSimpleValue<int8	>(data_, src, 0);						break;
		case MemberFieldType::Int16:	was_saved = SaveSimpleValue<int16	>(data_, src, 0);						break;
		case MemberFieldType::Int32:	was_saved = SaveSimpleValue<int32	>(data_, src, 0);						break;
		case MemberFieldType::Int64:	was_saved = SaveSimpleValue<int64	>(data_, src, 0);						break;
		case MemberFieldType::UInt8:	was_saved = SaveSimpleValue<uint8	>(data_, src, 0);						break;
		case MemberFieldType::UInt16:	was_saved = SaveSimpleValue<uint16	>(data_, src, 0);						break;
		case MemberFieldType::UInt32:	was_saved = SaveSimpleValue<uint32	>(data_, src, 0);						break;
		case MemberFieldType::UInt64:	was_saved = SaveSimpleValue<uint64	>(data_, src, 0);						break;
		case MemberFieldType::Float:	was_saved = SaveSimpleValue<float	>(data_, src, 0.0f);					break;
		case MemberFieldType::Double:	was_saved = SaveSimpleValue<double	>(data_, src, 0.0);						break;
		case MemberFieldType::String:	was_saved = SaveString(data_, src);											break;
		case MemberFieldType::ObjectPtr:was_saved = SaveObject(data_, src);											break;
		case MemberFieldType::Array:	was_saved = SaveArray(src, properties, property_index, nest_level + 1);		break;
		case MemberFieldType::Vector:	was_saved = SaveContainer(src, properties, property_index, nest_level + 1);	break;
		case MemberFieldType::Map:		was_saved = SaveMap(src, properties, property_index, nest_level + 1);		break;
		case MemberFieldType::Struct:
			const Structure& structure = Structure::GetStructure(property.GetOptionalStructID());
			Assert(structure.RepresentNonObjectStructure());
			was_saved = SaveStructure(structure, src, nest_level + 1);
			break;
	}
	if (!was_saved)
	{
		tags_.pop_back();
	}
	return was_saved;
}

void Serialization::DataTemplate::Save(const Object * obj)
{
	Assert(nullptr != obj);
	Assert(kWrongID == structure_id_); //uninitialized
	structure_id_ = obj->GetReflectionStructureID();
	Assert(kWrongID != structure_id_);
	const auto& structure = Structure::GetStructure(structure_id_);
	Assert(structure.RepresentsObjectClass());
	Assert(tags_.empty() && data_.empty());
	SaveStructure(structure, reinterpret_cast<const uint8*>(obj), 0);
	Assert(tags_.empty() == data_.empty());
}

void Serialization::DataTemplate::Load(Object *)
{
	Assert(kWrongID != structure_id_);
	const auto& structure = Structure::GetStructure(structure_id_);
	Assert(structure.RepresentsObjectClass());


}
