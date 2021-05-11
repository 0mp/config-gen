#include "test.h"
#include <cassert>
#include <iostream>

static const char config_string[] = "aString = \"hello world\";\n"
                                    "anObject {\n"
                                    "  aString = \"Inner string\";\n"
                                    "  anInt = 42;\n"
                                    "}\n";

int main()
{
	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
	ucl_parser_add_string(p, config_string, sizeof(config_string));
	if (ucl_parser_get_error(p))
	{
		std::cerr << "Parse error: " << ucl_parser_get_error(p) << std::endl;
		return EXIT_FAILURE;
	}
	auto obj         = ucl_parser_get_object(p);
	auto confOrError = make_config(obj);
	if (std::holds_alternative<ucl_schema_error>(confOrError))
	{
		auto &err = std::get<ucl_schema_error>(confOrError);
		std::cerr << "Schema validation failed " << err.msg
		          << (char *)ucl_object_emit(err.obj, UCL_EMIT_CONFIG);
		return EXIT_FAILURE;
	}
	auto conf = get<Config>(confOrError);
	assert(conf.aString() == "hello world");
	assert(conf.anObject().aString() == "Inner string");
	assert(conf.anObject().anInt() == 42);
	return EXIT_SUCCESS;
}
