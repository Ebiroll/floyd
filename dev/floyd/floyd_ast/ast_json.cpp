//
//  floyd_basics.cpp
//  Floyd
//
//  Created by Marcus Zetterquist on 2017-08-09.
//  Copyright © 2017 Marcus Zetterquist. All rights reserved.
//

#include "ast_json.h"

#include "ast_value.h"
#include "text_parser.h"
#include "json_support.h"
#include "compiler_basics.h"

namespace floyd {


json_t make_ast_node(const location_t& location, const std::string& opcode, const std::vector<json_t>& params){
	if(location == k_no_location){
		std::vector<json_t> e = { json_t(opcode) };
		e.insert(e.end(), params.begin(), params.end());
		return json_t(e);
	}
	else{
		const auto offset = static_cast<double>(location.offset);
		std::vector<json_t> e = { json_t(offset), json_t(opcode) };
		e.insert(e.end(), params.begin(), params.end());
		return json_t(e);
	}
}

QUARK_UNIT_TEST("", "make_ast_node()", "", ""){
	const auto r = make_ast_node(location_t(1234), "def-struct", std::vector<json_t>{});

	ut_verify(QUARK_POS, r, json_t::make_array({ 1234.0, "def-struct" }));
}

json_t make_statement1(const location_t& location, const std::string& opcode, const json_t& params){
	return make_ast_node(location, opcode, { params });
}
json_t make_statement2(const location_t& location, const std::string& opcode, const json_t& param1, const json_t& param2){
	return make_ast_node(location, opcode, { param1, param2 });
}





json_t make_expression1(const location_t& location, const std::string& opcode, const json_t& param){
	return make_ast_node(location, opcode, { param });
}

json_t make_expression2(const location_t& location, const std::string& opcode, const json_t& param1, const json_t& param2){
	return make_ast_node(location, opcode, { param1, param2 });
}






json_t maker__make2(const std::string op, const json_t& lhs, const json_t& rhs){
	QUARK_ASSERT(op != "");
	return make_expression2(floyd::k_no_location, op, lhs, rhs);
}


json_t maker__corecall(const std::string& name, const std::vector<json_t>& args){
	return make_expression2(floyd::k_no_location, expression_opcode_t::k_corecall, name, json_t::make_array(args));
}

json_t maker_vector_definition(const std::string& element_type, const std::vector<json_t>& elements){
	QUARK_ASSERT(element_type == "");

	const auto element_type2 = typeid_to_ast_json(typeid_t::make_vector(typeid_t::make_undefined()), json_tags::k_tag_resolve_state);
	return make_expression2(floyd::k_no_location, expression_opcode_t::k_value_constructor, element_type2, json_t::make_array(elements));
}
json_t maker_dict_definition(const std::string& value_type, const std::vector<json_t>& elements){
	QUARK_ASSERT(value_type == "");

	const auto element_type2 = typeid_to_ast_json(typeid_t::make_dict(typeid_t::make_undefined()), json_tags::k_tag_resolve_state);
	return make_expression2(floyd::k_no_location, expression_opcode_t::k_value_constructor, element_type2, json_t::make_array(elements));
}

json_t maker__member_access(const json_t& address, const std::string& member_name){
	return make_expression2(floyd::k_no_location, expression_opcode_t::k_resolve_member, address, json_t(member_name));
}

json_t maker__make_constant(const value_t& value){
	return make_expression2(
		floyd::k_no_location,
		expression_opcode_t::k_literal,
		value_to_ast_json(value, json_tags::k_tag_resolve_state),
		typeid_to_ast_json(value.get_type(), json_tags::k_tag_resolve_state)
	);
}

json_t maker_benchmark_definition(const json_t& body){
	return make_expression1(floyd::k_no_location, expression_opcode_t::k_benchmark, body);
}


std::pair<json_t, location_t> unpack_loc(const json_t& s){
	QUARK_ASSERT(s.is_array());

	const bool has_location = s.get_array_n(0).is_number();
	if(has_location){
		const location_t source_offset = has_location ? location_t(static_cast<std::size_t>(s.get_array_n(0).get_number())) : k_no_location;

		const auto elements = s.get_array();
		const std::vector<json_t> elements2 = { elements.begin() + 1, elements.end() };
		const auto statement = json_t::make_array(elements2);

		return { statement, source_offset };
	}
	else{
		return { s, k_no_location };
	}
}

location_t unpack_loc2(const json_t& s){
	QUARK_ASSERT(s.is_array());

	const bool has_location = s.get_array_n(0).is_number();
	if(has_location){
		const location_t source_offset = has_location ? location_t(static_cast<std::size_t>(s.get_array_n(0).get_number())) : k_no_location;
		return source_offset;
	}
	else{
		return k_no_location;
	}
}


}	// floyd
