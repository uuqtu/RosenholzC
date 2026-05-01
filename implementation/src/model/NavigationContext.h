#pragma once
// ============================================================
// NavigationContext.h
// ============================================================
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <map>

namespace Rosenholz {

// ── EntityType ────────────────────────────────────────────────
enum class EntityType {
    NONE = 0,
    F16,    // F16
    F22,    // F22
    F18,    // F18Operation
    AKT,    // Document (Akte)
    F77,    // F77W
    PER,    // Person
};

std::string entityTypeLabel(EntityType t);   // "F16", "F22", …
EntityType  entityTypeFromString(const std::string& s); // "f16"→F16

// ── EntityRef ─────────────────────────────────────────────────
// Immutable reference to a named entity in the system.
struct EntityRef {
    EntityType  type        { EntityType::NONE };
    std::string id;          // full generated ID  XV/F22/0001/26
    std::string displayName; // human title
    std::string regNr;       // registration number string (same as id for most)

    bool valid() const { return type != EntityType::NONE && !id.empty(); }

    // Short form: "F22:XV/F22/0001/26"
    std::string shortForm()    const;
    // Compact: "F22:0001/26" (drops department prefix for display)
    std::string compactForm()  const;
};

// ── NavigationStack ───────────────────────────────────────────
// Tracks where the user is in the entity hierarchy.
// Not DB-backed — session memory only.
class NavigationStack {
public:
    static NavigationStack& instance();

    void        push(const EntityRef& ref);
    void        pop();
    void        clear();
    bool        empty()   const;
    EntityRef   current() const;          // top of stack (or NONE)
    EntityRef   parent()  const;          // one below top
    std::vector<EntityRef> all() const;   // full stack, bottom first

    // Breadcrumb string: "F16:0001 → F22:0001 → AKT:0001"
    std::string breadcrumb() const;

    // Shell prompt suffix: " [F22:XV/F22/0001/26]" or ""
    std::string promptSuffix() const;

    // Pop back to a specific type (useful for ".." navigation)
    void        popTo(EntityType t);

private:
    std::vector<EntityRef> stack_;
};

// ── QuickResolver ─────────────────────────────────────────────
// Resolves short user input to full EntityRef.
// Short forms:
//   "1"       → #1 from current context's child list
//   "f22:1"   → #1 F22 in the session
//   "0001"    → sequence number search across all types
//   "XV/F22/0001/26" → full ID, direct load
//   ".."      → go up (pop navigation stack)
//
// Resolver needs loaders for each entity type.
// Loaders are injected by the CLI layer at startup — keeps this header
// free of concrete model includes (avoids circular deps).
struct ResolvedEntity {
    EntityRef ref;
    bool      isBack { false };   // ".." was typed
    bool      ambiguous { false }; // multiple matches
    std::vector<EntityRef> candidates; // if ambiguous
};

class QuickResolver {
public:
    static QuickResolver& instance();

    // Register loaders (called once at CLI startup):
    using Loader = std::function<std::optional<EntityRef>(const std::string& id)>;
    using Lister = std::function<std::vector<EntityRef>(const std::string& parentId)>;

    void registerLoader(EntityType t, Loader fn);
    void registerLister(EntityType t, Lister fn);  // list children

    // Resolve a short string to an EntityRef.
    // parentId: optional context for child-list indexing.
    ResolvedEntity resolve(const std::string& input,
                           EntityType contextType = EntityType::NONE,
                           const std::string& contextId = "") const;

    // List children of a given parent by type.
    std::vector<EntityRef> listChildren(EntityType childType,
                                        EntityType parentType,
                                        const std::string& parentId) const;
private:
    std::map<EntityType,Loader> loaders_;
    std::map<EntityType,Lister> listers_;
};

} // namespace Rosenholz
