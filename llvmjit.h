#include <llvm-c/Core.h>
#include <llvm-c/Error.h>

#ifdef __cplusplus
extern "C" {
#endif

LLVMErrorRef InitLLJIT(void (*pleaseRegisterCallback)(const char *));
LLVMErrorRef AddLLJITModule(LLVMModuleRef M);
LLVMErrorRef GetAddressFor(const char *s, uint64_t *addr);
LLVMErrorRef RegisterAddress(const char *s, uint64_t addr);

#ifdef __cplusplus
}
#endif
