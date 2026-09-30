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

#endif
