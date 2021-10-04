#pragma once

#include <memory>
#include <string>
#include "llvm/IR/Module.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/TargetSelect.h"


class MyJit {
public:
	// call this once in main() before threading
	static void init();

	MyJit(const std::string moduleName);
	llvm::LLVMContext* context() { return context_.get(); }
	llvm::Module* module() { return module_.get(); }
	llvm::DataLayout* dataLayout();

	bool verify(llvm::Function* fn);
	bool compile();
	intptr_t lookup(llvm::Function* fn);
	intptr_t lookup(const std::string fnname);

	const char* errmsg() const { return errmsg_.c_str(); }

private:
	std::unique_ptr<llvm::LLVMContext> context_;
	std::unique_ptr<llvm::Module> module_;
	std::unique_ptr<llvm::orc::LLJIT> lljit_;
	std::string errmsg_;
};
