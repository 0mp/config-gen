#include "config-generic.h"
#include <iostream>
#include <sstream>
#include <unordered_set>

using namespace config;
using namespace config::detail;

namespace Schema
{
	class Object
	{
		protected:
		UCLPtr obj;

		public:
		enum Type
		{
			TypeObject,
			TypeString,
			TypeNumber,
			TypeInteger,
			TypeBool,
		};

		using TypeAdaptor =
		  EnumAdaptor<Type,
		              EnumValueMap<Enum{"object", TypeObject},
		                           Enum{"string", TypeString},
		                           Enum{"integer", TypeInteger},
		                           Enum{"boolean", TypeBool},
		                           Enum{"number", TypeNumber}>>;

		Object(const ucl_object_t *o) : obj(o) {}
		Type type()
		{
			return TypeAdaptor(obj["type"]);
		}

		std::string_view title()
		{
			return StringViewAdaptor(obj["title"]);
		}

		std::optional<std::string_view> description()
		{
			return make_optional<StringViewAdaptor>(obj["description"]);
		}

		using Properties =
		  Range<PropertyAdaptor<Object>, PropertyAdaptor<Object>, true>;

		Properties properties()
		{
			return Properties(obj["properties"]);
		}

		std::optional<Range<std::string_view, StringViewAdaptor>> required()
		{
			return make_optional<Range<std::string_view, StringViewAdaptor>>(
			  obj["required"]);
		}

		std::optional<double> minimum()
		{
			return make_optional<DoubleAdaptor>(obj["minimum"]);
		}

		std::optional<double> exclusiveMinimum()
		{
			return make_optional<DoubleAdaptor>(obj["exclusiveMinimum"]);
		}

		std::optional<double> maximum()
		{
			return make_optional<DoubleAdaptor>(obj["maximum"]);
		}

		std::optional<double> exclusiveMaximum()
		{
			return make_optional<DoubleAdaptor>(obj["exclusiveMaximum"]);
		}

		std::optional<double> multipleOf()
		{
			return make_optional<DoubleAdaptor>(obj["multipleOf"]);
		}
	};

	class Root : public Object
	{
		public:
		Root(ucl_object_t *o) : Object(o) {}
		std::string_view schema()
		{
			return StringViewAdaptor(obj["$schema"]);
		}

		std::string_view id()
		{
			return StringViewAdaptor(obj["$id"]);
		}

		static Root parse(const char *filename)
		{
			struct ucl_parser *p =
			  ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
			ucl_parser_add_file(p, filename);
			auto obj = ucl_parser_get_object(p);
			ucl_parser_free(p);
			Root root(obj);
			ucl_object_unref(obj);
			return root;
		}
	};
} // namespace Schema

using namespace Schema;

template<typename T>
void emit_class(Object o, std::string_view name, T &out)
{
	std::stringstream                    types;
	std::stringstream                    methods;
	std::stringstream                    nested_classes;
	std::unordered_set<std::string_view> required_properties;

	if (auto required = o.required())
	{
		for (auto prop : *required)
		{
			required_properties.insert(prop);
		}
	}

	out << "class " << name << "{ UCLPtr obj; public:\n";

	for (auto prop : o.properties())
	{
		std::string_view prop_name   = prop.key();
		std::string_view method_name = prop_name;

		std::string method_name_buffer;

		bool isRequired = required_properties.contains(prop_name);

		// FIXME: Do a proper regex match
		if (method_name.find('-') != std::string::npos)
		{
			method_name_buffer = method_name;
			std::replace(
			  method_name_buffer.begin(), method_name_buffer.end(), '-', '_');
			method_name = method_name_buffer;
		}

		if (auto description = prop.description())
		{
			methods << "\n/** " << *description << " */\n";
		}

		std::string_view return_type;
		std::string_view adaptor;
		bool             isInteger = false;
		switch (prop.type())
		{
			case Schema::Object::TypeString:
			{
				return_type = "std::string_view";
				adaptor     = "StringViewAdaptor";
				break;
			}
			case Schema::Object::TypeBool:
			{
				return_type = "bool";
				adaptor     = "BoolAdaptor";
				break;
			}
			case Schema::Object::TypeInteger:
			{
				isInteger = true;
				[[clang::fallthrough]];
			}
			case Schema::Object::TypeNumber:
			{
				if (!isInteger)
				{
					auto multipleOf = prop.multipleOf();
					if (multipleOf)
					{
						isInteger =
						  (((double)(uint64_t)*multipleOf) == *multipleOf);
					}
				}
				if (!isInteger)
				{
					return_type = "double";
					adaptor     = "DoubleAdaptor";
					break;
				}
				int64_t min = std::numeric_limits<int64_t>::min();
				int64_t max = std::numeric_limits<int64_t>::max();
				min         = std::max(
                  min, static_cast<int64_t>(prop.minimum().value_or(min)));
				min = std::max(
				  min,
				  static_cast<int64_t>(prop.exclusiveMinimum().value_or(min)));
				max = std::min(
				  max, static_cast<int64_t>(prop.maximum().value_or(max)));
				max = std::min(
				  max,
				  static_cast<int64_t>(prop.exclusiveMaximum().value_or(max)));
				auto try_type = [&](auto             intty,
				                    std::string_view ty,
				                    std::string_view a) {
					if ((min >= std::numeric_limits<decltype(intty)>::min()) &&
					    (max <= std::numeric_limits<decltype(intty)>::max()))
					{
						return_type = ty;
						adaptor     = a;
					}
				};
				try_type(int64_t(), "int64_t", "Int64Adaptor");
				try_type(uint64_t(), "uint64_t", "UInt64Adaptor");
				try_type(int32_t(), "int32_t", "Int32Adaptor");
				try_type(uint32_t(), "uint32_t", "UInt32Adaptor");
				try_type(int16_t(), "int16_t", "Int16Adaptor");
				try_type(uint16_t(), "uint16_t", "UInt16Adaptor");
				try_type(int8_t(), "int8_t", "Int8Adaptor");
				try_type(uint8_t(), "uint8_t", "UInt8Adaptor");

				break;
			}
			case Schema::Object::TypeObject:
				break;
		}
		if (isRequired)
		{
			methods << return_type << ' ' << method_name
			        << "() const [[clang::lifetimebound]] {";
			methods << "return config::detail::" << adaptor << "(obj[\""
			        << prop_name << "\"]);}";
		}
		else
		{
			methods << "std::optional<" << return_type << "> " << method_name
			        << "() const [[clang::lifetimebound]] {";
			methods << "return config::detail::make_optional<config::detail::"
			        << adaptor << ">(obj[\"" << prop_name << "\"]);}";
		}
		methods << "\n\n";
	}

	out << types.str();
	out << nested_classes.str();
	out << methods.str();

	out << "}\n";
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		return -1;
	}
	Root conf = Root::parse(argv[1]);

	emit_class(conf, "Config", std::cout);
}
