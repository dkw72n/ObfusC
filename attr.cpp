#include <clang/Sema/ParsedAttr.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaDiagnostic.h>
#include <vector>


namespace obfusc{

    static clang::ConstantExpr* StringLiteralToConstantExpr(const clang::ASTContext& Context, clang::StringLiteral* stringLiteral){
        std::vector<clang::APValue> chrs;
        for(auto i:stringLiteral->getBytes()){
            chrs.emplace_back(llvm::APSInt(llvm::APInt(8, i)));
        }
        auto CE = clang::ConstantExpr::Create(Context, stringLiteral, clang::APValue(
            &chrs[0], chrs.size()
        ));
        // CE->getAPValueResult().dump();
        return CE;
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
            
            llvm::SmallVector<clang::Expr *, 16> ArgsBuf;
            // llvm::outs() << "[-] Args:" << Attr.getNumArgs() << "\n";
            std::vector<clang::Expr*> Args;
            for (int i = 0; i < Attr.getNumArgs(); i++){
                auto* stringLiteral = llvm::dyn_cast<clang::StringLiteral>(Attr.getArgAsExpr(i));
                ArgsBuf.push_back(
                    /*
                    官方 example 有问题...
                    */
                    StringLiteralToConstantExpr(S.Context, stringLiteral)
                );
            }
            
            if (ArgsBuf.size()){
                D->addAttr(clang::AnnotateAttr::Create(S.Context, "obfusc", ArgsBuf.data(), ArgsBuf.size(), Attr.getRange()));
            } else {
                D->addAttr(clang::AnnotateAttr::Create(S.Context, "obfusc", nullptr, 0, Attr.getRange()));
            }
            return AttributeApplied;
        }
        inline Obfs() {
            NumArgs = 0;
            OptArgs = 15;
            static constexpr const char nameStr[] = { 'o', 'b', 'f', 's', '\0' };
            static constexpr const char cxxNameStr[] = {'o', 'b', 'f', 'u', 's', 'c', ':', ':', 'o', 'b', 'f', 's', '\0' };

            static const Spelling S[] {{clang::ParsedAttr::AS_GNU, nameStr},
                        {clang::ParsedAttr::AS_CXX11, nameStr},
                        {clang::ParsedAttr::AS_CXX11, cxxNameStr}};

            Spellings = S;
        }

    };

    static clang::ParsedAttrInfoRegistry::Add<Obfs> obfsAttr("obfs", "");
}
