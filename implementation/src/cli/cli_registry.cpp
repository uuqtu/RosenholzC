// ============================================================
// cli_registry.cpp  —  Command Registry
//
// ONE PLACE to add/remove/rename commands.
// All dispatch, lo(), and tab-completion read from here.
// ============================================================
#include "cli_registry.h"
#include "cli_common.h"
#include "../model/NavigationContext.h"
#include <algorithm>

namespace CLI {

using ET = Rosenholz::EntityType;

// ── Forward declarations of all handlers ─────────────────────────────────────
// These are implemented in cli_nav.cpp and the other cli_*.cpp files.

void cmdContextual(const std::string& cmd, const std::vector<std::string>& args);
void cmdF16(const std::vector<std::string>& a);
void cmdF22(const std::vector<std::string>& a);
void cmdF18(const std::vector<std::string>& a);
void cmdAkt(const std::vector<std::string>& a);
void cmdF77(const std::vector<std::string>& a);
void cmdPer(const std::vector<std::string>& a);
void cmdDe(const std::vector<std::string>& a);
void cmdTasks(const std::vector<std::string>& a);
void cmdSearch(const std::string& q);
void cmdStatus();
void cmdBackup();
void cmdMfs(const std::vector<std::string>& a);
void cmdCd(const std::vector<std::string>& a);
void cmdLs(const std::vector<std::string>& a);
void cmdLo(const std::vector<std::string>& a);
void cmdGo(const std::vector<std::string>& a);
void cmdHist();
void cmdCal();

// ── Thin wrappers that call cmdContextual ────────────────────────────────────
// Contextual commands route through cmdContextual so the navigation context
// is always used. Global dash-commands call the model directly via cmdF22 etc.

static auto ctx(const std::string& name) {
    return [name](const std::vector<std::string>& a) {
        cmdContextual(name, a);
    };
}
static auto global(void(*fn)(const std::vector<std::string>&)) {
    return [fn](const std::vector<std::string>& a) { fn(a); };
}

// ── The registry ──────────────────────────────────────────────────────────────
//
// Format:
//   { name, dashName, requiredContext, loHint, {subHints...}, handler }
//
// requiredContext = ET::NONE  →  available everywhere
// requiredContext = ET::F16   →  only shown in lo() when in F16 context
//                               (but still callable from anywhere)
//
// ADD A NEW COMMAND: append one struct here. Done.
//
const std::vector<CliCommand>& registry() {
    using namespace Rosenholz;
    static const std::vector<CliCommand> kCommands = {

    // ── Navigation ──────────────────────────────────────────────────────────
    { "cd",    nullptr,  ET::NONE, "cd <ID>   In Entitaet navigieren", {}, ctx("cd") },
    { "ls",    nullptr,  ET::NONE, "ls        Inhalt der aktuellen Ebene", {"-rev"}, ctx("ls") },
    { "lo",    nullptr,  ET::NONE, "lo        Kontext-Optionen anzeigen", {}, ctx("lo") },
    { "..",    nullptr,  ET::NONE, "..        Eine Ebene zurueck", {}, ctx("..") },

    // ── F16 ─────────────────────────────────────────────────────────────────
    { "f16",   "-f16",   ET::NONE, "f16       F16-Aktionen",
      {"-n","-e","-v","-o","-so","-s","-arc","-note"},   ctx("f16") },

    // ── F22 ─────────────────────────────────────────────────────────────────
    { "f22",   "-f22",   ET::NONE, "f22 -o/-so  F22 listen/suchen+navigieren  f22 -n  neu",
      {"-n","-e","-v","-o","-so","-s","-ind","-note"},   ctx("f22") },

    // ── F18 ─────────────────────────────────────────────────────────────────
    { "f18",   "-f18",   ET::NONE, "f18 -o/-so  F18 listen/suchen+navigieren  f18 -n  neu",
      {"-n","-e","-v","-o","-so","-s","-stp","-note"},  ctx("f18") },

    // ── AKT ─────────────────────────────────────────────────────────────────
    { "akt",   "-akt",   ET::NONE, "akt -o/-so  Akten  akt -oo/-soo  Objekte (im Kontext) akt -n  neu",
      {"-n","-v","-o","-so","-oo","-soo","-obj","-url","-co","-ci","-rv","-note"}, ctx("akt") },

    // ── F77 ─────────────────────────────────────────────────────────────────
    { "f77",   "-f77",   ET::NONE, "f77 -o/-so  Workflows listen/suchen  f77 -s  starten  f77 -d  anzeigen",
      {"-s","-d","-o","-so","-tpl"},                ctx("f77") },

    // ── Revision (AKT only) ──────────────────────────────────────────────────
    { "rev",   nullptr,  ET::AKT,  "rev       Neue Revision anlegen",
      {},                                           ctx("rev") },

    // ── Kommunikation ────────────────────────────────────────────────────────
    { "kom",   "-kom",   ET::NONE, "kom -o/-so  KOM listen/suchen (im Kontext: nur zugehörige)",
      {"-n","-l","-o","-so"},
      [](const std::vector<std::string>& a) { cmdKom(a); } },

    // ── Notizen ─────────────────────────────────────────────────────────────
    { "note",  "-note",  ET::NONE, "note <Text>   Schnellnotiz (im Kontext: auf aktuelle Entitaet)",
      {},                                           ctx("note") },

    // ── Aufgaben ─────────────────────────────────────────────────────────────
    { "tsk",   "-tasks", ET::NONE, "tsk       Offene Aufgaben  tsk -a  alle  tsk -so <q>  suchen",
      {"-a","-o","-so"},
      [](const std::vector<std::string>& a) { cmdTasks(a); } },

    // ── Suche ────────────────────────────────────────────────────────────────
    { "srch",  "-search",ET::NONE, "srch <q>  Globale Suche",
      {},
      [](const std::vector<std::string>& a) {
          std::string q;
          for (auto& s : a) { if (!q.empty()) q += " "; q += s; }
          if (q.empty()) q = readLine("  Suche: ");
          cmdSearch(q);
      } },

    // ── Personen ─────────────────────────────────────────────────────────────
    { "per",   "-per",   ET::NONE, "per -s <q>  Personen suchen  per -n  neu",
      {"-n","-s"},
      global(cmdPer) },

    // ── Diensteinheiten ──────────────────────────────────────────────────────
    { "de",    "-de",    ET::NONE, "de        Diensteinheiten-Browser",
      {},
      global(cmdDe) },

    // ── MFS ─────────────────────────────────────────────────────────────────
    { "mfs",   "-mfs",   ET::NONE, "mfs       MFS neu aufbauen  mfs -sync  aendrungen",
      {"-sync"},
      global(cmdMfs) },

    // ── System ───────────────────────────────────────────────────────────────
    { "sts",   "-status",ET::NONE, "sts       Datenbankzaehler",
      {},
      [](const std::vector<std::string>&) { cmdStatus(); } },

    { "bak",   "-backup",ET::NONE, "bak       Backup starten",
      {},
      [](const std::vector<std::string>&) { cmdBackup(); } },

    { "wch",   "-watch", ET::NONE, "wch [N]   Watch-Polling (N=Sekunden, Standard=30)",
      {},
      ctx("wch") },

    { "tree",  "-tree",  ET::NONE, "tree      Hierarchiebaum ab aktuellem F16",
      {},
      ctx("tree") },

    { "cal",   "-cal",   ET::NONE, "cal       Kalenderansicht",
      {},
      ctx("cal") },

    { "his",   "-hist",  ET::NONE, "his       Verlauf zuletzt geoeffneter Entitaeten",
      {},
      [](const std::vector<std::string>&) { cmdHist(); } },

    { "go",    "-go",    ET::NONE, "go <ref>  Direkt oeffnen (ID / Seq#)",
      {},
      global(cmdGo) },

    }; // end kCommands
    return kCommands;
}

// ── dispatch ─────────────────────────────────────────────────────────────────

bool dispatch(const std::string& cmd, const std::vector<std::string>& args) {
    // Normalise: if cmd starts with '-', try both "cmd" and without '-':
    std::string plain  = (cmd.size() > 1 && cmd[0] == '-') ? cmd.substr(1) : cmd;
    bool hasDash = (cmd[0] == '-');

    for (auto& c : registry()) {
        bool matchPlain = (plain == c.name);
        bool matchDash  = (c.dashName && cmd == c.dashName);

        if (!matchPlain && !matchDash) continue;

        // dash form → always global (use underlying cmdF22 etc. directly)
        // plain form → always contextual (routes via cmdContextual)
        // Both point to c.handler — which for contextual commands calls
        // cmdContextual(name, args), and for global commands calls cmdFxx directly.
        // The distinction is handled inside cmdContextual itself.
        if (hasDash && matchDash) {
            // Global invocation: bypass context routing
            // For entity-type commands, call the underlying global function:
            static const struct { const char* name; void(*fn)(const std::vector<std::string>&); }
            kGlobals[] = {
                {"-f16", cmdF16}, {"-f22", cmdF22}, {"-f18", cmdF18},
                {"-akt", cmdAkt}, {"-f77", cmdF77}, {"-per", cmdPer},
                {"-de", cmdDe},   {"-mfs", cmdMfs}, {"-tasks", cmdTasks},
                {"-search", nullptr}, {"-status", nullptr}, {"-backup", nullptr},
                {"-watch", nullptr},  {"-hist", nullptr},   {"-tree", nullptr},
                {"-cal", nullptr},    {"-go", cmdGo},
                {nullptr, nullptr}
            };
            for (int i = 0; kGlobals[i].name; ++i) {
                if (cmd == kGlobals[i].name && kGlobals[i].fn) {
                    kGlobals[i].fn(args);
                    return true;
                }
            }
        }
        // Fall through to the registered handler for all other cases:
        c.handler(args);
        return true;
    }
    return false; // unknown
}

// ── printContextHelp (lo) ────────────────────────────────────────────────────

void printContextHelp() {
    auto& nav = Rosenholz::NavigationStack::instance();
    auto  cur = nav.current();
    ET    ctx = cur.valid() ? cur.type : ET::NONE;

    std::cout << "\n";
    if (cur.valid()) {
        std::cout << Color::bold("  " + std::string(Rosenholz::entityTypeLabel(cur.type))
                                 + " " + cur.id + " — " + cur.displayName) << "\n"
                  << "  " << std::string(54, '-') << "\n";
    } else {
        std::cout << Color::bold("  Rosenholz PM v7 — Befehle") << "\n"
                  << "  " << std::string(54, '-') << "\n";
    }

    std::cout << "  Navigation:\n"
              << "    cd <ID>     In Entitaet navigieren\n"
              << "    ls          Inhalt listen\n"
              << "    ls -rev     (AKT) Alle Revisionen\n"
              << "    ..          Eine Ebene zurueck\n"
              << "  Befehle:\n";

    for (auto& c : registry()) {
        if (!c.loHint || c.loHint[0] == '\0') continue;
        // Skip navigation (already shown above):
        if (std::string(c.name) == "cd" || std::string(c.name) == "ls" ||
            std::string(c.name) == "lo" || std::string(c.name) == "..") continue;
        // Context filter: only show if context matches or command is universal:
        if (c.context != ET::NONE && c.context != ctx) continue;
        std::cout << "    " << c.loHint << "\n";
    }
    std::cout << "  System:\n"
              << "    sts  bak  wch [N]  tree  cal  his  go <ref>\n"
              << "  Dash-Form = global (kein Kontext): -f22 -n\n\n";
}

// ── completions ──────────────────────────────────────────────────────────────

std::vector<std::string> completions(const std::string& prefix,
                                     const std::string& prev) {
    std::vector<std::string> result;

    // If previous token was "cd": complete with context children
    if (prev == "cd") {
        try {
            auto children = getContextChildren();
            for (auto& [id, title] : children)
                result.push_back(id);
        } catch (...) {}
        return result;
    }

    // Sub-command completion: if prev is a known command, offer its subHints
    for (auto& c : registry()) {
        if (prev == c.name || (c.dashName && prev == c.dashName)) {
            for (auto* s : c.subHints)
                if (prefix.empty() || std::string(s).find(prefix) == 0)
                    result.push_back(s);
            return result;
        }
    }

    // Top-level command completion:
    for (auto& c : registry()) {
        // Plain name:
        if (std::string(c.name).find(prefix) == 0)
            result.push_back(c.name);
        // Dash form:
        if (c.dashName && std::string(c.dashName).find(prefix) == 0)
            result.push_back(c.dashName);
    }
    // Always offer exit/help:
    for (auto* s : {"exit", "quit", "-h", "--help"})
        if (std::string(s).find(prefix) == 0)
            result.push_back(s);

    return result;
}

} // namespace CLI
