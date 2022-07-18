#ifndef LLVM_MY_FUNCTIONLOG_H
#define LLVM_MY_FUNCTIONLOG_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/InstIterator.h"
#include <llvm/IR/LLVMContext.h>

using namespace llvm;

// inherits PassInfoMixin: transformation pass
class FunctionLog : public PassInfoMixin<FunctionLog> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &);

private:
    bool runOnModule(Module& M);
    void initializeFnlogDepth(LLVMContext& CTX, Module& M);
    void initializeFnlogWrapper(LLVMContext& CTX, Module& M);
    void initializeDepthPrinter(LLVMContext& CTX, Module& M);
    void replaceFuncWithWrapper(LLVMContext& CTX, Module& M);

    Constant* createGlobalString(LLVMContext& CTX, Module& M, std::string s, std::string discriminator)
    {
        Constant* Str = ConstantDataArray::getString(CTX, s);
        Constant* StrVar = M.getOrInsertGlobal("Str_" + discriminator, Str->getType());
        dyn_cast<GlobalVariable>(StrVar)->setInitializer(Str);

        return StrVar;
    }

    PointerType* i8ptrTy;
    FunctionCallee Printf;
    void initializePrintf(Module& M)
    {
        LLVMContext& CTX = M.getContext();
        this->i8ptrTy = PointerType::getUnqual(Type::getInt8Ty(CTX));
        FunctionType* PrintfTy = FunctionType::get(IntegerType::getInt32Ty(CTX), this->i8ptrTy, /*isVarArgs*/true);
                                                        // return type              arg type    variable arg
                                                        // declare i32 @printf(i8*, ...)
        this->Printf = M.getOrInsertFunction("printf", PrintfTy);
    }

    // static bool isRequired() { return true; }
};

#endif
