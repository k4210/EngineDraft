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
		DEFINE_PROPERTY(ObjAdvanced, adv_string_);
		Assert(structure.Validate());
		return structure;
	}
};
REGISTER_STRUCTURE(ObjAdvanced);

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
		serialization::DataTemplate data_template; //higher
		std::string str_0;
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

			data_template.SaveFromObject(&obj, serialization::SaveFlags::None);
			str_0 = data_template.ToString();
			std::cout << str_0 << "\n";

			data_template.RefreshAfterLayoutChanged(obj.GetReflectionStructureID());
			const auto str_1 = data_template.ToString();
			std::cout << str_1 << "\n";
			Assert(str_0 == str_1);
		}
		
		{
			ObjAdvanced obj_clone;
			data_template.LoadIntoObject(&obj_clone);

			serialization::DataTemplate data_template_2;
			data_template_2.SaveFromObject(&obj_clone, serialization::SaveFlags::None);
			const auto str_2 = data_template_2.ToString();
			std::cout << str_2 << "\n";
			Assert(str_0 == str_2);
		}
		/*
		serialization::DataTemplate data_template_lower; //higher
		{
			ObjSample obj;
			obj.string_ = "lower_value";
			data_template_lower.SaveFromObject(&obj, serialization::SaveFlags::None);
			PrintDataTemplate(data_template_lower);
		}

		//const serialization::DataTemplate data_template_clone = data_template.Clone();
		//data_template_clone.SaveFromObject(&obj_clone, serialization::SaveFlags::None);//Flag32<serialization::SaveFlags>());//
		const serialization::DataTemplate data_template_diff = serialization::DataTemplate::Diff(data_template, data_template_lower);
		PrintDataTemplate(data_template_diff);
		*/
		
	}
	std::cout.rdbuf(coutbuf); //reset to standard output again

	return 0;
}