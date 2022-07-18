#include "FunctionLog.h"
#include <llvm/CodeGen/SelectionDAGAddressAnalysis.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>

using namespace llvm;

void FunctionLog::initializeFnlogDepth(LLVMContext& CTX, Module& M)
{
    GlobalVariable* fnlogdepth = new GlobalVariable(M, Type::getInt32Ty(CTX), false, GlobalValue::LinkageTypes::ExternalLinkage, ConstantInt::get(Type::getInt32Ty(CTX), 0), "fnlogdepth");
    fnlogdepth->setAlignment(MaybeAlign(4));
}

void FunctionLog::initializeDepthPrinter(LLVMContext& CTX, Module& M)
{
    GlobalVariable* fnlogdepthPtr = M.getGlobalVariable("fnlogdepth");

//    FunctionType* DepthPrinterFuncTy = FunctionType::get(Type::getVoidTy(CTX), {Type::getInt32Ty(CTX)}, false);
    FunctionType* DepthPrinterFuncTy = FunctionType::get(Type::getVoidTy(CTX), {}, false);
    Function* DepthPrinterFunc = Function::Create(DepthPrinterFuncTy, GlobalValue::LinkageTypes::ExternalLinkage, "FnlogDepthPrinter", M);

    BasicBlock* BlockInit = BasicBlock::Create(CTX, ".loop.init", DepthPrinterFunc);
    BasicBlock* BlockBody = BasicBlock::Create(CTX, ".loop.body", DepthPrinterFunc);
    BasicBlock* BlockIter = BasicBlock::Create(CTX, ".loop.iter", DepthPrinterFunc);
    BasicBlock* BlockEnd = BasicBlock::Create(CTX, ".loop.end", DepthPrinterFunc);

    IRBuilder<> BuilderInit(BlockInit);
    
    // %accLocal = alloca i32, align 4
    AllocaInst* accLocal = BuilderInit.CreateAlloca(Type::getInt32Ty(CTX), NULL, "accLocal");

    // %0 = load i32, ptr %fnlogdepth, align 4
    LoadInst* depthLoad = BuilderInit.CreateLoad(Type::getInt32Ty(CTX), fnlogdepthPtr);

    // store i32 %0, ptr %accLocal, align 4
    BuilderInit.CreateStore(depthLoad, accLocal);

    // br label %.loop.iter
    BuilderInit.CreateBr(BlockIter);

    IRBuilder<> BuilderBody(BlockBody);

    // %_ = call i32 @printf(ptr @Str_blank);
    Constant* BlankStrVar = FunctionLog::createGlobalString(CTX, M, "  ", "blank");
    Value* BlankStrPtr = BuilderBody.CreatePointerCast(BlankStrVar, this->i8ptrTy, "blankStr");
    BuilderBody.CreateCall(Printf, BlankStrPtr);

    BuilderBody.CreateBr(BlockIter);

    IRBuilder<> BuilderIter(BlockIter);

    // %1 = load i32, ptr %fnlogdepth, align 4
    LoadInst* accLocalLoad = BuilderIter.CreateLoad(Type::getInt32Ty(CTX), accLocal);

    // %2 = sub i32 %1, 1
    Value* accDec = BuilderIter.CreateSub(accLocalLoad, ConstantInt::get(Type::getInt32Ty(CTX), 1));

    // store i32 %2, ptr %accLocal, align 4
    BuilderIter.CreateStore(accDec, accLocal);

    // %cond = icmp sge %2, 0
    Value* cond = BuilderIter.CreateICmpSGE(accDec, ConstantInt::get(Type::getInt32Ty(CTX), 0), "cond");

    // br i1 %cond, label %loop.body, label %loop.end
    BuilderIter.CreateCondBr(cond, BlockBody, BlockEnd);

    IRBuilder<> BuilderEnd(BlockEnd);

    BuilderEnd.CreateRetVoid();

    // for loop
    // @Str_blank = global [1 x i8] c" "
    //
    //
    // .loop.init:
    //   %accLocal = alloca i32, align 4
    //   %acc = load i32, ptr %fnlogdepth, align 4
    //
    // .loop.iter:
    //   %cond = icmp sge %acc, 0 // if %acc >= 0
    //   br i1 %cond, label %loop.body, label %loop.end
    //
    // .loop.body:
    //   %_ = call i32 @printf(ptr @Str_blank)
    //   %accDec = sub i32 %acc, 1
    //   store i32 %accDec, ptr %accLocal, align 4
    //   %acc = load 
    //
    //
    //   br .loop.iter
    //
    // .loop.end:
}

void FunctionLog::initializeFnlogWrapper(LLVMContext& CTX, Module& M)
{
    std::vector<std::pair<FunctionType*, std::string>> WrapperFuncVec = {};
    for (auto &F : M) {
        if (F.isDeclaration()) {
            continue;
        }
        
        if (F.getName() == "main") {
            continue;
        }

        if (F.getName() == "FnlogDepthPrinter") {
            continue;
        }

        WrapperFuncVec.push_back(make_pair(F.getFunctionType(), F.getName().str()));
    }

    GlobalVariable* fnlogdepthPtr = M.getGlobalVariable("fnlogdepth");

    for (auto &FPair : WrapperFuncVec) {
        Function* WrapperFunc = Function::Create(FPair.first, GlobalValue::LinkageTypes::ExternalLinkage, "FnlogWrapper_" + FPair.second, M);

        BasicBlock* BB = BasicBlock::Create(CTX, "entry", WrapperFunc);
        IRBuilder<> Builder(BB);

        // increase fnlogdepth by 1
        // fnlogdepth++;

        // %depth = load i32, ptr %fnlogdepth, align 4
        LoadInst* depth = Builder.CreateLoad(Type::getInt32Ty(CTX), fnlogdepthPtr, "depth");

        // %newdepth = add i32 %depth, 1
        Value* newdepth = Builder.CreateAdd(depth, ConstantInt::get(Type::getInt32Ty(CTX), 1), "newdepth");

        // store i32 %newdepth, ptr %fnlogdepth, align 4
        Builder.CreateStore(newdepth, fnlogdepthPtr);

        Builder.CreateCall(M.getOrInsertFunction("FnlogDepthPrinter", M.getFunction("FnlogDepthPrinter")->getFunctionType()), {});

        // create printf call about func name and args
        // printf("%s(%d, %d)\n", funcname, arg0, arg1);
        std::string s = "%s(";

        std::string discriminator = "";

        std::vector<Value*> RealArgs = {};
        for (auto Argument = WrapperFunc->arg_begin(); Argument != WrapperFunc->arg_end(); Argument++) {
            Type* ArgType = Argument->getType();
            if (ArgType->isIntegerTy()) {
                s += "%lld";
                discriminator += "d";
            } else if (ArgType->isFloatingPointTy()) {
                s += "%llf";
                discriminator += "f";
            } else if (ArgType->isPointerTy()) {
                s += "%p";
                discriminator += "p";
            } else {
                s += "%x";
                discriminator += "x";
            }

            RealArgs.push_back(Argument);

            if (Argument + 1 != WrapperFunc->arg_end()) {
                s += ", ";
            }
        }
        s += ")\n";
        if (discriminator == "") {
            discriminator = "v";
        }

        Constant* PrintfFormatStrVar = FunctionLog::createGlobalString(CTX, M, s, "PrintfFormatStr_" + discriminator);
        Value* PrintfFormatStrPtr = Builder.CreatePointerCast(PrintfFormatStrVar, this->i8ptrTy, "formatStr");
        Constant* FuncNameVar = FunctionLog::createGlobalString(CTX, M, FPair.second, FPair.second);
        
        std::vector<Value*> PrintfArgs = {PrintfFormatStrPtr, FuncNameVar};
        PrintfArgs.insert(PrintfArgs.end(), RealArgs.begin(), RealArgs.end());

        // %_ = call i32 @printf(PrintfFormatStrPtr, FuncNameVar, ...)
        Builder.CreateCall(this->Printf, PrintfArgs);

        // create real function call
        // retval = func(arg0, arg1);
        CallInst* RetValue = Builder.CreateCall(M.getOrInsertFunction(FPair.second, FPair.first), RealArgs);

        Builder.CreateCall(M.getOrInsertFunction("FnlogDepthPrinter", M.getFunction("FnlogDepthPrinter")->getFunctionType()), {});

        // create printf call about return Value
        // printf("%s: return %d\n", funcname, retval);
        s = "%s: return";
        discriminator = "";

        if (!RetValue->getType()->isVoidTy()) {
            Type* RetType = RetValue->getType();

            s += " ";
            if (RetType->isIntegerTy()) {
                discriminator += "d";
                s += "%lld";
            } else if (RetType->isFloatingPointTy()) {
                discriminator += "f";
                s += "%llf";
            } else if (RetType->isPointerTy()) {
                discriminator += "p";
                s += "%p";
            } else {
                discriminator += "x";
                s += "%x";
            }
        }
        s += "\n";
        if (discriminator == "") {
            discriminator = "v";
        }

        PrintfFormatStrVar = FunctionLog::createGlobalString(CTX, M, s, "PrintfFormatStr_ret_" + discriminator);
        PrintfFormatStrPtr = Builder.CreatePointerCast(PrintfFormatStrVar, this->i8ptrTy, "formatStr");

        PrintfArgs = {PrintfFormatStrPtr, FuncNameVar};
        if (!RetValue->getType()->isVoidTy()) {
            PrintfArgs.push_back(RetValue);
        }

        Builder.CreateCall(this->Printf, PrintfArgs);

        // decrease fnlogdepth by 1
        // fnlogdepth--;

        // %depth = load i32, ptr %fnlogdepth, align 4
        depth = Builder.CreateLoad(Type::getInt32Ty(CTX), fnlogdepthPtr, "depth");

        // %newdepth = sub i32 %depth, 1
        newdepth = Builder.CreateSub(depth, ConstantInt::get(Type::getInt32Ty(CTX), 1), "newdepth");

        // store i32 %newdepth, ptr %fnlogdepth, ailgn 4
        Builder.CreateStore(newdepth, fnlogdepthPtr);

        // create return instruction
        // return retval;
        if (!RetValue->getType()->isVoidTy()) {
            Builder.CreateRet(RetValue);
        } else {
            Builder.CreateRetVoid();
        }
    }
}

void FunctionLog::replaceFuncWithWrapper(LLVMContext& CTX, Module& M)
{
    for (auto &F : M) {
        if (F.getName().str().find("FnlogWrapper_") == 0) {
            // F is wrapper function
            continue;
        }

        std::vector<Instruction*> worklist;
        for (auto I = inst_begin(F); I != inst_end(F); I++) {
            worklist.push_back(&*I);
        }

        for (auto &I : worklist) {
            if (auto CallI = dyn_cast<CallInst>(&*I)) {
                Function* Func = CallI->getCalledFunction();
                StringRef FuncName = Func->getName();
                FunctionType* FuncTy = Func->getFunctionType();

                if (M.getFunction("FnlogWrapper_" + FuncName.str()) == NULL) {
                    // wrapper function not exists
                    // e.g. library functions, like printf
                    continue;
                }

                IRBuilder<> Builder(CallI);

                std::vector<Value*> CallIOperands = {};
                for (auto Operand = CallI->op_begin(); std::next(Operand) != CallI->op_end(); Operand = std::next(Operand)) {
                    CallIOperands.push_back(*Operand);
                }

                CallInst* WrapperCallI = Builder.CreateCall(M.getOrInsertFunction("FnlogWrapper_" + FuncName.str(), FuncTy), CallIOperands);

                CallI->replaceAllUsesWith(WrapperCallI);
                CallI->eraseFromParent();
            }
        }
    }
}

bool FunctionLog::runOnModule(Module& M)
{
    bool isChanged = false;

    auto &CTX = M.getContext();

    FunctionLog::initializeFnlogDepth(CTX, M);
    FunctionLog::initializeDepthPrinter(CTX, M);
    FunctionLog::initializeFnlogWrapper(CTX, M);
    FunctionLog::replaceFuncWithWrapper(CTX, M);

    return isChanged = true;
}

PreservedAnalyses FunctionLog::run(Module& M, ModuleAnalysisManager &)
{
    FunctionLog::initializePrintf(M);

    bool isChanged = runOnModule(M);

    return (isChanged ? PreservedAnalyses::none() : PreservedAnalyses::all());
}

// PM registration
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

PassPluginLibraryInfo getFunctionLogPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "FunctionLog", LLVM_VERSION_STRING,
            [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                    [&](StringRef Name, ModulePassManager& MPM, ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "fnlog") {
                            MPM.addPass(FunctionLog());
                            return true;
                        }
                        return false;
                    }
                );
            }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getFunctionLogPluginInfo();
}
