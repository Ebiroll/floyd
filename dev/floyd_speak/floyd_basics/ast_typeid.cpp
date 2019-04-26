//
//  ast_typeid.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 2018-02-12.
//  Copyright © 2018 Marcus Zetterquist. All rights reserved.
//

#include "ast_typeid.h"

#include "parser_primitives.h"
#include "ast_json.h"
#include "json_support.h"
#include "utils.h"
#include "ast_typeid_helpers.h"



namespace floyd {



//////////////////////////////////////////////////		typeid_t


bool typeid_t::check_invariant() const{
	struct visitor_t {
		bool operator()(const internal_undefined_t& e) const{
			return true;
		}
		bool operator()(const any_t& e) const{
			return true;
		}

		bool operator()(const void_t& e) const{
			return true;
		}
		bool operator()(const bool_t& e) const{
			return true;
		}
		bool operator()(const int_t& e) const{
			return true;
		}
		bool operator()(const double_t& e) const{
			return true;
		}
		bool operator()(const string_t& e) const{
			return true;
		}

		bool operator()(const json_type_t& e) const{
			return true;
		}
		bool operator()(const typeid_type_t& e) const{
			return true;
		}

		bool operator()(const struct_t& e) const{
			QUARK_ASSERT(e._struct_def);
			QUARK_ASSERT(e._struct_def->check_invariant());
			return true;
		}
		bool operator()(const vector_t& e) const{
			QUARK_ASSERT(e._parts.size() == 1);
			QUARK_ASSERT(e._parts[0].check_invariant());
			return true;
		}
		bool operator()(const dict_t& e) const{
			QUARK_ASSERT(e._parts.size() == 1);
			QUARK_ASSERT(e._parts[0].check_invariant());
			return true;
		}
		bool operator()(const function_t& e) const{
			//	If function returns a DYN, it must have a dyn_return.
			QUARK_ASSERT(e._parts[0].is_any() == false || e.dyn_return != return_dyn_type::none);

			QUARK_ASSERT(e._parts.size() >= 1);

			for(const auto& m: e._parts){
				QUARK_ASSERT(m.check_invariant());
			}
			return true;
		}
		bool operator()(const internal_unresolved_type_identifier_t& e) const{
			QUARK_ASSERT(e._unresolved_type_identifier.empty() == false);
			return true;
		}
	};
	return std::visit(visitor_t{}, _contents);
}

void typeid_t::swap(typeid_t& other){
	QUARK_ASSERT(other.check_invariant());
	QUARK_ASSERT(check_invariant());

#if DEBUG
	std::swap(_DEBUG, other._DEBUG);
#endif
	std::swap(_contents, other._contents);

	QUARK_ASSERT(other.check_invariant());
	QUARK_ASSERT(check_invariant());
}


QUARK_UNIT_TESTQ("typeid_t", "make_undefined()"){
	ut_verify(QUARK_POS, typeid_t::make_undefined().get_base_type(), base_type::k_internal_undefined);
}
QUARK_UNIT_TESTQ("typeid_t", "is_undefined()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_undefined().is_undefined(), true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_undefined()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_bool().is_undefined(), false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_any()"){
	ut_verify(QUARK_POS, typeid_t::make_any().get_base_type(), base_type::k_any);
}
QUARK_UNIT_TESTQ("typeid_t", "is_any()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_any().is_any(), true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_any()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_bool().is_any(), false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_void()"){
	ut_verify(QUARK_POS, typeid_t::make_void().get_base_type(), base_type::k_void);
}
QUARK_UNIT_TESTQ("typeid_t", "is_void()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_void().is_void(), true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_void()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_bool().is_void(), false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_bool()"){
	ut_verify(QUARK_POS, typeid_t::make_bool().get_base_type(), base_type::k_bool);
}
QUARK_UNIT_TESTQ("typeid_t", "is_bool()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_bool().is_bool(), true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_bool()"){
	ut_verify_auto(QUARK_POS, typeid_t::make_void().is_bool(), false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_int()"){
	ut_verify(QUARK_POS, typeid_t::make_int().get_base_type(), base_type::k_int);
}
QUARK_UNIT_TESTQ("typeid_t", "is_int()"){
	QUARK_UT_VERIFY(typeid_t::make_int().is_int() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_int()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_int() == false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_double()"){
	QUARK_UT_VERIFY(typeid_t::make_double().is_double());
}
QUARK_UNIT_TESTQ("typeid_t", "is_double()"){
	QUARK_UT_VERIFY(typeid_t::make_double().is_double() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_double()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_double() == false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_string()"){
	QUARK_UT_VERIFY(typeid_t::make_string().get_base_type() == base_type::k_string);
}
QUARK_UNIT_TESTQ("typeid_t", "is_string()"){
	QUARK_UT_VERIFY(typeid_t::make_string().is_string() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_string()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_string() == false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_json_value()"){
	QUARK_UT_VERIFY(typeid_t::make_json_value().get_base_type() == base_type::k_json_value);
}
QUARK_UNIT_TESTQ("typeid_t", "is_json_value()"){
	QUARK_UT_VERIFY(typeid_t::make_json_value().is_json_value() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_json_value()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_json_value() == false);
}


QUARK_UNIT_TESTQ("typeid_t", "make_typeid()"){
	QUARK_UT_VERIFY(typeid_t::make_typeid().get_base_type() == base_type::k_typeid);
}
QUARK_UNIT_TESTQ("typeid_t", "is_typeid()"){
	QUARK_UT_VERIFY(typeid_t::make_typeid().is_typeid() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_typeid()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_typeid() == false);
}




typeid_t make_empty_struct(){
	return typeid_t::make_struct2({});
}

const auto k_struct_test_members_b = std::vector<member_t>({
	{ typeid_t::make_int(), "x" },
	{ typeid_t::make_string(), "y" },
	{ typeid_t::make_bool(), "z" }
});

typeid_t make_test_struct_a(){
	return typeid_t::make_struct2(k_struct_test_members_b);
}

QUARK_UNIT_TESTQ("typeid_t", "make_struct2()"){
	QUARK_UT_VERIFY(typeid_t::make_struct2({}).get_base_type() == base_type::k_struct);
}
QUARK_UNIT_TESTQ("typeid_t", "make_struct2()"){
	QUARK_UT_VERIFY(make_empty_struct().get_base_type() == base_type::k_struct);
}
QUARK_UNIT_TESTQ("typeid_t", "is_struct()"){
	QUARK_UT_VERIFY(make_empty_struct().is_struct() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_struct()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_struct() == false);
}

QUARK_UNIT_TESTQ("typeid_t", "make_struct2()"){
	const auto t = make_test_struct_a();
	QUARK_UT_VERIFY(t.get_struct() == k_struct_test_members_b);
}
QUARK_UNIT_TESTQ("typeid_t", "get_struct()"){
	const auto t = make_test_struct_a();
	QUARK_UT_VERIFY(t.get_struct() == k_struct_test_members_b);
}
QUARK_UNIT_TESTQ("typeid_t", "get_struct_ref()"){
	const auto t = make_test_struct_a();
	QUARK_UT_VERIFY(t.get_struct_ref()->_members == k_struct_test_members_b);
}





QUARK_UNIT_TESTQ("typeid_t", "make_vector()"){
	QUARK_UT_VERIFY(typeid_t::make_vector(typeid_t::make_int()).get_base_type() == base_type::k_vector);
}
QUARK_UNIT_TESTQ("typeid_t", "is_vector()"){
	QUARK_UT_VERIFY(typeid_t::make_vector(typeid_t::make_int()).is_vector() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_vector()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_vector() == false);
}
QUARK_UNIT_TESTQ("typeid_t", "get_vector_element_type()"){
	QUARK_UT_VERIFY(typeid_t::make_vector(typeid_t::make_int()).get_vector_element_type().is_int());
}
QUARK_UNIT_TESTQ("typeid_t", "get_vector_element_type()"){
	QUARK_UT_VERIFY(typeid_t::make_vector(typeid_t::make_string()).get_vector_element_type().is_string());
}


QUARK_UNIT_TESTQ("typeid_t", "make_dict()"){
	QUARK_UT_VERIFY(typeid_t::make_dict(typeid_t::make_int()).get_base_type() == base_type::k_dict);
}
QUARK_UNIT_TESTQ("typeid_t", "is_dict()"){
	QUARK_UT_VERIFY(typeid_t::make_dict(typeid_t::make_int()).is_dict() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_dict()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_dict() == false);
}
QUARK_UNIT_TESTQ("typeid_t", "get_dict_value_type()"){
	QUARK_UT_VERIFY(typeid_t::make_dict(typeid_t::make_int()).get_dict_value_type().is_int());
}
QUARK_UNIT_TESTQ("typeid_t", "get_dict_value_type()"){
	QUARK_UT_VERIFY(typeid_t::make_dict(typeid_t::make_string()).get_dict_value_type().is_string());
}



const auto k_test_function_args_a = std::vector<typeid_t>({
	{ typeid_t::make_int() },
	{ typeid_t::make_string() },
	{ typeid_t::make_bool() }
});

QUARK_UNIT_TESTQ("typeid_t", "make_function()"){
	const auto t = typeid_t::make_function(typeid_t::make_void(), {}, epure::pure);
	QUARK_UT_VERIFY(t.get_base_type() == base_type::k_function);
}
QUARK_UNIT_TESTQ("typeid_t", "is_function()"){
	const auto t = typeid_t::make_function(typeid_t::make_void(), {}, epure::pure);
	QUARK_UT_VERIFY(t.is_function() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_function()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_function() == false);
}
QUARK_UNIT_TESTQ("typeid_t", "get_function_return()"){
	const auto t = typeid_t::make_function(typeid_t::make_void(), {}, epure::pure);
	QUARK_UT_VERIFY(t.get_function_return().is_void());
}
QUARK_UNIT_TESTQ("typeid_t", "get_function_return()"){
	const auto t = typeid_t::make_function(typeid_t::make_string(), {}, epure::pure);
	QUARK_UT_VERIFY(t.get_function_return().is_string());
}
QUARK_UNIT_TESTQ("typeid_t", "get_function_args()"){
	const auto t = typeid_t::make_function(typeid_t::make_void(), k_test_function_args_a, epure::pure);
	QUARK_UT_VERIFY(t.get_function_args() == k_test_function_args_a);
}


QUARK_UNIT_TESTQ("typeid_t", "make_unresolved_type_identifier()"){
	const auto t = typeid_t::make_unresolved_type_identifier("xyz");
	QUARK_UT_VERIFY(t.get_base_type() == base_type::k_internal_unresolved_type_identifier);
}

QUARK_UNIT_TESTQ("typeid_t", "is_unresolved_type_identifier()"){
	const auto t = typeid_t::make_unresolved_type_identifier("xyz");
	QUARK_UT_VERIFY(t.is_unresolved_type_identifier() == true);
}
QUARK_UNIT_TESTQ("typeid_t", "is_unresolved_type_identifier()"){
	QUARK_UT_VERIFY(typeid_t::make_bool().is_unresolved_type_identifier() == false);
}

QUARK_UNIT_TESTQ("typeid_t", "get_unresolved_type_identifier()"){
	const auto t = typeid_t::make_unresolved_type_identifier("xyz");
	QUARK_UT_VERIFY(t.get_unresolved_type_identifier() == "xyz");
}
QUARK_UNIT_TESTQ("typeid_t", "get_unresolved_type_identifier()"){
	const auto t = typeid_t::make_unresolved_type_identifier("123");
	QUARK_UT_VERIFY(t.get_unresolved_type_identifier() == "123");
}






QUARK_UNIT_TESTQ("typeid_t", "operator==()"){
	const auto a = typeid_t::make_vector(typeid_t::make_int());
	const auto b = typeid_t::make_vector(typeid_t::make_int());
	QUARK_UT_VERIFY(a == b);
}
QUARK_UNIT_TESTQ("typeid_t", "operator==()"){
	const auto a = typeid_t::make_vector(typeid_t::make_int());
	const auto b = typeid_t::make_dict(typeid_t::make_int());
	QUARK_UT_VERIFY((a == b) == false);
}
QUARK_UNIT_TESTQ("typeid_t", "operator==()"){
	const auto a = typeid_t::make_vector(typeid_t::make_int());
	const auto b = typeid_t::make_vector(typeid_t::make_string());
	QUARK_UT_VERIFY((a == b) == false);
}


QUARK_UNIT_TESTQ("typeid_t", "operator=()"){
	const auto a = typeid_t::make_bool();
	const auto b = a;
	QUARK_UT_VERIFY(a == b);
}
QUARK_UNIT_TESTQ("typeid_t", "operator=()"){
	const auto a = typeid_t::make_vector(typeid_t::make_int());
	const auto b = a;
	QUARK_UT_VERIFY(a == b);
}
QUARK_UNIT_TESTQ("typeid_t", "operator=()"){
	const auto a = typeid_t::make_dict(typeid_t::make_int());
	const auto b = a;
	QUARK_UT_VERIFY(a == b);
}
QUARK_UNIT_TESTQ("typeid_t", "operator=()"){
	const auto a = typeid_t::make_function(typeid_t::make_string(), { typeid_t::make_int(), typeid_t::make_double() }, epure::pure);
	const auto b = a;
	QUARK_UT_VERIFY(a == b);
}



//////////////////////////////////////		FORMATS



std::string typeid_to_compact_string_int(const typeid_t& t){
//		QUARK_ASSERT(t.check_invariant());

	const auto basetype = t.get_base_type();

	if(basetype == floyd::base_type::k_internal_unresolved_type_identifier){
		return std::string() + "•" + t.get_unresolved_type_identifier() + "•";
	}
/*
	else if(basetype == floyd::base_type::k_typeid){
		const auto t2 = t.get_typeid_typeid();
		return "typeid(" + typeid_to_compact_string(t2) + ")";
	}
*/
	else if(basetype == floyd::base_type::k_struct){
		const auto struct_def = t.get_struct();
		return floyd::to_compact_string(struct_def);
	}
	else if(basetype == floyd::base_type::k_vector){
		const auto e = t.get_vector_element_type();
		return "[" + typeid_to_compact_string(e) + "]";
	}
	else if(basetype == floyd::base_type::k_dict){
		const auto e = t.get_dict_value_type();
		return "[string:" + typeid_to_compact_string(e) + "]";
	}
	else if(basetype == floyd::base_type::k_function){
		const auto ret = t.get_function_return();
		const auto args = t.get_function_args();
		const auto pure = t.get_function_pure();

		std::vector<std::string> args_str;
		for(const auto& a: args){
			args_str.push_back(typeid_to_compact_string(a));
		}

		return std::string() + "function " + typeid_to_compact_string(ret) + "(" + concat_strings_with_divider(args_str, ",") + ") " + (pure == epure::pure ? "pure" : "impure");
	}
	else{
		return base_type_to_string(basetype);
	}
}
std::string typeid_to_compact_string(const typeid_t& t){
	return typeid_to_compact_string_int(t);
//		return std::string() + "<" + typeid_to_compact_string_int(t) + ">";
}


struct typeid_str_test_t {
	typeid_t _typeid;
	std::string _ast_json;
	std::string _compact_str;
};


const std::vector<typeid_str_test_t> make_typeid_str_tests(){
	const auto s1 = typeid_t::make_struct2({});

	const auto tests = std::vector<typeid_str_test_t>{
		{ typeid_t::make_undefined(), quote(keyword_t::k_internal_undefined), keyword_t::k_internal_undefined },
		{ typeid_t::make_bool(), quote(keyword_t::k_bool), keyword_t::k_bool },
		{ typeid_t::make_int(), quote(keyword_t::k_int), keyword_t::k_int },
		{ typeid_t::make_double(), quote(keyword_t::k_double), keyword_t::k_double },
		{ typeid_t::make_string(), quote(keyword_t::k_string), keyword_t::k_string},

		//	Typeid
		{ typeid_t::make_typeid(), quote(keyword_t::k_typeid), keyword_t::k_typeid },
		{ typeid_t::make_typeid(), quote(keyword_t::k_typeid), keyword_t::k_typeid },


//??? vector
//??? dict

		//	Struct
		{ s1, R"(["struct", [[]]])", "struct {}" },
		{
			typeid_t::make_struct2(
				std::vector<member_t>{
					member_t(typeid_t::make_int(), "a"),
					member_t(typeid_t::make_double(), "b")
				}
			),
			R"(["struct", [[{ "type": "int", "name": "a"}, {"type": "double", "name": "b"}]]])",
			"struct {int a;double b;}"
		},


		//	Function
		{
			typeid_t::make_function(typeid_t::make_bool(), std::vector<typeid_t>{ typeid_t::make_int(), typeid_t::make_double() }, epure::pure),
			R"(["function", "bool", [ "int", "double"]])",
			"function bool(int,double)"
		},


		//	unknown_identifier
		{ typeid_t::make_unresolved_type_identifier("hello"), "\"hello\"", "hello" }
	};
	return tests;
}


OFF_QUARK_UNIT_TEST("typeid_to_ast_json()", "", "", ""){
	const auto f = make_typeid_str_tests();
	for(int i = 0 ; i < f.size() ; i++){
		QUARK_TRACE(std::to_string(i));
		const auto start_typeid = f[i]._typeid;
		const auto expected_ast_json = parse_json(seq_t(f[i]._ast_json)).first;

		//	Test typeid_to_ast_json().
		const auto result1 = typeid_to_ast_json(start_typeid, json_tags::k_tag_resolve_state);
		ut_verify(QUARK_POS, result1, expected_ast_json);
	}
}


OFF_QUARK_UNIT_TEST("typeid_from_ast_json", "", "", ""){
	const auto f = make_typeid_str_tests();
	for(int i = 0 ; i < f.size() ; i++){
		QUARK_TRACE(std::to_string(i));
		const auto start_typeid = f[i]._typeid;
		const auto expected_ast_json = parse_json(seq_t(f[i]._ast_json)).first;

		//	Test typeid_from_ast_json();
		const auto result2 = typeid_from_ast_json(expected_ast_json);
		QUARK_UT_VERIFY(result2 == start_typeid);
	}
	QUARK_TRACE("OK!");
}


OFF_QUARK_UNIT_TEST("typeid_to_compact_string", "", "", ""){
	const auto f = make_typeid_str_tests();
	for(int i = 0 ; i < f.size() ; i++){
		QUARK_TRACE(std::to_string(i));
		const auto start_typeid = f[i]._typeid;

		//	Test typeid_to_compact_string().
		const auto result3 = typeid_to_compact_string(start_typeid);
		ut_verify(QUARK_POS, result3, f[i]._compact_str);
	}
	QUARK_TRACE("OK!");
}



//////////////////////////////////////////////////		struct_definition_t


bool struct_definition_t::check_invariant() const{
//		QUARK_ASSERT(_struct!type.is_undefined() && _struct_type.check_invariant());

	for(const auto& m: _members){
		QUARK_ASSERT(m.check_invariant());
	}
	return true;
}

bool struct_definition_t::operator==(const struct_definition_t& other) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(other.check_invariant());

	return _members == other._members;
}

bool struct_definition_t::check_types_resolved() const{
	for(const auto& e: _members){
		bool result = e._type.check_types_resolved();
		if(result == false){
			return false;
		}
	}
	return true;
}


std::string to_compact_string(const struct_definition_t& v){
	auto s = std::string() + "struct {";
	for(const auto& e: v._members){
		s = s + typeid_to_compact_string(e._type) + " " + e._name + ";";
	}
	s = s + "}";
	return s;
}


int find_struct_member_index(const struct_definition_t& def, const std::string& name){
	int index = 0;
	while(index < def._members.size() && def._members[index]._name != name){
		index++;
	}
	if(index == def._members.size()){
		return -1;
	}
	else{
		return index;
	}
}






////////////////////////			member_t


member_t::member_t(const floyd::typeid_t& type, const std::string& name) :
	_type(type),
	_name(name)
{
	QUARK_ASSERT(type.check_invariant());
//		QUARK_ASSERT(name.size() > 0);

	QUARK_ASSERT(check_invariant());
}

bool member_t::check_invariant() const{
	QUARK_ASSERT(_type.check_invariant());
//		QUARK_ASSERT(_name.size() > 0);
	return true;
}

bool member_t::operator==(const member_t& other) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(other.check_invariant());

	return (_type == other._type) && (_name == other._name);
}


std::vector<floyd::typeid_t> get_member_types(const std::vector<member_t>& m){
	std::vector<floyd::typeid_t> r;
	for(const auto& a: m){
		r.push_back(a._type);
	}
	return r;
}


////////////////////////			typeid_t



bool check_types_resolved_int(const std::vector<typeid_t>& elements){
	for(const auto& e: elements){
		if(e.check_types_resolved() == false){
			return false;
		}
	}
	return true;
}

bool typeid_t::check_types_resolved() const{
	QUARK_ASSERT(check_invariant());

	struct visitor_t {
		bool operator()(const internal_undefined_t& e) const{
			return false;
		}
		bool operator()(const any_t& e) const{
			return true;
		}

		bool operator()(const void_t& e) const{
			return true;
		}
		bool operator()(const bool_t& e) const{
			return true;
		}
		bool operator()(const int_t& e) const{
			return true;
		}
		bool operator()(const double_t& e) const{
			return true;
		}
		bool operator()(const string_t& e) const{
			return true;
		}

		bool operator()(const json_type_t& e) const{
			return true;
		}
		bool operator()(const typeid_type_t& e) const{
			return true;
		}

		bool operator()(const struct_t& e) const{
			return e._struct_def->check_types_resolved();
		}
		bool operator()(const vector_t& e) const{
			return check_types_resolved_int(e._parts);
		}
		bool operator()(const dict_t& e) const{
			return check_types_resolved_int(e._parts);
		}
		bool operator()(const function_t& e) const{
			return check_types_resolved_int(e._parts);
		}
		bool operator()(const internal_unresolved_type_identifier_t& e) const{
			return false;
		}
	};
	return std::visit(visitor_t{}, _contents);
}


int count_function_dynamic_args(const std::vector<typeid_t>& args){
	int count = 0;
	for(const auto& e: args){
		if(e.is_any()){
			count++;
		}
	}
	return count;
}
int count_function_dynamic_args(const typeid_t& function_type){
	QUARK_ASSERT(function_type.is_function());

	return count_function_dynamic_args(function_type.get_function_args());
}
bool is_dynamic_function(const typeid_t& function_type){
	QUARK_ASSERT(function_type.is_function());

	const auto count = count_function_dynamic_args(function_type);
	return count > 0;
}



}


