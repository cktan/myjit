#include "myjit.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"

#include "llvm/IR/IRBuilder.h"
#include <iostream>


using std::string;

void MyJit::init()
{
	static bool initialized = false;
	if (!initialized) {
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		initialized = true;
	}
}



MyJit::MyJit(const string moduleName)
	: context_(std::make_unique<llvm::LLVMContext>()),
	  module_(std::make_unique<llvm::Module>(moduleName, *context_))
{
}


llvm::DataLayout* MyJit::dataLayout()
{
	return const_cast<llvm::DataLayout*>(&module_->getDataLayout());
}


bool MyJit::verify(llvm::Function* fn)
{
	string errstr;
	llvm::raw_string_ostream errstream(errstr);
	if (llvm::verifyFunction(*fn, &errstream)) {
		errmsg_ = "while verifying " + fn->getName().str() + " - " + errstream.str();
		return false;
	}
	return true;
}

static void optimize(llvm::Module& M)
{
	// Create a function pass manager.
	llvm::legacy::FunctionPassManager FPM(&M);

	// Add some optimizations.
	FPM.add(llvm::createInstructionCombiningPass());
	FPM.add(llvm::createReassociatePass());
	FPM.add(llvm::createGVNPass());
	FPM.add(llvm::createCFGSimplificationPass());

	// CK: add these
	FPM.add(llvm::createPromoteMemoryToRegisterPass()); // this helps? need to verify.
	FPM.add(llvm::createAggressiveDCEPass());
	FPM.add(llvm::createCFGSimplificationPass());
	FPM.add(llvm::createInstructionCombiningPass());

	// do it
	FPM.doInitialization();

	// Run the optimizations over all functions in the module being added to
	// the JIT.
	for (auto &F : M)
        FPM.run(F);

	// Todo: add module level optimization?
}


bool MyJit::compile()
{
	auto& DL = module_->getDataLayout();
	optimize(*module_);

	auto M = llvm::orc::ThreadSafeModule(std::move(module_), std::move(context_));

	// this is another way to run optimize on module
	// M.withModuleDo(optimize);

	// Create LLJIT
	{
		auto t = llvm::orc::LLJITBuilder().create();
		if (!t) {
			errmsg_ = toString(t.takeError());
			return false;
		}
		lljit_ = std::move(*t);
	}

	// Give module to LLJIT ... this will generate code
	llvm::Error err = lljit_->addIRModule(std::move(M));
	if (err) {
		errmsg_ = toString(std::move(err));
		return false;
	}

	// Let LLJIT lookup symbols dynamically
    lljit_->getMainJITDylib()
		.addGenerator(cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));


	return true;
}


intptr_t MyJit::lookup(llvm::Function* fn)
{
	return lookup(fn->getName().str());
}


intptr_t MyJit::lookup(const std::string fnname)
{
	auto sym = lljit_->lookup(fnname);
	if (!sym) {
		errmsg_ = toString(sym.takeError());
		return 0;
	}
	return (intptr_t) (*sym).getAddress();
}




llvm::Function* gen_hello(MyJit& jit)
{
	llvm::LLVMContext& context = *jit.context();
	llvm::Module* module = jit.module();

	// some types that we will be using
	auto voidty = llvm::Type::getVoidTy(context);
	auto i32ty = llvm::Type::getInt32Ty(context);

	// declare function: int putchar(int)
	auto putchar =
		llvm::Function::Create(llvm::FunctionType::get(i32ty, {i32ty}, false),
							   llvm::Function::ExternalLinkage,
							   "putchar",
							   module);

	// create hello()
	auto hello =
		llvm::Function::Create(llvm::FunctionType::get(voidty, {}, false),
							   llvm::Function::ExternalLinkage,
							   "hello",
							   module);

	// create first block in hello()
	auto entry_bb = llvm::BasicBlock::Create(context, "entry", hello);

	// insert call putchar() instructions into the the block
	llvm::IRBuilder<> builder(entry_bb);
	for (const char* p = "hello\n"; *p; p++) {
		llvm::Value* ch = llvm::ConstantInt::get(context, llvm::APInt(32, *p, true));

		// putchar(ch)
		builder.CreateCall(putchar, ch);
	}

	// insert return instruction
	builder.CreateRetVoid();

	// done
	return hello;
}

using namespace std;

void jit_and_run()
{
	MyJit::init();
	MyJit jit("myjit");

	auto hello = gen_hello(jit);
	if (!jit.verify(hello)) {
		cerr << jit.errmsg() << endl;
		exit(1);
	}

	if (!jit.compile()) {
		cerr << jit.errmsg() << endl;
		exit(1);
	}

	// run it
	void (*fn)() =  (void(*)()) jit.lookup(hello);
	fn();
}



int main()
{
	jit_and_run();
}
