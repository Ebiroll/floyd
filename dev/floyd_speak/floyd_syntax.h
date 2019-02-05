//
//  floyd_syntax.hpp
//  floyd_speak
//
//  Created by Marcus Zetterquist on 2019-02-05.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#ifndef floyd_syntax_hpp
#define floyd_syntax_hpp

#include <string>
#include <map>

namespace floyd {

/*
PEG
https://en.wikipedia.org/wiki/Parsing_expression_grammar
http://craftinginterpreters.com/representing-code.html

AST ABSTRACT SYNTAX TREE

https://en.wikipedia.org/wiki/Abstract_syntax_tree

https://en.wikipedia.org/wiki/Parsing_expression_grammar
https://en.wikipedia.org/wiki/Parsing
*/

/*
	C99-language constants.
	http://en.cppreference.com/w/cpp/language/operator_precedence
*/
const std::string k_c99_number_chars = "0123456789.";
const std::string k_c99_whitespace_chars = " \n\t\r";
const std::string k_c99_identifier_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";


const std::string whitespace_chars = " \n\t";
const std::string identifier_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
const std::string bracket_pairs = "(){}[]";


//////////////////////////////////////		base_type

/*
	This type is tracked by compiler, not stored in the value-type.
*/
enum class base_type {
	k_internal_undefined,	//	k_internal_undefined is never exposed in code, only used internally in compiler.
	k_internal_dynamic,	//	Used by host functions arguments / returns to tell this is a dynamic value, not static type.

	k_void,		//	Means no value. Used as return type for print() etc.

	k_bool,
	k_int,
	k_double,
	k_string,
	k_json_value,

	//	This is a type that specifies another type.
	k_typeid,

	k_struct,
	k_protocol,
	k_vector,
	k_dict,
	k_function,

	//	We have an identifier, like "pixel" or "print" but haven't resolved it to an actual type yet.
	//	Keep the identifier so it can be resolved later
	k_internal_unresolved_type_identifier
};

std::string base_type_to_string(const base_type t);


//??? use lookup for statements vs their JSON-strings: k_store2, "store" and "def-struct".


///////////////////////////////////			eoperator_precedence


/*
	Operator precedence is the same as C99.
	Lower number gives stronger precedence.
	Important: we use < and > to compare these.
*/
enum class eoperator_precedence {
	k_super_strong = 0,

	//	(xyz)
	k_parentesis = 0,

	//	a()
	k_function_call = 2,

	//	a[], aka subscript
	k_lookup = 2,

	//	a.b
	k_member_access = 2,


	k_multiply_divider_remainder = 5,

	k_add_sub = 6,

	//	<   <=	For relational operators < and ≤ respectively
	//	>   >=
	k_larger_smaller = 8,


	k_equal__not_equal = 9,

	k_logical_and = 13,
	k_logical_or = 14,

	k_comparison_operator = 15,

	k_super_weak
};



///////////////////////////////////			eoperation


/*
	These are the operations generated by parsing the C-style expression.
	The order of constants inside enum not important.

	The number tells how many operands it needs.
*/
//??? Use expression_type instead.
enum class eoperation {
	k_0_number_constant = 100,

	//	This is string specifying a local variable, member variable, argument, global etc. Only the first entry in a chain.
	k_0_resolve,

	k_0_string_literal,

	k_x_member_access,

	k_2_lookup,

	k_2_add,
	k_2_subtract,
	k_2_multiply,
	k_2_divide,
	k_2_remainder,

	k_2_smaller_or_equal,
	k_2_smaller,

	k_2_larger_or_equal,
	k_2_larger,

	//	a == b
	k_2_logical_equal,

	//	a != b
	k_2_logical_nonequal,

	//	a && b
	k_2_logical_and,

	//	a || b
	k_2_logical_or,

	//	cond ? a : b
	k_3_conditional_operator,

	k_n_call,

	//	!a
//	k_1_logical_not

	//	-a
	k_1_unary_minus,


//	k_0_identifier

//	k_1_construct_value

	k_1_vector_definition,
	k_1_dict_definition
};

std::string k_2_operator_to_string__func(eoperation op);



//////////////////////////////////////		expression_type



//??? Split and categories better. Logic vs equality vs math.

//	Number at end of name tells number of input expressions operation has.
enum class expression_type {

	//	c99: a + b			token: "+"
	k_arithmetic_add__2 = 10,

	//	c99: a - b			token: "-"
	k_arithmetic_subtract__2,

	//	c99: a * b			token: "*"
	k_arithmetic_multiply__2,

	//	c99: a / b			token: "/"
	k_arithmetic_divide__2,

	//	c99: a % b			token: "%"
	k_arithmetic_remainder__2,


	//	c99: a <= b			token: "<="
	k_comparison_smaller_or_equal__2,

	//	c99: a < b			token: "<"
	k_comparison_smaller__2,

	//	c99: a >= b			token: ">="
	k_comparison_larger_or_equal__2,

	//	c99: a > b			token: ">"
	k_comparison_larger__2,


	//	c99: a == b			token: "=="
	k_logical_equal__2,

	//	c99: a != b			token: "!="
	k_logical_nonequal__2,


	//	c99: a && b			token: "&&"
	k_logical_and__2,

	//	c99: a || b			token: "||"
	k_logical_or__2,

	//	c99: !a				token: "!"
//			k_logical_not,

	//	c99: 13				token: "k"
	k_literal,

	//	c99: -a				token: "unary_minus"
	k_arithmetic_unary_minus__1,

	//	c99: cond ? a : b	token: "?:"
	k_conditional_operator3,

	//	c99: a(b, c)		token: "call"
	k_call,

	//	c99: a				token: "@"
	k_load,

	//	c99: a				token: "@i"
	k_load2,

	//	c99: a.b			token: "->"
	k_resolve_member,

	//	c99: a[b]			token: "[]"
	k_lookup_element,

	//	"def-struct"
	k_define_struct,

	//???	use k_literal for function values?
	//	"def-func"
	k_define_function,

	//	"construct-value"
	k_construct_value,
};

inline bool is_arithmetic_expression(expression_type op){
	return false
		|| op == expression_type::k_arithmetic_add__2
		|| op == expression_type::k_arithmetic_subtract__2
		|| op == expression_type::k_arithmetic_multiply__2
		|| op == expression_type::k_arithmetic_divide__2
		|| op == expression_type::k_arithmetic_remainder__2
		|| op == expression_type::k_logical_and__2
		|| op == expression_type::k_logical_or__2
		;
}

inline bool is_comparison_expression(expression_type op){
	return false
		|| op == expression_type::k_comparison_smaller_or_equal__2
		|| op == expression_type::k_comparison_smaller__2
		|| op == expression_type::k_comparison_larger_or_equal__2
		|| op == expression_type::k_comparison_larger__2

		|| op == expression_type::k_logical_equal__2
		|| op == expression_type::k_logical_nonequal__2
		;
}

expression_type token_to_expression_type(const std::string& op);
std::string expression_type_to_token(const expression_type& op);


//	"+", "<=", "&&" etc.
inline bool is_simple_expression__2(const std::string& op){
	return
		op == "+" || op == "-" || op == "*" || op == "/" || op == "%"
		|| op == "<=" || op == "<" || op == ">=" || op == ">"
		|| op == "==" || op == "!=" || op == "&&" || op == "||";
}


//??? move operation codes here.

//	Keywords in source code.
struct keyword_t {
	static const std::string k_return;
	static const std::string k_while;
	static const std::string k_for;
	static const std::string k_if;
	static const std::string k_else;
	static const std::string k_func;
	static const std::string k_impure;

	static const std::string k_internal_undefined;
	static const std::string k_internal_dynamic;
	static const std::string k_void;
	static const std::string k_false;
	static const std::string k_true;
	static const std::string k_bool;
	static const std::string k_int;
	static const std::string k_double;
	static const std::string k_string;
	static const std::string k_typeid;
	static const std::string k_json_value;
	static const std::string k_struct;
	static const std::string k_protocol;

	static const std::string k_mutable;
	static const std::string k_let;

	static const std::string k_software_system;
	static const std::string k_container_def;

	static const std::string k_json_object;
	static const std::string k_json_array;
	static const std::string k_json_string;
	static const std::string k_json_number;
	static const std::string k_json_true;
	static const std::string k_json_false;
	static const std::string k_json_null;

/*
	"assert",
	"print",
	"to_string",
	"update",
	"size",
*/

/*
	"catch",
	"deserialize()",
	"diff()",
	"ensure",
	"foreach",
	"hash()",
	"invariant",
	"log",
	"namespace",
	"private",
	"property",
	"prove",
	"require",
	"serialize()",
	"swap",
	"switch",
	"tag",
	"test",
	"this",
	"try",
	"typecast",
	"typeof",
*/

/*
const std::vector<std::string> basic_types {
	"char",
	code_point",
	"float",
	"float32",
	"float80",
	hash",
	"int16",
	"int32",
	"int64",
	"int8",
	path",
	text"
};
const std::vector<std::string> advanced_types {
	clock",
	defect_exception",
	dyn",
	dyn**<>",
	enum",
	exception",
	"dict",
	protocol",
	rights",
	runtime_exception",
	"seq",
	typedef",
};
*/
};


}	//	floyd

#endif /* floyd_syntax_hpp */
