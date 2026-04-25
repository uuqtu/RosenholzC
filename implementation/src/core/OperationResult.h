#pragma once
// ============================================================
// OperationResult.h  —  Uniform result/error codes
//
// All model methods that can fail return OperationResult.
// The CLI and any other UI layer uses the code to produce
// appropriate, context-sensitive error messages.
//
// OPERATION_ACK  — operation succeeded ("acknowledged")
// Every other code is a failure.
//
// Naming convention:
//   <DOMAIN>_<REASON>
//   e.g. DOC_REV_NOT_IN_WORK   — document revision is not in_work
//        WF_ALREADY_ACTIVE     — a workflow is already running
//
// Usage (model):
//   OperationResult addObject(...) {
//       if (!inWork()) return OperationResult::DOC_REV_NOT_IN_WORK;
//       ...
//       return OperationResult::OPERATION_ACK;
//   }
//
// Usage (CLI):
//   auto res = doc->addObject(path);
//   if (res != OperationResult::OPERATION_ACK)
//       std::cout << "  >> " << opResultMessage(res) << "\n";
// ============================================================
#include <string>

namespace Rosenholz {

enum class OperationResult {
    // ── Success ─────────────────────────────────────────────
    OPERATION_ACK,          ///< Operation acknowledged — executed successfully

    // ── Document / Revision ──────────────────────────────────
    DOC_REV_NOT_IN_WORK,    ///< Active revision is not in_work — checkout/edit blocked
    DOC_REV_IS_IN_WORK,     ///< Active revision is still in_work — revise blocked
    DOC_REV_IS_CLOSED,      ///< Revision is closed — no transitions possible
    DOC_REV_TRANSITION_NOT_ALLOWED,  ///< This state transition is not permitted
    DOC_REVISION_NOT_FOUND, ///< No active revision found for this document
    DOC_CHECKOUT_OPEN,      ///< A checkout is already open — checkin or close it first
    DOC_NO_CHECKOUT,        ///< No checkout is open — checkin/revert unavailable
    DOC_NO_PARENT_REV,      ///< No parent revision to revert to
    DOC_FILE_NOT_FOUND,     ///< File path does not exist
    DOC_COMMIT_FAILED,      ///< One or more objects could not be committed to LMDB
    DOC_SAVE_FAILED,        ///< DB save failed
    DOC_OBJECT_IMPORT_FAILED, ///< File import into MFS failed

    // ── Workflow ──────────────────────────────────────────────
    WF_ALREADY_ACTIVE,      ///< An active workflow is already running on this entity
    WF_NOT_FOUND,           ///< No workflow found with this ID
    WF_NOT_ACTIVE,          ///< Workflow is not in active state
    WF_STEP_NOT_FOUND,      ///< Step not found in this workflow
    WF_STEP_BLOCKED,        ///< Step predecessors are not yet complete
    WF_STEP_IS_SYSTEM,      ///< Step is a system step — cannot be modified or deleted
    WF_COMMENT_REQUIRED,    ///< A comment is required to fire this step
    WF_DOCUMENT_REQUIRED,   ///< A document must be attached before firing this step

    // ── Entity state ─────────────────────────────────────────
    ENTITY_RELEASED,        ///< Entity is released — no edits or child creation allowed
    ENTITY_WF_COMPLETE,     ///< Workflow is completed — entity is permanently locked
    ENTITY_NOT_FOUND,       ///< Entity not found in database

    // ── Object / key file ────────────────────────────────────
    OBJ_ALREADY_EXISTS,     ///< An object with this ID already exists in this document
    OBJ_NOT_FOUND,          ///< Object not found
    KEY_FILE_WRITE_FAILED,  ///< Key file (_SCHLUESSEL.txt) could not be written

    // ── General ──────────────────────────────────────────────
    INVALID_ARGUMENT,       ///< A required argument is missing or invalid
    DB_ERROR,               ///< Unexpected database error
    IO_ERROR,               ///< File system error
    NOT_IMPLEMENTED,        ///< Feature is not yet implemented
};

/// Human-readable German error message for each result code.
/// Used by CLI to display context-appropriate messages.
inline std::string opResultMessage(OperationResult r) {
    switch (r) {
    case OperationResult::OPERATION_ACK:
        return "Operation erfolgreich ausgeführt.";
    case OperationResult::DOC_REV_NOT_IN_WORK:
        return "Die aktive Revision ist nicht 'in_work'. Checkout, Bearbeiten und Objekte "
               "hinzufügen sind nur im Zustand in_work möglich. Starten Sie einen Workflow, "
               "um die Revision zu entsperren, oder legen Sie eine neue Revision an.";
    case OperationResult::DOC_REV_IS_IN_WORK:
        return "Die aktive Revision ist noch 'in_work'. Eine neue Revision kann erst angelegt "
               "werden, wenn der Zustand durch einen Workflow geändert wurde.";
    case OperationResult::DOC_REV_IS_CLOSED:
        return "Die Revision ist 'closed' — kein weiterer Zustandswechsel möglich. "
               "Closed ist ein Terminalzustand.";
    case OperationResult::DOC_REV_TRANSITION_NOT_ALLOWED:
        return "Dieser Zustandswechsel ist im aktuellen Modus nicht erlaubt. "
               "Prüfen Sie die erlaubte Übergangsmatrix oder aktivieren Sie den Admin-Modus.";
    case OperationResult::DOC_REVISION_NOT_FOUND:
        return "Keine aktive Revision für dieses Dokument gefunden.";
    case OperationResult::DOC_CHECKOUT_OPEN:
        return "Es ist bereits ein Checkout offen. Bitte erst einchecken oder Checkout schließen.";
    case OperationResult::DOC_NO_CHECKOUT:
        return "Kein offener Checkout. Checkin und Revert sind nur nach einem Checkout möglich.";
    case OperationResult::DOC_NO_PARENT_REV:
        return "Keine Vorgänger-Revision vorhanden — Revert nicht möglich.";
    case OperationResult::DOC_FILE_NOT_FOUND:
        return "Datei nicht gefunden. Bitte Pfad prüfen.";
    case OperationResult::DOC_COMMIT_FAILED:
        return "Mindestens ein Objekt konnte nicht in den Langzeitspeicher (LMDB) übertragen "
               "werden. Prüfen Sie den LMDB-Speicherplatz und die MFS-Dateipfade.";
    case OperationResult::DOC_SAVE_FAILED:
        return "Datenbank-Speicherung fehlgeschlagen.";
    case OperationResult::DOC_OBJECT_IMPORT_FAILED:
        return "Datei konnte nicht in die MFS-Ablage importiert werden.";
    case OperationResult::WF_ALREADY_ACTIVE:
        return "Es läuft bereits ein aktiver Workflow auf dieser Entität. "
               "Bitte erst den bestehenden Workflow abschließen oder abbrechen.";
    case OperationResult::WF_NOT_FOUND:
        return "Workflow nicht gefunden.";
    case OperationResult::WF_NOT_ACTIVE:
        return "Workflow ist nicht aktiv (möglicherweise abgeschlossen oder abgebrochen).";
    case OperationResult::WF_STEP_NOT_FOUND:
        return "Schritt nicht gefunden.";
    case OperationResult::WF_STEP_BLOCKED:
        return "Schritt kann nicht ausgeführt werden — Vorgänger-Schritte sind noch nicht abgeschlossen.";
    case OperationResult::WF_STEP_IS_SYSTEM:
        return "Dies ist ein System-Schritt ('Create DB Objects') und kann nicht manuell "
               "bearbeitet oder gelöscht werden.";
    case OperationResult::WF_COMMENT_REQUIRED:
        return "Für diesen Schritt ist ein Kommentar erforderlich.";
    case OperationResult::WF_DOCUMENT_REQUIRED:
        return "Für diesen Schritt muss ein Dokument angehängt sein.";
    case OperationResult::ENTITY_RELEASED:
        return "Die Entität ist freigegeben (released) — Bearbeiten und Anlegen von "
               "untergeordneten Objekten ist nicht mehr möglich.";
    case OperationResult::ENTITY_WF_COMPLETE:
        return "Der Workflow dieser Entität ist abgeschlossen — die Entität ist dauerhaft gesperrt.";
    case OperationResult::ENTITY_NOT_FOUND:
        return "Entität nicht gefunden.";
    case OperationResult::OBJ_ALREADY_EXISTS:
        return "Ein Objekt mit dieser ID existiert bereits in diesem Dokument.";
    case OperationResult::OBJ_NOT_FOUND:
        return "Objekt nicht gefunden.";
    case OperationResult::KEY_FILE_WRITE_FAILED:
        return "Schlüsseldatei (_SCHLUESSEL.txt) konnte nicht geschrieben werden.";
    case OperationResult::INVALID_ARGUMENT:
        return "Ungültiges oder fehlendes Argument.";
    case OperationResult::DB_ERROR:
        return "Datenbankfehler. Bitte Log prüfen.";
    case OperationResult::IO_ERROR:
        return "Dateisystemfehler. Bitte Pfad und Berechtigungen prüfen.";
    case OperationResult::NOT_IMPLEMENTED:
        return "Funktion noch nicht implementiert.";
    default:
        return "Unbekannter Fehler.";
    }
}

/// Convenience: true when the result represents success.
inline bool opOk(OperationResult r) {
    return r == OperationResult::OPERATION_ACK;
}

} // namespace Rosenholz
