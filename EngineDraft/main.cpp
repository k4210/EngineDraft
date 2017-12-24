#include "reflection.h"

struct StructSample
{
	IMPLEMENT_STATIC_REFLECTION(StructSample);

	const int32 integer_ = 0;

	static Reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = Reflection::Structure::CreateStructure(StaticGetReflectionStructureID());
		DEFINE_PROPERTY(StructSample, integer_);
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
	StructSample arr1_[4];
	std::array<Reflection::Object*, 4> arr2_;
	IMPLEMENT_VIRTUAL_REFLECTION(ObjSample);

	static Reflection::Structure& StaticRegisterStructure()
	{
		auto& structure = Reflection::Structure::CreateStructure(StaticGetReflectionStructureID());
		structure.super_id_ = Reflection::Object::StaticGetReflectionStructureID();
		DEFINE_PROPERTY(ObjSample, string_);
		DEFINE_PROPERTY(ObjSample, obj_);
		DEFINE_PROPERTY(ObjSample, sample_);
		DEFINE_PROPERTY(ObjSample, vec_);
		DEFINE_PROPERTY(ObjSample, map_);
		DEFINE_PROPERTY(ObjSample, arr1_);
		DEFINE_PROPERTY(ObjSample, arr2_);
		return structure;
	}
};
REGISTER_STRUCTURE(ObjSample);

int main()
{
	ObjSample obj;
	auto& structure = obj.GetReflectionStructure();
	printf("Property\tname\tType\tprop_id\t\tstruct_id\taccess\tconst\toffset\tflags/array_size\n");
	for (auto& p : structure.properties_)
	{
		printf(p.ToString().c_str());
	}
	getchar();
	return 0;
}