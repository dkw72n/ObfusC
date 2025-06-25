#include "ObfuscationPassManager.hpp"
#include "IObfuscationPass.hpp"
#include "FuncAttributeStore.hpp"
#include <llvm/IR/Module.h>
#include <string_view>
#include <ranges>
namespace obfusc {

    
    static bool is_excluded(llvm::Module& mod, llvm::Function& func){
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
            // llvm::outs() << word.data() << "\n";
            auto pass = FuncAttributeStore::GetInstance().GetAttrPass(llvm::StringRef(word.data(), word.size()));
            if (!pass){
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
                                llvm::outs() << "[-] running " << pass << "\n";
                                changed |= pass->obfuscate(mod, func); //Call obfuscate func
                            }
                        }
                    }
                }
            }
            if (is_marked) continue;
            if (is_excluded(mod, func)) continue;
            for(auto pass: {"icall", "bcf"}){
                changed |= ObfsRegistar::GetInstance().passes[pass]->obfuscate(mod, func);
            }
        }

        if (changed) {
            return llvm::PreservedAnalyses::none();
        }
        return llvm::PreservedAnalyses::all();
    }
}