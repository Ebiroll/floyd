//
//  ast_helpers.hpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-07-30.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#ifndef ast_helpers_hpp
#define ast_helpers_hpp

#include <vector>
#include <memory>

namespace floyd {

struct general_purpose_ast_t;
struct function_definition_t;
struct expression_t;
struct statement_t;
struct body_t;
struct typeid_t;
struct struct_definition_t;

bool check_types_resolved(const expression_t& e);
bool check_types_resolved(const std::vector<expression_t>& expressions);

bool check_types_resolved(const function_definition_t& def);

bool check_types_resolved(const body_t& body);

bool check_types_resolved(const statement_t& s);
bool check_types_resolved(const std::vector<std::shared_ptr<statement_t>>& s);

bool check_types_resolved(const struct_definition_t& s);
bool check_types_resolved_int(const std::vector<typeid_t>& elements);
bool check_types_resolved(const typeid_t& t);

bool check_types_resolved(const general_purpose_ast_t& ast);



}	//	floyd


#endif /* ast_helpers_hpp */
