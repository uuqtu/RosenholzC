// ============================================================
// cli_f99.cpp  —  F99 Notizen CLI
//
// Commands:
//   cmdF99({"-s", q})       global search
//   cmdF99({"-so", q})      F99 Manager (search + edit/delete)
//   cmdF99({"-o", id})      open specific note
//   cmdF99({text...})       create note (prompts for entity if no context)
// ============================================================
#include "cli_common.h"
#include "../model/Note.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/f18/F18Operation.h"
#include "../model/akt/Folder.h"
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── Print search results ─────────────────────────────────────────────────────
static void printF99Results(const std::vector<Note::SearchResult>& hits) {
    if (hits.empty()) { std::cout << "  (keine F99-Notizen gefunden)\n"; return; }
    std::cout << "\n  " << std::left << std::setw(4) << "#"
              << std::setw(22) << "ID"
              << std::setw(16) << "DATUM"
              << std::setw(30) << "PFAD / TITEL"
              << "INHALT\n"
              << "  " << std::string(88,'-') << "\n";
    int n = 1;
    for (auto& r : hits) {
        std::string loc = r.entityPath;
        if (!r.entityTitle.empty())
            loc += (loc.empty() ? "" : " ") + ("\"" + r.entityTitle.substr(0,16) + "\"");
        std::cout << "  " << std::setw(4) << n++
                  << std::setw(22) << r.note->noteId
                  << std::setw(16) << r.note->createdAt.substr(0,16)
                  << std::setw(30) << loc.substr(0,28)
                  << r.note->body.substr(0, 36)
                  << (r.note->body.size() > 36 ? "..." : "") << "\n";
    }
    std::cout << "\n";
}

// ── F99 Manager — list, select, edit, delete ─────────────────────────────────
static void f99Manager(const std::vector<Note::SearchResult>& hits) {
    if (hits.empty()) { std::cout << "  (keine Treffer)\n"; return; }
    printF99Results(hits);

    int pick = readInt("  Notiz öffnen [0=Abbrechen]", 0, (int)hits.size());
    if (pick < 1) return;
    auto& n = hits[pick-1].note;

    while (true) {
        std::cout << "\n  F99: " << Color::bold(n->noteId) << "\n"
                  << "  Erstellt : " << n->createdAt.substr(0,16) << "\n"
                  << "  Pfad     : " << hits[pick-1].entityPath
                  << " \"" << hits[pick-1].entityTitle.substr(0,30) << "\"\n"
                  << "  Inhalt   : " << n->body << "\n\n"
                  << "  1. Inhalt ändern\n"
                  << "  2. Löschen\n"
                  << "  0. Zurück\n";
        int ch = readInt("  Wahl", 0, 2);
        if (ch == 0) break;
        if (ch == 1) {
            std::string newBody = readLine("  Neuer Inhalt: ");
            if (newBody.empty()) continue;
            n->body = newBody;
            if (opOk(n->update()))
                std::cout << "  >> Gespeichert.\n";
            else
                printErr("Fehler beim Speichern");
        } else if (ch == 2) {
            if (!yesno("  Wirklich löschen?")) continue;
            if (opOk(n->remove())) {
                std::cout << "  >> F99 " << n->noteId << " gelöscht.\n";
                break;
            } else {
                printErr("Fehler beim Löschen");
            }
        }
    }
}

// ── cmdF99 ───────────────────────────────────────────────────────────────────

void cmdF99(const std::vector<std::string>& args) {

    // -s <q>: global search, display results
    if (!args.empty() && args[0] == "-s") {
        std::string q;
        for (std::size_t i=1; i<args.size(); i++) { if(!q.empty()) q+=" "; q+=args[i]; }
        if (q.empty()) q = readLine("  Suche: ");
        auto hits = Note::search(q);
        printF99Results(hits);
        return;
    }

    // -so [q]: global search + F99 Manager
    if (!args.empty() && args[0] == "-so") {
        std::string q;
        for (std::size_t i=1; i<args.size(); i++) { if(!q.empty()) q+=" "; q+=args[i]; }
        if (q.empty()) q = readLine("  Suche (leer=alle): ");
        auto hits = q.empty() ? Note::search("") : Note::search(q);
        // If query empty and search returns empty (LIKE '%' won't work), load differently:
        if (q.empty() && hits.empty()) {
            // Load all recent notes:
            auto* d = DatabasePool::instance().get("f99");
            if (d) {
                auto rows = d->query(
                    "SELECT * FROM f99_entries ORDER BY created_at DESC LIMIT 50;", {});
                for (auto& r : rows) {
                    auto n = std::make_shared<Note>(); n->fromRow(r);
                    Note::SearchResult sr;
                    sr.note = n;
                    // Resolve path via search with empty entity filter:
                    hits.push_back({n,
                        [&]() -> std::string {
                            if (n->entityType == "f16") { auto p = F16::loadById(n->entityId); return p ? p->title : n->entityId; }
                            if (n->entityType == "f22") { auto t = F22::loadById(n->entityId); return t ? t->title : n->entityId; }
                            return n->entityId;
                        }(),
                        n->entityType + ":" + n->entityId});
                }
            }
        }
        f99Manager(hits);
        return;
    }

    // -o <id>: open specific note
    if (!args.empty() && args[0] == "-o") {
        std::string nid = (args.size() > 1 && !args[1].empty()) ? args[1] : readLine("  F99-ID: ");
        auto n = Note::loadById(nid);
        if (!n) { printErr("F99 nicht gefunden: " + nid); return; }
        // Build search result for manager:
        Note::SearchResult sr;
        sr.note = n;
        sr.entityTitle = n->entityId;
        sr.entityPath  = n->entityType + ":" + n->entityId;
        f99Manager({sr});
        return;
    }

    // No flag: list all recent F99 notes (global overview)
    if (args.empty()) {
        auto* d = DatabasePool::instance().get("f99");
        if (!d) { printErr("F99-Datenbank nicht verfügbar"); return; }
        auto rows = d->query(
            "SELECT * FROM f99_entries ORDER BY created_at DESC LIMIT 30;", {});
        if (rows.empty()) { std::cout << "  (keine F99-Notizen)\n"; return; }
        std::vector<Note::SearchResult> all;
        for (auto& r : rows) {
            auto n = std::make_shared<Note>(); n->fromRow(r);
            std::string title;
            if (n->entityType == "f16") { auto p = F16::loadById(n->entityId); if(p) title=p->title; }
            else if (n->entityType == "f22") { auto t = F22::loadById(n->entityId); if(t) title=t->title; }
            all.push_back({n, title, n->entityType + ":" + n->entityId});
        }
        printF99Results(all);
        return;
    }

    // Otherwise: args is the note text — but no entity to attach to
    // (global create makes no sense without entity — show hint)
    std::cout << "  Hinweis: f99 <Text>  im Kontext verwenden (cd <ID> zuerst)\n"
              << "  Oder:    f99 -s <q>  suchen  |  f99 -so  Manager\n";
}

} // namespace CLI
