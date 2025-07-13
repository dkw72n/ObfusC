#include "FuncAnnotationsParser.hpp"
#include "FuncAttributeStore.hpp"
#include <llvm/IR/Module.h>
#if USE_CLANG_ATTR
#include <clang/Sema/ParsedAttr.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaDiagnostic.h>
#endif
#include <vector>
namespace obfusc {
    llvm::PreservedAnalyses FuncAnnotationsParser::run(llvm::Module& M, llvm::ModuleAnalysisManager&) {
        llvm::GlobalVariable* annotations = M.getGlobalVariable("llvm.global.annotations"); //Get llvm.global.annotations from IR data
        if (!annotations) {
            return llvm::PreservedAnalyses::all();
        }
        
        llvm::ConstantArray* array = llvm::cast<llvm::ConstantArray>(annotations->getOperand(0)); //Get array of operands    
        for (llvm::Value* annotation : array->operands()) { //Get each operand
            // operand->dump();
            llvm::ConstantStruct* annoDef = llvm::dyn_cast<llvm::ConstantStruct>(annotation); //Cast operand to a struct (i.e. the annotation struct)
            if (!annoDef || annoDef->getNumOperands() < 2) { //Must be at least two operands (FUNCTION_OPERAND and ANNOTATE_OPERAND) 
                continue;
            }
            llvm::Function* annoFunc = llvm::cast<llvm::Function>(annoDef->getOperand(AnnotationOperands::FUNCTION_OPERAND)); //Get function
            llvm::GlobalVariable* annoName = llvm::cast<llvm::GlobalVariable>(annoDef->getOperand(AnnotationOperands::ANNOTATE_OPERAND));
            if (!annoFunc || !annoName) {
                continue;
            }
            if (llvm::ConstantDataArray* annoNameBytes = llvm::dyn_cast<llvm::ConstantDataArray>(annoName->getOperand(0))) { //Get Annotation str
                llvm::StringRef str = annoNameBytes->getAsString();
                auto nameWithOutZero = std::string(str.str().c_str());
                /// llvm::outs() << " annoNameBytes: " << str <<", " << (str == "obfusc") << "," << strcmp(str.str().c_str(), "obfusc") << "\n";
                if (nameWithOutZero != "obfusc"){
                    continue;
                }
                llvm::outs() << "[-] found obfusc @ " << annoFunc->getName() << "\n";
                auto args = annoDef->getOperand(AnnotationOperands::ARGS_OPERAND);
                if (args){
                    auto argsDef = llvm::dyn_cast<llvm::ConstantStruct>(args->getOperand(0));
                    for (auto& arg: argsDef->operands()){
                        auto cda = llvm::dyn_cast<llvm::ConstantDataVector>(arg.get());
                        llvm::outs() << "   + arg: " << cda->getAsString() << "\n";
                    }
                }
            }
        }

        return llvm::PreservedAnalyses::all();
    }
#if USE_CLANG_ATTR
    class Obfs: public clang::ParsedAttrInfo{
        public:
        virtual bool diagAppertainsToDecl(clang::Sema& S, const clang::ParsedAttr& Attr, const clang::Decl* D) const override {
            if (!clang::isa<clang::FunctionDecl>(D)) { //This attribute appertains to functions only.
                S.Diag(Attr.getLoc(), clang::diag::warn_attribute_wrong_decl_type_str) << Attr << "functions";
                return false;
            }
            return true;
        }

        virtual AttrHandling handleDeclAttribute(clang::Sema& S, clang::Decl* D, const clang::ParsedAttr& Attr) const override {
            if ((!D->getDeclContext()->isFileContext())) { //Check if the decl is at file scope
                if (D->getDeclContext()->getDeclKind() != clang::Decl::Kind::CXXRecord) { //or if it's a lambda (other CXXRecords are covered by diagAppertainsToDecl)
                    std::string attrStr(Attr.getAttrName()->deuglifiedName().data());
                    attrStr.append(" attribute only allowed at file scope and on lambdas");

                    unsigned ID = S.getDiagnostics().getDiagnosticIDs()->getCustomDiagID(clang::DiagnosticIDs::Error, llvm::StringRef(attrStr));
                    S.Diag(Attr.getLoc(), ID);
                    return AttributeNotApplied;
                }
            }
            // llvm::outs() << Attr.getAttrName()->deuglifiedName() << "  NumArgs:" << Attr.getNumArgs() << "\n";
            // Attr.getArgAsExpr(0)->dump();
            // llvm::outs() << Attr.getAttrName()->getName() << "\n";
            if (auto* stringLiteral = llvm::dyn_cast<clang::StringLiteral>(Attr.getArgAsExpr(0))) {
                // return stringLiteral->getBytes().data();
                llvm::outs() << "dyn_cast:" << stringLiteral->getString() << "\n";
            
                D->addAttr(clang::AnnotateAttr::Create(S.Context, stringLiteral->getString(), nullptr, 0, Attr.getRange()));
                return AttributeApplied;
            }
            return AttributeNotApplied;
        }
        inline Obfs() {
            NumArgs = 1;
            OptArgs = 0;
            static constexpr const char nameStr[] = { 'o', 'b', 'f', 's', '\0' };
            static constexpr const char cxxNameStr[] = {'o', 'b', 'f', 'u', 's', 'c', ':', ':', 'o', 'b', 'f', 's', '\0' };

            static const Spelling S[] {{clang::ParsedAttr::AS_GNU, nameStr},
                        {clang::ParsedAttr::AS_CXX11, nameStr},
                        {clang::ParsedAttr::AS_CXX11, cxxNameStr}};

            Spellings = S;

            // FuncAttributeStore::GetInstance().StoreAttributeInfo(nameStr, new passType());
        }

    };

    static clang::ParsedAttrInfoRegistry::Add<Obfs> obfsAttr("obfs", "");
#endif
}
