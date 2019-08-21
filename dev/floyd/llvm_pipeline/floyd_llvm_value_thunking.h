//
//  floyd_llvm_value_thunking.hpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-19.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#ifndef floyd_llvm_value_thunking_hpp
#define floyd_llvm_value_thunking_hpp


#include "floyd_llvm_values.h"


namespace floyd {

////////////////////////////////		value_mgr_t


struct value_mgr_t {
	bool check_invariant() const {
		QUARK_ASSERT(type_lookup.check_invariant());
		QUARK_ASSERT(heap.check_invariant());
		return true;
	}

	
	llvm_type_lookup type_lookup;
	const llvm::DataLayout& data_layout;
	heap_t heap;
	std::vector<std::pair<link_name_t, void*>> native_func_lookup;
};


runtime_value_t to_runtime_string2(value_mgr_t& value_mgr, const std::string& s);
std::string from_runtime_string2(const value_mgr_t& value_mgr, runtime_value_t encoded_value);

runtime_value_t to_runtime_value2(value_mgr_t& value_mgr, const value_t& value);
value_t from_runtime_value2(const value_mgr_t& value_mgr, const runtime_value_t encoded_value, const typeid_t& type);






void retain_value(value_mgr_t& value_mgr, runtime_value_t value, const typeid_t& type);
void release_deep(value_mgr_t& value_mgr, runtime_value_t value, const typeid_t& type);
void release_dict_deep(value_mgr_t& value_mgr, DICT_CPPMAP_T* dict, const typeid_t& type);
void release_vec_deep(value_mgr_t& value_mgr, VECTOR_ARRAY_T* vec, const typeid_t& type);
void release_struct_deep(value_mgr_t& value_mgr, STRUCT_T* s, const typeid_t& type);


}	// floyd

#endif /* floyd_llvm_value_thunking_hpp */
