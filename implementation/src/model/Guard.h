#pragma once
// ============================================================
// Guard.h  —  Entity mutation guard framework
//
// Every guard is a free function with a clear signature.
// Callers compose guards:  if (!guard::canEdit(entity)) ...
//
// Naming:  guard::<VERB><SUBJECT>
//   e.g.   guard::canEdit(F22)
//          guard::canAddChild(F22, ChildType)
//          guard::canStartWorkflow(F22)
//
// Return type: OpResult — ok() on success, fail(code, detail)
// on failure. The caller passes the result directly to the CLI
// to display a precise, actionable message.
//
// Adding a new check: add ONE function here and ONE call in
// the relevant model method. No changes to CLI code needed.
// ============================================================
#include "../core/OperationResult.h"
#include "Utils.h"
#include <string>

namespace Rosenholz {

// Forward declarations (avoids circular includes):
struct F16;
struct F22;
struct F18Operation;
struct FolderRevision;

namespace guard {

// ── Shared predicates ────────────────────────────────────────────────────────

/// Entity is editable: status is IN_WORK and no active workflow.
inline OpResult isEditable(EntityStatus status, bool wfLocked,
                            const std::string& entityId,
                            const std::string& caller = "") {
    if (wfLocked)
        return OpResult::fail(OperationResult::ENTITY_WF_LOCKED,
            "wfLocked=true on " + entityId, caller);
    if (status == EntityStatus::RELEASED)
        return OpResult::fail(OperationResult::ENTITY_RELEASED,
            entityId + " is released", caller);
    if (status == EntityStatus::LOCKED)
        return OpResult::fail(OperationResult::ENTITY_LOCKED,
            entityId + " is locked", caller);
    if (status == EntityStatus::CLOSED || status == EntityStatus::CANCELLED)
        return OpResult::fail(OperationResult::ENTITY_WF_COMPLETE,
            entityId + " is closed/cancelled", caller);
    return OpResult::ok();
}

/// Parent entity allows child creation / child edits.
inline OpResult parentIsEditable(EntityStatus parentStatus, bool parentWfLocked,
                                  const std::string& parentId,
                                  const std::string& caller = "") {
    if (parentWfLocked)
        return OpResult::fail(OperationResult::ENTITY_PARENT_LOCKED,
            "parent wfLocked=true on " + parentId, caller);
    if (parentStatus != EntityStatus::IN_WORK)
        return OpResult::fail(OperationResult::ENTITY_PARENT_LOCKED,
            "parent " + parentId + " is not in_work ("
            + std::string(entityStatusToString(parentStatus)) + ")", caller);
    return OpResult::ok();
}

/// Workflow can be started on this entity (no active WF running).
inline OpResult canStartWorkflow(const std::string& releaseWorkflowId,
                                  const std::string& entityId,
                                  const std::string& caller = "") {
    if (!releaseWorkflowId.empty())
        return OpResult::fail(OperationResult::WF_ALREADY_ACTIVE,
            "active WF " + releaseWorkflowId + " on " + entityId, caller);
    return OpResult::ok();
}

// ── AKT / FolderRevision guards ──────────────────────────────────────────────

/// Revision is in_work and editable (for object import, checkout, etc.).
inline OpResult revisionIsEditable(const std::string& revState,
                                    const std::string& entityId,
                                    const std::string& caller = "") {
    if (revState != "in_work")
        return OpResult::fail(OperationResult::DOC_REV_NOT_IN_WORK,
            "revision of " + entityId + " is '" + revState + "'", caller);
    return OpResult::ok();
}

/// Can create a new revision (current revision must NOT be in_work).
inline OpResult canRevise(const std::string& revState,
                           const std::string& entityId,
                           const std::string& caller = "") {
    if (revState == "in_work")
        return OpResult::fail(OperationResult::DOC_REV_IS_IN_WORK,
            "current revision of " + entityId + " is still in_work", caller);
    if (revState == "closed")
        return OpResult::fail(OperationResult::DOC_REV_IS_CLOSED,
            "revision of " + entityId + " is closed", caller);
    return OpResult::ok();
}

} // namespace guard
} // namespace Rosenholz
