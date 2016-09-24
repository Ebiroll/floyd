//
//  main.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 27/03/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#include "floyd_parser.h"

#include "parser_primitives.h"
#include "text_parser.h"
#include "parse_statement.h"
#include "parse_expression.h"
#include "parse_function_def.h"
#include "parse_struct_def.h"
#include "utils.h"
#include "json_support.h"
#include "json_parser.h"

#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <cmath>

namespace floyd_parser {


using namespace std;

/*
	AST ABSTRACT SYNTAX TREE

https://en.wikipedia.org/wiki/Abstract_syntax_tree

https://en.wikipedia.org/wiki/Parsing_expression_grammar
https://en.wikipedia.org/wiki/Parsing
*/



struct test_cpp_value_class_a {
	int _a = 10;
	int _b = 10;

	bool operator==(const test_cpp_value_class_a& other){
		return _a == other._a && _b == other._b;
	}
};

QUARK_UNIT_TESTQ("test_cpp_value_class_a", "what is needed for basic operations"){
	test_cpp_value_class_a a;
	test_cpp_value_class_a b = a;

	QUARK_TEST_VERIFY(b._a == 10);
	QUARK_TEST_VERIFY(a == b);
}






//////////////////////////////////////////////////		read_statement()


/*
	Read one statement, including any expressions it uses.
	Supports all statments:
		- return statement
		- struct-definition
		- function-definition
		- define constant, with initializating.

	Never simplifes expressions- the parser is non-lossy.

	Returns:
		_statement
			[return_statement]
				[expression]
			...
		_ast
			May have been updated to hold new function-definition or struct-definition.
		_rest
			will never start with whitespace.
			trailing ";" will be consumed.



	Example return statement:
		#1	"return 3;..."
		#2	"return f(3, 4) + 2;..."

	Example function definitions:
		#1	"int test_func1(){ return 100; };..."
		#2	"string test_func2(int a, float b){ return "sdf" };..."

	Example struct definitions:
		"struct my_image { int width; int height; };"
		"struct my_sprite { string name; my_image image; };..."

	Example variable definitions
		"int a = 10;..."
		"int b = f(a);..."


	OUTPUT

	["return", EXPRESSION ]
	["bind", "<string>", "local_name", EXPRESSION ]
	["define_struct", STRUCT_DEF ]
	["define_function", FUNCTION_DEF ]


	FUTURE
	- Add local scopes / blocks.
	- Include comments
	- Add mutable variables
*/

struct statement_result_t {
	json_value_t _statement;
	std::string _rest;
};

statement_result_t read_statement(const string& pos){
	const auto token_pos = read_until(pos, whitespace_chars);

	//	return statement?
	if(token_pos.first == "return"){
		const auto return_statement_pos = parse_return_statement(pos);
		return { return_statement_pos.first, skip_whitespace(return_statement_pos.second) };
	}

	//	struct definition?
	else if(token_pos.first == "struct"){
		const auto a = parse_struct_definition(pos);
		return { json_value_t::make_array2({ json_value_t("define_struct"), a.first }), skip_whitespace(a.second) };
	}

	else {
		const auto type_pos = read_required_type_identifier(pos);
		const auto identifier_pos = read_required_single_symbol(type_pos.second);

		/*
			Function definition?
			"int xyz(string a, string b){ ... }
		*/
		if(peek_string(skip_whitespace(identifier_pos.second), "(")){
			const auto function = parse_function_definition(pos);
			{
				QUARK_SCOPED_TRACE("FUNCTION DEF");
				QUARK_TRACE(json_to_pretty_string(function.first));
			}
            return { json_value_t::make_array2({ json_value_t("define_function"), function.first }), skip_whitespace(function.second) };
		}

		//	Define variable?
		/*
			"int a = 10;"
			"string hello = f(a) + \"_suffix\";";
		*/

		else if(peek_string(skip_whitespace(identifier_pos.second), "=")){
			const auto assignment_statement = parse_assignment_statement(pos);
			return { assignment_statement.first, skip_whitespace(assignment_statement.second) };
		}

		else{
			throw std::runtime_error("syntax error");
		}
	}
}

QUARK_UNIT_TESTQ("read_statement()", ""){
	try{
		const auto result = read_statement("int f()");
		QUARK_TEST_VERIFY(false);
	}
	catch(...){
	}
}

QUARK_UNIT_TESTQ("read_statement()", ""){
	const auto result = read_statement("float test = testx(1234);\n\treturn 3;\n");
	QUARK_TEST_VERIFY(json_to_compact_string(result._statement)
		== R"(["bind", "<float>", "test", ["call", ["@", "testx"], [["k", 1234, "<int>"]]]])");
	QUARK_TEST_VERIFY(result._rest == "return 3;\n");
}

QUARK_UNIT_TESTQ("read_statement()", ""){
	const auto result = read_statement(test_function1);
	QUARK_TEST_VERIFY(json_to_compact_string(result._statement)
		== R"(["define_function", { "_args": [], "_locals": [], "_members": [], "_name": "test_function1", "_return_type": "<int>", "_statements": [["return", ["k", 100, "<int>"]]], "_type": "function", "_types": {} }])");
	QUARK_TEST_VERIFY(result._rest == "");
}

QUARK_UNIT_TESTQ("read_statement()", ""){
	const auto result = read_statement("struct test_struct0 {int x; string y; float z;};");

	quark::ut_compare(
		json_to_compact_string(result._statement),
		R"(["define_struct", { "_args": [], "_locals": [], "_members": [{ "name": "x", "type": "<int>" }, { "name": "y", "type": "<string>" }, { "name": "z", "type": "<float>" }], "_name": "test_struct0", "_return_type": "", "_statements": [], "_type": "struct", "_types": {} }])"
	);
	QUARK_TEST_VERIFY(result._rest == ";");
}




json_value_t define_scope_type(const json_value_t& result_scope, const json_value_t& new_scope){
	QUARK_ASSERT(result_scope.check_invariant());
	QUARK_ASSERT(new_scope.check_invariant());

	const auto name = new_scope.get_object_element("_name").get_string();
	const auto type = new_scope.get_object_element("_type").get_string();
	const auto type_entry = json_value_t::make_object({
		{ "base_type", type },
		{ "scope_def", new_scope }
	});
	if(exists_in(result_scope, make_vec({ "_types", name }))){
		const auto index = get_in(result_scope, make_vec({ "_types", name })).get_array_size();
		return assoc_in(result_scope, make_vec({"_types", name, index }), type_entry);
	}
	else{
		return assoc_in(
			result_scope,
			make_vec({"_types", name }),
			json_value_t::make_array2({ type_entry })
		);
	}
}

//??? Track when / order of definitions and binds so statements can't access them before they're in scope.

json_value_t make_function_prototype(const json_value_t& return_type, const json_value_t& args){
	const auto args2 = args.get_array();
	vector<json_value_t> proto;
	proto.push_back(return_type);
	proto.insert(proto.end(), args2.begin(), args2.end());
	const auto function_prototype = json_value_t::make_array2(proto);
	return function_prototype;
}

QUARK_UNIT_TESTQ("make_function_prototype()", ""){
	const auto r = make_function_prototype(
		json_value_t("int"),
		json_value_t::make_array2({})
	);
	QUARK_UT_VERIFY(json_to_compact_string(r) == "[");
}




std::pair<json_value_t, std::string> read_statements_into_scope_def(const json_value_t& scope_def2, const string& s){
	QUARK_ASSERT(scope_def2.check_invariant());
	QUARK_ASSERT(s.size() > 0);

	//	Copy input scope_def.
	json_value_t result_scope = scope_def2;

	auto pos = skip_whitespace(s);
	while(!pos.empty()){
		const auto statement_pos = read_statement(pos);

		//	Ex: ["return", EXPRESSION]
		const auto statement = statement_pos._statement;

		const auto statement_type = statement.get_array_n(0).get_string();

		//	Definition statements are immediately removed from AST and the types are defined instead.
		if(statement_type == "define_struct"){
			result_scope = define_scope_type(result_scope, statement.get_array_n(1));
		}
		else if(statement_type == "define_function"){
			result_scope = define_scope_type(result_scope, statement.get_array_n(1));

			const auto function_name = statement.get_array_n(1).get_object_element("_name");
			const auto return_type = statement.get_array_n(1).get_object_element("_return_type");
			const auto args_array = statement.get_array_n(1).get_object_element("_args").get_array();
			vector<json_value_t> proto;
			proto.push_back(return_type);
			proto.insert(proto.back(), args.begin(), arg.end());
			const auto function_prototype = json_value_t::make_array2(proto);



			auto loc = make_member_def(bind_type.get_string(), local_name.get_string(), json_value_t());
			result_scope = store_object_member(result_scope, "_locals", push_back(result_scope.get_object_element("_locals"), loc));


			//	Add a local variable pointing to our function.??? This removes need for scoped type_defs?
//			auto loc = make_member_def("", local_name.get_string(), expr);
//			result_scope = store_object_member(result_scope, "_locals", push_back(result_scope.get_object_element("_locals"), loc));
		}
		else if(statement_type == "bind"){
			const auto bind_type = statement.get_array_n(1);
			const auto local_name = statement.get_array_n(2);
			const auto expr = statement.get_array_n(3);
			const auto loc = make_member_def(bind_type.get_string(), local_name.get_string(), json_value_t());

			//	Reserve an entry in _members-vector for our variable.
			result_scope = store_object_member(result_scope, "_locals", push_back(result_scope.get_object_element("_locals"), loc));
			result_scope = store_object_member(result_scope, "_statements", push_back(result_scope.get_object_element("_statements"), statement));
		}
		else{
			result_scope = store_object_member(result_scope, "_statements", push_back(result_scope.get_object_element("_statements"), statement));
		}
		pos = skip_whitespace(statement_pos._rest);
	}

	return { result_scope, pos };
}



json_value_t parse_program(const string& program){
	json_value_t a = make_scope_def();
	a = store_object_member(a, "_name", "global");
	a = store_object_member(a, "_type", "global");

	//	#Basic-types
	//	a = store_object_member(a, "_types", make_builtin_types());

	const auto statements_pos = read_statements_into_scope_def(a, program);
	QUARK_TRACE(json_to_pretty_string(statements_pos.first));
	return statements_pos.first;
}


QUARK_UNIT_TEST("", "parse_program()", "kProgram1", ""){
	const string kProgram1 =
		"int main(string args){\n"
		"	return 3;\n"
		"}\n";

	auto result = parse_program(kProgram1);
	QUARK_UT_VERIFY(get_in(result, { "_types", "main", 0.0, "base_type" }) == "function");
	QUARK_UT_VERIFY(get_in(result, { "_types", "main", 0.0, "scope_def", "_args", 0.0 }) == json_value_t::make_object({ { "name", "args"}, {"type", "<string>"}}));
	QUARK_UT_VERIFY(get_in(result, { "_types", "main", 0.0, "scope_def", "_return_type" }) == "<int>");
}

QUARK_UNIT_TEST("", "parse_program()", "three arguments", ""){
	const string kProgram =
		"int f(int x, int y, string z){\n"
		"	return 3;\n"
		"}\n";

	const auto result = parse_program(kProgram);
	QUARK_UT_VERIFY(result);
#if false
	const auto f = make_function_def(
		type_identifier_t::make("f"),
		type_identifier_t::make_int(),
		{
			member_t{ type_identifier_t::make_int(), "x" },
			member_t{ type_identifier_t::make_int(), "y" },
			member_t{ type_identifier_t::make_string(), "z" }
		},
		executable_t({
			make_shared<statement_t>(make__return_statement(expression_t::make_constant(3)))
		}),
		{},
		{}
	);

//	QUARK_TEST_VERIFY((*resolve_function_type(result._global_scope->_types_collector, "f") == *f));
	QUARK_TEST_VERIFY(resolve_function_type(result._global_scope->_types_collector, "f"));

	const auto f2 = resolve_function_type(result._global_scope->_types_collector, "f");
	QUARK_UT_VERIFY(f2->_type == scope_def_t::k_function_scope);

	const auto body = resolve_function_type(f2->_types_collector, "___body");
	QUARK_UT_VERIFY(body->_type == scope_def_t::k_subscope);
	QUARK_UT_VERIFY(body->_executable._statements.size() == 1);
#endif
}


QUARK_UNIT_TEST("", "parse_program()", "Local variables", ""){
	const string kProgram1 =
		"int main(string args){\n"
		"	int a = 4;\n"
		"	return 3;\n"
		"}\n";

	auto result = parse_program(kProgram1);
	QUARK_UT_VERIFY(result);
#if false
	const auto f2 = resolve_function_type(result._global_scope->_types_collector, "main");
	QUARK_UT_VERIFY(f2);
	QUARK_UT_VERIFY(f2->_type == scope_def_t::k_function_scope);

	const auto body = resolve_function_type(f2->_types_collector, "___body");
	QUARK_UT_VERIFY(body);
	QUARK_UT_VERIFY(body->_type == scope_def_t::k_subscope);
	QUARK_UT_VERIFY(body->_members.size() == 1);
	QUARK_UT_VERIFY(body->_members[0]._name == "a");
	QUARK_UT_VERIFY(body->_executable._statements.size() == 2);
#endif
}



QUARK_UNIT_TEST("", "parse_program()", "two functions", ""){
	const string kProgram =
		"string hello(int x, int y, string z){\n"
		"	return \"test abc\";\n"
		"}\n"
		"int main(string args){\n"
		"	return 3;\n"
		"}\n";

	const auto result = parse_program(kProgram);
	QUARK_UT_VERIFY(result);

#if false
	QUARK_TEST_VERIFY(result._global_scope->_executable._statements.size() == 0);

	const auto f = make_function_def(
		type_identifier_t::make("hello"),
		type_identifier_t::make_string(),
		{
			member_t{ type_identifier_t::make_int(), "x" },
			member_t{ type_identifier_t::make_int(), "y" },
			member_t{ type_identifier_t::make_string(), "z" }
		},
		executable_t({
			make_shared<statement_t>(make__return_statement(expression_t::make_constant("test abc")))
		}),
		{}
	);
//	QUARK_TEST_VERIFY((*resolve_function_type(result._global_scope->_types_collector, "hello") == *f));
	QUARK_TEST_VERIFY(resolve_function_type(result._global_scope->_types_collector, "hello"));

	const auto f2 = make_function_def(
		type_identifier_t::make("main"),
		type_identifier_t::make_int(),
		{
			member_t{ type_identifier_t::make_string(), "args" }
		},
		executable_t({
			make_shared<statement_t>(make__return_statement(expression_t::make_constant(3)))
		}),
		{}
	);
//	QUARK_TEST_VERIFY((*resolve_function_type(result._global_scope->_types_collector, "main") == *f2));
	QUARK_TEST_VERIFY(resolve_function_type(result._global_scope->_types_collector, "main"));
#endif
}

QUARK_UNIT_TESTQ("parse_program()", "Call function a from function b"){
	const string kProgram2 =
	"float testx(float v){\n"
	"	return 13.4;\n"
	"}\n"
	"int main(string args){\n"
	"	float test = testx(1234);\n"
	"	return 3;\n"
	"}\n";
	auto result = parse_program(kProgram2);
	QUARK_UT_VERIFY(result);
#if false
	QUARK_TEST_VERIFY(result._global_scope->_executable._statements.size() == 0);

	const auto f = make_function_def(
		type_identifier_t::make("testx"),
		type_identifier_t::make_float(),
		{
			member_t{ type_identifier_t::make_float(), "v" }
		},
		executable_t({
			make_shared<statement_t>(make__return_statement(expression_t::make_constant(13.4f)))
		}),
		{}
	);
//	QUARK_TEST_VERIFY((*resolve_function_type(result._global_scope->_types_collector, "testx") == *f));
	QUARK_TEST_VERIFY(resolve_function_type(result._global_scope->_types_collector, "testx"));

/*
	QUARK_TEST_VERIFY((*result._types_collector.resolve_function_type("main") ==
		make_function_def(
			type_identifier_t::make_int(),
			vector<member_t>{
				member_t{ type_identifier_t::make_string(), "arg" }
			},
			{
				make__return_statement(bind_statement_t{"test", function_call_expr_t{"testx", }})
				make__return_statement(make_constant(value_t(13.4f)))
			}
		)
	));
*/
#endif
}



////////////////////////////		STRUCT SUPPORT




QUARK_UNIT_TESTQ("parse_program()", "Proves we can instantiate a struct"){
	const auto result = parse_program(
		"struct pixel { string s; }"
		"string main(){\n"
		"	return \"\";"
		"}\n"
	);

#if false
	QUARK_TEST_VERIFY(result._global_scope->_executable._statements.size() == 0);
#endif

/*
	QUARK_TEST_VERIFY((*result._types_collector.resolve_function_type("main") ==
		make_function_def(
			type_identifier_t::make_string(),
			vector<member_t>{},
			{
				make__return_statement(make_constant(value_t(3)))
			}
		)
	));
*/
}

QUARK_UNIT_TESTQ("parse_program()", "Proves we can address a struct member variable"){
	const auto result = parse_program(
		"string main(){\n"
		"	return p.s + a;"
		"}\n"
	);
#if false
	QUARK_TEST_VERIFY(result._global_scope->_executable._statements.size() == 0);
#endif

/*
	QUARK_TEST_VERIFY((*result._types_collector.resolve_function_type("main") ==
		make_function_def(
			type_identifier_t::make_string(),
			vector<member_t>{},
			{
				make__return_statement(make_constant(value_t(3)))
			}
		)
	));
*/
}


}	//	floyd_parser

