#pragma once
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <array>
#include "utils.h"

namespace reflection
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
		Vector,	// requires one handler and one sub-type 
		Map,		// requires one handler and two sub-types
		Array,		// requires one sub-type and size
		//WeakPtr, AssetPtr, etc...
		__NUM
	};
	static_assert(FitsInBits(static_cast<uint32>(MemberFieldType::__NUM), 5));

	enum class AccessSpecifier
	{
		Private = 0,
		Protected = 1,
		Public = 2
	};

	enum class ConstSpecifier
	{
		Const = 0,
		NotConst = 1
	};

	using PropertyID = uint32;
	using StructID = uint32;
	using ObjectID = uint64;
	constexpr uint32 kWrongID = 0xFFFFFFFF;
	constexpr uint64 kNullObjectID = 0xFFFFFFFFFFFFFFFF;

	enum class EPropertyUsage
	{
		Main = 0,
		Handler = 1,
		SubType = 2,
	};

	struct IHandler
	{
		typedef void(*TCreateHandler)(uint8*);

		virtual uint32 GetNumElements(const uint8*) const = 0;

		//Vec
		virtual uint8* GetElement(uint8*, uint32) const { Assert(false); return nullptr; };
		virtual const uint8* GetElement(const uint8*, uint32) const { Assert(false); return nullptr; };

		//Map
		virtual const uint8* GetKey(const uint8*, uint32) const { Assert(false); return nullptr; }
		virtual uint8* GetValue(uint8*, uint32) const { Assert(false); return nullptr; }
		virtual const uint8* GetValue(const uint8*, uint32) const { Assert(false); return nullptr; }

		virtual ~IHandler() = default;
	};

	namespace details
	{
		constexpr uint32 HashString32(const char* const str, const uint32 value = 0x811c9dc5) noexcept
		{
			return (str[0] == '\0') ? value : HashString32(&str[1], (value ^ uint32(str[0])) * 0x1000193);
		}

		constexpr uint64 HashString64(const char* const str)
		{
			uint64 hash = 0xcbf29ce484222325;
			const uint64 prime = 0x100000001b3;
			for (const char* it = str; *it != '\0'; it++)
			{
				hash = (hash ^ uint64(*it)) * prime;
			}
			return hash;
		}
	};

	class Property
	{
		uint16 offset_ = 0;
		uint8 flags_ = 0;	// declared outside reflection system
		uint8 type_			: 5;	//MemberFieldType
		uint8 constant_		: 1;	//ConstSpecifier
		uint8 access_		: 2;	//AccessSpecifier
		std::array<uint8, 8> handler_data_;
		static_assert(sizeof(std::array<uint8, 8>) == (sizeof(PropertyID) + sizeof(StructID)));
		PropertyID& PropertyIDRef()
		{
			return *reinterpret_cast<PropertyID*>(&handler_data_[0]);
		}
		const PropertyID& PropertyIDRef() const
		{
			return *reinterpret_cast<const PropertyID*>(&handler_data_[0]);
		}
		StructID& StructIDOrArraySIzeRef()
		{
			return *reinterpret_cast<StructID*>(&handler_data_[sizeof(PropertyID)]);
		}
		const StructID& StructIDOrArraySIzeRef() const
		{
			return *reinterpret_cast<const StructID*>(&handler_data_[sizeof(PropertyID)]);
		}

		static const uint16 kHandlerOffsetValue = 0xFFFF;
		static const uint16 kSubtypeOffsetValue = 0xFFFE;
	public:
		DEBUG_ONLY(std::string name_);
		std::string ToString() const;

		EPropertyUsage	GetPropertyUsage()		const 
		{ 
			switch (offset_)
			{
			case kHandlerOffsetValue: return EPropertyUsage::Handler;
			case kSubtypeOffsetValue: return EPropertyUsage::SubType;
			}
			return EPropertyUsage::Main;
		}
		uint32			GetFieldOffset()		const { return offset_; }
		PropertyID		GetPropertyID()			const { return PropertyIDRef(); }
		MemberFieldType GetFieldType()			const { return static_cast<MemberFieldType>(type_); }
		ConstSpecifier	GetConstSpecifier()		const { return static_cast<ConstSpecifier>(constant_); }
		AccessSpecifier GetAccessSpecifier()	const { return static_cast<AccessSpecifier>(access_); }
		uint8			GetFlags()				const { return flags_; }
		uint32			GetArraySize()			const 
		{ 
			Assert(MemberFieldType::Array == GetFieldType()); 
			return StructIDOrArraySIzeRef();
		}
		StructID		GetOptionalStructID()	const 
		{ 
			Assert(MemberFieldType::Struct == GetFieldType() || MemberFieldType::ObjectPtr == GetFieldType());
			return StructIDOrArraySIzeRef();
		}
		const IHandler&	GetHandler()			const
		{
			Assert(EPropertyUsage::Handler == GetPropertyUsage());
			return *reinterpret_cast<const IHandler*>(handler_data_.data());
		}
	private:
		IHandler*		GetHandler()
		{
			Assert(EPropertyUsage::Handler == GetPropertyUsage());
			return reinterpret_cast<IHandler*>(handler_data_.data());
		}

	public:
		Property(uint16 offset, MemberFieldType type, ConstSpecifier constant, AccessSpecifier access
			, PropertyID property_id, uint8 flags, StructID optional_struct_id_or_num = kWrongID)
			: offset_(offset)
			, flags_(flags)
			, type_(static_cast<uint8>(type))
			, constant_(static_cast<uint8>(constant))
			, access_(static_cast<uint8>(access))
		{
			PropertyIDRef() = property_id;
			StructIDOrArraySIzeRef() = optional_struct_id_or_num;
			Assert(EPropertyUsage::Main == GetPropertyUsage());
		}

		Property(MemberFieldType type, PropertyID property_id, StructID optional_struct_id_or_num = kWrongID)
			: offset_(kSubtypeOffsetValue)
			, type_(static_cast<uint8>(type))
			, constant_(0) 
			, access_(0)
		{
			PropertyIDRef() = property_id;
			StructIDOrArraySIzeRef() = optional_struct_id_or_num;
			Assert(EPropertyUsage::SubType == GetPropertyUsage());
		}

		Property(IHandler::TCreateHandler create_func)
			: type_(0)
			, constant_(0)
			, access_(0)
			, offset_(kHandlerOffsetValue)
		{
			create_func(handler_data_.data());
		}

		Property(const Property& other) = default;
		Property& operator=(const Property& other) = default;
		Property(Property&& other) = default;
		Property& operator=(Property&& other) = default;
		~Property()
		{
			if (EPropertyUsage::Handler == GetPropertyUsage())
			{
				GetHandler()->~IHandler();
			}
		}
	};

	class Object;
	struct Structure
	{
		StructID id_ = kWrongID;
		StructID super_id_ = kWrongID;
		std::vector<Property> properties_; //sorted by offset of main prop
		uint32 size_ = 0;
		DEBUG_ONLY(std::string name_);

		typedef ObjectID(*GetCustomObjID)(const Object* obj);
		GetCustomObjID get_obj_id = nullptr;
		typedef Object* (*ObjFromID)(ObjectID);
		ObjFromID obj_from_id = nullptr;

	private: 
		Structure(StructID id) : id_(id) {}

	public:
		static Structure& CreateStructure(const StructID id);
		static const Structure& GetStructure(const StructID id);
		bool RepresentsObjectClass() const
		{
			return nullptr != get_obj_id && nullptr != obj_from_id && kWrongID != super_id_;
		}
		bool RepresentNonObjectStructure() const
		{
			return nullptr == get_obj_id 
				&& nullptr == obj_from_id 
				&& kWrongID == super_id_
				&& !properties_.empty();
		}
		bool Validate() const;
	};

	class Object
	{
	public: 
		static StructID StaticGetReflectionStructureID()
		{ 
			return details::HashString32("Object");
		}

		virtual StructID GetReflectionStructureID() const
		{ 
			return StaticGetReflectionStructureID(); 
		}

		static Structure& StaticRegisterStructure()
		{
			return Structure::CreateStructure(StaticGetReflectionStructureID());
		}

		virtual ~Object() = default;
	};

	uint32 NextPropertyIndexOnThisLevel(const std::vector<Property>& properties, uint32 idx);

	namespace details
	{
		template<class C> struct RegisterStruct
		{
			RegisterStruct(DEBUG_ONLY(const char* name))
			{
				C::StaticRegisterStructure()DEBUG_ONLY(.name_ = name);
			}
		};

		template<typename V> struct VectorHandler : public IHandler
		{
			static void CreateHandler(uint8* ptr)
			{
				new (ptr) VectorHandler<V>();
			}

			static const V& GetVector(const uint8* vec_ptr)
			{
				Assert(vec_ptr);
				return *reinterpret_cast<const V*>(vec_ptr);
			}

			static V& GetVector(uint8* vec_ptr)
			{
				Assert(vec_ptr);
				return *reinterpret_cast<V*>(vec_ptr);
			}

			uint32 GetNumElements(const uint8* vec_ptr) const override
			{
				return GetVector(vec_ptr).size();
			}

			uint8* GetElement(uint8* vec_ptr, uint32 element_index) const override
			{
				return reinterpret_cast<uint8*>(&(GetVector(vec_ptr)[element_index]));
			}

			const uint8* GetElement(const uint8* vec_ptr, uint32 element_index) const override
			{
				return reinterpret_cast<const uint8*>(&(GetVector(vec_ptr)[element_index]));
			}

			virtual ~VectorHandler() = default;
		};

		template<class M> struct MapHandler : public IHandler
		{
			static void CreateHandler(uint8* ptr)
			{
				new (ptr) MapHandler<M>();
			}

			static auto GetCIter(const uint8* map_ptr, uint32 idx)
			{
				Assert(map_ptr);
				auto it = reinterpret_cast<const M*>(map_ptr)->begin();
				std::advance(it, idx);
				return it;
			}

			static auto GetIter(uint8* map_ptr, uint32 idx)
			{
				Assert(map_ptr);
				auto it = reinterpret_cast<M*>(map_ptr)->begin();
				std::advance(it, idx);
				return it;
			}

			uint32 GetNumElements(const uint8* map_ptr) const override
			{
				return reinterpret_cast<const M*>(map_ptr)->size();
			}
			//canot get writable/mutable key
			const uint8* GetKey(const uint8* map_ptr, uint32 index) const 
			{ 
				return reinterpret_cast<const uint8*>(&GetCIter(map_ptr, index)->first);
			}
			uint8* GetValue(uint8* map_ptr, uint32 index) const 
			{ 
				return reinterpret_cast<uint8*>(&GetIter(map_ptr, index)->second);
			}
			const uint8* GetValue(const uint8* map_ptr, uint32 index) const 
			{ 
				return reinterpret_cast<const uint8*>(&GetCIter(map_ptr, index)->second);
			}

			virtual ~MapHandler() = default;
		};

		template<typename M> constexpr MemberFieldType GetMemberType()
		{
			static_assert(std::is_class<M>::value || std::is_array<M>::value, "Unknown type!");

			if constexpr(std::is_array<M>::value || is_std_array<M>::value)
				return MemberFieldType::Array;
			else if constexpr(is_vector<M>::value)
				return MemberFieldType::Vector;
			else if constexpr (is_map<M>::value)
				return MemberFieldType::Map;
			else
				return MemberFieldType::Struct;
		}
		template<> constexpr MemberFieldType GetMemberType<int8>() { return MemberFieldType::Int8; }
		template<> constexpr MemberFieldType GetMemberType<uint8>() { return MemberFieldType::UInt8; }
		template<> constexpr MemberFieldType GetMemberType<int16>() { return MemberFieldType::Int16; }
		template<> constexpr MemberFieldType GetMemberType<uint16>() { return MemberFieldType::UInt16; }
		template<> constexpr MemberFieldType GetMemberType<int32>() { return MemberFieldType::Int32; }
		template<> constexpr MemberFieldType GetMemberType<uint32>() { return MemberFieldType::UInt32; }
		template<> constexpr MemberFieldType GetMemberType<int64>() { return MemberFieldType::Int64; }
		template<> constexpr MemberFieldType GetMemberType<uint64>() { return MemberFieldType::UInt64; }
		template<> constexpr MemberFieldType GetMemberType<float>() { return MemberFieldType::Float; }
		template<> constexpr MemberFieldType GetMemberType<double>() { return MemberFieldType::Double; }
		template<> constexpr MemberFieldType GetMemberType<std::string>() { return MemberFieldType::String; }
		template<> constexpr MemberFieldType GetMemberType<Object*>() { return MemberFieldType::ObjectPtr; }

		template<typename M> StructID GetArraySizeOrStructID()
		{
			StructID struct_id_or_array_size = kWrongID;
			const constexpr MemberFieldType member_field_type = GetMemberType<M>();
			if constexpr(MemberFieldType::Struct == member_field_type
				|| MemberFieldType::ObjectPtr == member_field_type)
			{
				struct_id_or_array_size = std::remove_pointer<M>::type::StaticGetReflectionStructureID();
			}
			else if constexpr(MemberFieldType::Array == member_field_type)
			{
				struct_id_or_array_size = array_length<M>::value;
			}
			return struct_id_or_array_size;
		}

		template<typename V> void CreateVectorHandlerProperty(Structure& structure)
		{
			static_assert(sizeof(VectorHandler<V>) <= 8);
			structure.properties_.emplace_back(Property(VectorHandler<V>::CreateHandler));
		}

		template<typename M> void CreateMapHandlerProperty(Structure& structure)
		{
			static_assert(sizeof(MapHandler<M>) <= 8);
			structure.properties_.emplace_back(Property(MapHandler<M>::CreateHandler));
		}

		template<typename M> void CreateSubTypePropertyOptional(Structure& structure, PropertyID property_id)
		{
			const constexpr MemberFieldType member_field_type = GetMemberType<M>();
			if constexpr(member_field_type == MemberFieldType::Array)
			{
				CreateSubTypeProperty<array_element<M>::type>(structure, property_id);
			}
			else if constexpr(member_field_type == MemberFieldType::Vector)
			{
				CreateVectorHandlerProperty<M>(structure);
				CreateSubTypeProperty<M::value_type>(structure, property_id);
			}
			else if constexpr(member_field_type == MemberFieldType::Map)
			{
				CreateMapHandlerProperty<M>(structure);
				CreateSubTypeProperty<M::key_type>(structure, property_id);
				CreateSubTypeProperty<M::mapped_type>(structure, property_id);
			}
			else
			{
				UNREFERENCED_PARAMETER(structure);
				UNREFERENCED_PARAMETER(property_id);
			}
		}

		template<typename MOrg> void CreateSubTypeProperty(Structure& structure, PropertyID property_id)
		{
			using M = std::remove_cv<MOrg>::type;
			structure.properties_.emplace_back(Property(GetMemberType<M>(), property_id, GetArraySizeOrStructID<M>()));
			CreateSubTypePropertyOptional<M>(structure, property_id);
		}

		template<typename MOrg> void CreateProperty(Structure& structure, const char* name, uint16 offset, uint8 flags)
		{
			using M = std::remove_cv<MOrg>::type;
			const PropertyID property_id = HashString32(name);
			Property p(offset, GetMemberType<M>()
				, std::is_const<M>::value ? ConstSpecifier::Const : ConstSpecifier::NotConst
				, AccessSpecifier::Public, property_id, flags, GetArraySizeOrStructID<M>());
			DEBUG_ONLY(p.name_ = name);
			structure.properties_.emplace_back(p);

			CreateSubTypePropertyOptional<M>(structure, property_id);
		}
	};
}

#define IMPLEMENT_VIRTUAL_REFLECTION(name) static reflection::StructID StaticGetReflectionStructureID() \
	{ return reflection::details::HashString32(#name); } \
	reflection::StructID GetReflectionStructureID() const override \
	{ return StaticGetReflectionStructureID(); }

#define IMPLEMENT_STATIC_REFLECTION(name) static reflection::StructID StaticGetReflectionStructureID() \
	{ return reflection::details::HashString32(#name); } \
	static const reflection::Structure& StaticGetReflectionStructure() \
	{ return reflection::Structure::GetStructure(StaticGetReflectionStructureID()); }

#define REGISTER_STRUCTURE(name) reflection::details::RegisterStruct<name> UNIQUE_NAME(RegisterStruct) \
	DEBUG_ONLY((#name));

#define DEFINE_PROPERTY(struct_name, field_name) reflection::details::CreateProperty<decltype(field_name)>(structure, \
	#field_name, offsetof(struct_name, field_name), 0)