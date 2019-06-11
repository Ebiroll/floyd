//
//  pass2.hpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 09/08/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#ifndef pass3_hpp
#define pass3_hpp

/*
	Performs semantic analysis of a Floyd program.

	Converts an pass2_ast_t to a semantic_ast_t.

	- All language-level syntax checks passed.
	- Builds symbol tables, resolves all symbols.
	- Checks types.
	- Infers types when not specified.
	- Replaces operations with other, equivalent operations.
	- has the pass2_ast_t and symbol tables for all lexical scopes.
	- Inserts host functions.
	- Insert built-in types.

	- All nested blocks remain and have their own symbols and statements.

	Output is a program that is correct with no type/semantic errors.
*/

#include "quark.h"

#include <string>
#include "ast.h"

namespace floyd {

struct pass2_ast_t;

//////////////////////////////////////		semantic_ast_t

/*
	The semantic_ast_t is a ready-to-run program, all symbols resolved, all semantics are OK.
*/
struct semantic_ast_t {
	public: explicit semantic_ast_t(const general_purpose_ast_t& tree);

#if DEBUG
	public: bool check_invariant() const;
#endif


	////////////////////////////////	STATE
	public: general_purpose_ast_t _tree;
};


/*
	Semantic Analysis -> SYMBOL TABLE + annotated AST
*/
semantic_ast_t run_semantic_analysis(const pass2_ast_t& ast);

ast_json_t semantic_ast_to_json(const semantic_ast_t& ast);
semantic_ast_t json_to_semantic_ast(const ast_json_t& json);


}	// Floyd
#endif /* pass3_hpp */


