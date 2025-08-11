#pragma once

#ifndef OBFUSC_VERSION_MAJOR
    #define OBFUSC_VERSION_MAJOR 0
#endif

#ifndef OBFUSC_VERSION_MINOR
    #define OBFUSC_VERSION_MINOR 0
#endif

#ifndef OBFUSC_VERSION_MICRO
    #define OBFUSC_VERSION_MICRO 0
#endif

#ifndef OBFUSC_GIT_REV
    #define OBFUSC_GIT_REV 0
#endif

#define STRINGIFY(x)        #x
#define TOSTRING(x)         STRINGIFY(x)
#define OBFUSC_VERSION_STR  "v" TOSTRING(OBFUSC_VERSION_MAJOR) "." TOSTRING(OBFUSC_VERSION_MINOR) "." TOSTRING(OBFUSC_VERSION_MICRO) "-" TOSTRING(OBFUSC_GIT_REV)

// namespace obfusc {
llvm::Value* MakeN(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, int32_t N);
llvm::Value* MakeN64(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, int64_t N);
// }