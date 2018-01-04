#pragma once
#include "reflection.h"
#include "data_template.h"

#include <sstream>
#include <iomanip>

namespace serialization
{
	template <typename Writer> 
	struct IncreaseIndentOnScope
	{
		Writer& writer_;
		uint32 saved_value_;

		IncreaseIndentOnScope(Writer& writer)
			: writer_(writer)
			, saved_value_(writer.GetIndent())
		{
			writer.SetIndent(' ', saved_value_ + 4);
		}

		~IncreaseIndentOnScope()
		{
			writer_.SetIndent(' ', saved_value_);
		}
	};

	class JsonDataStorage
	{
		int32 indent = 0;

		template <typename Writer> void SaveTag(Writer& writer, const Tag tag, const Property& property);

		template <typename Writer> uint32 SaveStruct(Writer& writer, const Structure& structure
			, const DataTemplate& data_template, uint32 tag_index);

		template <typename Writer> uint32 SaveValue(Writer& writer, const Structure& structure, const DataTemplate& data_template, uint32 tag_index);

		template <typename Writer> uint32 SaveMany(Writer& writer, const Structure& structure, const DataTemplate& data_template, uint32 tag_index);
		template <typename Writer> uint32 SaveMap(Writer& writer, const Structure& structure, const DataTemplate& data_template, uint32 tag_index);

		template <typename Writer> void SaveObj(Writer& writer, const uint8* const data, const uint32 offset);
		template <typename Writer> void SaveTagSuperStruct(Writer& writer, const Tag tag);
	public:
		template <typename Writer> void Save(Writer& writer, const DataTemplate& data_template);

		virtual ~JsonDataStorage() = default;
	};

	template <typename Writer> void JsonDataStorage::SaveTagSuperStruct(Writer& writer, const Tag tag)
	{
		Assert(tag.GetPropertyIndex() == kSuperStructPropertyIndex);
		Assert(tag.GetDataOffset() == 0);
		writer.Key("tag");
		std::stringstream str;
		str << "property_id: " << "super-struct"
			<< " property_name: '" << "super-struct"
			<< "' property_type: " << "Struct"
			<< " nest_level: " << tag.GetNestLevel()
			<< " element_index: " << tag.GetElementIndex()
			<< " is_key: " << tag.IsKey();
		writer.String(str.str());
	}

	template <typename Writer> void JsonDataStorage::SaveTag(Writer& writer, const Tag tag, const Property& property)
	{
		Assert(property.GetPropertyUsage() == EPropertyUsage::Main || property.GetPropertyUsage() == EPropertyUsage::SubType);

		writer.Key("tag");
		std::stringstream str;
		str << "property_id: " << property.GetPropertyID()
			<< " property_name: '" << property.GetName()
			<< "' property_type: " << ToStr(property.GetFieldType())
			<< " nest_level: " << tag.GetNestLevel()
			<< " element_index: " << tag.GetElementIndex()
			<< " is_key: "<< tag.IsKey();
		writer.String(str.str());

		/*
		writer.Key("property_id");
		writer.Uint(property.GetPropertyID());

		writer.Key("property_name");
		writer.String(property.GetName());

		writer.Key("property_type");
		const char* type_str = ToStr(property.GetFieldType());
		writer.String(type_str, strlen(type_str));

		writer.Key("nest_level");
		writer.Uint(tag.GetNestLevel());

		writer.Key("element_index");
		writer.Uint(tag..GetElementIndex());

		writer.Key("is_key");
		writer.Uint(tag.IsKey());
		*/
	}

	template<typename M> static const M& GetConstRef(const uint8* const data, const uint32 offset)
	{
		const uint8* const ptr = data + offset;
		return *reinterpret_cast<const std::remove_cv<M>::type*>(ptr);
	}
	template<typename M> static const M* GetConstPtr(const uint8* const data, const uint32 offset)
	{
		const uint8* const ptr = data + offset;
		return reinterpret_cast<const std::remove_cv<M>::type*>(ptr);
	}

	template <typename Writer> uint32 JsonDataStorage::SaveMany(Writer& writer, const Structure& structure, const DataTemplate& data_template, uint32 tag_index)
	{
		const Tag tag = data_template.tags_[tag_index - 1];
		const auto& upper_property = structure.GetProperty(tag.GetPropertyIndex());
		Assert(upper_property.GetFieldType() == MemberFieldType::Array || upper_property.GetFieldType() == MemberFieldType::Vector);
		if (upper_property.GetFieldType() == MemberFieldType::Vector)
		{
			writer.Key("length");
			writer.Uint(GetConstRef<uint16>(data_template.data_.data(), tag.GetDataOffset()));
		}

		const uint32 inner_property_index = structure.GetSubPropertyIndex(tag.GetPropertyIndex()
			, (upper_property.GetFieldType() == MemberFieldType::Array) ? ESubType::Array_Element : ESubType::Vector_Element);
		while (tag_index < data_template.tags_.size())
		{
			const Tag inner_tag = data_template.tags_[tag_index];
			if (upper_property.GetFieldType() == MemberFieldType::Array)
			{
				Assert(inner_tag.GetElementIndex() < upper_property.GetArraySize());
			}
			const bool expected_property_idx = inner_tag.GetPropertyIndex() == inner_property_index;//error
			const bool expected_nest_idx = inner_tag.GetNestLevel() == tag.GetNestLevel() + 1;
			Assert(expected_property_idx == expected_nest_idx);
			if (!expected_property_idx || !expected_nest_idx)
				break;
			tag_index = SaveValue<Writer>(writer, structure, data_template, tag_index);
		}
		return tag_index;
	}

	template <typename Writer> uint32 JsonDataStorage::SaveMap(Writer& writer, const Structure& structure, const DataTemplate& data_template, uint32 tag_index)
	{
		const Tag tag = data_template.tags_[tag_index - 1];
		const uint32 key_property_index = structure.GetSubPropertyIndex(tag.GetPropertyIndex(), ESubType::Key);
		const uint32 value_property_index = structure.GetSubPropertyIndex(tag.GetPropertyIndex(), ESubType::Map_Value);
		const uint32 map_size = GetConstRef<uint16>(data_template.data_.data(), tag.GetDataOffset()); //number of keys

		writer.Key("length");
		writer.Uint(map_size);

		while (tag_index < data_template.tags_.size())
		{
			const Tag inner_tag = data_template.tags_[tag_index];
			if (inner_tag.GetNestLevel() != tag.GetNestLevel() + 1)
				break;
			Assert(inner_tag.GetElementIndex() < map_size);
			Assert(!!inner_tag.IsKey() == (inner_tag.GetPropertyIndex() == key_property_index));
			Assert(!!inner_tag.IsKey() != (inner_tag.GetPropertyIndex() == value_property_index));
			tag_index = SaveValue<Writer>(writer, structure, data_template, tag_index);
		}
		return tag_index;
	}

	template <typename Writer> void JsonDataStorage::SaveObj(Writer& writer, const uint8* const data, const uint32 offset)
	{
		writer.Key("obj_struct_id");
		writer.Uint64(GetConstRef<StructID>(data, offset));
		writer.Key("obj_id");
		writer.Uint64(GetConstRef<ObjectID>(data, offset + sizeof(StructID)));
	}

	template <typename Writer> uint32 JsonDataStorage::SaveValue(Writer& writer, const Structure& structure, const DataTemplate& data_template, uint32 tag_index)
	{
		const IncreaseIndentOnScope<Writer> intend(writer);

		const Tag tag = data_template.tags_[tag_index];
		tag_index++;
		const auto& property = structure.GetProperty(tag.GetPropertyIndex());
		SaveTag<Writer>(writer, tag, property);

		switch (property.GetFieldType())
		{
			case MemberFieldType::Array:
			case MemberFieldType::Vector:	tag_index = SaveMany<Writer>(writer, structure, data_template, tag_index); break;
			case MemberFieldType::Map:		tag_index = SaveMap <Writer>(writer, structure, data_template, tag_index); break;
			case MemberFieldType::Struct:	tag_index = SaveStruct<Writer>(writer, Structure::GetStructure(property.GetOptionalStructID()), data_template, tag_index); break;
			case MemberFieldType::ObjectPtr:SaveObj(writer, data_template.data_.data(), tag.GetDataOffset()); break;
			default: writer.Key("value");
		}
			
		switch (property.GetFieldType())
		{
			case MemberFieldType::Int8:		writer.Int(GetConstRef<int8>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::Int16:	writer.Int(GetConstRef<int16>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::Int32:	writer.Int(GetConstRef<int32>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::Int64:	writer.Int64(GetConstRef<int64>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::UInt8:	writer.Uint(GetConstRef<uint8>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::UInt16:	writer.Uint(GetConstRef<uint16>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::UInt32:	writer.Uint(GetConstRef<uint32>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::UInt64:	writer.Uint64(GetConstRef<uint64>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::Float:	writer.Double(GetConstRef<float>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::Double:	writer.Double(GetConstRef<double>(data_template.data_.data(), tag.GetDataOffset())); break;
			case MemberFieldType::String:	writer.String(GetConstPtr<char>(data_template.data_.data(), tag.GetDataOffset() + sizeof(uint16))
				, GetConstRef<int16>(data_template.data_.data(), tag.GetDataOffset())); break;
		}
		return tag_index;
	}

	template <typename Writer> uint32 JsonDataStorage::SaveStruct(Writer& writer, const Structure& structure, const DataTemplate& data_template, uint32 tag_index)
	{
		writer.Key("struct_id");
		writer.Uint(structure.id_);
		writer.Key("struct_name");
		writer.String(structure.GetName());
		if (tag_index < data_template.tags_.size())
		{
			const Tag first_tag = data_template.tags_[tag_index];
			while (tag_index < data_template.tags_.size())
			{
				const Tag tag = data_template.tags_[tag_index];
				Assert(tag.GetNestLevel() <= first_tag.GetNestLevel());
				if (tag.GetNestLevel() != first_tag.GetNestLevel() || tag.GetElementIndex() != first_tag.GetElementIndex() || tag.IsKey() != first_tag.IsKey())
					break;
				if (kSuperStructPropertyIndex == tag.GetPropertyIndex())
				{
					tag_index++;
					SaveTagSuperStruct<Writer>(writer, tag);
					tag_index = SaveStruct<Writer>(writer, Structure::GetStructure(structure.super_id_), data_template, tag_index);
				}
				else
				{
					tag_index = SaveValue<Writer>(writer, structure, data_template, tag_index);
				}
			}
		}
		return tag_index;
	}

	template <typename Writer> void JsonDataStorage::Save(Writer& writer, const DataTemplate& data_template)
	{
		writer.StartObject();
		const Structure& structure = Structure::GetStructure(data_template.structure_id_);
		const uint32 saved_tags = SaveStruct<Writer>(writer, structure, data_template, 0);

		Assert(saved_tags == data_template.tags_.size());
		writer.EndObject();
	}
};