//  floyd_llvm_compat.h
//  Floyd
//
//  Header shims for LLVM headers that moved into the "TargetParser" library
//  in newer LLVM releases (LLVM 15 still has them under Support/ADT/MC).

#ifndef floyd_llvm_compat_h
#define floyd_llvm_compat_h

#if __has_include(<llvm/TargetParser/Host.h>)
	#include <llvm/TargetParser/Host.h>
#else
	#include <llvm/Support/Host.h>
#endif

#if __has_include(<llvm/TargetParser/Triple.h>)
	#include <llvm/TargetParser/Triple.h>
#else
	#include <llvm/ADT/Triple.h>
#endif

#if __has_include(<llvm/TargetParser/SubtargetFeature.h>)
	#include <llvm/TargetParser/SubtargetFeature.h>
#else
	#include <llvm/MC/SubtargetFeature.h>
#endif

#include <llvm/Config/llvm-config.h>
#include <llvm/Support/CodeGen.h>
#include <optional>

// LLVM <= 17 uses the CGFT_-prefixed enumerators, LLVM >= 18 dropped the prefix.
#if LLVM_VERSION_MAJOR >= 18
	inline constexpr auto floyd_CGFT_ObjectFile = llvm::CodeGenFileType::ObjectFile;
	inline constexpr auto floyd_CGFT_AssemblyFile = llvm::CodeGenFileType::AssemblyFile;
#else
	inline constexpr auto floyd_CGFT_ObjectFile = llvm::CodeGenFileType::CGFT_ObjectFile;
	inline constexpr auto floyd_CGFT_AssemblyFile = llvm::CodeGenFileType::CGFT_AssemblyFile;
#endif

// LLVM <= 15 takes llvm::Optional<T> in its APIs (e.g. Target::createTargetMachine()),
// LLVM >= 16 switched those APIs over to std::optional<T>.
#if LLVM_VERSION_MAJOR >= 16
	template<typename T> using floyd_optional = std::optional<T>;
#else
	template<typename T> using floyd_optional = llvm::Optional<T>;
#endif

#endif

#endif
