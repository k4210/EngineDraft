#pragma once

#include "utils.h"
#include "reflection.h"

namespace serialization
{
	class ObjectArchive;
}

namespace asset
{
	using AssetId = uint64;
	constexpr uint64 kWrongID64 = 0xFFFFFFFFFFFFFFFF;

	class Asset
	{
	public:
		AssetId asset_id_ = kWrongID64;

		virtual ~Asset() = default;
	};

	class AssetManager
	{
		std::map<AssetId, std::shared_ptr<Asset>> assets_in_memory_;
		std::map<AssetId, std::string> all_paths_;
	public:
		AssetManager& Get();

		void ScanAssets();
		//
		AssetId PathToAssetId(const std::string& path) const;
		std::shared_ptr<serialization::ObjectArchive> GetObjectArchive(AssetId asset_id, bool load_if_not_found);
		void SaveObjectArchive(std::shared_ptr<serialization::ObjectArchive> oa);
		std::shared_ptr<serialization::ObjectArchive> CreateObjectArchive(const std::string& path);
		void UnloadAsset(AssetId asset_in);
	};
}