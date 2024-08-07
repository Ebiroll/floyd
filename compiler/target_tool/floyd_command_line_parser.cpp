//
//  command_line_parser.cpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-09.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "floyd_command_line_parser.h"

#include "compiler_basics.h"
#include "file_handling.h"
#include "text_parser.h"
#include "quark.h"

#include <vector>
#include <string>
#include <sstream>

namespace floyd {


/*
	git commit -m "Commit message"
		command: "git"
		subcommand: "commit"
		flags
			m: ""
		extra_arguments
			"Commit message"


floyd bench mygame.floyd
floyd bench --module img_lib blur
floyd bench "img_lib:Linear veq" "img_lib:Smart tiling" "blur:blur1" "blur:b2"
*/

std::string get_help(){
	std::stringstream ss;

ss << "Floyd Programming Language MIT license."
<<
R"___(
USAGE

|help     | floyd help                         | Show built in help for command line tool
|run      | floyd run game.floyd [arg1 arg2]   | compile and run the floyd program "game.floyd" using native execution. arg1 and arg2 are inputs to your main()
|run      | floyd run -t mygame.floyd          | -t turns on tracing, which shows compilation steps
|compile  | floyd compile mygame.floyd         | compile the floyd program "mygame.floyd" to a native object file, output to stdout
|compile  | floyd compile game.floyd myl.floyd | compile the floyd program "game.floyd" and "myl.floyd" to one native object file, output to stdout
|compile  | floyd compile game.floyd -o test.o | compile the floyd program "game.floyd" to a native object file .o, called "test.o"
|bench    | floyd bench mygame.floyd           | Runs all benchmarks, as defined by benchmark-def statements in Floyd program
|bench    | floyd bench game.floyd rle game_lp | Runs specified benchmarks: "rle" and "game_lp"
|bench    | floyd bench -l mygame.floyd        | Returns list of benchmarks
|hwcaps   | floyd hwcaps                       | Outputs hardware capabilities
|runtests | floyd runtests                     | Runs Floyd built internal unit tests

FLAGS

| -t       | Verbose tracing
| -p       | Output parse tree as a JSON
| -a       | Output Abstract syntax tree (AST) as a JSON
| -i       | Output intermediate representation (IR / ASM) as assembly
| -b       | Use Floyd's bytecode backend instead of default LLVM
| -g       | Compiler with debug info, no optimizations
| -O1      | Enable trivial optimizations
| -O2      | Enable default optimizations
| -O3      | Enable expensive optimizations
| -l       | floyd bench returns a list of all benchmarks
| -vcarray | Force vectors to use carray backend
| -vhamt   | Force vectors to use HAMT backend (this is default)
| -dcppmap | Force dictionaries to use c++ map as backend
| -dhamt   | Force dictionaries to use HAMT backend (this is default)

MORE EXAMPLES

Compile "examples/fibonacci.floyd" to LLVM IR code, disable optimization, write to file "a.ir"
>	floyd compile -i -g examples/fibonacci.floyd -o a.ir

)___";

	return ss.str();
}

#if 0
static void XXXXX_trace_function_link_map(const std::vector<function_link_entry_t>& defs){
	std::vector<line_t> table = {
		line_t( { "LINK-NAME", "MODULE", "LLVM_FUNCTION_TYPE", "LLVM_CODEGEN_F", "FUNCTION TYPE", "ARG NAMES", "NATIVE_F" }, ' ', '|'),
		line_t( { "", "", "", "", "", "", "" }, '-', '|'),
	};

	for(const auto& e: defs){
		const auto f0 = e.function_type_or_undef.is_undefined() ? "" : json_to_compact_string(typeid_to_compact_string(e.function_type_or_undef));

		std::string arg_names;
		for(const auto& m: e.arg_names_or_empty){
			arg_names = m._name + ",";
		}
		arg_names = arg_names.empty() ? "" : arg_names.substr(0, arg_names.size() - 1);


		const auto f1 = f0.substr(0, 100);
		const auto f = f1.size() != f0.size() ? (f1 + "...") : f1;

		const auto l = line_t {
			{
				e.link_name.s,
				e.module,
				print_type(e.llvm_function_type),
				e.llvm_codegen_f != nullptr ? ptr_to_hexstring(e.llvm_codegen_f) : "",
				f,
				arg_names,
				e.native_f != nullptr ? ptr_to_hexstring(e.native_f) : "",
			},
			' ',
			'|'
		};
		table.push_back(l);
	}

	table.push_back(line_t( { "", "", "", "", "", "", "" }, '-', '|'));

	const auto default_column = column_t{ 0, -1, 0 };
	const auto columns0 = std::vector<column_t>(table[0].columns.size(), default_column);
	const auto columns = fit_columns(columns0, table);
	const auto r = generate_table(table, columns);

	std::stringstream ss;
	ss << std::endl;
	for(const auto& e: r){
		ss << e << std::endl;
	}
	QUARK_TRACE(ss.str());
}
#endif




//	Supports args with spaces if you quote them.
std::vector<std::string> string_to_args(const std::string& s){
	const auto start = seq_t(s);

	std::vector<std::string> result;

	auto pos = start;
	std::string acc;
	bool in_quote = false;
	while(pos.empty() == false){
		const auto ch = pos.first1();
		if(ch == "\""){
			if(in_quote){
				if(acc.empty() == false){
					result.push_back(acc);
					acc = "";
				}
				in_quote = false;
			}
			else{
				in_quote = true;
			}
		}
		else if(ch == " "){
			if(in_quote){
				acc = acc + ch;
			}
			else{
				if(acc.empty() == false){
					result.push_back(acc);
					acc = "";
				}
			}
		}
		else{
			acc = acc + ch;
		}
		pos = pos.rest();
	}
	if(acc.empty() == false){
		result.push_back(acc);
		acc = "";
	}
	return result;
}

QUARK_TEST("", "string_to_args()", "", ""){
	const auto r = string_to_args("");
	QUARK_VERIFY(r == std::vector<std::string>{});
}
QUARK_TEST("", "string_to_args()", "", ""){
	const auto r = string_to_args("one");
	QUARK_VERIFY(r == std::vector<std::string>{"one"});
}
QUARK_TEST("", "string_to_args()", "", ""){
	const auto r = string_to_args("one two");
	QUARK_VERIFY(r == (std::vector<std::string>{"one", "two"}));
}

QUARK_TEST("", "string_to_args()", "", ""){
	const auto r = string_to_args(" one two");
	QUARK_VERIFY(r == (std::vector<std::string>{"one", "two"}));
}
QUARK_TEST("", "string_to_args()", "", ""){
	const auto r = string_to_args("one  two");
	QUARK_VERIFY(r == (std::vector<std::string>{"one", "two"}));
}
QUARK_TEST("", "string_to_args()", "", ""){
	const auto r = string_to_args("one two ");
	QUARK_VERIFY(r == (std::vector<std::string>{"one", "two"}));
}

QUARK_TEST("", "string_to_args()", "", ""){
	const auto r = string_to_args("one \"two 2\" three ");
	QUARK_VERIFY(r == (std::vector<std::string>{"one", "two 2", "three"}));
}


eoutput_type get_output_type(const command_line_args_t& args){
	if(args.flags.find("p") != args.flags.end()){
	 	return eoutput_type::parse_tree;
	}
	if(args.flags.find("a") != args.flags.end()){
	 	return eoutput_type::ast;
	}
	if(args.flags.find("i") != args.flags.end()){
	 	return eoutput_type::ir;
	}

	return eoutput_type::object_file;
}


const std::string k_flags = "tlpaiogO:v:d:";


struct compile_more_t {
	std::vector<std::string> source_paths;
	std::string output_path;
	compiler_settings_t compiler_settings;
};

static eoptimization_level get_optimization_level(const std::map<std::string, flag_info_t>& flags){
	if(flags.find("g") != flags.end()){
		return eoptimization_level::g_no_optimizations_enable_debugging;
	}
	else{
		const auto it = flags.find("O");
		if(it != flags.end()){
			if(it->second == flag_info_t { flag_info_t::etype::flag_with_parameter, "1"} ){
				return eoptimization_level::O1_enable_trivial_optimizations;
			}
			else if(it->second == flag_info_t { flag_info_t::etype::flag_with_parameter, "2"} ){
				return eoptimization_level::O2_enable_default_optimizations;
			}
			else if(it->second == flag_info_t { flag_info_t::etype::flag_with_parameter, "3"} ){
				return eoptimization_level::O3_enable_expensive_optimizations;
			}
			else{
				throw std::exception();
			}
		}
		else{
			return eoptimization_level::O2_enable_default_optimizations;
		}
	}
}
static vector_backend get_vector_backend(const std::map<std::string, flag_info_t>& flags){
	const auto it = flags.find("v");
	if(it != flags.end()){
		if(it->second == flag_info_t { flag_info_t::etype::flag_with_parameter, "hamt"} ){
			return vector_backend::hamt;
		}
		else if(it->second == flag_info_t { flag_info_t::etype::flag_with_parameter, "carray"} ){
			return vector_backend::carray;
		}
		else{
			throw std::exception();
		}
	}
	else{
		return vector_backend::hamt;
	}
}
static dict_backend get_dict_backend(const std::map<std::string, flag_info_t>& flags){
	const auto it = flags.find("d");
	if(it != flags.end()){
		if(it->second == flag_info_t { flag_info_t::etype::flag_with_parameter, "hamt"} ){
			return dict_backend::hamt;
		}
		else if(it->second == flag_info_t { flag_info_t::etype::flag_with_parameter, "cppmap"} ){
			return dict_backend::cppmap;
		}
		else{
			throw std::exception();
		}
	}
	else{
		return dict_backend::hamt;
	}
}

static compiler_settings_t get_compiler_settings(const std::map<std::string, flag_info_t>& flags){
	const auto optimization_level = get_optimization_level(flags);
	const auto vector_backend = get_vector_backend(flags);
	const auto dict_backend = get_dict_backend(flags);

	return compiler_settings_t { { vector_backend, dict_backend, false }, optimization_level }; 
}

compile_more_t parse_floyd_compile_command_more(const command_line_args_t& command_line_args){
	if(command_line_args.extra_arguments.size() == 0){
		throw std::runtime_error("Command requires source file name.");
	}

	const auto compiler_settings = get_compiler_settings(command_line_args.flags);
	const auto floyd_args = command_line_args.extra_arguments;

	std::vector<std::string> source_paths;
	int index = 0;
	while(index < floyd_args.size() && floyd_args[index].substr(0, 1) != "-"){
		const auto s = floyd_args[index];
		source_paths.push_back(s);
		index++;
	}

	if(index < floyd_args.size()){
		if(floyd_args[index].substr(0, 2) == "-o"){
		}
		else{
			throw std::runtime_error("Unexpected parameters: \"" + floyd_args[index] + "\".");
		}
		index++;
	}

	std::string output_path;
	if(index < floyd_args.size()){
		const auto s = floyd_args[index];
		if(s.substr(0, 1) == "-"){
			throw std::runtime_error("Unexpected flag \"" + s + "\" after source files.");
		}
		output_path = s;
		index++;
	}
	if(index < floyd_args.size()){
		throw std::runtime_error("Unexpected parameters at end of command line: \"" + floyd_args[index] + "\".");
	}

	if(source_paths.size() == 0){
		throw std::runtime_error("No source file provided.");
	}

	return compile_more_t { source_paths, output_path, compiler_settings };
}



command_t parse_floyd_command_line(const std::vector<std::string>& args){
#if DEBUG && 0
	for(const auto& e: args){
		std::cout << "\"" << e << "\"" << std::endl;
	}
#endif

	const auto command_line_args = parse_command_line_args_subcommand(args, k_flags);
	const auto path_parts = SplitPath(command_line_args.command);

	std::cout << "name " << path_parts.fName << std::endl;

	// WINDOWS TODO fix QUARK_ASSERT(path_parts.fName == "floyd" || path_parts.fName == ".\floyd.exe"	|| path_parts.fName == "floydut");
	const bool trace_on = command_line_args.flags.find("t") != command_line_args.flags.end();
	const bool bytecode_on = command_line_args.flags.find("b") != command_line_args.flags.end();
	const ebackend backend = bytecode_on ? ebackend::bytecode : ebackend::llvm;
	const eoutput_type output_type = get_output_type(command_line_args);

#if DEBUG && 0
	std::cout << command_line_args.subcommand << std::endl;
#endif

	if(command_line_args.subcommand == "help"){
		return command_t { command_t::help_t { } };
	}
	else if(command_line_args.subcommand == "run"){
		if(command_line_args.extra_arguments.size() == 0){
			throw std::runtime_error("Command requires source file name.");
		}

		const auto floyd_args = command_line_args.extra_arguments;

		const auto source_path = floyd_args[0];
		const std::vector<std::string> args2(floyd_args.begin() + 1, floyd_args.end());

		const auto compiler_settings = get_compiler_settings(command_line_args.flags);
		return command_t { command_t::compile_and_run_t { source_path, args2, backend, compiler_settings, trace_on } };
	}
	else if(command_line_args.subcommand == "compile"){
		const auto a = parse_floyd_compile_command_more(command_line_args);
		return command_t { command_t::compile_t { a.source_paths, a.output_path, output_type, backend, a.compiler_settings, trace_on } };
	}
	else if(command_line_args.subcommand == "bench"){
		if(command_line_args.extra_arguments.size() == 0){
			throw std::runtime_error("Command requires source file name.");
		}
		const auto floyd_args = command_line_args.extra_arguments;

		const auto source_path = floyd_args[0];
		const std::vector<std::string> args2(floyd_args.begin() + 1, floyd_args.end());

		const auto compiler_settings = get_compiler_settings(command_line_args.flags);

		const bool list_mode = command_line_args.flags.find("l") != command_line_args.flags.end();
		if(list_mode){
			return command_t { command_t::user_benchmarks_t { command_t::user_benchmarks_t::mode::list, source_path, args2, backend, compiler_settings, trace_on } };
		}
		else{
			if(args2.size() == 0){
				return command_t { command_t::user_benchmarks_t { command_t::user_benchmarks_t::mode::run_all, source_path, {}, backend, compiler_settings, trace_on } };
			}
			else{
				return command_t { command_t::user_benchmarks_t { command_t::user_benchmarks_t::mode::run_specified, source_path, args2, backend, compiler_settings, trace_on } };
			}
		}
	}


	else if(command_line_args.subcommand == "hwcaps"){
		return command_t { command_t::hwcaps_t { trace_on } };
	}
	else if(command_line_args.subcommand == "runtests"){
		return command_t { command_t::runtests_internals_t { trace_on } };
	}
	else{
		return command_t { command_t::help_t { } };
	}
}


//??? test all examples from help

QUARK_TEST("", "parse_floyd_command_line()", "floyd xyz", ""){
	try {
		parse_floyd_command_line(string_to_args("floyd xyz"));
		QUARK_VERIFY(true);
	}
	catch(...){
	}
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd run", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd run mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_and_run_t>(r._contents);
	QUARK_VERIFY(r2.source_path == "mygame.floyd");
	QUARK_VERIFY(r2.floyd_main_args == (std::vector<std::string>{}));
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.trace == false);
}
QUARK_TEST("", "parse_floyd_command_line()", "floyd run", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd run mygame.floyd arg1 arg2"));
	const auto& r2 = std::get<command_t::compile_and_run_t>(r._contents);
	QUARK_VERIFY(r2.source_path == "mygame.floyd");
	QUARK_VERIFY(r2.floyd_main_args == (std::vector<std::string>{ "arg1", "arg2" }));
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.trace == false);
}
QUARK_TEST("", "parse_floyd_command_line()", "floyd run", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd run -b mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_and_run_t>(r._contents);
	QUARK_VERIFY(r2.source_path == "mygame.floyd");
	QUARK_VERIFY(r2.floyd_main_args == (std::vector<std::string>{}));
	QUARK_VERIFY(r2.backend == ebackend::bytecode);
	QUARK_VERIFY(r2.trace == false);
}



//??? compile several source files. Linking?

// Marcuss-iMac-Retina:test_clang marcus$ clang a.cpp -o aaa.out

/*
| floyd compile mygame.floyd				| compile the floyd program "mygame.floyd" to a native object file, output to stdout
| floyd compile mygame.floyd mylib.floyd	| compile the floyd program "mygame.floyd" and "mylib.floyd" to one native object file, output to stdout
| floyd compile mygame.floyd -o test			| compile the floyd program "mygame.floyd" to a native object file .o, called "test.o"
*/

/*
Try it out (assuming you add llvm/build/bin to your path):
clang --help
clang file.c -fsyntax-only (check for correctness)
clang file.c -S -emit-llvm -o - (print out unoptimized llvm code)
clang file.c -S -emit-llvm -o - -O3
clang file.c -S -O3 -o - (output native machine code)
*/

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd" } );
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::object_file);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}
QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -t mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd" });
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::object_file);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == true);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -p mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd" });
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::parse_tree);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -a mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd" });
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::ast);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}
QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -i mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd" });
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::ir);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}
QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -pait mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd"});
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::parse_tree);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == true);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile mygame.floyd mylib.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == (std::vector<std::string>{ "mygame.floyd", "mylib.floyd" } ));
	QUARK_VERIFY(r2.dest_path == "")
	QUARK_VERIFY(r2.output_type == eoutput_type::object_file);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile mygame.floyd -o outdir/outfile"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd" });
	QUARK_VERIFY(r2.dest_path == "outdir/outfile")
	QUARK_VERIFY(r2.output_type == eoutput_type::object_file);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -ig mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd"});
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::ir);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::g_no_optimizations_enable_debugging }));
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -i -O1 mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd"});
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::ir);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O1_enable_trivial_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -i -O2 mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd"});
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::ir);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -i -O3 mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd"});
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::ir);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O3_enable_expensive_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -i -O3 a.txt b.txt -o c.txt"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == (std::vector<std::string>{ "a.txt", "b.txt" }) );
	QUARK_VERIFY(r2.dest_path == "c.txt");
	QUARK_VERIFY(r2.output_type == eoutput_type::ir);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::hamt, dict_backend::hamt, false }, eoptimization_level::O3_enable_expensive_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}



QUARK_TEST("", "parse_floyd_command_line()", "floyd compile", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd compile -vcarray -dcppmap mygame.floyd"));
	const auto& r2 = std::get<command_t::compile_t>(r._contents);
	QUARK_VERIFY(r2.source_paths == std::vector<std::string>{ "mygame.floyd" });
	QUARK_VERIFY(r2.dest_path == "");
	QUARK_VERIFY(r2.output_type == eoutput_type::object_file);
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.compiler_settings == (compiler_settings_t { config_t{ vector_backend::carray, dict_backend::cppmap, false }, eoptimization_level::O2_enable_default_optimizations }));
	QUARK_VERIFY(r2.trace == false);
}





QUARK_TEST("", "parse_floyd_command_line()", "floyd help", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd help"));
	std::get<command_t::help_t>(r._contents);
}





QUARK_TEST("", "parse_floyd_command_line()", "floyd runtests", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd runtests"));
	std::get<command_t::runtests_internals_t>(r._contents);
}




QUARK_TEST("", "parse_floyd_command_line()", "floyd bench mygame.floyd", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd bench mygame.floyd"));
	const auto& r2 = std::get<command_t::user_benchmarks_t>(r._contents);
	QUARK_VERIFY(r2.mode == command_t::user_benchmarks_t::mode::run_all);
	QUARK_VERIFY(r2.source_path == "mygame.floyd");
	QUARK_VERIFY(r2.optional_benchmark_keys == (std::vector<std::string>{}));
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.trace == false);
}

QUARK_TEST("", "parse_floyd_command_line()", "floyd bench -t mygame.floyd quicksort1 merge-sort", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd bench -t mygame.floyd quicksort1 \"merge sort\""));
	const auto& r2 = std::get<command_t::user_benchmarks_t>(r._contents);
	QUARK_VERIFY(r2.mode == command_t::user_benchmarks_t::mode::run_specified);
	QUARK_VERIFY(r2.source_path == "mygame.floyd");
	QUARK_VERIFY(r2.optional_benchmark_keys == (std::vector<std::string>{ "quicksort1", "merge sort" }));
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.trace == true);
}


QUARK_TEST("", "parse_floyd_command_line()", "floyd bench -tl mygame.floyd", ""){
	const auto r = parse_floyd_command_line(string_to_args("floyd bench -tl mygame.floyd"));
	const auto& r2 = std::get<command_t::user_benchmarks_t>(r._contents);
	QUARK_VERIFY(r2.mode == command_t::user_benchmarks_t::mode::list);
	QUARK_VERIFY(r2.source_path == "mygame.floyd");
	QUARK_VERIFY(r2.optional_benchmark_keys == (std::vector<std::string>{}));
	QUARK_VERIFY(r2.backend == ebackend::llvm);
	QUARK_VERIFY(r2.trace == true);
}




//	Test real-world complicated command line

QUARK_TEST("", "parse_floyd_command_line()", "ld -o output /lib/crt0.o hello.o -lc", ""){
	const auto r = parse_command_line_args(string_to_args("ld -o output /lib/crt0.o hello.o -lc"), "olc");
	QUARK_VERIFY(r.command == "ld");
	QUARK_VERIFY(r.subcommand == "");
	QUARK_VERIFY(r.flags.find("o")->second == (flag_info_t { flag_info_t::etype::simple_flag, ""}) );
	QUARK_VERIFY(r.extra_arguments == (std::vector<std::string> { "output", "/lib/crt0.o", "hello.o", "-lc" }) );
}

QUARK_TEST("", "parse_floyd_command_line()", "ld -o3 output /lib/crt0.o hello.o -lc", ""){
	const auto r = parse_command_line_args(string_to_args("ld -o3 output /lib/crt0.o hello.o -lc"), "o:lc");
	QUARK_VERIFY(r.command == "ld");
	QUARK_VERIFY(r.subcommand == "");
	QUARK_VERIFY(r.flags.find("o")->second == (flag_info_t { flag_info_t::etype::flag_with_parameter, "3"}) );
	QUARK_VERIFY(r.extra_arguments == (std::vector<std::string> { "output", "/lib/crt0.o", "hello.o", "-lc" }) );
}


}	// floyd
