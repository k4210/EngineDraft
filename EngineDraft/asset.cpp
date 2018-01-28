#include "asset.h"
#include "utils.h"
#include <windows.h>
//#include <wrl.h>
//#include <process.h>
//#include <shellapi.h>

#include <experimental/filesystem>
namespace fs = std::filesystem;

using namespace asset;

AssetManager& AssetManager::Get()
{
	static AssetManager instance;
	return instance;
}

static std::string GetContentPath()
{
	CHAR exe_path[512];
	const DWORD size = GetModuleFileName(nullptr, exe_path, _countof(exe_path));
	if (size == 0 || size == _countof(exe_path))
	{
		// Method failed or path was truncated.
		throw std::exception();
	}

	CHAR* lastSlash = strrchr(exe_path, '\\');
	if (lastSlash)
	{
		*(lastSlash + 1) = '\0';
	}

	std::string content_path = exe_path;
	content_path += "..\\content\\";
	return content_path;
}

AssetId AssetManager::PathToAssetId(const std::string& path) const
{
	/*
	Sample paths:
	"filename.ast"
	"filename.txt" don't conflict with .ast, the extension is included in Id
	"characters\warrior_dwarf\face.ast"
	"maps\mapname\lvl.ast"
	"config\video.txt"
	*/
	//Todo: validate path
	return HashString64(path.c_str());
}

std::shared_ptr<serialization::ObjectArchive> AssetManager::GetObjectArchive(AssetId , bool)
{

	return nullptr;
}

void AssetManager::SaveObjectArchive(std::shared_ptr<serialization::ObjectArchive>)
{

}

std::shared_ptr<serialization::ObjectArchive> AssetManager::CreateObjectArchive(const std::string&)
{
	return nullptr;
}

void AssetManager::ScanAssets()
{
	fs::path content_path(GetContentPath());
}