//
//  floyd_llvm_helpers.hpp
//  floyd_speak
//
//  Created by Marcus Zetterquist on 2019-04-16.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#ifndef floyd_llvm_helpers_hpp
#define floyd_llvm_helpers_hpp

#include "ast_typeid.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>

namespace floyd {




void NOT_IMPLEMENTED_YET() __dead2;
void UNSUPPORTED() __dead2;


//	Must LLVMContext be kept while using the execution engine? Keep it!
struct llvm_instance_t {
	bool check_invariant() const {
		return true;
	}

	llvm::LLVMContext context;
};



//	LLVM-functions pass GENs as two 64bit arguments.
//	Return: First is the type of the value. Second is tells if
/*
std::pair<llvm::Type*, bool> intern_type_generics(llvm::Module& module, const typeid_t& type){
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(type.is_function() == false);

	auto& context = module.getContext();

	if(type.is_internal_dynamic()){
		return { llvm::Type::getIntNTy(context, 64), llvm::Type::getIntNTy(context, 64) };
	}
	else{
		return { intern_type(module, type), nullptr };
	}
}
*/


/*
	FLOYD ARGS				LLVM ARGS
	----------				--------------------
							int32*		"floyd_runtime_ptr"
	int icecreams			int			icecreams
	----------				--------------------
							int32*		"floyd_runtime_ptr"
	string nick				int8*		nick
	----------				--------------------
							int32*		"floyd_runtime_ptr"
	DYN val					int64_t		val-DYNVAL
							int64_t		val-typei
	----------				--------------------
							int32*		"floyd_runtime_ptr"
	int icecreams			int			icecreams
	DYN val					int64_t		val-dynval
							int64_t		val-type
	string nick				int8*		nick

*/





bool check_invariant__function(const llvm::Function* f);
bool check_invariant__module(llvm::Module* module);
bool check_invariant__builder(llvm::IRBuilder<>* builder);

std::string print_module(llvm::Module& module);
std::string print_type(llvm::Type* type);
std::string print_function(const llvm::Function* f);
std::string print_value(llvm::Value* value);


////////////////////////////////	floyd_runtime_ptr


//	This pointer is passed as argument 0 to all compiled floyd functions and all runtime functions.

bool check_callers_fcp(llvm::Function& emit_f);
bool check_emitting_function(llvm::Function& emit_f);
llvm::Value* get_callers_fcp(llvm::Function& emit_f);
llvm::Type* make_frp_type(llvm::LLVMContext& context);




////////////////////////////////		VEC_T



//??? need word for "encoded". How data is stuffed into the LLVM instruction set..
/*
	Vectors

	- Vector instance is a 16 byte struct.
	- No RC or shared state -- always copied fully.
	- Mutation = copy entire vector every time.

	- The runtime handles all vectors as std::vector<uint64_t>. You need to pack and address other types of data manually.
*/
struct VEC_T {
	uint64_t* element_ptr;
	uint32_t magic;	//	0xDABBAD00
	uint32_t element_count;
};

enum class VEC_T_MEMBERS {
	element_ptr = 0,
	magic = 1,
	element_count = 2
};

//	Makes a type for VEC_T.
llvm::Type* make_vec_type(llvm::LLVMContext& context);

llvm::Value* get_vec_ptr(llvm::IRBuilder<>& builder, llvm::Value* vec_byvalue);



////////////////////////////////		DYN_RETURN_T


//	??? Also use for arguments, not only return.
struct DYN_RETURN_T {
	uint64_t a;
	uint64_t b;
};

enum class DYN_RETURN_MEMBERS {
	a = 0,
	b = 1
};


DYN_RETURN_T make_dyn_return(uint64_t a, uint64_t b);
DYN_RETURN_T make_dyn_return(const VEC_T& vec);
llvm::Type* make_dynreturn_type(llvm::LLVMContext& context);

llvm::Type* make_dyn_value_type(llvm::LLVMContext& context);
llvm::Type* make_dyn_type_type(llvm::LLVMContext& context);

llvm::Type* make_encode_type(llvm::LLVMContext& context);

//	Encode a VEC_T usings its address. Alternative: could pass it as argument by-value.
llvm::Value* get_vec_as_dyn(llvm::IRBuilder<>& builder, llvm::Value* vec_byvalue);







////////////////////////////////		llvm_arg_mapping_t


//	One element for each LLVM argument.
struct llvm_arg_mapping_t {
	llvm::Type* llvm_type;

	std::string floyd_name;
	typeid_t floyd_type;
	int floyd_arg_index;	//-1 is none. Several elements can specify the same Floyd arg index, since dynamic value use two.
	enum class map_type { k_floyd_runtime_ptr, k_simple_value, k_dyn_value, k_dyn_type } map_type;
};

struct llvm_function_def_t {
	llvm::Type* return_type;
	std::vector<llvm_arg_mapping_t> args;
	std::vector<llvm::Type*> llvm_args;
};

llvm_function_def_t name_args(const llvm_function_def_t& def, const std::vector<member_t>& args);
llvm_function_def_t map_function_arguments(llvm::Module& module, const floyd::typeid_t& function_type);





////////////////////////////////		intern_type()

llvm::Type* make_function_type(llvm::Module& module, const typeid_t& function_type);

llvm::Type* intern_type(llvm::Module& module, const typeid_t& type);


}	//	floyd

#endif /* floyd_llvm_helpers_hpp */
