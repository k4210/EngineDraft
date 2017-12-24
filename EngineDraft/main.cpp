#include "reflection.h"

struct StructSample
{
	IMPLEMENT_STATIC_REFLECTION(StructSample);

	const int32 integer_ = 0;

	static Reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = Reflection::Structure::CreateStructure(StaticGetReflectionStructureID());
		Reflection::CreateProperty<decltype(integer_)>(structure, "integer_", offsetof(StructSample, integer_));
		return structure;
	}
};
REGISTER_STRUCTURE(StructSample);

class ObjSample : public Reflection::Object
{
public:
	std::string string_;
	Reflection::Object* obj_ = nullptr;
	StructSample sample_;
	std::vector<StructSample> vec_;
	std::map<StructSample, Reflection::Object*> map_;
	IMPLEMENT_VIRTUAL_REFLECTION(ObjSample);

	static Reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = Reflection::Structure::CreateStructure(StaticGetReflectionStructureID());
		Reflection::CreateProperty<decltype(string_)>(structure, "string_", offsetof(ObjSample, string_));
		Reflection::CreateProperty<decltype(obj_)>(structure, "obj_", offsetof(ObjSample, obj_));
		Reflection::CreateProperty<decltype(sample_)>(structure, "sample_", offsetof(ObjSample, sample_));
		Reflection::CreateProperty<decltype(vec_)>(structure, "vec_", offsetof(ObjSample, vec_));
		Reflection::CreateProperty<decltype(map_)>(structure, "map_", offsetof(ObjSample, map_));
		return structure;
	}
};
REGISTER_STRUCTURE(ObjSample);

int main()
{
	ObjSample obj;
	auto& structure = obj.GetReflectionStructure();
	printf("Property\tname\tType\tprop_id\t\tstruct_id\taccess\tconst\toffset\n");
	for (auto& p : structure.properties_)
	{
		printf(p.ToString().c_str());
	}
	getchar();
	return 0;
}