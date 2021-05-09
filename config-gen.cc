#include "config-generic.h"

using namespace config;
using namespace config::detail;

class Schema
{
	UCLPtr obj;
	public:
	Schema(ucl_object_t *o) : obj(o) {}
	std::string_view schema()
	{
		return StringViewAdaptor(obj["$schema"]);
	}
	std::string_view id()
	{
		return StringViewAdaptor(obj["$id"]);
	}
	std::string_view title()
	{
		return StringViewAdaptor(obj["title"]);
	}
	std::string_view description()
	{
		return StringViewAdaptor(obj["description"]);
	}
	enum Type
	{
		Object,
		String,
		Number
	};

	using TypeAdaptor = EnumAdaptor<Type, EnumValueMap<Enum{"object", Object},
	                                                   Enum{"string", String},
	                                                   Enum{"number", Number}>>;

	Type type()
	{
		return TypeAdaptor(obj["type"]);
	}


	/*
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://example.com/product.schema.json",
  "title": "Product",
  "description": "A product in the catalog",
  "type": "object"
  */

};

int main()
{
}

