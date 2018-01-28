#include "text_serialization.h"
#include "data_template.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/prettywriter.h"

std::string serialization::DataTemplate::ToString() const
{
	rapidjson::StringBuffer sb;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

	serialization::JsonDataStorage data_storage;
	data_storage.Save(writer, *this);

	return sb.GetString();
}