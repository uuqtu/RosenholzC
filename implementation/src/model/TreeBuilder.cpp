#include "Utils.h"
// ============================================================
// TreeBuilder.cpp
// ============================================================
#include "TreeBuilder.h"
#include "f16/ProjectF16.h"
#include "f22/TaskF22.h"
#include "dok/Document.h"
#include "f18/F18Operation.h"
#include <sstream>

namespace Rosenholz {

TreeNode TreeBuilder::buildF16Tree(const std::string& projectId) {
    auto p = ProjectF16::loadById(projectId);
    TreeNode root;
    if (!p) return root;
    root.ref = {EntityType::F16, p->projectId, p->title, p->regNumber.toString()};
    root.statusLabel = p->status;

    // F22 tasks:
    for (auto& t : TaskF22::loadForProject(projectId)) {
        TreeNode tn;
        tn.ref = {EntityType::F22, t->taskId, t->title, t->regNumber.toString()};
        tn.statusLabel = t->status;

        // AKT under each F22:
        for (auto& d : Document::loadForEntity("f22", t->taskId)) {
            TreeNode dn;
            dn.ref = {EntityType::AKT, d->documentId, d->title, d->documentId};
            dn.statusLabel = d->format;
            tn.children.push_back(std::move(dn));
        }
        // F18 under each F22:
        for (auto& v : F18Operation::loadForTask(t->taskId)) {
            TreeNode vn;
            vn.ref = {EntityType::F18, v->vorgangId, v->title, v->vorgangId};
            vn.statusLabel = v->status;
            tn.children.push_back(std::move(vn));
        }
        root.children.push_back(std::move(tn));
    }
    // AKT directly on F16:
    for (auto& d : Document::loadForEntity("f16", projectId)) {
        TreeNode dn;
        dn.ref = {EntityType::AKT, d->documentId, d->title, d->documentId};
        dn.statusLabel = d->format;
        root.children.push_back(std::move(dn));
    }
    return root;
}

std::vector<TreeNode> TreeBuilder::buildAllF16() {
    std::vector<TreeNode> result;
    for (auto& p : ProjectF16::loadAll()) {
        TreeNode n;
        n.ref = {EntityType::F16, p->projectId, p->title, p->regNumber.toString()};
        n.statusLabel = p->status;
        // Count children without loading full tree:
        int f22c = (int)TaskF22::loadForProject(p->projectId).size();
        n.children.resize(f22c); // placeholder count
        result.push_back(std::move(n));
    }
    return result;
}

std::string TreeBuilder::formatNode(const TreeNode& n,
                                     const std::string& prefix,
                                     bool isLast,
                                     int /*indent*/) {
    std::ostringstream oss;
    std::string connector = isLast ? "└── " : "├── ";
    std::string typeTag   = "[" + entityTypeLabel(n.ref.type) + "]";
    std::string title     = n.ref.displayName.substr(0, 32);
    std::string status    = n.statusLabel.empty() ? "" : "  " + n.statusLabel;
    oss << prefix << connector
        << typeTag << " " << title << status << "\n";

    std::string childPrefix = prefix + (isLast ? "    " : "│   ");
    for (int i = 0; i < (int)n.children.size(); i++) {
        bool last = (i == (int)n.children.size() - 1);
        oss << formatNode(n.children[i], childPrefix, last, 2);
    }
    return oss.str();
}

std::string TreeBuilder::format(const TreeNode& root, int indent) {
    if (!root.ref.valid()) return "";
    std::ostringstream oss;
    oss << "[" << entityTypeLabel(root.ref.type) << "] "
        << root.ref.displayName.substr(0, 40)
        << "  " << root.statusLabel << "\n";
    for (int i = 0; i < (int)root.children.size(); i++) {
        bool last = (i == (int)root.children.size() - 1);
        oss << formatNode(root.children[i], "", last, indent);
    }
    return oss.str();
}

std::string TreeBuilder::formatAll(const std::vector<TreeNode>& roots, int indent) {
    std::ostringstream oss;
    for (int i = 0; i < (int)roots.size(); i++) {
        bool last = (i == (int)roots.size() - 1);
        std::string conn = last ? "└── " : "├── ";
        oss << conn << "[F16] " << roots[i].ref.displayName.substr(0, 32)
            << "  " << roots[i].statusLabel
            << "  [F22:" << roots[i].children.size() << "]\n";
    }
    return oss.str();
}

} // namespace Rosenholz
