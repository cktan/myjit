#pragma once

#include <memory>
#include <string>
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/TargetSelect.h"


class MyJit {
public:
	// call this once in main() before threading
	static void init();

	MyJit(const std::string moduleName);
	llvm::LLVMContext* context() { return m_context.get(); }
	llvm::Module* module() { return m_module.get(); }
	llvm::DataLayout* dataLayout();

	llvm::Function* createFunction(const std::string name, llvm::FunctionType* type);
	llvm::BasicBlock* createBlock(const std::string name, llvm::Function* fn);

	llvm::IRBuilder<>* enter(llvm::BasicBlock*);
	llvm::IRBuilder<>* builder() { return m_builder.get(); }


	bool verify(llvm::Function* fn);
	bool compile();
	intptr_t lookup(llvm::Function* fn);
	intptr_t lookup(const std::string fnname);

	const std::string& errmsg() const { return m_errmsg; }

private:
	std::unique_ptr<llvm::LLVMContext> m_context;
	std::unique_ptr<llvm::Module> m_module;
	std::unique_ptr<llvm::orc::LLJIT> m_lljit;

	std::unique_ptr<llvm::IRBuilder<> > m_builder;
	std::string m_errmsg;

	MyJit(MyJit&) = delete;
	MyJit& operator=(MyJit&) = delete;
};
