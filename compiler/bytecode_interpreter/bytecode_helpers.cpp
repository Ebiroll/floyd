//
//  bytecode_helpers.cpp
//  Floyd
//
//  Created by Marcus Zetterquist on 2019-07-25.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "bytecode_helpers.h"

#include "bytecode_interpreter.h"
#include "ast_value.h"


namespace floyd {



//////////////////////////////////////		value_t -- helpers



std::vector<bc_value_t> values_to_bcs(const std::vector<value_t>& values){
	std::vector<bc_value_t> result;
	for(const auto& e: values){
		result.push_back(value_to_bc(e));
	}
	return result;
}

value_t bc_to_value(const bc_value_t& value){
	QUARK_ASSERT(value.check_invariant());

	const auto& type = value._type;
	const auto basetype = value._type.get_base_type();

	if(basetype == base_type::k_undefined){
		return value_t::make_undefined();
	}
	else if(basetype == base_type::k_any){
		return value_t::make_any();
	}
	else if(basetype == base_type::k_void){
		return value_t::make_void();
	}
	else if(basetype == base_type::k_bool){
		return value_t::make_bool(value.get_bool_value());
	}
	else if(basetype == base_type::k_int){
		return value_t::make_int(value.get_int_value());
	}
	else if(basetype == base_type::k_double){
		return value_t::make_double(value.get_double_value());
	}
	else if(basetype == base_type::k_string){
		return value_t::make_string(value.get_string_value());
	}
	else if(basetype == base_type::k_json){
		return value_t::make_json(value.get_json());
	}
	else if(basetype == base_type::k_typeid){
		return value_t::make_typeid_value(value.get_typeid_value());
	}
	else if(basetype == base_type::k_struct){
		const auto& members = value.get_struct_value();
		std::vector<value_t> members2;
		for(int i = 0 ; i < members.size() ; i++){
			const auto& member_value = members[i];
			const auto& member_value2 = bc_to_value(member_value);
			members2.push_back(member_value2);
		}
		return value_t::make_struct_value(type, members2);
	}
	else if(basetype == base_type::k_vector){
		const auto& element_type  = type.get_vector_element_type();
		std::vector<value_t> vec2;
		const bool vector_w_inplace_elements = encode_as_vector_w_inplace_elements(type);
		if(vector_w_inplace_elements){
			for(const auto e: value._pod._external->_vector_w_inplace_elements){
				vec2.push_back(bc_to_value(bc_value_t(element_type, e)));
			}
		}
		else{
			for(const auto& e: value._pod._external->_vector_w_external_elements){
				QUARK_ASSERT(e.check_invariant());
				vec2.push_back(bc_to_value(bc_value_t(element_type, e)));
			}
		}
		return value_t::make_vector_value(element_type, vec2);
	}
	else if(basetype == base_type::k_dict){
		const auto& value_type  = type.get_dict_value_type();
		const bool dict_w_inplace_values = encode_as_dict_w_inplace_values(type);
		std::map<std::string, value_t> entries2;
		if(dict_w_inplace_values){
			for(const auto& e: value._pod._external->_dict_w_inplace_values){
				entries2.insert({ e.first, bc_to_value(bc_value_t(value_type, e.second)) });
			}
		}
		else{
			for(const auto& e: value._pod._external->_dict_w_external_values){
				entries2.insert({ e.first, bc_to_value(bc_value_t(value_type, e.second)) });
			}
		}
		return value_t::make_dict_value(value_type, entries2);
	}
	else if(basetype == base_type::k_function){
		return value_t::make_function_value(type, value.get_function_value());
	}
	else{
		QUARK_ASSERT(false);
		quark::throw_exception();
	}
}

bc_value_t value_to_bc(const value_t& value){
	QUARK_ASSERT(value.check_invariant());

	const auto basetype = value.get_basetype();
	if(basetype == base_type::k_undefined){
		return bc_value_t::make_undefined();
	}
	else if(basetype == base_type::k_any){
		return bc_value_t::make_any();
	}
	else if(basetype == base_type::k_void){
		return bc_value_t::make_void();
	}
	else if(basetype == base_type::k_bool){
		return bc_value_t::make_bool(value.get_bool_value());
	}
	else if(basetype == base_type::k_bool){
		return bc_value_t::make_bool(value.get_bool_value());
	}
	else if(basetype == base_type::k_int){
		return bc_value_t::make_int(value.get_int_value());
	}
	else if(basetype == base_type::k_double){
		return bc_value_t::make_double(value.get_double_value());
	}

	else if(basetype == base_type::k_string){
		return bc_value_t::make_string(value.get_string_value());
	}
	else if(basetype == base_type::k_json){
		return bc_value_t::make_json(value.get_json());
	}
	else if(basetype == base_type::k_typeid){
		return bc_value_t::make_typeid_value(value.get_typeid_value());
	}
	else if(basetype == base_type::k_struct){
		return bc_value_t::make_struct_value(value.get_type(), values_to_bcs(value.get_struct_value()->_member_values));
	}

	else if(basetype == base_type::k_vector){
		const auto vector_type = value.get_type();
		const auto element_type = vector_type.get_vector_element_type();

		if(encode_as_vector_w_inplace_elements(vector_type)){
			const auto& vec = value.get_vector_value();
			immer::vector<bc_inplace_value_t> vec2;
			for(const auto& e: vec){
				const auto bc = value_to_bc(e);
				vec2.push_back(bc._pod._inplace);
			}
			return make_vector(element_type, vec2);
		}
		else{
			const auto& vec = value.get_vector_value();
			immer::vector<bc_external_handle_t> vec2;
			for(const auto& e: vec){
				const auto bc = value_to_bc(e);
				const auto hand = bc_external_handle_t(bc);
				vec2 = vec2.push_back(hand);
			}
			return make_vector(element_type, vec2);
		}
	}
	else if(basetype == base_type::k_dict){
		const auto dict_type = value.get_type();
		const auto value_type = dict_type.get_dict_value_type();

		const auto elements = value.get_dict_value();
		immer::map<std::string, bc_external_handle_t> entries2;

		if(encode_as_dict_w_inplace_values(dict_type)){
			QUARK_ASSERT(false);//??? fix
		}
		else{
			for(const auto& e: elements){
				entries2 = entries2.insert({e.first, bc_external_handle_t(value_to_bc(e.second))});
			}
		}
		return make_dict(value_type, entries2);
	}
	else if(basetype == base_type::k_function){
		return bc_value_t::make_function_value(value.get_type(), value.get_function_value());
	}
	else{
		QUARK_ASSERT(false);
		quark::throw_exception();
	}
}

//??? add tests!


}	// floyd
