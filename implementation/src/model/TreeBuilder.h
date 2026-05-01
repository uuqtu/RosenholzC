#pragma once
// ============================================================
// TreeBuilder.h  —  Entity hierarchy tree (model layer)
//
// Builds a printable ASCII tree of the entity hierarchy:
//   F16 → F22 → AKT
//         F22 → F18
// All loading logic lives here; CLI only calls print().
// ============================================================
#include "NavigationContext.h"
#include <vector>
#include <string>
#include <functional>

namespace Rosenholz {

struct TreeNode {
    EntityRef               ref;
    std::string             statusLabel;  // e.g. "in_work", "released"
    std::vector<TreeNode>   children;
};

class TreeBuilder {
public:
    // Build the complete tree under an F16 project.
    // Loads F22 tasks and their AKT/F18 children.
    static TreeNode buildF16Tree(const std::string& projectId);

    // Build a flat overview of all F16 projects with counts.
    static std::vector<TreeNode> buildAllF16();

    // Format the tree as an ASCII string.
    // indent: number of spaces per level (default 2).
    static std::string format(const TreeNode& root, int indent = 2);
    static std::string formatAll(const std::vector<TreeNode>& roots, int indent = 2);

private:
    static std::string formatNode(const TreeNode& n,
                                  const std::string& prefix,
                                  bool isLast,
                                  int indent);
};

} // namespace Rosenholz
