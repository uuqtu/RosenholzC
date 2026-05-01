// ============================================================
// NavigationContext.cpp
// ============================================================
#include "NavigationContext.h"
#include "HistoryLog.h"
#include <sstream>
#include <algorithm>
#include <map>
#include <regex>

namespace Rosenholz {

// ── EntityType helpers ────────────────────────────────────────
std::string entityTypeLabel(EntityType t) {
    switch (t) {
        case EntityType::F16: return "F16";
        case EntityType::F22: return "F22";
        case EntityType::F18: return "F18";
        case EntityType::AKT: return "AKT";
        case EntityType::F77: return "F77";
        case EntityType::PER: return "PER";
        default:              return "?";
    }
}

EntityType entityTypeFromString(const std::string& s) {
    if (s == "f16" || s == "F16") return EntityType::F16;
    if (s == "f22" || s == "F22") return EntityType::F22;
    if (s == "f18" || s == "F18") return EntityType::F18;
    if (s == "akt" || s == "AKT" || s == "dok" || s == "DOK") return EntityType::AKT;
    if (s == "f77" || s == "F77") return EntityType::F77;
    if (s == "per" || s == "PER") return EntityType::PER;
    return EntityType::NONE;
}

// ── EntityRef ─────────────────────────────────────────────────
std::string EntityRef::shortForm() const {
    if (!valid()) return "";
    return entityTypeLabel(type) + ":" + id;
}

std::string EntityRef::compactForm() const {
    if (!valid()) return "";
    // Extract just the sequence+year from the reg number: XV/F22/0001/26 → 0001/26
    auto pos = id.rfind('/', id.rfind('/') - 1);
    std::string suffix = (pos != std::string::npos) ? id.substr(pos+1) : id;
    return entityTypeLabel(type) + ":" + suffix;
}

// ── NavigationStack ───────────────────────────────────────────
NavigationStack& NavigationStack::instance() {
    static NavigationStack inst;
    return inst;
}

void NavigationStack::push(const EntityRef& ref) {
    if (!ref.valid()) return;
    if (!stack_.empty() && stack_.back().id == ref.id) return;
    stack_.push_back(ref);
    // Persist to history log:
    HistoryLog::instance().record(ref);
}

void NavigationStack::pop() {
    if (!stack_.empty()) stack_.pop_back();
}

void NavigationStack::clear() { stack_.clear(); }

bool NavigationStack::empty() const { return stack_.empty(); }

EntityRef NavigationStack::current() const {
    return stack_.empty() ? EntityRef{} : stack_.back();
}

EntityRef NavigationStack::parent() const {
    return stack_.size() < 2 ? EntityRef{} : stack_[stack_.size()-2];
}

std::vector<EntityRef> NavigationStack::all() const { return stack_; }

// Helper: extract short "seq/year" from a full reg-ID (XV/F22/0001/26 -> 0001/26)
static std::string shortSeq(const std::string& id) {
    // Find second-to-last slash: XV/F22/0001/26 -> position of /0001
    auto lastSlash = id.rfind('/');
    if (lastSlash == std::string::npos) return id;
    auto prevSlash = id.rfind('/', lastSlash - 1);
    return (prevSlash != std::string::npos) ? id.substr(prevSlash + 1) : id;
}

std::string NavigationStack::breadcrumb() const {
    if (stack_.empty()) return "";
    std::ostringstream oss;
    for (std::size_t i = 0; i < stack_.size(); i++) {
        if (i) oss << " > ";
        const auto& r = stack_[i];
        oss << entityTypeLabel(r.type) << ":" << shortSeq(r.id);
        if (i == stack_.size() - 1 && !r.displayName.empty()) {
            std::string name = r.displayName;
            if (name.size() > 24) name = name.substr(0, 22) + "..";
            oss << " - " << name;
        }
    }
    return oss.str();
}

std::string NavigationStack::promptSuffix() const {
    if (stack_.empty()) return "";
    // Format: "F16:0001/26 > F22:0001/26 > AKT:0003/26 - Titel des aktuellen Elements"
    // Only the CURRENT (last) level shows the title after " - "
    std::ostringstream oss;
    for (std::size_t i = 0; i < stack_.size(); i++) {
        if (i) oss << " > ";
        const auto& r = stack_[i];
        oss << entityTypeLabel(r.type) << ":" << shortSeq(r.id);
        // Current (last) level: append " - Title"
        if (i == stack_.size() - 1 && !r.displayName.empty()) {
            std::string name = r.displayName;
            if (name.size() > 24) name = name.substr(0, 22) + "..";
            oss << " - " << name;
        }
    }
    return oss.str();
}

void NavigationStack::popTo(EntityType t) {
    while (!stack_.empty() && stack_.back().type != t)
        stack_.pop_back();
}

// ── QuickResolver ─────────────────────────────────────────────
QuickResolver& QuickResolver::instance() {
    static QuickResolver inst;
    return inst;
}

void QuickResolver::registerLoader(EntityType t, Loader fn) {
    loaders_[t] = std::move(fn);
}
void QuickResolver::registerLister(EntityType t, Lister fn) {
    listers_[t] = std::move(fn);
}

std::vector<EntityRef> QuickResolver::listChildren(
    EntityType childType, EntityType /*parentType*/, const std::string& parentId) const
{
    auto it = listers_.find(childType);
    if (it == listers_.end()) return {};
    return it->second(parentId);
}

ResolvedEntity QuickResolver::resolve(const std::string& input,
                                      EntityType contextType,
                                      const std::string& contextId) const
{
    ResolvedEntity result;
    if (input.empty()) return result;

    // ".." → go back
    if (input == ".." || input == "zurück" || input == "back") {
        result.isBack = true;
        return result;
    }

    // Full ID: contains '/'  → direct load
    if (input.find('/') != std::string::npos) {
        // Determine type from prefix (XV/F16/…, XV/F22/…, …)
        EntityType t = EntityType::NONE;
        if      (input.find("/F16/") != std::string::npos) t = EntityType::F16;
        else if (input.find("/F22/") != std::string::npos) t = EntityType::F22;
        else if (input.find("/F18/") != std::string::npos) t = EntityType::F18;
        else if (input.find("/AKT/") != std::string::npos) t = EntityType::AKT;
        else if (input.find("/F77W/") != std::string::npos) t = EntityType::F77;
        else if (input.find("/PER/") != std::string::npos) t = EntityType::PER;

        if (t != EntityType::NONE) {
            auto it = loaders_.find(t);
            if (it != loaders_.end()) {
                auto ref = it->second(input);
                if (ref) { result.ref = *ref; return result; }
            }
        }
        return result; // not found
    }

    // "type:N" format e.g. "f22:1"
    auto colon = input.find(':');
    if (colon != std::string::npos) {
        EntityType t = entityTypeFromString(input.substr(0, colon));
        std::string rest = input.substr(colon+1);
        if (t != EntityType::NONE) {
            // If rest is a number, treat as index in child list
            if (!rest.empty() && std::all_of(rest.begin(), rest.end(), ::isdigit)) {
                int idx = std::stoi(rest) - 1;
                auto children = listChildren(t, contextType, contextId);
                if (idx >= 0 && idx < (int)children.size()) {
                    result.ref = children[idx];
                    return result;
                }
            } else {
                // rest is partial ID — try direct load
                auto it = loaders_.find(t);
                if (it != loaders_.end()) {
                    auto ref = it->second(rest);
                    if (ref) { result.ref = *ref; return result; }
                }
            }
        }
        return result;
    }

    // Pure number: index into context's child list (context-sensitive)
    if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
        int idx = std::stoi(input) - 1;
        // Try children of current context first
        if (contextType != EntityType::NONE) {
            // Determine natural child type from parent:
            static const std::map<EntityType,EntityType> childOf = {
                {EntityType::F16, EntityType::F22},
                {EntityType::F22, EntityType::AKT},
                {EntityType::F18, EntityType::AKT},
            };
            auto cit = childOf.find(contextType);
            if (cit != childOf.end()) {
                auto children = listChildren(cit->second, contextType, contextId);
                if (idx >= 0 && idx < (int)children.size()) {
                    result.ref = children[idx];
                    return result;
                }
            }
        }
        // No context — try sequence number across all types
        // Build padded sequence like "0001" → try XV/F16/0001/YY etc.
        // Return ambiguous if multiple match
        result.ambiguous = true;
        for (auto& [t, loader] : loaders_) {
            auto children = listChildren(t, EntityType::NONE, "");
            if (idx >= 0 && idx < (int)children.size()) {
                result.candidates.push_back(children[idx]);
            }
        }
        if (result.candidates.size() == 1) {
            result.ref = result.candidates[0];
            result.ambiguous = false;
            result.candidates.clear();
        }
        return result;
    }

    return result; // unresolved
}

} // namespace Rosenholz
