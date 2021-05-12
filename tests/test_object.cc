#include "test_object.h"
#include <cassert>
#include <iostream>

static const char config_string[] = "aString = \"hello world\";\n"
                                    "anObject {\n"
                                    "  aString = \"Inner string\";\n"
                                    "  anInt = 42;\n"
                                    "}\n";

static const char config_wrong[] = "aString = \"hello world\";\n"
                                   "anObject {\n"
                                   "  aString = 12;\n"
                                   "  anInt = 42;\n"
                                   "}\n";

ucl_object_t *parse(const char *str, size_t len)
{
	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
	ucl_parser_add_string(p, str, len);
	if (ucl_parser_get_error(p))
	{
		std::cerr << "Parse error: " << ucl_parser_get_error(p) << std::endl;
		exit(EXIT_FAILURE);
	}
	return ucl_parser_get_object(p);
}
int main()
{
	auto obj         = parse(config_string, sizeof(config_string));
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
	obj = parse(config_wrong, sizeof(config_wrong));
	assert(std::holds_alternative<ucl_schema_error>(make_config(obj)));
	return EXIT_SUCCESS;
}
