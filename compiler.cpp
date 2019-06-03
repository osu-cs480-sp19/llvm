#include <iostream>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

// using namespace llvm;

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> TheBuilder(TheContext);
static llvm::Module* TheModule;

static std::map<std::string, llvm::Value*> TheSymbolTable;

llvm::Value* numericConstant(float val) {
  return llvm::ConstantFP::get(TheContext, llvm::APFloat(val));
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


int main(int argc, char const *argv[]) {
  TheModule = new llvm::Module("LLVM_Demo", TheContext);

  llvm::FunctionType* fooFnType = llvm::FunctionType::get(
    llvm::Type::getVoidTy(TheContext), false
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

  TheBuilder.CreateRetVoid();

  llvm::verifyFunction(*fooFn);
  TheModule->print(llvm::outs(), NULL);

  delete TheModule;
  return 0;
}
