#include "reflection.h"

//Reflection::RegisterStruct<Reflection::Object> yada1;

REGISTER_STRUCTURE(Reflection::Object);

namespace Reflection
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
}