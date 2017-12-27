#include "reflection.h"
#include "data_template.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

struct StructSample
{
	IMPLEMENT_STATIC_REFLECTION(StructSample);

	const int32 integer_ = 0;

	static reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = reflection::Structure::CreateStructure(StaticGetReflectionStructureID());
		structure.size_ = sizeof(StructSample);
		DEFINE_PROPERTY(StructSample, integer_);
		return structure;
	}
};
REGISTER_STRUCTURE(StructSample);

//Stubs for linker loader..
reflection::ObjectID GetObjectID_Stub(const reflection::Object*)
{
	return reflection::kNullObjectID;
}

reflection::Object* GetObjectFromID_Stub(reflection::ObjectID)
{
	return nullptr;
}

class ObjSample : public reflection::Object
{
public:
	std::string string_;
	reflection::Object* obj_ = nullptr;
	StructSample sample_;
	std::vector<StructSample> vec_;
	std::map<StructSample, int64> map_;
	StructSample arr1_[4];
	std::array<reflection::Object*, 4> arr2_;

	ObjSample()
	{
		arr2_.fill(nullptr);
	}

	IMPLEMENT_VIRTUAL_REFLECTION(ObjSample);

	static reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = reflection::Structure::CreateStructure(StaticGetReflectionStructureID());
		structure.super_id_ = reflection::Object::StaticGetReflectionStructureID();
		structure.size_ = sizeof(ObjSample);
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


int main()
{
	std::ofstream out(fs::path("out.txt"), std::ofstream::out);
	std::streambuf *coutbuf = std::cout.rdbuf(); //save old buf
	std::cout.rdbuf(out.rdbuf()); //redirect std::cout to out.txt!
	{
		ObjSample obj;
		auto& structure = reflection::Structure::GetStructure(obj.GetReflectionStructureID());
		std::cout << "no   use    name             type     prop_id  struct_id num  access   const    offset   flags\n";
		uint32 i = 0;
		for (auto& p : structure.properties_)
		{
			std::cout << std::setfill(' ') << std::setw(4) << i << ' ';
			std::cout << p.ToString();
			i++;
		}

		Serialization::DataTemplate data_template;
		data_template.Save(&obj);
	}
	std::cout.rdbuf(coutbuf); //reset to standard output again

	return 0;
}