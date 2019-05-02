//
//  floyd_llvm_runtime.cpp
//  floyd_speak
//
//  Created by Marcus Zetterquist on 2019-04-24.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "floyd_llvm_runtime.h"

#include "sha1_class.h"
#include "text_parser.h"
#include "host_functions.h"

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DataLayout.h>

#include "llvm/Bitcode/BitstreamWriter.h"


namespace floyd {



//	The names of these are computed from the host-id in the symbol table, not the names of the functions/symbols.
//	They must use C calling convention so llvm JIT can find them.
//	Make sure they are not dead-stripped out of binary!
void floyd_runtime__unresolved_func(void* floyd_runtime_ptr);

llvm_execution_engine_t& get_floyd_runtime(void* floyd_runtime_ptr);

value_t runtime_value_to_floyd(const llvm_execution_engine_t& runtime, const runtime_value_t encoded_value, const typeid_t& type);



const function_def_t& find_function_def2(const std::vector<function_def_t>& function_defs, const std::string& function_name){
	auto it = std::find_if(function_defs.begin(), function_defs.end(), [&] (const function_def_t& e) { return e.def_name == function_name; } );
	QUARK_ASSERT(it != function_defs.end());

	QUARK_ASSERT(it->llvm_f != nullptr);
	return *it;
}


void copy_elements(runtime_value_t dest[], runtime_value_t source[], uint32_t count){
	for(auto i = 0 ; i < count ; i++){
		dest[i] = source[i];
	}
}




void* get_global_ptr(llvm_execution_engine_t& ee, const std::string& name){
	QUARK_ASSERT(ee.check_invariant());
	QUARK_ASSERT(name.empty() == false);

	const auto addr = ee.ee->getGlobalValueAddress(name);
	return  (void*)addr;
}

void* get_global_function(llvm_execution_engine_t& ee, const std::string& name){
	QUARK_ASSERT(ee.check_invariant());
	QUARK_ASSERT(name.empty() == false);

	const auto addr = ee.ee->getFunctionAddress(name);
	return (void*)addr;
}


std::pair<void*, typeid_t> bind_function(llvm_execution_engine_t& ee, const std::string& name){
	QUARK_ASSERT(ee.check_invariant());
	QUARK_ASSERT(name.empty() == false);

	const auto f = reinterpret_cast<FLOYD_RUNTIME_F*>(get_global_function(ee, name));
	if(f != nullptr){
		const function_def_t def = find_function_def2(ee.function_defs, std::string() + "floyd_funcdef__" + name);
		const auto function_type = def.floyd_fundef._function_type;
		return { f, function_type };
	}
	else{
		return { nullptr, typeid_t::make_undefined() };
	}
}

value_t call_function(llvm_execution_engine_t& ee, const std::pair<void*, typeid_t>& f){
	QUARK_ASSERT(f.first != nullptr);
	QUARK_ASSERT(f.second.is_function());

	const auto function_ptr = reinterpret_cast<FLOYD_RUNTIME_F*>(f.first);
	runtime_value_t result = (*function_ptr)(&ee, "?dummy arg to main()?");

	const auto return_type = f.second.get_function_return();
	return runtime_value_to_floyd(ee, result, return_type);
}

std::pair<void*, typeid_t> bind_global(llvm_execution_engine_t& ee, const std::string& name){
	QUARK_ASSERT(ee.check_invariant());
	QUARK_ASSERT(name.empty() == false);

	const auto global_ptr = get_global_ptr(ee, name);
	if(global_ptr != nullptr){
		auto symbol = find_symbol(ee.global_symbols, name);
		QUARK_ASSERT(symbol != nullptr);

		return { global_ptr, symbol->get_type() };
	}
	else{
		return { nullptr, typeid_t::make_undefined() };
	}
}

//??? Use visitor for typeid
// IMPORTANT: The value can be a global variable of various sizes, for example a BYTE. We cannot dereference pointer as a uint64*!!
value_t load_global_from_ptr(const llvm_execution_engine_t& runtime, const void* value_ptr, const typeid_t& type){
	QUARK_ASSERT(runtime.check_invariant());
	QUARK_ASSERT(value_ptr != nullptr);
	QUARK_ASSERT(type.check_invariant());

	//??? more types.
	if(type.is_undefined()){
	}
	else if(type.is_bool()){
		//??? How is int1 encoded by LLVM?
		const auto temp = *static_cast<const uint8_t*>(value_ptr);
		return value_t::make_bool(temp == 0 ? false : true);
	}
	else if(type.is_int()){
		const auto temp = *static_cast<const uint64_t*>(value_ptr);
		return value_t::make_int(temp);
	}
	else if(type.is_double()){
		const auto temp = *static_cast<const double*>(value_ptr);
		return value_t::make_double(temp);
	}
	else if(type.is_string()){
		const char* s = *(const char**)(value_ptr);
		return value_t::make_string(s);
	}
	else if(type.is_json_value()){
		const json_t* json_ptr = *(const json_t**)(value_ptr);
		if(json_ptr == nullptr){
			return value_t::make_json_value(json_t());
		}
		else{
			return value_t::make_json_value(*json_ptr);
		}
	}
	else if(type.is_typeid()){
		const auto value = *static_cast<const int32_t*>(value_ptr);
		const auto runtime_type = make_runtime_type(value);
		return runtime_value_to_floyd(runtime, make_runtime_typeid(runtime_type), type);
	}
	else if(type.is_struct()){
		const auto struct_ptr_as_int = *reinterpret_cast<const uint64_t*>(value_ptr);
		auto struct_ptr = reinterpret_cast<void*>(struct_ptr_as_int);
		return runtime_value_to_floyd(runtime, make_runtime_struct(struct_ptr), type);
	}
	else if(type.is_vector()){
		auto vec0 = *static_cast<const runtime_value_t*>(value_ptr);
		return runtime_value_to_floyd(runtime, vec0, type);
	}
	else if(type.is_dict()){
		NOT_IMPLEMENTED_YET();
	}
	else if(type.is_function()){
		NOT_IMPLEMENTED_YET();
	}
	else{
	}
	NOT_IMPLEMENTED_YET();
	QUARK_ASSERT(false);
	throw std::exception();
}

value_t load_global(llvm_execution_engine_t& ee, const std::pair<void*, typeid_t>& v){
	QUARK_ASSERT(v.first != nullptr);
	QUARK_ASSERT(v.second.is_undefined() == false);

	return load_global_from_ptr(ee, v.first, v.second);
}

value_t runtime_value_to_floyd(const llvm_execution_engine_t& runtime, const runtime_value_t encoded_value, const typeid_t& type){
	QUARK_ASSERT(type.check_invariant());

	//??? more types. Use visitor.
	if(type.is_undefined()){
		NOT_IMPLEMENTED_YET();
	}
	else if(type.is_bool()){
		//??? How is int1 encoded by LLVM?
		return value_t::make_bool(encoded_value.bool_value == 0 ? false : true);
	}
	else if(type.is_int()){
		return value_t::make_int(encoded_value.int_value);
	}
	else if(type.is_double()){
		return value_t::make_double(encoded_value.double_value);
	}
	else if(type.is_string()){
		return value_t::make_string(encoded_value.string_ptr);
	}
	else if(type.is_json_value()){
		if(encoded_value.json_ptr == nullptr){
			return value_t::make_json_value(json_t());
		}
		else{
			return value_t::make_json_value(*encoded_value.json_ptr);
		}
	}
	else if(type.is_typeid()){
		const auto type1 = lookup_type(runtime.type_interner, encoded_value.typeid_itype);
		const auto type2 = value_t::make_typeid_value(type1);
		return type2;
	}
	else if(type.is_struct()){
		const auto& struct_def = type.get_struct();
		std::vector<value_t> members;
		int member_index = 0;
		auto t2 = make_struct_type(runtime.instance->context, type);
		const llvm::DataLayout& data_layout = runtime.ee->getDataLayout();
		const llvm::StructLayout* layout = data_layout.getStructLayout(t2);
		const auto struct_base_ptr = reinterpret_cast<const uint8_t*>(encoded_value.struct_ptr);
		for(const auto& e: struct_def._members){
			const auto offset = layout->getElementOffset(member_index);
			const auto member_ptr = reinterpret_cast<const runtime_value_t*>(struct_base_ptr + offset);
			const auto member_value = runtime_value_to_floyd(runtime, *member_ptr, e._type);
			members.push_back(member_value);
			member_index++;
		}
		return value_t::make_struct_value(type, members);
	}
	else if(type.is_vector()){
		const auto element_type = type.get_vector_element_type();

		//??? test with non-string vectors.
		const auto vec = encoded_value.vector_ptr;

		std::vector<value_t> elements;
		const int count = vec->element_count;
		for(int i = 0 ; i < count ; i++){
			const auto value_encoded = vec->element_ptr[i];
			const auto value = runtime_value_to_floyd(runtime, value_encoded, element_type);
			elements.push_back(value);
		}
		const auto val = value_t::make_vector_value(element_type, elements);
		return val;

/*
		std::vector<value_t> vec2;
		const auto element_type = type.get_vector_element_type();
		if(element_type.is_string()){
			for(int i = 0 ; i < vec.element_count ; i++){
				auto s = (const char*)vec.element_ptr[i];
				const auto a = std::string(s);
				vec2.push_back(value_t::make_string(a));
			}
			return value_t::make_vector_value(element_type, vec2);
		}
		else if(element_type.is_json_value()){
			for(int i = 0 ; i < vec.element_count ; i++){
				const auto& s = *reinterpret_cast<const json_t*>(vec.element_ptr[i]);
				vec2.push_back(value_t::make_json_value(s));
			}
			return value_t::make_vector_value(element_type, vec2);
		}
		else if(element_type.is_bool()){
			for(int i = 0 ; i < vec.element_count ; i++){
				auto s = vec.element_ptr[i];
				vec2.push_back(value_t::make_bool(s != 0x00));
			}
			return value_t::make_vector_value(element_type, vec2);
		}
		else if(element_type.is_int()){
			for(int i = 0 ; i < vec.element_count ; i++){
				auto s = vec.element_ptr[i];
				vec2.push_back(value_t::make_int(s));
			}
			return value_t::make_vector_value(element_type, vec2);
		}
		else if(element_type.is_double()){
			for(int i = 0 ; i < vec.element_count ; i++){
				auto s = vec.element_ptr[i];
				auto d = *(double*)&s;
				vec2.push_back(value_t::make_double(d));
			}
			return value_t::make_vector_value(element_type, vec2);
		}
		else{
			NOT_IMPLEMENTED_YET();
		}
*/
/*
!!!
	else if(type.is_vector()){
		auto vec = *static_cast<const VEC_T*>(value_ptr);
		std::vector<value_t> vec2;
		if(type.get_vector_element_type().is_string()){
			for(int i = 0 ; i < vec.element_count ; i++){
				auto s = (const char*)vec.element_ptr[i];
				const auto a = std::string(s);
				vec2.push_back(value_t::make_string(a));
			}
			return value_t::make_vector_value(typeid_t::make_string(), vec2);
		}
		else if(type.get_vector_element_type().is_bool()){
			for(int i = 0 ; i < vec.element_count ; i++){
				auto s = vec.element_ptr[i / 64];
				bool masked = s & (1 << (i & 63));
				vec2.push_back(value_t::make_bool(masked == 0 ? false : true));
			}
			return value_t::make_vector_value(typeid_t::make_bool(), vec2);
		}
		else{
		NOT_IMPLEMENTED_YET();
		}
	}
*/
	}
	else if(type.is_dict()){
		const auto value_type = type.get_dict_value_type();
		const auto dict = encoded_value.dict_ptr;

		std::map<std::string, value_t> values;
		const auto& map2 = dict->body_ptr->map;
		for(const auto& e: map2){
			const auto value = runtime_value_to_floyd(runtime, e.second, value_type);
			values.insert({ e.first, value} );
		}
		const auto val = value_t::make_dict_value(type, values);
		return val;
	}
	else if(type.is_function()){
		NOT_IMPLEMENTED_YET();
	}
	else{
		NOT_IMPLEMENTED_YET();
	}
	QUARK_ASSERT(false);
	throw std::exception();
}




/*
@variable = global i32 21
define i32 @main() {
%1 = load i32, i32* @variable ; load the global variable
%2 = mul i32 %1, 2
store i32 %2, i32* @variable ; store instruction to write to global variable
ret i32 %2
}
*/

llvm_execution_engine_t& get_floyd_runtime(void* floyd_runtime_ptr){
	QUARK_ASSERT(floyd_runtime_ptr != nullptr);

	auto ptr = reinterpret_cast<llvm_execution_engine_t*>(floyd_runtime_ptr);
	QUARK_ASSERT(ptr != nullptr);
	QUARK_ASSERT(ptr->debug_magic == k_debug_magic);
	return *ptr;
}

void hook(const std::string& s, void* floyd_runtime_ptr, runtime_value_t arg){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	throw std::runtime_error("HOST FUNCTION NOT IMPLEMENTED FOR LLVM");
}



std::string gen_to_string(llvm_execution_engine_t& runtime, runtime_value_t arg_value, runtime_type_t arg_type){
	QUARK_ASSERT(runtime.check_invariant());

	const auto type = lookup_type(runtime.type_interner, arg_type);
	const auto value = runtime_value_to_floyd(runtime, arg_value, type);
	const auto a = to_compact_string2(value);
	return a;
}


//	The names of these are computed from the host-id in the symbol table, not the names of the functions/symbols.
//	They must use C calling convention so llvm JIT can find them.
//	Make sure they are not dead-stripped out of binary!

void floyd_runtime__unresolved_func(void* floyd_runtime_ptr){
	std:: cout << __FUNCTION__ << std::endl;
}

















//	Host functions, automatically called by the LLVM execution engine.
////////////////////////////////////////////////////////////////////////////////


int32_t floyd_runtime__compare_strings(void* floyd_runtime_ptr, int64_t op, const char* lhs, const char* rhs){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	/*
		strcmp()
			Greater than zero ( >0 ): A value greater than zero is returned when the first not matching character in leftStr have the greater ASCII value than the corresponding character in rightStr or we can also say

			Less than Zero ( <0 ): A value less than zero is returned when the first not matching character in leftStr have lesser ASCII value than the corresponding character in rightStr.

	*/
	const auto result = std::strcmp(lhs, rhs);

	const auto op2 = static_cast<expression_type>(op);
	if(op2 == expression_type::k_comparison_smaller_or_equal__2){
		return result <= 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_smaller__2){
		return result < 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_larger_or_equal__2){
		return result >= 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_larger__2){
		return result > 0 ? 1 : 0;
	}

	else if(op2 == expression_type::k_logical_equal__2){
		return result == 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_logical_nonequal__2){
		return result != 0 ? 1 : 0;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

host_func_t floyd_runtime__compare_strings__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(context),
		{
			make_frp_type(context),
			llvm::Type::getInt64Ty(context),
			llvm::Type::getInt8PtrTy(context),
			llvm::Type::getInt8PtrTy(context)
		},
		false
	);
	return { "floyd_runtime__compare_strings", function_type, reinterpret_cast<void*>(floyd_runtime__compare_strings) };
}


const char* floyd_runtime__concatunate_strings(void* floyd_runtime_ptr, const char* lhs, const char* rhs){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	QUARK_ASSERT(lhs != nullptr);
	QUARK_ASSERT(rhs != nullptr);
	QUARK_TRACE_SS(__FUNCTION__ << " " << std::string(lhs) << " comp " <<std::string(rhs));

	std::size_t len = strlen(lhs) + strlen(rhs);
	char* s = reinterpret_cast<char*>(std::malloc(len + 1));
	strcpy(s, lhs);
	strcpy(s + strlen(lhs), rhs);
	return s;
}

host_func_t floyd_runtime__concatunate_strings__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getInt8PtrTy(context),
		{
			make_frp_type(context),
			llvm::Type::getInt8PtrTy(context),
			llvm::Type::getInt8PtrTy(context)
		},
		false
	);
	return { "floyd_runtime__concatunate_strings", function_type, reinterpret_cast<void*>(floyd_runtime__concatunate_strings) };
}


const uint8_t* floyd_runtime__allocate_memory(void* floyd_runtime_ptr, int64_t bytes){
	auto s = reinterpret_cast<uint8_t*>(std::calloc(1, bytes));
	return s;
}

host_func_t floyd_runtime__allocate_memory__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(context)->getPointerTo(),
		{
			make_frp_type(context),
			llvm::Type::getInt64Ty(context)
		},
		false
	);
	return { "floyd_runtime__allocate_memory", function_type, reinterpret_cast<void*>(floyd_runtime__allocate_memory) };
}





////////////////////////////////		allocate_vector()


//	Creates a new VEC_T with element_count. All elements are blank. Caller owns the result.
VEC_T* floyd_runtime__allocate_vector(void* floyd_runtime_ptr, uint32_t element_count){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	VEC_T v = make_vec(element_count);
	return new VEC_T(v);
}

host_func_t floyd_runtime__allocate_vector__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		make_vec_type(context)->getPointerTo(),
		{
			make_frp_type(context),
			llvm::Type::getInt32Ty(context)
		},
		false
	);
	return { "floyd_runtime__allocate_vector", function_type, reinterpret_cast<void*>(floyd_runtime__allocate_vector) };
}


////////////////////////////////		delete_vector()


void floyd_runtime__delete_vector(void* floyd_runtime_ptr, VEC_T* vec){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	QUARK_ASSERT(vec != nullptr);
	QUARK_ASSERT(vec->check_invariant());

	delete_vec(*vec);
}

host_func_t floyd_runtime__delete_vector__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(context),
		{
			make_frp_type(context),
			make_vec_type(context)->getPointerTo()
		},
		false
	);
	return { "floyd_runtime__delete_vector", function_type, reinterpret_cast<void*>(floyd_runtime__delete_vector) };
}

VEC_T* floyd_runtime__concatunate_vectors(void* floyd_runtime_ptr, const VEC_T* lhs, const VEC_T* rhs){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	QUARK_ASSERT(lhs != nullptr);
	QUARK_ASSERT(lhs->check_invariant());
	QUARK_ASSERT(rhs != nullptr);
	QUARK_ASSERT(rhs->check_invariant());

	auto result = make_vec(lhs->element_count + rhs->element_count);
	for(int i = 0 ; i < lhs->element_count ; i++){
		result.element_ptr[i] = lhs->element_ptr[i];
	}
	for(int i = 0 ; i < rhs->element_count ; i++){
		result.element_ptr[lhs->element_count + i] = rhs->element_ptr[i];
	}
	return new VEC_T(result);
}

host_func_t floyd_runtime__concatunate_vectors__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		make_vec_type(context)->getPointerTo(),
		{
			make_frp_type(context),
			make_vec_type(context)->getPointerTo(),
			make_vec_type(context)->getPointerTo()
		},
		false
	);
	return { "floyd_runtime__concatunate_vectors", function_type, reinterpret_cast<void*>(floyd_runtime__concatunate_vectors) };
}




////////////////////////////////		allocate_dict()


void* floyd_runtime__allocate_dict(void* floyd_runtime_ptr){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	auto v = make_dict();
	return new DICT_T(v);
}

host_func_t floyd_runtime__allocate_dict__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		make_dict_type(context)->getPointerTo(),
		{
			make_frp_type(context)
		},
		false
	);
	return { "floyd_runtime__allocate_dict", function_type, reinterpret_cast<void*>(floyd_runtime__allocate_dict) };
}


////////////////////////////////		delete_dict()


void floyd_runtime__delete_dict(void* floyd_runtime_ptr, DICT_T* dict){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	QUARK_ASSERT(dict != nullptr);
	QUARK_ASSERT(dict->check_invariant());

	delete_dict(*dict);
}

host_func_t floyd_runtime__delete_dict__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(context),
		{
			make_frp_type(context),
			make_dict_type(context)->getPointerTo()
		},
		false
	);
	return { "floyd_runtime__delete_dict", function_type, reinterpret_cast<void*>(floyd_runtime__delete_dict) };
}


////////////////////////////////		store_dict()


DICT_T* floyd_runtime__store_dict(void* floyd_runtime_ptr, DICT_T* dict, const char* key_string, runtime_value_t element_value, runtime_type_t element_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

//	const auto element_type2 = lookup_type(r.type_interner, element_type);

	//	Deep copy dict.
	auto v = make_dict();
	v.body_ptr->map = dict->body_ptr->map;

	v.body_ptr->map.insert_or_assign(std::string(key_string), element_value);
	return new DICT_T(v);
}

host_func_t floyd_runtime__store_dict__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		make_dict_type(context)->getPointerTo(),
		{
			make_frp_type(context),
			make_dict_type(context)->getPointerTo(),
			llvm::Type::getInt8PtrTy(context),
			make_runtime_value_type(context),
			make_runtime_type_type(context)
		},
		false
	);
	return { "floyd_runtime__store_dict", function_type, reinterpret_cast<void*>(floyd_runtime__store_dict) };
}



////////////////////////////////		lookup_dict()


runtime_value_t floyd_runtime__lookup_dict(void* floyd_runtime_ptr, DICT_T* dict, const char* key_string){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

//	const auto element_type2 = lookup_type(r.type_interner, element_type);
	const auto it = dict->body_ptr->map.find(std::string(key_string));
	if(it == dict->body_ptr->map.end()){
		throw std::exception();
	}
	else{
		return it->second;
	}
}

host_func_t floyd_runtime__lookup_dict__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		make_runtime_value_type(context),
		{
			make_frp_type(context),
			make_dict_type(context)->getPointerTo(),
			llvm::Type::getInt8PtrTy(context)
		},
		false
	);
	return { "floyd_runtime__lookup_dict", function_type, reinterpret_cast<void*>(floyd_runtime__lookup_dict) };
}




////////////////////////////////		allocate_json()


int16_t* floyd_runtime__allocate_json(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto value = runtime_value_to_floyd(r, arg0_value, type0);

	const auto a = value_to_ast_json(value, json_tags::k_plain);
	auto result = new json_t(a);
	return reinterpret_cast<int16_t*>(result);

#if 0
	if(type0.is_string()){
		auto result = new json_t(value.get_string_value());
		return reinterpret_cast<int16_t*>(result);
	}
	else if(type0.is_json_value()){
		return reinterpret_cast<int16_t*>(arg0_value);
	}
	else if(type0.is_int()){
		auto result = new json_t(value.get_int_value());
		return reinterpret_cast<int16_t*>(result);
	}
	else if(type0.is_bool()){
		auto result = new json_t(value.get_bool_value());
		return reinterpret_cast<int16_t*>(result);
	}
	else if(type0.is_vector()){
		QUARK_ASSERT(type0.get_vector_element_type().is_json_value());

		const auto v = value.get_vector_value();
		std::vector<json_t> json_vec;
		for(const auto& e: v){
			json_vec.push_back(e.get_json_value());
		}

		auto result = new json_t(json_vec);
		return reinterpret_cast<int16_t*>(result);
	}
	else if(type0.is_dict()){
		QUARK_ASSERT(type0.get_dict_value_type().is_json_value());

		const auto v = value.get_vector_value();
		std::vector<json_t> json_vec;
		for(const auto& e: v){
			json_vec.push_back(e.get_json_value());
		}

		auto result = new json_t(json_vec);
		return reinterpret_cast<int16_t*>(result);
	}
	else{
		NOT_IMPLEMENTED_YET();
	}
#endif

}

host_func_t floyd_runtime__allocate_json__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getInt16PtrTy(context),
		{
			make_frp_type(context),
			make_runtime_value_type(context),
			make_runtime_type_type(context)
		},
		false
	);
	return { "floyd_runtime__allocate_json", function_type, reinterpret_cast<void*>(floyd_runtime__allocate_json) };
}




////////////////////////////////		lookup_json()



int16_t* floyd_runtime__lookup_json(void* floyd_runtime_ptr, int16_t* json_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto& json = *reinterpret_cast<const json_t*>(json_ptr);
	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto value = runtime_value_to_floyd(r, arg0_value, type0);

	if(json.is_object()){
		if(type0.is_string() == false){
			quark::throw_runtime_error("Attempting to lookup a json-object with a key that is not a string.");
		}

		const auto result = json.get_object_element(value.get_string_value());
		return reinterpret_cast<int16_t*>(new json_t(result));
	}
	else if(json.is_array()){
		if(type0.is_int() == false){
			quark::throw_runtime_error("Attempting to lookup a json-object with a key that is not a number.");
		}

		const auto result = json.get_array_n(value.get_int_value());
		return reinterpret_cast<int16_t*>(new json_t(result));
	}
	else{
		quark::throw_runtime_error("Attempting to lookup a json value -- lookup requires json-array or json-object.");
	}
}

host_func_t floyd_runtime__lookup_json__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getInt16PtrTy(context),
		{
			make_frp_type(context),
			llvm::Type::getInt16PtrTy(context),
			make_runtime_value_type(context),
			make_runtime_type_type(context)
		},
		false
	);
	return { "floyd_runtime__lookup_json", function_type, reinterpret_cast<void*>(floyd_runtime__lookup_json) };
}







////////////////////////////////		json_to_string()


char* floyd_runtime__json_to_string(void* floyd_runtime_ptr, uint16_t* json_ptr){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto& json = *reinterpret_cast<const json_t*>(json_ptr);

	if(json.is_string()){
		auto new_str = strdup(json.get_string().c_str());
		return new_str;
	}
	else{
		quark::throw_runtime_error("Attempting to assign a non-string JSON to a string.");
	}
}

host_func_t floyd_runtime__json_to_string__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getInt8PtrTy(context),
		{
			make_frp_type(context),
			llvm::Type::getInt16PtrTy(context),
		},
		false
	);
	return { "floyd_runtime__json_to_string", function_type, reinterpret_cast<void*>(floyd_runtime__json_to_string) };
}






////////////////////////////////		compare_values()


int32_t floyd_runtime__compare_values(void* floyd_runtime_ptr, int64_t op, const runtime_type_t type, runtime_value_t lhs, runtime_value_t rhs){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto value_type = lookup_type(r.type_interner, type);

	const auto left_value = runtime_value_to_floyd(r, lhs, value_type);
	const auto right_value = runtime_value_to_floyd(r, rhs, value_type);

	const int result = value_t::compare_value_true_deep(left_value, right_value);
//	int result = runtime_compare_value_true_deep((const uint64_t)lhs, (const uint64_t)rhs, vector_type);
	const auto op2 = static_cast<expression_type>(op);
	if(op2 == expression_type::k_comparison_smaller_or_equal__2){
		return result <= 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_smaller__2){
		return result < 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_larger_or_equal__2){
		return result >= 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_larger__2){
		return result > 0 ? 1 : 0;
	}

	else if(op2 == expression_type::k_logical_equal__2){
		return result == 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_logical_nonequal__2){
		return result != 0 ? 1 : 0;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

host_func_t floyd_runtime__compare_values__make(llvm::LLVMContext& context){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(context),
		{
			make_frp_type(context),
			llvm::Type::getInt64Ty(context),
			make_runtime_type_type(context),
			make_runtime_value_type(context),
			make_runtime_value_type(context)
		},
		false
	);
	return { "floyd_runtime__compare_values", function_type, reinterpret_cast<void*>(floyd_runtime__compare_values) };
}




std::vector<host_func_t> get_runtime_functions(llvm::LLVMContext& context){
	std::vector<host_func_t> result = {
		floyd_runtime__compare_strings__make(context),
		floyd_runtime__concatunate_strings__make(context),

		floyd_runtime__allocate_memory__make(context),

		floyd_runtime__allocate_vector__make(context),
		floyd_runtime__delete_vector__make(context),
		floyd_runtime__concatunate_vectors__make(context),

		floyd_runtime__allocate_dict__make(context),
		floyd_runtime__delete_dict__make(context),
		floyd_runtime__store_dict__make(context),
		floyd_runtime__lookup_dict__make(context),

		floyd_runtime__allocate_json__make(context),
		floyd_runtime__lookup_json__make(context),

		floyd_runtime__json_to_string__make(context),

		floyd_runtime__compare_values__make(context)
	};
	return result;
}






struct native_binary_t {
	const char* ascii40;
};

struct native_sha1_t {
	const char* ascii40;
};




//??? should be an int1?
void floyd_funcdef__assert(void* floyd_runtime_ptr, runtime_value_t arg){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	bool ok = arg.bool_value == 0 ? false : true;
	if(!ok){
		r._print_output.push_back("Assertion failed.");
		quark::throw_runtime_error("Floyd assertion failed.");
	}
}

void* floyd_funcdef__calc_binary_sha1(void* floyd_runtime_ptr, void* binary_ptr){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	QUARK_ASSERT(binary_ptr != nullptr);

	const auto& binary = *reinterpret_cast<const native_binary_t*>(binary_ptr);

	const auto& s = std::string(binary.ascii40);
	const auto sha1 = CalcSHA1(s);
	const auto ascii40 = SHA1ToStringPlain(sha1);

	auto result = new native_sha1_t();
	result->ascii40 = strdup(ascii40.c_str());
	return reinterpret_cast<uint32_t*>(result);
}

void* floyd_funcdef__calc_string_sha1(void* floyd_runtime_ptr, const char* str_ptr){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	QUARK_ASSERT(str_ptr != nullptr);

	const auto& s = std::string(str_ptr);
	const auto sha1 = CalcSHA1(s);
	const auto ascii40 = SHA1ToStringPlain(sha1);

	auto result = new native_sha1_t();
	result->ascii40 = strdup(ascii40.c_str());
	return reinterpret_cast<void*>(result);
}

void floyd_host_function_1003(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1004(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1005(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

WIDE_RETURN_T floyd_host_function__erase(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, runtime_value_t arg1_value, runtime_type_t arg1_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);

	if(type0.is_dict()){
		const auto& dict = unpack_dict_arg(r.type_interner, arg0_value, arg0_type);

		//	Deep copy dict.
		auto dict2 = make_dict();
		dict2.body_ptr->map = dict->body_ptr->map;

		const auto key_string = std::string(arg1_value.string_ptr);
		const auto erase_count = dict2.body_ptr->map.erase(key_string);
		return make_wide_return_dict(new DICT_T(dict2));
	}
	else{
		NOT_IMPLEMENTED_YET();
	}
}

uint32_t floyd_funcdef__exists(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, runtime_value_t arg1_value, runtime_type_t arg1_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);

	if(type0.is_dict()){
		const auto& dict = unpack_dict_arg(r.type_interner, arg0_value, arg0_type);
		const auto key_string = std::string(arg1_value.string_ptr);

		const auto it = dict->body_ptr->map.find(key_string);
		return it != dict->body_ptr->map.end() ? 1 : 0;
	}
	else{
		NOT_IMPLEMENTED_YET();
	}
}





typedef runtime_value_t (*FILTER_F)(void* floyd_runtime_ptr, runtime_value_t element_value);


//	[E] filter([E], bool f(E e))
WIDE_RETURN_T floyd_funcdef__filter(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, runtime_value_t arg1_value, runtime_type_t arg1_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);

/*
	if(type0.is_vector() == false || type2.is_function() == false || type2.get_function_args().size () != 2){
		quark::throw_runtime_error("reduce() parameter error.");
	}
*/

	const auto& vec = *arg0_value.vector_ptr;
	const auto f = reinterpret_cast<FILTER_F>(arg1_value.function_ptr);

	const auto e_type = lookup_runtime_type(r.type_interner, type0.get_vector_element_type());
	auto count = vec.element_count;

	std::vector<runtime_value_t> acc;

	for(int i = 0 ; i < count ; i++){
		const auto element_value = vec.element_ptr[i];
		const auto keep = (*f)(floyd_runtime_ptr, element_value);
		if(keep.bool_value != 0){
			acc.push_back(element_value);
		}
	}

	const auto count2 = (int32_t)acc.size();
	auto result_vec = new VEC_T(make_vec(count2));

	if(count2 > 0){
		//	Count > 0 required to get address to first element in acc.
		copy_elements(result_vec->element_ptr, &acc[0], count2);
	}
	return make_wide_return_vec(result_vec);
}
	
	


int64_t floyd_funcdef__find__string(llvm_execution_engine_t& floyd_runtime_ptr, const char s[], const char find[]){
	QUARK_ASSERT(s != nullptr);
	QUARK_ASSERT(find != nullptr);

	const auto str = std::string(s);
	const auto wanted2 = std::string(find);

	const auto r = str.find(wanted2);
	const auto result = r == std::string::npos ? -1 : static_cast<int64_t>(r);
	return result;
}

int64_t floyd_funcdef__find(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, const runtime_value_t arg1_value, runtime_type_t arg1_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);

	if(type0.is_string()){
		if(type1.is_string() == false){
			quark::throw_runtime_error("find(string) requires argument 2 to be a string.");
		}
		return floyd_funcdef__find__string(r, arg0_value.string_ptr, arg1_value.string_ptr);
	}
	else if(type0.is_vector()){
		if(type1 != type0.get_vector_element_type()){
			quark::throw_runtime_error("find([]) requires argument 2 to be of vector's element type.");
		}

		const auto vec = unpack_vec_arg(r.type_interner, arg0_value, arg0_type);

//		auto it = std::find_if(function_defs.begin(), function_defs.end(), [&] (const function_def_t& e) { return e.def_name == function_name; } );
		const auto it = std::find_if(vec->element_ptr, vec->element_ptr + vec->element_count, [&] (const runtime_value_t& e) { return e.int_value == arg1_value.int_value; } );
//		const auto it = std::find(vec->element_ptr, vec->element_ptr + vec->element_count, arg1_value);
		if(it == vec->element_ptr + vec->element_count){
			return -1;
		}
		else{
			const auto pos = it - vec->element_ptr;
			return pos;
		}
	}
	else{
		//	No other types allowed.
		throw std::exception();
	}
}



void floyd_host_function_1010(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1011(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1012(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1013(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

int64_t floyd_host_function__get_json_type(void* floyd_runtime_ptr, uint16_t* json_ptr){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto& json = *reinterpret_cast<const json_t*>(json_ptr);

	const auto result = get_json_type(json);
	return result;
}

int64_t floyd_funcdef__get_time_of_day(void* floyd_runtime_ptr){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	std::chrono::time_point<std::chrono::high_resolution_clock> t = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_seconds = t - r._start_time;
	const auto ms = elapsed_seconds.count() * 1000.0;
	return static_cast<int64_t>(ms);
}

char* floyd_funcdef__jsonvalue_to_script(void* floyd_runtime_ptr, int32_t* json_ptr){
	QUARK_ASSERT(json_ptr != nullptr);

	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto& json = *reinterpret_cast<const json_t*>(json_ptr);

	const std::string s = json_to_compact_string(json);
	auto result = strdup(s.c_str());
	return reinterpret_cast<char*>(result);
}

void floyd_host_function_1017(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}



//??? test map() with complex types of values
//??? map() and other functions should be select at compile time, not runtime!

	typedef WIDE_RETURN_T (*MAP_F)(void* floyd_runtime_ptr, runtime_value_t arg0_value);

WIDE_RETURN_T floyd_funcdef__map(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, runtime_value_t arg1_value, runtime_type_t arg1_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);
	if(type0.is_vector() == false){
		quark::throw_runtime_error("map() arg 1 must be a vector.");
	}
	const auto e_type = type0.get_vector_element_type();

	if(type1.is_function() == false){
		quark::throw_runtime_error("map() requires start and end to be integers.");
	}
	const auto f_arg_types = type1.get_function_args();
	const auto r_type = type1.get_function_return();

	if(f_arg_types.size() != 1){
		quark::throw_runtime_error("map() function f requries 1 argument.");
	}

	if(f_arg_types[0] != e_type){
		quark::throw_runtime_error("map() function f must accept collection elements as its argument.");
	}

	const auto input_element_type = f_arg_types[0];
	const auto output_element_type = r_type;

	const auto f = reinterpret_cast<MAP_F>(arg1_value.function_ptr);

	auto count = arg0_value.vector_ptr->element_count;
	auto result_vec = new VEC_T(make_vec(count));
	for(int i = 0 ; i < count ; i++){
		const auto wide_result1 = (*f)(floyd_runtime_ptr, arg0_value.vector_ptr->element_ptr[i]);
		result_vec->element_ptr[i] = wide_result1.a;
	}
	return make_wide_return_vec(result_vec);
}



//??? should use int to send character around, not string.
//??? Also function should always return ONE character, not a string!
typedef const char* (*MAP_STRING_F)(void* floyd_runtime_ptr, const char* char_s);

const char* floyd_funcdef__map_string(void* floyd_runtime_ptr, runtime_value_t string_s, runtime_value_t func){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const char* input_string = string_s.string_ptr;
	const auto f = reinterpret_cast<MAP_STRING_F>(func.function_ptr);

	auto count = strlen(input_string);

	std::string acc;
	for(int i = 0 ; i < count ; i++){
		const char element_str[2] = { input_string[i], 0x00 };
		const auto temp_char_str = (*f)(floyd_runtime_ptr, element_str);
		acc.insert(acc.end(), temp_char_str, temp_char_str + strlen(temp_char_str));
	}
	return strdup(acc.c_str());
}




void floyd_funcdef__print(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);
	const auto s = gen_to_string(r, arg0_value, arg0_type);
	r._print_output.push_back(s);
}




WIDE_RETURN_T floyd_funcdef__push_back(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, runtime_value_t arg1_value, runtime_type_t arg1_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);
	if(type0.is_string()){
		const auto value = arg0_value.string_ptr;

		QUARK_ASSERT(type1.is_int());

		std::size_t len = strlen(value);
		char* s = reinterpret_cast<char*>(std::malloc(len + 1 + 1));
		strcpy(s, value);
		s[len + 0] = (char)arg1_value.int_value;
		s[len + 1] = 0x00;

		return make_wide_return_charptr(s);
	}
	else if(type0.is_vector()){
		const auto vs = unpack_vec_arg(r.type_interner, arg0_value, arg0_type);

		QUARK_ASSERT(type1 == type0.get_vector_element_type());

		const auto element = arg1_value;

		auto v2 = floyd_runtime__allocate_vector(floyd_runtime_ptr, vs->element_count + 1);
		for(int i = 0 ; i < vs->element_count ; i++){
			v2->element_ptr[i] = vs->element_ptr[i];
		}
		v2->element_ptr[vs->element_count] = element;
		return make_wide_return_vec(v2);
	}
	else{
		//	No other types allowed.
		throw std::exception();
	}
}

void floyd_host_function_1022(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}



//		make_rec("reduce", host__reduce, 1035, typeid_t::make_function_dyn_return({ DYN, DYN, DYN }, epure::pure, typeid_t::return_dyn_type::arg1)),

	typedef runtime_value_t (*REDUCE_F)(void* floyd_runtime_ptr, runtime_value_t acc_value, runtime_value_t element_value);

//	R map([E] elements, R init, R f(R acc, E e))
WIDE_RETURN_T floyd_funcdef__reduce(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, runtime_value_t arg1_value, runtime_type_t arg1_type, runtime_value_t arg2_value, runtime_type_t arg2_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);
	const auto type2 = lookup_type(r.type_interner, arg2_type);

	if(type0.is_vector() == false || type2.is_function() == false || type2.get_function_args().size () != 2){
		quark::throw_runtime_error("reduce() parameter error.");
	}

	const auto& vec = *arg0_value.vector_ptr;
	const auto& init = arg1_value;
	const auto f = reinterpret_cast<REDUCE_F>(arg2_value.function_ptr);

/*
	if(
		elements._type.get_vector_element_type() != f._type.get_function_args()[1]
		&& init._type != f._type.get_function_args()[0]
	)
	{
		quark::throw_runtime_error("R reduce([E] elements, R init_value, R (R acc, E element) f");
	}
*/

	const auto e_type = lookup_runtime_type(r.type_interner, type0.get_vector_element_type());
	const auto r_type = lookup_runtime_type(r.type_interner, type2.get_function_return());

	auto count = vec.element_count;

	runtime_value_t acc = init;
	for(int i = 0 ; i < count ; i++){
		const auto element_value = vec.element_ptr[i];
		const auto acc2 = (*f)(floyd_runtime_ptr, acc, element_value);
		acc = acc2;
	}
	return make_wide_return_2x64(acc, {} );
}


void floyd_host_function_1024(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

char* floyd_funcdef__replace__string(llvm_execution_engine_t& floyd_runtime_ptr, const char s[], std::size_t start, std::size_t end, const char replace[]){
	auto s_len = std::strlen(s);
	auto replace_len = std::strlen(replace);

	auto end2 = std::min(end, s_len);
	auto start2 = std::min(start, end2);

	const std::size_t len2 = start2 + replace_len + (s_len - end2);
	auto s2 = reinterpret_cast<char*>(std::malloc(len2 + 1));
	std::memcpy(&s2[0], &s[0], start2);
	std::memcpy(&s2[start2], &replace[0], replace_len);
	std::memcpy(&s2[start2 + replace_len], &s[end2], s_len - end2);
	s2[start2 + replace_len + (s_len - end2)] = 0x00;
	return s2;
}


const WIDE_RETURN_T floyd_funcdef__replace(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, int64_t start, int64_t end, runtime_value_t arg3_value, runtime_type_t arg3_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	if(start < 0 || end < 0){
		quark::throw_runtime_error("replace() requires start and end to be non-negative.");
	}
	if(start > end){
		quark::throw_runtime_error("replace() requires start <= end.");
	}

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type3 = lookup_type(r.type_interner, arg3_type);

	if(type3 != type0){
		quark::throw_runtime_error("replace() requires argument 4 to be same type of collection.");
	}

	if(type0.is_string()){
		auto ret = floyd_funcdef__replace__string(r, arg0_value.string_ptr, (std::size_t)start, (std::size_t)end, arg3_value.string_ptr);
		return make_wide_return_charptr(ret);
	}
	else if(type0.is_vector()){
		const auto vec = unpack_vec_arg(r.type_interner, arg0_value, arg0_type);
		const auto replace_vec = unpack_vec_arg(r.type_interner, arg3_value, arg3_type);

		auto end2 = std::min(static_cast<uint32_t>(end), vec->element_count);
		auto start2 = std::min(static_cast<uint32_t>(start), end2);

		const int32_t section1_len = start2;
		const int32_t section2_len = replace_vec->element_count;
		const int32_t section3_len = vec->element_count - end2;

		const uint32_t len2 = section1_len + section2_len + section3_len;
		auto vec2 = make_vec(len2);
		copy_elements(&vec2.element_ptr[0], &vec->element_ptr[0], section1_len);
		copy_elements(&vec2.element_ptr[section2_len], &replace_vec->element_ptr[0], section2_len);
		copy_elements(&vec2.element_ptr[section1_len + section2_len], &vec->element_ptr[end2], section3_len);
		return make_wide_return_vec(new VEC_T(vec2));
	}
	else{
		//	No other types allowed.
		throw std::exception();
	}
}

int16_t* floyd_funcdef__script_to_jsonvalue(void* floyd_runtime_ptr, const char* string_s){
	QUARK_ASSERT(string_s != nullptr);

	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	std::pair<json_t, seq_t> result0 = parse_json(seq_t(string_s));
	auto result = new json_t(result0.first);
	return reinterpret_cast<int16_t*>(result);
}

void floyd_host_function_1027(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}



int64_t floyd_funcdef__size(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	if(type0.is_string()){
		const auto value = arg0_value.string_ptr;
		return std::strlen(value);
	}
	else if(type0.is_json_value()){
		const auto& json_value = *arg0_value.json_ptr;

		if(json_value.is_object()){
			return json_value.get_object_size();
		}
		else if(json_value.is_array()){
			return json_value.get_array_size();
		}
		else if(json_value.is_string()){
			return json_value.get_string().size();
		}
		else{
			quark::throw_runtime_error("Calling size() on unsupported type of value.");
		}
	}
	else if(type0.is_vector()){
		const auto vs = unpack_vec_arg(r.type_interner, arg0_value, arg0_type);
		return vs->element_count;
	}
	else if(type0.is_dict()){
		DICT_T* dict = unpack_dict_arg(r.type_interner, arg0_value, arg0_type);
		return dict->body_ptr->map.size();
	}
	else{
		//	No other types allowed.
		throw std::exception();
	}
}

const WIDE_RETURN_T floyd_funcdef__subset(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, int64_t start, int64_t end){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	if(start < 0 || end < 0){
		quark::throw_runtime_error("subset() requires start and end to be non-negative.");
	}

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	if(type0.is_string()){
		const auto value = arg0_value.string_ptr;

		std::size_t len = strlen(value);
		const std::size_t end2 = std::min(static_cast<std::size_t>(end), len);
		const std::size_t start2 = std::min(static_cast<std::size_t>(start), end2);
		std::size_t len2 = end2 - start2;

		char* s = reinterpret_cast<char*>(std::malloc(len2 + 1));
		std::memcpy(&s[0], &value[start2], len2);
		s[len2] = 0x00;
		return make_wide_return_charptr(s);
	}
	else if(type0.is_vector()){
		const auto vec = unpack_vec_arg(r.type_interner, arg0_value, arg0_type);

		const std::size_t end2 = std::min(static_cast<std::size_t>(end), static_cast<std::size_t>(vec->element_count));
		const std::size_t start2 = std::min(static_cast<std::size_t>(start), end2);
		std::size_t len2 = end2 - start2;

		if(len2 >= INT32_MAX){
			throw std::exception();
		}
		VEC_T vec2 = make_vec(static_cast<int32_t>(len2));
		for(int i = 0 ; i < len2 ; i++){
			vec2.element_ptr[i] = vec->element_ptr[start2 + i];
		}
		return make_wide_return_vec(new VEC_T(vec2));
	}
	else{
		//	No other types allowed.
		throw std::exception();
	}
}




void floyd_host_function_1030(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1031(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

const char* floyd_host__to_string(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto s = gen_to_string(r, arg0_value, arg0_type);

	//??? leaks. Lose new(), calloc() strdup() etc.
	return strdup(s.c_str());
}



//		make_rec("typeof", host__typeof, 1004, typeid_t::make_function(typeid_t::make_typeid(), { DYN }, epure::pure)),

int32_t floyd_host__typeof(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

#if DEBUG
	const auto type0 = lookup_type(r.type_interner, arg0_type);
	QUARK_ASSERT(type0.check_invariant());
#endif
	return arg0_type;
}


//	??? promote update() to a statement, rather than a function call. Replace all statement and expressions with function calls? LISP!
//	???	Update of structs should resolve member-name at compile time, replace with index.
//	??? Range should be integers, not DYN! Change host function prototype.
const WIDE_RETURN_T floyd_funcdef__update(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type, runtime_value_t arg1_value, runtime_type_t arg1_type, runtime_value_t arg2_value, runtime_type_t arg2_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	auto& context = r.instance->context;

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto type1 = lookup_type(r.type_interner, arg1_type);
	const auto type2 = lookup_type(r.type_interner, arg2_type);
	if(type0.is_string()){
		if(type1.is_int() == false){
			throw std::exception();
		}
		if(type2.is_int() == false){
			throw std::exception();
		}

		const auto str = arg0_value.string_ptr;
		const auto index = arg1_value.int_value;
		const auto new_char = (char)arg2_value.int_value;

		const auto len = strlen(str);

		if(index < 0 || index >= len){
			throw std::runtime_error("Position argument to update() is outside collection span.");
		}

		auto result = strdup(str);
		result[index] = new_char;
		return make_wide_return_charptr(result);
	}
	else if(type0.is_vector()){
		if(type1.is_int() == false){
			throw std::exception();
		}

		const auto vec = unpack_vec_arg(r.type_interner, arg0_value, arg0_type);
		const auto element_type = type0.get_vector_element_type();
		const auto index = arg1_value.int_value;

		if(element_type != type2){
			throw std::runtime_error("New value's type must match vector's element type.");
		}

		if(index < 0 || index >= vec->element_count){
			throw std::runtime_error("Position argument to update() is outside collection span.");
		}

		auto result = make_vec(vec->element_count);
		for(int i = 0 ; i < result.element_count ; i++){
			result.element_ptr[i] = vec->element_ptr[i];
		}
		result.element_ptr[index] = arg2_value;
		return make_wide_return_vec(new VEC_T(result));
	}
	else if(type0.is_dict()){
		if(type1.is_string() == false){
			throw std::exception();
		}
		const auto key_strptr = arg1_value.string_ptr;
		const auto dict = unpack_dict_arg(r.type_interner, arg0_value, arg0_type);

		//	Deep copy dict.
		auto dict2 = make_dict();
		dict2.body_ptr->map = dict->body_ptr->map;

		dict2.body_ptr->map.insert_or_assign(std::string(key_strptr), arg2_value);
		return make_wide_return_dict(new DICT_T(dict2));
	}
	else if(type0.is_struct()){
		if(type1.is_string() == false){
			throw std::exception();
		}
		if(type2.is_void() == true){
			throw std::exception();
		}

		const auto source_struct_ptr = arg0_value.string_ptr;

		const auto member_name = runtime_value_to_floyd(r, arg1_value, typeid_t::make_string()).get_string_value();
		if(member_name == ""){
			throw std::runtime_error("Must provide name of struct member to update().");
		}

		const auto& struct_def = type0.get_struct();
		int member_index = find_struct_member_index(struct_def, member_name);
		if(member_index == -1){
			throw std::runtime_error("Position argument to update() is outside collection span.");
		}

		const auto member_value = runtime_value_to_floyd(r, arg2_value, type2);

		//	Make copy of struct, overwrite member in copy.
 
		auto& struct_type_llvm = *make_struct_type(context, type0);

		const llvm::DataLayout& data_layout = r.ee->getDataLayout();
		const llvm::StructLayout* layout = data_layout.getStructLayout(&struct_type_llvm);
		const auto struct_bytes = layout->getSizeInBytes();

		//??? Touches memory twice.
		auto struct_ptr = reinterpret_cast<uint8_t*>(std::calloc(1, struct_bytes));
		std::memcpy(struct_ptr, source_struct_ptr, struct_bytes);

		const auto member_offset = layout->getElementOffset(member_index);
		const auto member_ptr = reinterpret_cast<const void*>(struct_ptr + member_offset);

		const auto member_type = struct_def._members[member_index]._type;

		if(type2 != member_type){
			throw std::runtime_error("New value must be same type as struct member's type.");
		}

		if(member_type.is_string()){
			auto dest = *(char**)(member_ptr);
			const char* source = member_value.get_string_value().c_str();
			char* new_string = strdup(source);
			dest = new_string;
		}
		else if(member_type.is_int()){
			auto dest = (int64_t*)member_ptr;
			*dest = member_value.get_int_value();
		}
		else{
			NOT_IMPLEMENTED_YET();
		}
		return make_wide_return_structptr(struct_ptr);
	}
	else{
		//	No other types allowed.
		throw std::exception();
	}
}

int16_t* floyd_funcdef__value_to_jsonvalue(void* floyd_runtime_ptr, runtime_value_t arg0_value, runtime_type_t arg0_type){
	auto& r = get_floyd_runtime(floyd_runtime_ptr);

	const auto type0 = lookup_type(r.type_interner, arg0_type);
	const auto value0 = runtime_value_to_floyd(r, arg0_value, type0);
	const auto j = value_to_ast_json(value0, json_tags::k_plain);
	auto result = new json_t(j);
	return reinterpret_cast<int16_t*>(result);
}

void floyd_host_function_1036(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1037(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1038(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_1039(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}




///////////////		TEST

void floyd_host_function_2002(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}

void floyd_host_function_2003(void* floyd_runtime_ptr, runtime_value_t arg){
	hook(__FUNCTION__, floyd_runtime_ptr, arg);
}



std::map<std::string, void*> get_host_functions_map2(){

	////////////////////////////////		CORE FUNCTIONS AND HOST FUNCTIONS
	const std::map<std::string, void*> host_functions_map = {
		{ "floyd_funcdef__assert", reinterpret_cast<void *>(&floyd_funcdef__assert) },
		{ "floyd_funcdef__calc_binary_sha1", reinterpret_cast<void *>(&floyd_funcdef__calc_binary_sha1) },
		{ "floyd_funcdef__calc_string_sha1", reinterpret_cast<void *>(&floyd_funcdef__calc_string_sha1) },
		{ "floyd_funcdef__create_directory_branch", reinterpret_cast<void *>(&floyd_host_function_1003) },
		{ "floyd_funcdef__delete_fsentry_deep", reinterpret_cast<void *>(&floyd_host_function_1004) },
		{ "floyd_funcdef__does_fsentry_exist", reinterpret_cast<void *>(&floyd_host_function_1005) },
		{ "floyd_funcdef__erase", reinterpret_cast<void *>(&floyd_host_function__erase) },
		{ "floyd_funcdef__exists", reinterpret_cast<void *>(&floyd_funcdef__exists) },
		{ "floyd_funcdef__filter", reinterpret_cast<void *>(&floyd_funcdef__filter) },
		{ "floyd_funcdef__find", reinterpret_cast<void *>(&floyd_funcdef__find) },

		{ "floyd_funcdef__get_fs_environment", reinterpret_cast<void *>(&floyd_host_function_1010) },
		{ "floyd_funcdef__get_fsentries_deep", reinterpret_cast<void *>(&floyd_host_function_1011) },
		{ "floyd_funcdef__get_fsentries_shallow", reinterpret_cast<void *>(&floyd_host_function_1012) },
		{ "floyd_funcdef__get_fsentry_info", reinterpret_cast<void *>(&floyd_host_function_1013) },
		{ "floyd_funcdef__get_json_type", reinterpret_cast<void *>(&floyd_host_function__get_json_type) },
		{ "floyd_funcdef__get_time_of_day", reinterpret_cast<void *>(&floyd_funcdef__get_time_of_day) },
		{ "floyd_funcdef__jsonvalue_to_script", reinterpret_cast<void *>(&floyd_funcdef__jsonvalue_to_script) },
		{ "floyd_funcdef__jsonvalue_to_value", reinterpret_cast<void *>(&floyd_host_function_1017) },
		{ "floyd_funcdef__map", reinterpret_cast<void *>(&floyd_funcdef__map) },
		{ "floyd_funcdef__map_string", reinterpret_cast<void *>(&floyd_funcdef__map_string) },

		{ "floyd_funcdef__print", reinterpret_cast<void *>(&floyd_funcdef__print) },
		{ "floyd_funcdef__push_back", reinterpret_cast<void *>(&floyd_funcdef__push_back) },
		{ "floyd_funcdef__read_text_file", reinterpret_cast<void *>(&floyd_host_function_1022) },
		{ "floyd_funcdef__reduce", reinterpret_cast<void *>(&floyd_funcdef__reduce) },
		{ "floyd_funcdef__rename_fsentry", reinterpret_cast<void *>(&floyd_host_function_1024) },
		{ "floyd_funcdef__replace", reinterpret_cast<void *>(&floyd_funcdef__replace) },
		{ "floyd_funcdef__script_to_jsonvalue", reinterpret_cast<void *>(&floyd_funcdef__script_to_jsonvalue) },
		{ "floyd_funcdef__send", reinterpret_cast<void *>(&floyd_host_function_1027) },
		{ "floyd_funcdef__size", reinterpret_cast<void *>(&floyd_funcdef__size) },
		{ "floyd_funcdef__subset", reinterpret_cast<void *>(&floyd_funcdef__subset) },

		{ "floyd_funcdef__supermap", reinterpret_cast<void *>(&floyd_host_function_1030) },
		{ "floyd_funcdef__to_pretty_string", reinterpret_cast<void *>(&floyd_host_function_1031) },
		{ "floyd_funcdef__to_string", reinterpret_cast<void *>(&floyd_host__to_string) },
		{ "floyd_funcdef__typeof", reinterpret_cast<void *>(&floyd_host__typeof) },
		{ "floyd_funcdef__update", reinterpret_cast<void *>(&floyd_funcdef__update) },
		{ "floyd_funcdef__value_to_jsonvalue", reinterpret_cast<void *>(&floyd_funcdef__value_to_jsonvalue) },
		{ "floyd_funcdef__write_text_file", reinterpret_cast<void *>(&floyd_host_function_1036) }
	};
	return host_functions_map;
}

uint64_t call_floyd_runtime_init(llvm_execution_engine_t& ee){
	QUARK_ASSERT(ee.check_invariant());

	auto a_func = reinterpret_cast<FLOYD_RUNTIME_INIT>(get_global_function(ee, "floyd_runtime_init"));
	QUARK_ASSERT(a_func != nullptr);

	int64_t a_result = (*a_func)((void*)&ee);
	QUARK_ASSERT(a_result == 667);
	return a_result;
}

}	//	namespace floyd
