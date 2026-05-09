#pragma once
// ============================================================
// OperationResult.h  —  Uniform result with call-chain tracing
//
// All model methods that can fail return OpResult.
// The call-chain (stack trace equivalent) is built by each
// layer adding its own context string on propagation.
//
// DESIGN
// ──────
// OpResult is a small value type. On success it carries
// OPERATION_ACK. On failure it carries the error code, a
// human-readable detail string, and a call-chain vector that
// grows as the error propagates up the stack.
//
// USAGE (model — leaf):
//   OpResult Folder::update() {
//       if (wfLocked)
//           return OpResult::fail(Code::ENTITY_LOCKED,
//                                 "wfLocked=true on " + folderId,
//                                 "Folder::update");
//       ...
//       return OpResult::ok();
//   }
//
// USAGE (model — propagation):
//   OpResult F22::save() {
//       auto r = db->exec(...);
//       if (!r.ok()) return r.push("F22::save");
//       return OpResult::ok();
//   }
//
// USAGE (CLI):
//   auto r = folder->update();
//   if (!r.ok()) {
//       std::cout << "  >> " << r.message() << "\n";
//       if (Logger::level() == DEBUG)
//           for (auto& f : r.chain())
//               std::cout << "     at " << f << "\n";
//   }
//
// Backwards compatibility:
//   The old enum OperationResult still works (typedef).
//   opOk(), opResultMessage() still work.
// ============================================================
#include <string>
#include <vector>

namespace Rosenholz {

// ── Error codes ──────────────────────────────────────────────────────────────
enum class OperationResult {
    // Success
    OPERATION_ACK,

    // Document / Revision
    DOC_REV_NOT_IN_WORK,
    DOC_REV_IS_IN_WORK,
    DOC_REV_IS_CLOSED,
    DOC_REV_TRANSITION_NOT_ALLOWED,
    DOC_REVISION_NOT_FOUND,
    DOC_CHECKOUT_OPEN,
    DOC_NO_CHECKOUT,
    DOC_NO_PARENT_REV,
    DOC_FILE_NOT_FOUND,
    DOC_COMMIT_FAILED,
    DOC_SAVE_FAILED,
    DOC_OBJECT_IMPORT_FAILED,

    // Workflow
    WF_ALREADY_ACTIVE,
    WF_NOT_FOUND,
    WF_NOT_ACTIVE,
    WF_STEP_NOT_FOUND,
    WF_STEP_BLOCKED,
    WF_STEP_IS_SYSTEM,
    WF_COMMENT_REQUIRED,
    WF_DOCUMENT_REQUIRED,
    WF_CHILDREN_PENDING,    ///< Child entities are not yet in target state

    // Entity state
    ENTITY_RELEASED,
    ENTITY_LOCKED,
    ENTITY_WF_LOCKED,       ///< Entity has active workflow — mutations blocked
    ENTITY_LOCKED_WF = ENTITY_WF_LOCKED,  ///< alias
    ENTITY_WF_COMPLETE,
    ENTITY_NOT_FOUND,
    ENTITY_PARENT_LOCKED,   ///< Parent entity is locked/released
    ENTITY_PARENT_NOT_FOUND,

    // Object / key file
    OBJ_ALREADY_EXISTS,
    OBJ_NOT_FOUND,
    KEY_FILE_WRITE_FAILED,

    // General
    INVALID_ARGUMENT,
    DB_ERROR,
    IO_ERROR,
    NOT_IMPLEMENTED,
    PERMISSION_DENIED,      ///< Action requires admin role
};

// ── OpResult value type ──────────────────────────────────────────────────────
struct OpResult {
    OperationResult           code  { OperationResult::OPERATION_ACK };
    std::string              detail;             ///< Machine-readable context
    std::vector<std::string> callChain;          ///< Grows as error propagates

    // ── Constructors ─────────────────────────────────────────────────────────
    static OpResult ok() {
        return OpResult{OperationResult::OPERATION_ACK, {}, {}};
    }

    static OpResult fail(OperationResult c,
                         const std::string& detail_  = "",
                         const std::string& caller   = "") {
        OpResult r;
        r.code   = c;
        r.detail = detail_;
        if (!caller.empty()) r.callChain.push_back(caller);
        return r;
    }

    // ── Propagation ──────────────────────────────────────────────────────────
    /// Add caller to the chain and return *this (for chaining).
    OpResult& push(const std::string& caller) {
        if (!caller.empty()) callChain.push_back(caller);
        return *this;
    }

    // ── Accessors ────────────────────────────────────────────────────────────
    bool isOk()   const { return code == OperationResult::OPERATION_ACK; }
    bool isFail() const { return !isOk(); }

    /// Human-readable German message for the error code.
    std::string message() const;

    /// Full trace: message + call chain (for debug output).
    std::string trace() const {
        std::string s = message();
        if (!detail.empty())   s += "\n    Detail: "  + detail;
        for (auto& f : callChain) s += "\n    bei: " + f;
        return s;
    }

    const std::vector<std::string>& chain() const { return callChain; }
};

// ── Backwards-compatible alias ────────────────────────────────────────────────
inline bool opOk(OperationResult c) { return c == OperationResult::OPERATION_ACK; }

/// Legacy: German message for error code only (no detail/chain).
// ── opResultMessage implementation ───────────────────────────────────────────
inline std::string opResultMessage(OperationResult r) {
    switch (r) {
    case OperationResult::OPERATION_ACK:
        return "Operation erfolgreich ausgefuehrt.";
    case OperationResult::DOC_REV_NOT_IN_WORK:
        return "Die aktive Revision ist nicht 'in_work'. "
               "Checkout und Bearbeiten sind nur im Zustand in_work moeglich. "
               "Aktion: Starten Sie einen Workflow oder legen Sie eine neue Revision an.";
    case OperationResult::DOC_REV_IS_IN_WORK:
        return "Die aktive Revision ist noch 'in_work'. "
               "Eine neue Revision kann erst nach einem Workflow-Uebergang angelegt werden.";
    case OperationResult::DOC_REV_IS_CLOSED:
        return "Die Revision ist 'closed' — Terminalzustand, keine Aenderungen moeglich.";
    case OperationResult::DOC_REV_TRANSITION_NOT_ALLOWED:
        return "Dieser Zustandswechsel ist nicht erlaubt. "
               "Pruefe die Uebergangsmatrix (in_work → locked/pre_released/released/closed).";
    case OperationResult::DOC_REVISION_NOT_FOUND:
        return "Keine aktive Revision gefunden. "
               "Aktion: Legen Sie zuerst eine Revision an.";
    case OperationResult::DOC_CHECKOUT_OPEN:
        return "Checkout offen — erst einchecken (. -ci) oder revert (. -rv).";
    case OperationResult::DOC_NO_CHECKOUT:
        return "Kein offener Checkout. Aktion: Erst auschecken (. -co).";
    case OperationResult::DOC_NO_PARENT_REV:
        return "Keine Vorgaenger-Revision — Revert nicht moeglich.";
    case OperationResult::DOC_FILE_NOT_FOUND:
        return "Datei nicht gefunden. Aktion: Pfad pruefen.";
    case OperationResult::DOC_COMMIT_FAILED:
        return "LMDB-Uebertragung fehlgeschlagen. "
               "Aktion: Speicherplatz und MFS-Pfade pruefen.";
    case OperationResult::DOC_SAVE_FAILED:
        return "Datenbank-Speicherung fehlgeschlagen. Aktion: Log pruefen.";
    case OperationResult::DOC_OBJECT_IMPORT_FAILED:
        return "Datei konnte nicht in MFS importiert werden.";
    case OperationResult::WF_ALREADY_ACTIVE:
        return "Ein aktiver Workflow laeuft bereits auf dieser Entitaet. "
               "Aktion: Bestehenden Workflow abschliessen (. -f77 -d) oder abbrechen.";
    case OperationResult::WF_NOT_FOUND:
        return "Workflow nicht gefunden.";
    case OperationResult::WF_NOT_ACTIVE:
        return "Workflow nicht aktiv (abgeschlossen oder abgebrochen).";
    case OperationResult::WF_STEP_NOT_FOUND:
        return "Schritt nicht gefunden.";
    case OperationResult::WF_STEP_BLOCKED:
        return "Schritt blockiert — Vorgaenger-Schritte noch nicht abgeschlossen.";
    case OperationResult::WF_STEP_IS_SYSTEM:
        return "System-Schritt — kann nicht manuell bearbeitet werden.";
    case OperationResult::WF_COMMENT_REQUIRED:
        return "Kommentar fuer diesen Schritt erforderlich.";
    case OperationResult::WF_DOCUMENT_REQUIRED:
        return "Dokument muss angehaengt sein.";
    case OperationResult::WF_CHILDREN_PENDING:
        return "Kindelemente noch nicht im Zielstatus. "
               "Aktion: Offene Tasks unter -tasks abarbeiten.";
    case OperationResult::ENTITY_RELEASED:
        return "Entitaet ist freigegeben (released) — Bearbeiten nicht moeglich. "
               "Aktion: Neue Revision anlegen oder neuen Vorgang erstellen.";
    case OperationResult::ENTITY_LOCKED:
        return "Entitaet ist gesperrt (locked). "
               "Aktion: Admin-Entsperrung oder Workflow abwarten.";
    case OperationResult::ENTITY_WF_LOCKED:
        return "Aktiver Freigabe-Workflow laeuft — Aenderungen gesperrt. "
               "Aktion: -tasks oeffnen und offene Aufgaben abarbeiten.";
    case OperationResult::ENTITY_WF_COMPLETE:
        return "Workflow abgeschlossen — Entitaet dauerhaft gesperrt.";
    case OperationResult::ENTITY_NOT_FOUND:
        return "Entitaet nicht gefunden.";
    case OperationResult::ENTITY_PARENT_LOCKED:
        return "Eltern-Entitaet ist gesperrt oder freigegeben — keine Kinder-Aenderungen. "
               "Aktion: Status der Eltern-Entitaet pruefen.";
    case OperationResult::ENTITY_PARENT_NOT_FOUND:
        return "Eltern-Entitaet nicht gefunden.";
    case OperationResult::OBJ_ALREADY_EXISTS:
        return "Objekt existiert bereits.";
    case OperationResult::OBJ_NOT_FOUND:
        return "Objekt nicht gefunden.";
    case OperationResult::KEY_FILE_WRITE_FAILED:
        return "Schluesseldatei konnte nicht geschrieben werden.";
    case OperationResult::INVALID_ARGUMENT:
        return "Ungueltiges oder fehlendes Argument.";
    case OperationResult::DB_ERROR:
        return "Datenbankfehler. Aktion: Log pruefen.";
    case OperationResult::IO_ERROR:
        return "Dateisystemfehler. Aktion: Pfad und Berechtigungen pruefen.";
    case OperationResult::NOT_IMPLEMENTED:
        return "Funktion noch nicht implementiert.";
    case OperationResult::PERMISSION_DENIED:
        return "Keine Berechtigung. Aktion: Als Admin anmelden (user -pw admin).";
    default:
        return "Unbekannter Fehler.";
    }
}

/// OpResult::message() — delegates to opResultMessage
inline std::string OpResult::message() const { return opResultMessage(code); }

} // namespace Rosenholz
