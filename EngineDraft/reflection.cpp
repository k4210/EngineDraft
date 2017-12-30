#include "reflection.h"
#include <sstream>
#include <iomanip>

REGISTER_STRUCTURE(reflection::Object);

namespace reflection
{
	class ReflectionManager
	{
		std::map<StructID, std::unique_ptr<Structure>> structures_;
	public:
		static ReflectionManager& Get()
		{
			static ReflectionManager instance;
			return instance;
		}

		Structure& GetStructure(StructID id)
		{
			Assert(structures_.find(id) != structures_.end());
			return *structures_.at(id);
		}

		Structure& RegisterStructure(Structure* struct_ptr)
		{
			Assert(nullptr != struct_ptr);
			const auto id = struct_ptr->id_;
			Assert(kWrongID != id);
			Assert(structures_.find(struct_ptr->id_) == structures_.end());
			auto result = structures_.try_emplace(id, std::unique_ptr<Structure>(struct_ptr));
			Assert(result.second);
			return *(result.first->second);
		}
	};

	Structure& Structure::CreateStructure(const StructID id)
	{
		return ReflectionManager::Get().RegisterStructure(new Structure(id));
	}

	const Structure& Structure::GetStructure(const StructID id)
	{
		return ReflectionManager::Get().GetStructure(id);
	}

	uint32 NextPropertyIndexOnThisLevel(const std::vector<Property>& properties, uint32 idx)
	{
		uint32 properties_to_consume = 1;
		while (properties_to_consume)
		{
			properties_to_consume--;
			const auto& prop = properties[idx];
			switch (prop.GetFieldType())
			{
			case MemberFieldType::Array:			properties_to_consume += 1; break;
			case MemberFieldType::Vector:	idx++;	properties_to_consume += 1; break;
			case MemberFieldType::Map:		idx++;	properties_to_consume += 2; break;
			}
			idx++;
		}
		return idx;
	}

	bool Structure::Validate() const
	{
		if (RepresentsObjectClass() == RepresentNonObjectStructure())
			return false;

		uint32 prev_index = 0;
		for (uint32 property_idx = 1; property_idx < properties_.size(); property_idx++)
		{
			const Property& curr = properties_[property_idx];
			if (EPropertyUsage::Main == curr.GetPropertyUsage())
			{
				const Property& prev = properties_[prev_index];
				const bool proper_order = curr.GetFieldOffset() > prev.GetFieldOffset();
				if (!proper_order) 
					return false;

				if(property_idx != NextPropertyIndexOnThisLevel(properties_, prev_index))
					return false;

				prev_index = property_idx;
		}
		}
		return true;
	}

	std::string Property::ToString() const
	{
		std::stringstream str;
		str << std::left << std::setfill(' ') << std::setw(6) << ToStr(GetPropertyUsage()) << ' ';
		if (EPropertyUsage::Handler != GetPropertyUsage())
		{
			DEBUG_ONLY(str << std::left << std::setfill(' ') << std::setw(16) << name_ << ' ');
			str << std::left << std::setfill(' ') << std::setw(8) << ToStr(GetFieldType()) << ' ';
			str << std::right << std::setfill('0') << std::setw(8) << std::hex << GetPropertyID() << ' ';

			if (HasStructID())
				str << std::setfill('0') << std::setw(8) << std::hex << GetOptionalStructID() << "  ";
			else
				str << "          ";

			if (GetFieldType() == MemberFieldType::Array)
				str << std::setfill('0') << std::setw(4) << GetArraySize() << ' ';
			else
				str << "     ";

			if (EPropertyUsage::Main == GetPropertyUsage())
			{
				str << std::left << std::setfill(' ') << std::setw(8) << ToStr(GetAccessSpecifier()) << ' ';
				str << std::left << std::setfill(' ') << std::setw(8) << ToStr(GetConstSpecifier()) << ' ';
				str << std::right << std::setfill('0') << std::setw(8) << std::hex << GetFieldOffset() << ' ';
				str << std::right << std::setfill('0') << std::setw(2) << std::hex << static_cast<uint32>(GetFlags());
			}
		}
		str << '\n';
		return str.str();
	}

	const char* ToStr(MemberFieldType e)
	{
		switch (e)
		{
			case MemberFieldType::Int8: return "Int8";
			case MemberFieldType::Int16: return "Int16";
			case MemberFieldType::Int32: return "Int32";
			case MemberFieldType::Int64: return "Int64";
			case MemberFieldType::UInt8: return "UInt8";
			case MemberFieldType::UInt16: return "UInt16";
			case MemberFieldType::UInt32: return "UInt32";
			case MemberFieldType::UInt64: return "UInt64";
			case MemberFieldType::Float: return "Float";
			case MemberFieldType::Double: return "Double";
			case MemberFieldType::String: return "String";
			case MemberFieldType::ObjectPtr: return "ObjPtr";
			case MemberFieldType::Struct: return "Struct";
			case MemberFieldType::Vector: return "Vector";
			case MemberFieldType::Map: return "Map";
			case MemberFieldType::Array: return "Array";
		}
		return "error";
	}

	const char* ToStr(AccessSpecifier e)
	{
		switch (e)
		{
			case AccessSpecifier::Private: return "Private";
			case AccessSpecifier::Protected: return "Protected";
			case AccessSpecifier::Public: return "Public";
		}
		return "error";
	}

	const char* ToStr(ConstSpecifier e)
	{
		switch (e)
		{
			case ConstSpecifier::Const: return "Const";
			case ConstSpecifier::NotConst: return "Not";
		}
		return "error";
	}

	const char* ToStr(EPropertyUsage e)
	{
		switch (e)
		{
			case EPropertyUsage::Main: return "Main";
			case EPropertyUsage::SubType: return "Sub";
			case EPropertyUsage::Handler: return "Hdlr";
		}
		return "error";
	}
}