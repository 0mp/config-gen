#include "config-generic.h"
#include <fstream>
#include <getopt.h>
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
	const char *configNamespace = "::config::detail::";

	out << "class " << name << "{" << configNamespace
	    << "UCLPtr obj; public:\n";

	out << name << "(const ucl_object_t *o) : obj(o) {}\n";

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
		std::string      className;
		bool             isInteger        = false;
		std::string_view adaptorNamespace = configNamespace;
		std::string_view lifetimeAttribute;
		switch (prop.type())
		{
			case Schema::Object::TypeString:
			{
				return_type       = "std::string_view";
				adaptor           = "StringViewAdaptor";
				lifetimeAttribute = "[[clang::lifetimebound]]";
				break;
			}
			case Schema::Object::TypeBool:
			{
				return_type = "bool";
				adaptor     = "booladaptor";
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
			{
				className = method_name;
				className += "Class";
				emit_class(prop, className, types);
				return_type      = className;
				adaptor          = className;
				adaptorNamespace = "";
				break;
			}
		}
		if (isRequired)
		{
			methods << return_type << ' ' << method_name << "() const "
			        << lifetimeAttribute << " {"
			        << "return " << adaptorNamespace << adaptor << "(obj[\""
			        << prop_name << "\"]);}";
		}
		else
		{
			methods << "std::optional<" << return_type << "> " << method_name
			        << "() const " << lifetimeAttribute << " {"
			        << "return config::detail::make_optional<"
			        << adaptorNamespace << adaptor << ">(obj[\"" << prop_name
			        << "\"]);}";
		}
		methods << "\n\n";
	}

	out << types.str();
	out << nested_classes.str();
	out << methods.str();

	out << "};\n";
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		return -1;
	}

	std::unique_ptr<std::ofstream> file_out{nullptr};

	const char *in_filename = argv[1];

	static struct option long_options[] = {
	  {"output", required_argument, nullptr, 'o'},
	  {nullptr, 0, nullptr, 0},
	};

	if (argc > 2)
	{
		int c = -1;
		int option_index;
		while ((c = getopt_long(
		          argc, argv, "o:", long_options, &option_index)) != -1)
		{
			switch (c)
			{
				case 'o':
				{
					std::cerr << optarg;
					file_out = std::make_unique<std::ofstream>(optarg);
					break;
				}
			}
		}
	}

	std::ostream &out = file_out ? *file_out : std::cout;

	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
	ucl_parser_add_file(p, in_filename);
	if (ucl_parser_get_error(p))
	{
		printf("Error occurred: %s\n", ucl_parser_get_error(p));
		return EXIT_FAILURE;
	}

	auto obj = ucl_parser_get_object(p);
	ucl_parser_free(p);
	Root  conf(obj);
	char *schemaCString =
	  reinterpret_cast<char *>(ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT));
	std::string schema(schemaCString);
	free(schemaCString);
	// Escape as a C string:
	auto replace = [&](std::string_view search, std::string_view replace) {
		size_t pos = 0;
		while ((pos = schema.find(search, pos)) != std::string::npos)
		{
			schema.replace(pos, search.length(), replace);
			pos += replace.length();
		}
	};
	replace("\\", "\\\\");
	replace("\"", "\\\"");
	replace("\n", "\\n");
	ucl_object_unref(obj);

	out << "#include \"config-generic.h\"\n\n";
	out << "#include <variant>\n\n";
	out << "#ifdef CONFIG_NAMESPACE_BEGIN\nCONFIG_NAMESPACE_BEGIN\n#endif\n";
	emit_class(conf, "Config", out);
	out << "inline std::variant<Config, ucl_schema_error> "
	       "make_config(ucl_object_t *obj) {"
	    << "static const ucl_object_t *schema = []() {"
	    << "static const char embeddedSchema[] = \"" << schema << "\";\n"
	    << "struct ucl_parser *p = "
	       "ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);\n"
	    << "ucl_parser_add_string(p, embeddedSchema, sizeof(embeddedSchema));\n"
	    << "if (ucl_parser_get_error(p)) { std::terminate(); }\n"
	    << "auto obj = ucl_parser_get_object(p);\n"
	    << "ucl_parser_free(p);\n"
	    << "return obj;\n"
	    << "}();"
	    << "ucl_schema_error err;\n"
	    << "if (!ucl_object_validate(schema, obj, &err)) { return err; }"
	    << "return Config(obj);\n"
	    << "}\n\n"
	    << "#ifdef CONFIG_NAMESPACE_END\nCONFIG_NAMESPACE_END\n#endif\n\n";
}
