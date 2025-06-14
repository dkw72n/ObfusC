#include "FuncAnnotationsParser.hpp"
#include "FuncAttributeStore.hpp"
#include <llvm/IR/Module.h>
#include <clang/Sema/ParsedAttr.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaDiagnostic.h>
#include <vector>
namespace obfusc {
    llvm::PreservedAnalyses FuncAnnotationsParser::run(llvm::Module& M, llvm::ModuleAnalysisManager&) {
        llvm::GlobalVariable* globalAttrs = M.getGlobalVariable("llvm.global.annotations"); //Get llvm.global.annotations from IR data
        if (!globalAttrs) {
            return llvm::PreservedAnalyses::all();
        }
        
        llvm::ConstantArray* array = llvm::cast<llvm::ConstantArray>(globalAttrs->getOperand(0)); //Get array of operands    
        for (llvm::Value* operand : array->operands()) { //Get each operand
            llvm::ConstantStruct* constant = llvm::dyn_cast<llvm::ConstantStruct>(operand); //Cast operand to a struct (i.e. the annotation struct)
            if (!constant || constant->getNumOperands() < 2) { //Must be at least two operands (FUNCTION_OPERAND and ANNOTATE_OPERAND) 
                continue;
            }
            
            llvm::Function* func = llvm::cast<llvm::Function>(constant->getOperand(AnnotationOperands::FUNCTION_OPERAND)); //Get function
            llvm::GlobalVariable* globalStrPtr = llvm::cast<llvm::GlobalVariable>(constant->getOperand(AnnotationOperands::ANNOTATE_OPERAND));
            if (!func || !globalStrPtr) {
                continue;
            }

            if (llvm::ConstantDataArray* strArray = llvm::dyn_cast<llvm::ConstantDataArray>(globalStrPtr->getOperand(0))) { //Get Annotation str
                llvm::StringRef str = strArray->getAsString();
                // llvm::outs() << "[-] FuncAnnotationsParser::run " << str << "\n";
                if (FuncAttributeStore::GetInstance().IsAttrStored(str)) {
                    // llvm::outs() << "[-] func->addFnAttr " << str << "\n";
                    func->addFnAttr(str); //add Annotation to function
                }
            }
        }

        return llvm::PreservedAnalyses::all();
    }

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
}
