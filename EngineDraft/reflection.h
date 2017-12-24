#pragma once
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <array>
#include "utils.h"

namespace Reflection
{
	enum class MemberFieldType : uint32
	{
		Int8 = 0,
		Int16,
		Int32,
		Int64,
		UInt8,
		UInt16,
		UInt32,
		UInt64,
		Float,
		Double,
		String,
		ObjectPtr,
		Struct,
		Container,	// requires one sub-type
		Map,		// requires two sub-types
		Array,		// requires one sub-type and size
		//WeakPtr, AssetPtr, etc...
		__NUM
	};
	static_assert(static_cast<uint32>(MemberFieldType::__NUM) <= 16);

	template<typename T> inline const char* ToStr(T e)
	{
		static_assert(false, "unknown type");
	}

	template<> inline const char* ToStr<MemberFieldType>(MemberFieldType e)
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
			case MemberFieldType::Container: return "Vector";
			case MemberFieldType::Map: return "Map";
			case MemberFieldType::Array: return "Array";
		}
		return "error";
	}

	enum class AccessSpecifier
	{
		Private = 0,
		Protected = 1,
		Public = 2
	};
	template<> inline const char* ToStr<AccessSpecifier>(AccessSpecifier e)
	{
		switch (e)
		{
			case AccessSpecifier::Private: return "Private";
			case AccessSpecifier::Protected: return "Protected";
			case AccessSpecifier::Public: return "Public";
		}
		return "error";
	}

	enum class ConstSpecifier
	{
		Const = 0,
		NotConst = 1
	};
	template<> inline const char* ToStr<ConstSpecifier>(ConstSpecifier e)
	{
		switch (e)
		{
			case ConstSpecifier::Const: return "Const";
			case ConstSpecifier::NotConst: return "NotConst";
		}
		return "error";
	}

	using PropertyID = uint32;
	using StructID = uint32;
	constexpr uint32 kWrongID = 0xFFFFFFFF;

	constexpr uint32 HashString32(const char* const str, const uint32 value = 0x811c9dc5) noexcept
	{
		return (str[0] == '\0') ? value : HashString32(&str[1], (value ^ uint32(str[0])) * 0x1000193);
	}

	constexpr uint64 HashString64(const char* const str)
	{
		uint64 hash = 0xcbf29ce484222325;
		const uint64 prime = 0x100000001b3;
		for(const char* it = str; *it != '\0'; it++)
		{
			hash = (hash ^ uint64(*it)) * prime;
		}
		return hash;
	}

	struct Property
	{
	private:
		uint16 offset_ = 0;
		uint8 array_size_or_flags_ = 0;
		uint8 is_sub_type	: 1;	
		uint8 type_			: 4;	//MemberFieldType
		uint8 constant_		: 1;	//ConstSpecifier
		uint8 access_		: 2;	//AccessSpecifier

		PropertyID property_id_ = kWrongID;
		StructID optional_struct_id_ = kWrongID; //not owner! addditional type info
	public:
		DEBUG_ONLY(std::string name_);

		uint32			GetFieldOffset()		const { return offset_; }
		uint8			GetFlags()				const { Assert(!is_sub_type); return array_size_or_flags_; } // specific flags, are not part of the refection system
		uint8			GetArraySize()			const { Assert( is_sub_type); return array_size_or_flags_; }
		PropertyID		GetPropertyID()			const { return property_id_; }
		StructID		GetOptionalStructID()	const { return optional_struct_id_; }
		bool			IsSubType()				const { return 0 != is_sub_type; }
		MemberFieldType GetFieldType()			const { return static_cast<MemberFieldType>(type_); }
		ConstSpecifier	GetConstSpecifier()		const { return static_cast<ConstSpecifier>(constant_); }
		AccessSpecifier GetAccessSpecifier()	const { return static_cast<AccessSpecifier>(access_); }

		std::string ToString() const
		{
			std::string str = "Property\t";
			DEBUG_ONLY(str += name_ + '\t');
			str += ToStr(GetFieldType());

			{
				char buff[260];
				sprintf_s(buff, "\t%.8X\t%.8X\t", GetPropertyID(), GetOptionalStructID());
				str += buff;
			}
			if (!IsSubType())
			{
				str += ToStr(GetAccessSpecifier());
				str += '\t';
				str += ToStr(GetConstSpecifier());
				str += '\t';
				str += std::to_string(GetFieldOffset());
				char buff[260];
				sprintf_s(buff, "\t%.2X\t", GetFlags());
				str += buff;
			}
			else
			{
				char buff[260];
				sprintf_s(buff, "\t\t\t\t%.2X\t", GetArraySize());
				str += buff;
			}
			str += "\n";
			return str;
		}

		Property() 
			: is_sub_type(0)
			, type_(0)
			, constant_(0)
			, access_(0) 
		{}
		Property(uint16 offset, MemberFieldType type, ConstSpecifier constant, AccessSpecifier access, PropertyID property_id, StructID struct_id = kWrongID)
			: offset_(offset)
			, is_sub_type(0)
			, type_(static_cast<uint8>(type))
			, constant_(static_cast<uint8>(constant))
			, access_(static_cast<uint8>(access))
			, property_id_(property_id)
			, optional_struct_id_(struct_id)
		{}
		Property(MemberFieldType type, PropertyID property_id, StructID struct_id, uint8 array_size = 0)
			: array_size_or_flags_(array_size)
			, is_sub_type(1)
			, type_(static_cast<uint8>(type))
			, constant_(0)
			, access_(0)
			, property_id_(property_id)
			, optional_struct_id_(struct_id)
		{}

		Property(const Property& other) = default;
		Property& operator=(const Property& other) = default;
		Property(Property&& other) = default;
		Property& operator=(Property&& other) = default;
		~Property() = default;
	};

	struct Structure
	{
		StructID id_ = kWrongID;
		StructID super_id_ = kWrongID;
		std::vector<Property> properties_; //sorted by id
		DEBUG_ONLY(std::string name_);
	private: 
		Structure(StructID id) : id_(id) {}

	public:
		static Structure& CreateStructure(const StructID id);
		static const Structure& GetStructure(const StructID id);
	};

	class Object
	{
	public: 
		static StructID StaticGetReflectionStructureID()
		{ 
			return HashString32("Object");
		}

		virtual StructID GetReflectionStructureID() const
		{ 
			return StaticGetReflectionStructureID(); 
		}

		const Structure& GetReflectionStructure() const
		{
			return Structure::GetStructure(GetReflectionStructureID());
		}

		static Structure& StaticRegisterStructure()
		{
			return Structure::CreateStructure(StaticGetReflectionStructureID());
		}

		virtual ~Object() = default;
	};

	template<class C> struct RegisterStruct
	{
		RegisterStruct(DEBUG_ONLY(const char* name))
		{
			C::StaticRegisterStructure()DEBUG_ONLY(.name_ = name);
		}
	};

	template<typename T> struct is_vector : public std::false_type {};
	template<typename T, typename A> struct is_vector<std::vector<T, A>> : public std::true_type {};

	template<typename T> struct is_map : public std::false_type {};
	template<typename K, typename T, typename P, typename A> struct is_map<std::map<K, T, P, A>> : public std::true_type {};

	template<typename T> struct is_std_array : public std::false_type {};
	template < class T, size_t N > struct is_std_array<std::array<T, N>> : public std::true_type {};

	template<class T> struct array_element {};
	template<class T, size_t N> struct array_element<T[N]> { using type = T; };
	template<class T, size_t N> struct array_element<std::array<T, N>> { using type = T; };

	template<class T> struct array_length {};
	template<class T, size_t N> struct array_length<T[N]> { static constexpr size_t value = N; };
	template<class T, size_t N> struct array_length<std::array<T, N>> { static constexpr size_t value = N; };

	template<typename M> constexpr MemberFieldType GetMemberType()
	{
		static_assert(std::is_class<M>::value || std::is_array<M>::value, "Unknown type!");

		if constexpr(std::is_array<M>::value || is_std_array<M>::value)
			return MemberFieldType::Array;
		if constexpr(is_vector<M>::value)
			return MemberFieldType::Container;
		if constexpr (is_map<M>::value)
			return MemberFieldType::Map;

		return MemberFieldType::Struct;
	}
	template<> constexpr MemberFieldType GetMemberType<int8>()			{ return MemberFieldType::Int8; }
	template<> constexpr MemberFieldType GetMemberType<uint8>()			{ return MemberFieldType::UInt8; }
	template<> constexpr MemberFieldType GetMemberType<int16>()			{ return MemberFieldType::Int16; }
	template<> constexpr MemberFieldType GetMemberType<uint16>()		{ return MemberFieldType::UInt16; }
	template<> constexpr MemberFieldType GetMemberType<int32>()			{ return MemberFieldType::Int32; }
	template<> constexpr MemberFieldType GetMemberType<uint32>()		{ return MemberFieldType::UInt32; }
	template<> constexpr MemberFieldType GetMemberType<int64>()			{ return MemberFieldType::Int64; }
	template<> constexpr MemberFieldType GetMemberType<uint64>()		{ return MemberFieldType::UInt64; }
	template<> constexpr MemberFieldType GetMemberType<float>()			{ return MemberFieldType::Float; }
	template<> constexpr MemberFieldType GetMemberType<double>()		{ return MemberFieldType::Double; }
	template<> constexpr MemberFieldType GetMemberType<std::string>()	{ return MemberFieldType::String; }
	template<> constexpr MemberFieldType GetMemberType<Object*>()		{ return MemberFieldType::ObjectPtr; }

	template<typename M> void CreateSubTypeProperty(Structure& structure, PropertyID property_id, uint8 optional_array_size = 0)
	{
		using M_not_const = std::remove_cv<M>::type;
		const constexpr MemberFieldType member_field_type = GetMemberType<M_not_const>();
		StructID struct_id = kWrongID;
		if constexpr(MemberFieldType::Struct == member_field_type || MemberFieldType::ObjectPtr == member_field_type)
		{
			struct_id = std::remove_pointer<M_not_const>::type::StaticGetReflectionStructureID();
		}

		structure.properties_.emplace_back(Property(member_field_type, property_id, struct_id, optional_array_size));

		if constexpr(member_field_type == MemberFieldType::Array)
		{
			CreateSubTypeProperty<array_element<M_not_const>::type>(structure, property_id, array_length<M_not_const>::value);
		}
		else if constexpr(member_field_type == MemberFieldType::Container)
		{
			CreateSubTypeProperty<M_not_const::value_type>(structure, property_id);
		}
		else if constexpr(member_field_type == MemberFieldType::Map)
		{
			CreateSubTypeProperty<M_not_const::key_type>(structure, property_id);
			CreateSubTypeProperty<M_not_const::mapped_type>(structure, property_id);
		}
	}
	template<typename M> void CreateProperty(Structure& structure, const char* name, uint16 offset)
	{
		using M_not_const = std::remove_cv<M>::type;
		const constexpr MemberFieldType member_field_type = GetMemberType<M_not_const>();
		StructID struct_id = kWrongID;
		if constexpr(MemberFieldType::Struct == member_field_type || MemberFieldType::ObjectPtr == member_field_type)
		{
			struct_id = std::remove_pointer<M_not_const>::type::StaticGetReflectionStructureID();
		}

		const PropertyID property_id = HashString32(name);
		Property p(offset
			, member_field_type
			, std::is_const<M>::value ? Reflection::ConstSpecifier::Const : Reflection::ConstSpecifier::NotConst
			, Reflection::AccessSpecifier::Public
			, property_id
			, struct_id);
		DEBUG_ONLY(p.name_ = name);
		structure.properties_.emplace_back(p);

		if constexpr(member_field_type == MemberFieldType::Array)
		{
			CreateSubTypeProperty<array_element<M_not_const>::type>(structure, property_id, array_length<M_not_const>::value);
		}
		else if constexpr(member_field_type == MemberFieldType::Container)
		{
			CreateSubTypeProperty<M_not_const::value_type>(structure, property_id);
		}
		else if constexpr(member_field_type == MemberFieldType::Map)
		{
			CreateSubTypeProperty<M_not_const::key_type>(structure, property_id);
			CreateSubTypeProperty<M_not_const::mapped_type>(structure, property_id);
		}
	}
}

#define IMPLEMENT_VIRTUAL_REFLECTION(struct_name) static Reflection::StructID StaticGetReflectionStructureID() \
	{ return Reflection::HashString32(#struct_name); } \
	Reflection::StructID GetReflectionStructureID() const override \
	{ return StaticGetReflectionStructureID(); }

#define IMPLEMENT_STATIC_REFLECTION(struct_name) static Reflection::StructID StaticGetReflectionStructureID() \
	{ return Reflection::HashString32(#struct_name); } \
	static const Reflection::Structure& StaticGetReflectionStructure() \
	{ return Reflection::Structure::GetStructure(StaticGetReflectionStructureID()); }

#define REGISTER_STRUCTURE(struct_name) Reflection::RegisterStruct<struct_name> UNIQUE_NAME(RegisterStruct) DEBUG_ONLY((#struct_name));

#define DEFINE_PROPERTY(struct_name, field_name) Reflection::CreateProperty<decltype(field_name)>(structure, #field_name, offsetof(struct_name, field_name))