//
//  bytecode_intrinsics_h
//  Floyd
//
//  Created by Marcus Zetterquist on 2018-02-23.
//  Copyright © 2018 Marcus Zetterquist. All rights reserved.
//

#ifndef bytecode_intrinsics_h
#define bytecode_intrinsics_h

/*
	Provides built-in intrinsics: these are operators built into the language itself.
*/

#include "quark.h"

#include <map>
#include "bytecode_interpreter.h"


namespace floyd {


//	Create lookup from function id -> C function pointer.
std::map<function_id_t, BC_NATIVE_FUNCTION_PTR> bc_get_intrinsics();


}	//	floyd

#endif /* bytecode_intrinsics_h */
