//
//  parser_statement.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 26/07/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#include "statement.h"


namespace floyd {

bool body_t::check_invariant() const {
	for(const auto& i: _statements){
		QUARK_ASSERT(i.check_invariant());
	};
	return true;
}


std::string symbol_to_string(const symbol_t& s){
	std::stringstream out;

	out << "<symbol> {"
		<< (s._symbol_type == symbol_t::type::immutable_local ? "immutable_local" : "mutable_local" )
		<< " type: " << typeid_to_compact_string(s._value_type)
		<< " init: " << (s._init.is_undefined() ? "<none>" : value_and_type_to_string(s._init))
	<< "}";
	return out.str();
}

int add_constant_literal(symbol_table_t& symbols, const std::string& name, const floyd::value_t& value){
	const auto s = symbol_t::make_constant(value);
	symbols._symbols.push_back(std::pair<std::string, symbol_t>(name, s));
	return static_cast<int>(symbols._symbols.size() - 1);
}


int add_temp(symbol_table_t& symbols, const std::string& name, const floyd::typeid_t& value_type){
	const auto s = symbol_t::make_immutable_local(value_type);
	symbols._symbols.push_back(std::pair<std::string, symbol_t>(name, s));
	return static_cast<int>(symbols._symbols.size() - 1);
}

const floyd::symbol_t* find_symbol(const symbol_table_t& symbol_table, const std::string& name){
	const auto it = std::find_if(
		symbol_table._symbols.begin(), symbol_table._symbols.end(),
		[&name](const std::pair<std::string, symbol_t>& x) { return x.first == name; }
	);
	
	return (it == symbol_table._symbols.end()) ? nullptr : &(it->second);
}
const floyd::symbol_t& find_symbol_required(const symbol_table_t& symbol_table, const std::string& name){
	const auto it = std::find_if(
		symbol_table._symbols.begin(), symbol_table._symbols.end(),
		[&name](const std::pair<std::string, symbol_t>& x) { return x.first == name; }
	);
	if(it == symbol_table._symbols.end()){
		throw std::exception();
	}
	return it->second;
}


////////////////////////////////////////////		statement_t


bool statement_t::check_types_resolved(const std::vector<std::shared_ptr<statement_t>>& s){
	for(const auto& e: s){
		if(e->check_types_resolved() == false){
			return false;
		}
	}
	return true;
}

bool statement_t::check_types_resolved() const{
	QUARK_ASSERT(check_invariant());

	struct visitor_t {
		bool operator()(const return_statement_t& s) const{
			return s._expression.check_types_resolved();
		}
		bool operator()(const define_struct_statement_t& s) const{
			return s._def->check_types_resolved();
		}
		bool operator()(const define_function_statement_t& s) const{
			return s._def->check_types_resolved();
		}

		bool operator()(const bind_local_t& s) const{
			return true
				&& s._bindtype.check_types_resolved()
				&& s._expression.check_types_resolved()
				;
		}
		bool operator()(const store_t& s) const{
			return s._expression.check_types_resolved();
		}
		bool operator()(const store2_t& s) const{
			return s._expression.check_types_resolved();
		}
		bool operator()(const block_statement_t& s) const{
			return s._body.check_types_resolved();
		}

		bool operator()(const ifelse_statement_t& s) const{
			return true
				&& s._condition.check_types_resolved()
				&& s._then_body.check_types_resolved()
				&& s._else_body.check_types_resolved()
				;
		}
		bool operator()(const for_statement_t& s) const{
			return true
				&& s._start_expression.check_types_resolved()
				&& s._end_expression.check_types_resolved()
				&& s._body.check_types_resolved()
				;
		}
		bool operator()(const while_statement_t& s) const{
			return true
				&& s._condition.check_types_resolved()
				&& s._body.check_types_resolved()
				;
		}

		bool operator()(const expression_statement_t& s) const{
			return s._expression.check_types_resolved();
		}
		bool operator()(const software_system_statement_t& s) const{
			return true;
		}
		bool operator()(const container_def_statement_t& s) const{
			return true;
		}
	};

	return std::visit(visitor_t{}, _contents);
}




QUARK_UNIT_TEST("", "", "", ""){
	const auto a = statement_t::make__block_statement(k_no_location, {});
	const auto b = statement_t::make__block_statement(k_no_location, {});
	QUARK_UT_VERIFY(a == b);
}

}
