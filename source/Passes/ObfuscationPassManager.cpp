#include "ObfuscationPassManager.hpp"
#include "IObfuscationPass.hpp"
#include "FuncAttributeStore.hpp"
#include <llvm/IR/Module.h>
#include <string_view>
#include <ranges>
namespace obfusc {

    
    static bool is_excluded(llvm::Module& mod, llvm::Function& func){
        // llvm::errs() << "[~] FUNC: " << func.getName() << "\n";
        // llvm::errs() << "    + isIntrinsic      : " << func.isIntrinsic() << "\n";
        // llvm::errs() << "    + isDeclaration    : " << func.isDeclaration() << "\n";
        if (func.isIntrinsic()) return true;
        if (func.isDeclaration()) return true;
        return false;
    }

    static std::optional<std::vector<IObfuscationPass*>> get_passes_from_attr(std::string attr){
        using std::operator""sv;
        constexpr auto delim{","sv};
        std::vector<IObfuscationPass*> vec;
        auto p = attr.c_str();
        auto l = attr.size();
        if (p[0] == '"'){
            p++;
            l-=2;
        }
        for (const auto word : std::views::split(std::string_view(p, l), delim)){
            auto s = word.size();
            // 删除串尾 null
            while (s > 0 && word.data()[s - 1] == '\0'){
                s--;
            }
            auto pass_name = llvm::StringRef(word.data(), s);
            // auto pass = FuncAttributeStore::GetInstance().GetAttrPass(llvm::StringRef(word.data(), word.size()));
            auto pass = ObfsRegistar::GetInstance().passes[pass_name.str()];
            // llvm::outs() << llvm::StringRef(word.data(), word.size()) << ": " << pass << "\n";
            if (!pass){
                // llvm::outs() << "[OBFS]" << llvm::StringRef(word.data(), s) << " NOT FOUND " << "\n";
                return {};
            }
            vec.emplace_back(pass);
        }
        return {vec};
    }
    llvm::PreservedAnalyses ObfuscationPassManager::run(llvm::Module& mod, llvm::ModuleAnalysisManager&) {
        bool changed = false;
        for (auto& func : mod.getFunctionList()) { //Get all functions in module
            bool is_marked = false;
            for (auto& attrs : func.getAttributes()) { //Get each attribute lists attached to function
                for (auto& attr : attrs) { //Get attributes one by one
                    std::vector<IObfuscationPass*> vec;
                    if (attr.isStringAttribute()) { //If attribute is a string
                        // llvm::outs() << "[~] attr: " << attr.getAsString() << "\n";
                        auto res = get_passes_from_attr(attr.getAsString());
                        if (res){
                            is_marked = true;
                            for(auto pass: *res){
                                // llvm::outs() << "[-] running " << pass << "\n";
                                changed |= pass->obfuscate(mod, func); //Call obfuscate func
                            }
                        }
                    }
                }
            }
            // llvm::outs() << "FUNC " << func.getName() << "  MARK " << is_marked << "\n";
            if (is_marked) continue;
            if (is_excluded(mod, func)) continue;
            #if 1
            for(auto passName: {"estr","icall"}){
                auto pass = ObfsRegistar::GetInstance().passes[passName];
                if (!pass){
                    llvm::errs() << "[!] WRONG PASS NAME: " << passName << "\n";
                    continue;
                }
                changed |= pass->obfuscate(mod, func);
            }
            #endif
        }

        
        for(auto p: ObfsRegistar::GetInstance().passes){
            if (p.second){
                p.second->fini();
            }
        }
        if (changed) {
            return llvm::PreservedAnalyses::none();
        }
        return llvm::PreservedAnalyses::all();
    }
}