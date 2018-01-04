#include "reflection.h"
#include "data_template.h"
#include "data_storage.h"
#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/prettywriter.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

struct StructSample
{
	IMPLEMENT_STATIC_REFLECTION(StructSample);

	int32 integer_ = 0;

	StructSample() = default;
	StructSample(int32 i) : integer_(i) {}

	static reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = reflection::Structure::CreateStructure(StaticGetReflectionStructureID(), sizeof(StructSample));
		DEFINE_PROPERTY(StructSample, integer_);
		return structure;
	}
};
REGISTER_STRUCTURE(StructSample);

bool operator < (const StructSample& a, const StructSample& b)
{
	return a.integer_ < b.integer_;
}

//Stubs for linker loader..
reflection::ObjectID GetObjectID_Stub(const reflection::Object* obj)
{
	return (nullptr == obj) ? reflection::kNullObjectID : reinterpret_cast<uint64>(obj);
}

reflection::Object* GetObjectFromID_Stub(reflection::ObjectID id)
{
	return (reflection::kNullObjectID == id) ? nullptr : reinterpret_cast<reflection::Object*>(id);
}

class ObjSample : public reflection::Object
{
public:
	std::string string_;
	ObjSample* obj_ = nullptr;
	StructSample sample_;
	std::vector<StructSample> vec_;
	std::map<StructSample, int64> map_;
	StructSample arr1_[4];
	std::array<ObjSample*, 4> arr2_;

	ObjSample()
	{
		arr2_.fill(nullptr);
	}

	IMPLEMENT_VIRTUAL_REFLECTION(ObjSample);

	static reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = reflection::Structure::CreateStructure(StaticGetReflectionStructureID(), sizeof(ObjSample), reflection::Object::StaticGetReflectionStructureID());
		structure.get_obj_id = GetObjectID_Stub;
		structure.obj_from_id = GetObjectFromID_Stub;
		DEFINE_PROPERTY(ObjSample, string_);
		DEFINE_PROPERTY(ObjSample, obj_);
		DEFINE_PROPERTY(ObjSample, sample_);
		DEFINE_PROPERTY(ObjSample, vec_);
		DEFINE_PROPERTY(ObjSample, map_);
		DEFINE_PROPERTY(ObjSample, arr1_);
		DEFINE_PROPERTY(ObjSample, arr2_);
		Assert(structure.Validate());
		return structure;
	}
};
REGISTER_STRUCTURE(ObjSample);

class ObjAdvanced : public ObjSample
{
public:
	std::string adv_string_ = "yay";

	ObjAdvanced() = default;

	IMPLEMENT_VIRTUAL_REFLECTION(ObjAdvanced);

	static reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = reflection::Structure::CreateStructure(StaticGetReflectionStructureID(), sizeof(ObjAdvanced), ObjSample::StaticGetReflectionStructureID());
		structure.get_obj_id = GetObjectID_Stub;
		structure.obj_from_id = GetObjectFromID_Stub;
		DEFINE_PROPERTY(ObjAdvanced, adv_string_);
		Assert(structure.Validate());
		return structure;
	}
};
REGISTER_STRUCTURE(ObjAdvanced);

void PrintDataTemplate(const serialization::DataTemplate& dt)
{
	rapidjson::StringBuffer sb;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

	serialization::JsonDataStorage data_storage;
	data_storage.Save(writer, dt);

	std::cout << sb.GetString();
	std::cout << "\n";
}

void PrintStructure(const reflection::Structure& structure, bool print_label)
{
	if (print_label)
	{
		std::cout << "no   use    name             type     prop_id  struct_id num  access   const    offset   flags\n";
	}

	auto* super_struct = structure.TryGetSuperStructure();
	if (super_struct)
	{
		PrintStructure(*super_struct, false);
	}

	
	for (reflection::PropertyIndex i = 0; i < structure.GetNumberOfProperties(); i++)
	{
		std::cout << std::setfill(' ') << std::setw(4) << i << ' ';
		std::cout << structure.GetProperty(i).ToString();
	}
	if (print_label)
	{
		std::cout << "\n";
	}
}

int main()
{
	std::ofstream out(fs::path("out.txt"), std::ofstream::out);
	std::streambuf *coutbuf = std::cout.rdbuf(); //save old buf
	std::cout.rdbuf(out.rdbuf()); //redirect std::cout to out.txt!
	{
		serialization::DataTemplate data_template;
		{
			ObjAdvanced obj;
			PrintStructure(reflection::Structure::GetStructure(obj.GetReflectionStructureID()), true);
			obj.vec_.emplace_back(StructSample(3));
			obj.vec_.emplace_back(StructSample());
			obj.arr1_[2].integer_ = 9;
			obj.arr2_[1] = &obj;
			obj.map_[StructSample(1)] = 4;
			obj.map_[StructSample(2)] = 8;
			obj.map_[StructSample(3)] = 16;

			data_template.Save(&obj, serialization::SaveFlags::SaveNativeDefaultValues);//Flag32<serialization::SaveFlags>());// 
			PrintDataTemplate(data_template);

			std::cout.flush();
			out.flush();

			data_template.RefreshAfterLayoutChanged(obj.GetReflectionStructureID());
			PrintDataTemplate(data_template);

			std::cout.flush();
			out.flush();
		}
		/*
		ObjAdvanced obj_clone;
		data_template.Load(&obj_clone);

		serialization::DataTemplate data_template_clone;
		data_template_clone.Save(&obj_clone, serialization::SaveFlags::SaveNativeDefaultValues);//Flag32<serialization::SaveFlags>());//
		PrintDataTemplate(data_template_clone);
		*/
	}
	std::cout.rdbuf(coutbuf); //reset to standard output again

	return 0;
}