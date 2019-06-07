#include <iostream>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetMachine.h"

#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"

// using namespace llvm;

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> TheBuilder(TheContext);
static llvm::Module* TheModule;

static std::map<std::string, llvm::Value*> TheSymbolTable;

llvm::Value* numericConstant(float val) {
  return llvm::ConstantFP::get(TheContext, llvm::APFloat(val));
}


llvm::Value* variableValue(const std::string& name) {
  llvm::Value* val = TheSymbolTable[name];
  if (!val) {
    std::cerr << "Invalid variable name: " << name << std::endl;
    return NULL;
  }
  return TheBuilder.CreateLoad(val, name.c_str());
}


llvm::Value* binaryOperation(llvm::Value* lhs, llvm::Value* rhs, char op) {
  if (!lhs || !rhs) {
    return NULL;
  }

  switch (op) {
  case '+':
    return TheBuilder.CreateFAdd(lhs, rhs, "addtmp");
  case '-':
    return TheBuilder.CreateFSub(lhs, rhs, "subtmp");
  case '*':
    return TheBuilder.CreateFMul(lhs, rhs, "multmp");
  case '/':
    return TheBuilder.CreateFDiv(lhs, rhs, "divtmp");
  case '<':
    lhs = TheBuilder.CreateFCmpULT(lhs, rhs, "lttmp");
    return TheBuilder.CreateUIToFP(lhs, llvm::Type::getFloatTy(TheContext), "booltmp");
  default:
    std::cerr << "Invalid operator:" << op << std::endl;
    return NULL;
  }
}


llvm::AllocaInst* generateEntryBlockAlloca(std::string& name) {
  llvm::Function* currFn =
    TheBuilder.GetInsertBlock()->getParent();
  llvm::IRBuilder<> tmpBuilder(&currFn->getEntryBlock(),
    currFn->getEntryBlock().begin());

  return tmpBuilder.CreateAlloca(llvm::Type::getFloatTy(TheContext), 0, name.c_str());
}


llvm::Value* assignmentStatement(std::string& lhs, llvm::Value* rhs) {
  if (!rhs) {
    return NULL;
  }
  if (!TheSymbolTable.count(lhs)) {
    // Allocate lhs
    TheSymbolTable[lhs] = generateEntryBlockAlloca(lhs);
  }
  return TheBuilder.CreateStore(rhs, TheSymbolTable[lhs]);
}


llvm::Value* ifElseStatement() {
  llvm::Value* cond = binaryOperation(variableValue("b"), numericConstant(8), '<');
  if (!cond) {
    return NULL;
  }
  cond = TheBuilder.CreateFCmpONE(cond, numericConstant(0), "ifcond");

  llvm::Function* currFn =
    TheBuilder.GetInsertBlock()->getParent();
  llvm::BasicBlock* ifBlock =
    llvm::BasicBlock::Create(TheContext, "ifBlock", currFn);
  llvm::BasicBlock* elseBlock =
    llvm::BasicBlock::Create(TheContext, "elseBlock");
  llvm::BasicBlock* contBlock =
    llvm::BasicBlock::Create(TheContext, "contBlock");

  TheBuilder.CreateCondBr(cond, ifBlock, elseBlock);
  TheBuilder.SetInsertPoint(ifBlock);

  /* If block */
  llvm::Value* aTimesB = binaryOperation(
    variableValue("a"),
    variableValue("b"),
    '*'
  );
  std::string var("c");
  llvm::Value* ifBlockStatement = assignmentStatement(var, aTimesB);
  TheBuilder.CreateBr(contBlock);

  /* Else block */
  currFn->getBasicBlockList().push_back(elseBlock);
  TheBuilder.SetInsertPoint(elseBlock);
  llvm::Value* aPlusB = binaryOperation(
    variableValue("a"),
    variableValue("b"),
    '+'
  );
  llvm::Value* elseBlockStatement = assignmentStatement(var, aPlusB);
  TheBuilder.CreateBr(contBlock);

  currFn->getBasicBlockList().push_back(contBlock);
  TheBuilder.SetInsertPoint(contBlock);

  return contBlock;
}


void generateObjCode(const std::string& filename) {
  std::string targetTriple = llvm::sys::getDefaultTargetTriple();

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  std::string error;
  const llvm::Target* target =
    llvm::TargetRegistry::lookupTarget(targetTriple, error);
  if (!target) {
    std::cerr << error << std::endl;
  } else {
    std::string cpu = "generic";
    std::string features = "";
    llvm::TargetOptions options;
    llvm::TargetMachine* targetMachine =
      target->createTargetMachine(targetTriple, cpu, features,
        options, llvm::Optional<llvm::Reloc::Model>());

    TheModule->setDataLayout(targetMachine->createDataLayout());
    TheModule->setTargetTriple(targetTriple);

    std::error_code ec;
    llvm::raw_fd_ostream file(filename, ec, llvm::sys::fs::F_None);
    if (ec) {
      std::cerr << "Could not open output file: " << ec.message() << std::endl;
    } else {
      llvm::TargetMachine::CodeGenFileType type = llvm::TargetMachine::CGFT_ObjectFile;
      llvm::legacy::PassManager pm;
      if (targetMachine->addPassesToEmitFile(pm, file, NULL, type)) {
        std::cerr << "Unable to emit target code" << std::endl;
      } else {
        pm.run(*TheModule);
        file.close();
      }
    }
  }
}


void doOptimizations(llvm::Function* fn) {
  llvm::legacy::FunctionPassManager* fpm =
    new llvm::legacy::FunctionPassManager(TheModule);

  fpm->add(llvm::createPromoteMemoryToRegisterPass());
  // fpm->add(llvm::createInstructionCombiningPass());
  fpm->add(llvm::createReassociatePass());
  fpm->add(llvm::createGVNPass());
  fpm->add(llvm::createCFGSimplificationPass());

  fpm->run(*fn);
}


int main(int argc, char const *argv[]) {
  TheModule = new llvm::Module("LLVM_Demo", TheContext);

  llvm::FunctionType* fooFnType = llvm::FunctionType::get(
    llvm::Type::getFloatTy(TheContext), false
  );

  llvm::Function* fooFn = llvm::Function::Create(
    fooFnType,
    llvm::GlobalValue::ExternalLinkage,
    "foo",
    TheModule
  );

  llvm::BasicBlock* entryBlock =
    llvm::BasicBlock::Create(TheContext, "entry", fooFn);
  TheBuilder.SetInsertPoint(entryBlock);

  llvm::Value* expr1 = binaryOperation(
    numericConstant(4),
    numericConstant(2),
    '*'
  );
  llvm::Value* expr2 = binaryOperation(
    expr1,
    numericConstant(8),
    '+'
  );

  std::string var1("a");
  llvm::Value* assn = assignmentStatement(var1, expr2);

  llvm::Value* expr3 = binaryOperation(
    variableValue(var1),
    numericConstant(4),
    '/'
  );
  std::string var2("b");
  llvm::Value* assn2 = assignmentStatement(var2, expr3);

  llvm::Value* ifElse = ifElseStatement();

  // TheBuilder.CreateRetVoid();
  TheBuilder.CreateRet(variableValue("c"));

  doOptimizations(fooFn);

  llvm::verifyFunction(*fooFn);
  TheModule->print(llvm::outs(), NULL);

  std::string filename = "foo.o";
  generateObjCode(filename);

  delete TheModule;
  return 0;
}
