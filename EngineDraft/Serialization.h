#pragma once
#include "reflection.h"

namespace Serialization
{
	struct DataDiff
	{
		//contains a diff of object state
		//can fill an object
		//can be added or substracted
	};

	class DataSource
	{
		//Abstrac class. Represent source of data (stream, binary file, text file, database, etc..), used to create an instance of DataDiff
	};
}