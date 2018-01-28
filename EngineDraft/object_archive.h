#pragma once
#include "data_template.h"
#include "asset.h"

namespace game
{
	class GameObject;
	class World;
}

namespace serialization
{
	using namespace reflection;
	using namespace asset;

	enum class ObjectArchiveFlags : uint32
	{
		None = 0,
		DefaultData = 1 << 0,
	};

	class ObjectArchive : public Asset, public ObjectSolver
	{
		struct SingleObjectArchive
		{
			ObjectID object_id_ = kWrongID;
			std::string name_;
			AssetId base_archive_id_ = kWrongID64;
			ObjectID id_in_base_archive_ = kWrongID;

			DataTemplate diff_against_base_;
			DataTemplate optional_merged_with_base_;

			void Save(std::ostream& os) const
			{
				os << object_id_ << name_ << base_archive_id_ << id_in_base_archive_ << diff_against_base_;
			}

			void Load(std::istream& is)
			{
				is >> object_id_ >> name_ >> base_archive_id_ >> id_in_base_archive_ >> diff_against_base_;
			}
		};

		Flag32<ObjectArchiveFlags> flags_;
		std::vector<SingleObjectArchive> data_templates;

	public:
		std::vector<game::GameObject*> CreateObjects(game::World* owner);

		~ObjectArchive() = default;

		friend std::istream& operator>> (std::istream& is, ObjectArchive& arch);
		friend std::ostream& operator<< (std::ostream& os, const ObjectArchive& arch);
	};

	std::istream& operator>> (std::istream& is, ObjectArchive& arch)
	{
		is >> arch.flags_;
		uint32 dt_num = 0;
		is >> dt_num;
		Assert(arch.data_templates.empty());
		arch.data_templates.resize(dt_num);
		for (uint32 i = 0; i < dt_num; i++)
		{
			arch.data_templates[i].Load(is);
		}
		return is;
	}
	std::ostream& operator<< (std::ostream& os, const ObjectArchive& arch)
	{
		os << arch.flags_;
		os << static_cast<uint32>(arch.data_templates.size());
		for (uint32 i = 0; i < arch.data_templates.size(); i++)
		{
			arch.data_templates[i].Save(os);
		}
		return os;
	}
}