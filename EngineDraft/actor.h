#pragma once

#include "reflection.h"

namespace game
{
	class GameObject : public reflection::Object
	{
		IMPLEMENT_VIRTUAL_REFLECTION(GameObject);

		static reflection::Structure& StaticRegisterStructure()
		{
			auto& structure = reflection::Structure::CreateStructure(
				StaticGetReflectionStructureID()
				, sizeof(GameObject)
				, reflection::Object::StaticGetReflectionStructureID());
			Assert(structure.Validate());
			return structure;
		}
	};

	class World : public GameObject
	{
		IMPLEMENT_VIRTUAL_REFLECTION(World);

		static reflection::Structure& StaticRegisterStructure()
		{
			auto& structure = reflection::Structure::CreateStructure(
				StaticGetReflectionStructureID()
				, sizeof(World)
				, GameObject::StaticGetReflectionStructureID());
			Assert(structure.Validate());
			return structure;
		}
	};
}