// ============================================================
// cli_registry.cpp  —  Command Registry (single source of truth)
//
// ADD A COMMAND: append one CliCommand entry to kCommands[].
// dispatch(), lo(), Tab-completion all read from here.
// No other files need editing.
//
// RULE:  dash form  ("-f22 -n") → globalHandler   (no nav context used)
//        plain form ("f22 -n")  → contextHandler  (nav context used)
// ============================================================
#include "cli_registry.h"
#include "cli_common.h"
#include "../model/f18/F18Operation.h"
#include "../model/NavigationContext.h"
#include <algorithm>

namespace CLI {

using ET = Rosenholz::EntityType;

// ── Forward declarations ─────────────────────────────────────────────────────
// All implementations live in cli_nav.cpp or the specific cli_*.cpp files.

void cmdCd(const std::vector<std::string>&);
void cmdLs(const std::vector<std::string>&);
void cmdContextual(const std::string& name, const std::vector<std::string>&);
void cmdF16(const std::vector<std::string>&);
void cmdF22(const std::vector<std::string>&);
void cmdF18(const std::vector<std::string>&);
void cmdF18s(const std::vector<std::string>&, std::shared_ptr<Rosenholz::F18Operation>);
void cmdAkt(const std::vector<std::string>&);
void cmdF77(const std::vector<std::string>&);
void cmdPer(const std::vector<std::string>&);
void cmdDe(const std::vector<std::string>&);
void cmdTasks(const std::vector<std::string>&);
void cmdSearch(const std::string&);
void cmdStatus();
void cmdBackup();
void cmdMfs(const std::vector<std::string>&);
void cmdGo(const std::vector<std::string>&);
void cmdHist();
void cmdKom(const std::vector<std::string>&);
void cmdF99(const std::vector<std::string>&);

// ── Handler factories ────────────────────────────────────────────────────────

// ctx: context-aware handler — routes to cmdContextual(name, args)
static Handler ctx(const std::string& name) {
    return [name](const std::vector<std::string>& a) {
        cmdContextual(name, a);
    };
}

// global: wraps a global function (no context used)
static Handler global(void(*fn)(const std::vector<std::string>&)) {
    return [fn](const std::vector<std::string>& a) { fn(a); };
}

// ── currentCtxMask ────────────────────────────────────────────────────────────
CtxMask currentCtxMask() {
    auto cur = Rosenholz::NavigationStack::instance().current();
    if (!cur.valid()) return CTX_NONE;
    switch (cur.type) {
        case Rosenholz::EntityType::F16: return CTX_F16;
        case Rosenholz::EntityType::F22: return CTX_F22;
        case Rosenholz::EntityType::F18: return CTX_F18;
        case Rosenholz::EntityType::AKT: return CTX_AKT;
        default: return CTX_NONE;
    }
}

// ── Registry table ───────────────────────────────────────────────────────────
//
// Columns: name | dashName | validIn | loHint | subHints | contextHandler | globalHandler
//
// validIn:         CTX bitmask — plain form only works in these contexts
//                  CTX_NONE = works everywhere (global or nav commands)
// contextHandler:  called for plain "f22 -n" (context-aware, checked against validIn)
// globalHandler:   called for dash "-f22 -n" (always global, no context check)
//                  nullptr → contextHandler used for both
//
const std::vector<CliCommand>& registry() {
    using namespace Rosenholz;
    static const std::vector<CliCommand> kCommands = {

    // ── Navigation (plain only, no dash form, no context filter) ────────────
    { "cd",  nullptr, CTX_NONE, "cd <ID>    In Entitaet navigieren",
      {},     global(cmdCd),     nullptr },

    { "ls",  nullptr, CTX_NONE, "ls         Inhalt listen  ls -rev  alle Revisionen",
      {"-rev"}, global(cmdLs),   nullptr },

    { "lo",  nullptr, CTX_NONE, "lo         Kontext-Optionen anzeigen",
      {},     [](const std::vector<std::string>&) { printContextHelp(); }, nullptr },

    // ── Entity commands — contextHandler=contextual, globalHandler=global ───
    { "f16", "-f16",  CTX_NONE,
      "f16 -o/-so  listen/suchen  f16 -n  neu  f16 -e  bearbeiten",
      {"-n","-e","-v","-o","-so","-s","-arc","-note"},
      ctx("f16"),   global(cmdF16) },

    { "f22", "-f22",  CTX_NONE,
      "f22 -n  neue F22 (im Kontext direkt verknüpft)  f22 -o/-so  listen/suchen",
      {"-n","-e","-v","-o","-so","-s","-ind","-note"},
      ctx("f22"),   global(cmdF22) },

    { "f18", "-f18",  CTX_NONE,
      "f18 -n  neuer F18 (im Kontext)  f18 -o/-so  listen/suchen  f18 -stp  Schritte",
      {"-n","-e","-v","-o","-so","-s","-stp","-note"},
      ctx("f18"),   global(cmdF18) },

    { "akt", "-akt",  CTX_NONE,
      "akt -o/-so  Akten  akt -oo/-soo  Objekte (kontextuell)  akt -n  neu",
      {"-n","-v","-o","-so","-oo","-soo","-obj","-url","-co","-ci","-rv","-note"},
      ctx("akt"),   global(cmdAkt) },

    { "f77", "-f77",  CTX_NONE,
      "f77 -s  Workflow starten  f77 -d  anzeigen  f77 -o/-so  listen/suchen",
      {"-s","-d","-o","-so","-tpl"},
      ctx("f77"),   global(cmdF77) },

    // ── F18S: F18-Schritte ────────────────────────────────────────────────────
    { "f18s", "-f18s", CTX_F18,
      "f18s -n  Neuer Schritt  f18s -o/-so  listen/suchen  f18s -e <n>  bearbeiten",
      {"-n","-e","-o","-so","-s"},
      [](const std::vector<std::string>& a) {
          auto& nav = Rosenholz::NavigationStack::instance();
          auto cur = nav.current();
          std::shared_ptr<Rosenholz::F18Operation> v;
          if (cur.valid() && cur.type == Rosenholz::EntityType::F18)
              v = Rosenholz::F18Operation::loadById(cur.id);
          cmdF18s(a, v);
      },
      [](const std::vector<std::string>& a) { cmdF18s(a, nullptr); }
    },

    // ── AKT-only ─────────────────────────────────────────────────────────────
    { "rev", nullptr,  CTX_AKT, "rev        Neue Revision anlegen",
      {},     ctx("rev"),   nullptr },

    // ── Communications ────────────────────────────────────────────────────────
    { "kom", "-kom",  CTX_NONE,
      "kom -o/-so  KOM listen/suchen (im Kontext: nur zugehörige)  kom -n  neu",
      {"-n","-l","-o","-so"},
      ctx("kom"),   global(cmdKom) },

    // ── F99 Notizen ────────────────────────────────────────────────────────────
    { "f99",  "-f99",  CTX_NONE,
      "f99 <Text>  F99-Notiz (im Kontext)  -s  suchen  -so  Manager  -o <id>  öffnen",
      {"-s","-so","-o"},
      ctx("f99"),  global(cmdF99) },

    // ── F77 Tasks ─────────────────────────────────────────────────────────────
    { "tsk",  "-tasks", CTX_NONE,
      "tsk  offene Aufgaben  tsk -a  alle  tsk -so <q>  suchen",
      {"-a","-o","-so"},
      global(cmdTasks), global(cmdTasks) },

    // ── People / Org ──────────────────────────────────────────────────────────
    { "per",  "-per",  CTX_NONE,
      "per -s <q>  Personen suchen  per -n  neu",
      {"-n","-s"},
      global(cmdPer), global(cmdPer) },

    { "de",   "-de",   CTX_NONE,
      "de  Diensteinheiten-Browser",
      {},     global(cmdDe),  global(cmdDe) },

    // ── Search ────────────────────────────────────────────────────────────────
    { "srch", "-search", CTX_NONE,
      "srch <q>  Globale Suche",
      {},
      [](const std::vector<std::string>& a) {
          std::string q;
          for (auto& s : a) { if (!q.empty()) q += " "; q += s; }
          if (q.empty()) q = CLI::readLine("  Suche: ");
          cmdSearch(q);
      },
      nullptr },

    // ── MFS ──────────────────────────────────────────────────────────────────
    { "mfs",  "-mfs",  CTX_NONE,
      "mfs  MFS aktuell neu / -mfs  vollständiger Rebuild",
      {"-sync"},
      ctx("mfs"), global(cmdMfs) },

    // ── System ────────────────────────────────────────────────────────────────
    { "sts",  "-status",  CTX_NONE, "sts  Datenbankzähler",
      {},
      [](const std::vector<std::string>&) { cmdStatus(); },
      nullptr },

    { "bak",  "-backup",  CTX_NONE, "bak  Backup starten",
      {},
      [](const std::vector<std::string>&) { cmdBackup(); },
      nullptr },

    { "wch",  "-watch",   CTX_NONE, "wch [N]  Watch-Polling (N=Sekunden, Standard=30)",
      {},
      [](const std::vector<std::string>& a) {
          int secs = 30;
          if (!a.empty()) { try { secs = std::stoi(a[0]); } catch (...) {} }
          WatchPoller::run([](const std::string& m) {
              std::cout << "  " << nowIso().substr(11,8) << "  " << m << "\n";
              std::cout.flush();
          }, secs);
      }, nullptr },

    { "tree", "-tree",    CTX_NONE, "tree  Hierarchiebaum ab aktuellem F16",
      {},
      [](const std::vector<std::string>& a) {
          auto& nav = Rosenholz::NavigationStack::instance();
          auto  cur = nav.current();
          std::string rootId;
          if (!a.empty()) rootId = a[0];
          else if (cur.valid() && cur.type == Rosenholz::EntityType::F16) rootId = cur.id;
          if (!rootId.empty()) {
              auto t = TreeBuilder::buildF16Tree(rootId);
              std::cout << "\n" << TreeBuilder::format(t) << "\n";
          } else {
              auto all = TreeBuilder::buildAllF16();
              for (auto& t2 : all) std::cout << TreeBuilder::format(t2);
          }
      }, nullptr },

    { "cal",  "-cal",     CTX_NONE, "cal  Kalenderansicht",
      {},
      [](const std::vector<std::string>&) {
          std::cout << "\n  -- KALENDER --\n";
          auto projs = Rosenholz::F16::loadWithDates();
          if (projs.empty()) { std::cout << "  (keine Eintraege)\n"; return; }
          for (auto& p : projs)
              std::cout << "  F16 " << std::left << std::setw(26)
                        << p->regNumber.toString().substr(0,24)
                        << "  " << std::setw(28) << p->title.substr(0,26)
                        << "  Start:" << (p->startDatePlanned.empty() ? "-" : p->startDatePlanned.substr(0,10))
                        << "  Ende:"  << (p->endDatePlanned.empty() ? "-" : p->endDatePlanned.substr(0,10)) << "\n";
      }, nullptr },

    { "his",  "-hist",    CTX_NONE, "his  Verlauf zuletzt geöffneter Entitäten",
      {},
      [](const std::vector<std::string>&) { cmdHist(); },
      nullptr },

    { "go",   "-go",      CTX_NONE, "go <ref>  Direkt öffnen (ID / Seq#)",
      {},
      global(cmdGo), global(cmdGo) },

    { "log",  "-log",     CTX_NONE, "log <level>  Verbosität: debug|info|warn|error",
      {"debug","info","warn","error"},
      [](const std::vector<std::string>& a) {
          if (a.empty()) return;
          auto& logger = Rosenholz::Logger::instance();
          if      (a[0]=="debug") logger.setLevel(Rosenholz::LogLevel::DEBUG);
          else if (a[0]=="info")  logger.setLevel(Rosenholz::LogLevel::INFO);
          else if (a[0]=="warn")  logger.setLevel(Rosenholz::LogLevel::WARN);
          else if (a[0]=="error") logger.setLevel(Rosenholz::LogLevel::ERR);
          std::cout << "  >> Log-Level: " << a[0] << "\n";
      },
      nullptr },

    }; // end kCommands
    return kCommands;
}

// ── dispatch ─────────────────────────────────────────────────────────────────
// Single, clean routing rule:
//   dash form  ("-f22") → c.globalHandler  (or contextHandler if no global)
//   plain form ("f22")  → c.contextHandler

bool dispatch(const std::string& cmd, const std::vector<std::string>& args) {
    if (cmd.empty()) return false;
    bool hasDash = (cmd[0] == '-');

    for (auto& c : registry()) {
        bool matchPlain = (!hasDash && cmd == c.name);
        bool matchDash  = (hasDash && c.dashName && cmd == c.dashName);
        if (!matchPlain && !matchDash) continue;

        if (hasDash) {
            // Dash form: always global, no context check
            if (c.globalHandler) c.globalHandler(args);
            else                 c.contextHandler(args);
        } else {
            // Plain form: check validIn mask
            if (c.validIn != CTX_NONE) {
                CtxMask cur = currentCtxMask();
                if (!(cur & c.validIn)) {
                    // Command is known but not valid in current context:
                    return false;  // caller prints "unbekannter Befehl"
                }
            }
            c.contextHandler(args);
        }
        return true;
    }
    return false;
}

// ── printContextHelp (lo) ─────────────────────────────────────────────────────

void printContextHelp() {
    auto& nav = Rosenholz::NavigationStack::instance();
    auto  cur = nav.current();

    std::cout << "\n";
    if (!cur.valid()) {
        // ── Global (top level): show concise command reference ──────────────
        std::cout << Color::bold("  Rosenholz PM v9  |  rh <BEFEHL> [ARGS]") << "\n\n"
                  << "  Navigation (Linux-Stil):\n"
                  << "    cd <ID>    In Entität navigieren (F16/F22/F18/AKT)\n"
                  << "    ls         Inhalt der aktuellen Ebene auflisten\n"
                  << "    lo / -h    Kontextabhängige Optionen anzeigen\n"
                  << "    ..         Eine Ebene zurück\n\n"
                  << "  F16: -f16           F16-Karten   -f16 -n  Neu   -f16 -o  Auswahl\n"
                  << "  F22: -f22           F22-Vorgänge  -f22 -n  Neu   -f22 -o  Auswahl\n"
                  << "  F18: -f18           Vorgänge      -f18 -n  Neu   -f18 -o  Auswahl\n"
                  << "  AKT: -akt           Akten          -akt -n  Neu   -akt -o  Auswahl\n"
                  << "  F77: -f77 -o/-so    Workflows listen/suchen\n"
                  << "  TSK: tsk / -tasks   Offene F77-Aufgaben  tsk -a  alle  tsk -so <q>  suchen\n"
                  << "  F18S:-f18s -o/-so   F18-Schritte\n"
                  << "  F99: -f99 -s/-so    Notizen suchen/Manager\n"
                  << "  PER: -per -s <q>    Personen      -de  Diensteinheiten\n\n"
                  << "  SYS: -search <q>  Globale Suche   -status  Zählungen\n"
                  << "       -backup       Backup          -mfs  MFS aufbauen\n"
                  << "       -log <level>  debug|info|warn|error\n"
                  << "       -go <ref>     Direkt öffnen   -hist  Verlauf\n"
                  << "       -tree [f16]   Hierarchiebaum  -watch [N]  Polling\n"
                  << "       -cal          Kalenderansicht\n\n"
                  << "  exit  Beenden\n\n";
        return;
    }

    // ── In context: header ───────────────────────────────────────────────────
    std::string typeName = Rosenholz::entityTypeLabel(cur.type);
    std::cout << Color::bold("  " + typeName + " " + cur.id + " — " + cur.displayName)
              << "\n  " << std::string(54, '-') << "\n"
              << "  Navigation:\n"
              << "    cd <ID>    In Unterobjekt navigieren\n"
              << "    ls         Inhalt listen\n"
              << "    ..         Zurück\n\n";

    switch (cur.type) {

    // ── F16 ────────────────────────────────────────────────────────────────
    case ET::F16:
        std::cout
          << "  Dieses F16:\n"
          << "    . -e          F16 bearbeiten\n"
          << "    . -arc        F16 archivieren\n"
          << "    . -v          F16 Detailansicht\n\n"
          << "  F22 Vorgänge:\n"
          << "    f22 -n        Neue F22 in diesem Projekt anlegen\n"
          << "    f22 -o/-so    F22 auswählen / suchen\n\n"
          << "  Workflow:\n"
          << "    . -f77 -n     Freigabe-Workflow für dieses F16 starten\n"
          << "    . -f77 -d     Laufende Workflows anzeigen\n\n"
          << "  Suche & Notizen:\n"
          << "    kom -n/-o     Kommunikation anlegen / öffnen\n"
          << "    f99 <Text>    Notiz auf dieses F16\n"
          << "    f99 -s/-so    Notizen suchen / Manager\n"
          << "    srch <q>      Globale Suche\n"
          << "    mfs           MFS neu aufbauen\n";
        break;

    // ── F22 ─────────────────────────────────────────────────────────────────
    case ET::F22:
        std::cout
          << "  Dieses F22:\n"
          << "    . -e          F22 bearbeiten\n"
          << "    . -v          F22 Detailansicht\n"
          << "    . -ind        Nacherfassung (Aufgabe einbuchen)\n\n"
          << "  F18 Vorgänge:\n"
          << "    f18 -n        Neuen F18 anlegen\n"
          << "    f18 -o/-so    F18 auswählen / suchen\n\n"
          << "  Akten:\n"
          << "    akt -n        Neue Akte in diesem F22 anlegen\n"
          << "    akt -o/-so    Akten auswählen / suchen\n"
          << "    akt -oo       Alle Objekte im Kontext listen\n\n"
          << "  Workflow:\n"
          << "    . -f77 -n     Freigabe-Workflow für dieses F22 starten\n"
          << "    . -f77 -d     Laufende Workflows anzeigen\n\n"
          << "  Suche & Notizen:\n"
          << "    kom -n/-o     Kommunikation anlegen / öffnen\n"
          << "    f99 <Text>    Notiz auf dieses F22\n"
          << "    f99 -s/-so    Notizen suchen / Manager\n"
          << "    srch <q>      Globale Suche\n"
          << "    mfs           MFS neu aufbauen\n";
        break;

    // ── F18 ─────────────────────────────────────────────────────────────────
    case ET::F18:
        std::cout
          << "  Dieses F18:\n"
          << "    . -e          F18 bearbeiten\n"
          << "    . -v          F18 Detailansicht\n\n"
          << "  Schritte (F18S):\n"
          << "    f18s          Schritte listen → öffnen\n"
          << "    f18s -n       Neuen Schritt anlegen\n"
          << "    f18s -e <n>   Schritt bearbeiten\n"
          << "    f18s -o/-so   Schritte auswählen / suchen\n\n"
          << "  Akten:\n"
          << "    akt -n        Neue Akte anlegen\n"
          << "    akt -o/-so    Akten suchen\n"
          << "    akt -oo       Alle Objekte im Kontext\n\n"
          << "  Workflow:\n"
          << "    . -f77 -n     Freigabe-Workflow für dieses F18 starten\n"
          << "    . -f77 -d     Laufende Workflows anzeigen\n\n"
          << "  Suche & Notizen:\n"
          << "    kom -n/-o     Kommunikation anlegen / öffnen\n"
          << "    f99 <Text>    Notiz auf dieses F18\n"
          << "    f99 -s/-so    Notizen suchen / Manager\n";
        break;

    // ── AKT ─────────────────────────────────────────────────────────────────
    case ET::AKT:
        std::cout
          << "  Diese Akte:\n"
          << "    . -e          Akte bearbeiten\n"
          << "    . -r          Neue Revision anlegen (= rev)\n"
          << "    . -hist       Revisionsverlauf anzeigen\n\n"
          << "  Objekte dieser Akte:\n"
          << "    ls            Alle Objekte der aktuellen Revision\n"
          << "    ls -rev       Alle Revisionen\n"
          << "    . -obj        Neues Objekt hinzufügen\n"
          << "    . -co <n>     Objekt auschecken\n"
          << "    . -ci         Objekt einchecken\n"
          << "    . -rv <n>     Revision wechseln\n"
          << "    . -url        URL-Objekte aktualisieren\n"
          << "    . -oo/-soo    Objekte auflisten / suchen\n\n"
          << "  Workflow:\n"
          << "    . -f77 -n     Freigabe-Workflow für diese Akte starten\n"
          << "    . -f77 -d     Laufende Workflows anzeigen\n\n"
          << "  Notizen:\n"
          << "    f99 <Text>    Notiz auf diese Akte\n"
          << "    f99 -s/-so    Notizen suchen / Manager\n";
        break;

    default:
        // Fallback for any other entity type:
        std::cout << "  Keine Kontextoptionen definiert für: " << typeName << "\n";
        break;
    }

    std::cout << "\n  exit  Beenden\n\n";
}


// ── completions ───────────────────────────────────────────────────────────────

std::vector<std::string> completions(const std::string& prefix,
                                     const std::string& prev) {
    std::vector<std::string> result;

    // After 'cd': complete with context children
    if (prev == "cd") {
        try {
            for (auto& [id, title] : getContextChildren())
                result.push_back(id);
        } catch (...) {}
        return result;
    }

    // "." expands to the current entity command — complete its subHints
    if (prev == ".") {
        auto cur = Rosenholz::NavigationStack::instance().current();
        if (cur.valid()) {
            std::string entCmd;
            switch (cur.type) {
                case Rosenholz::EntityType::F16: entCmd = "f16"; break;
                case Rosenholz::EntityType::F22: entCmd = "f22"; break;
                case Rosenholz::EntityType::F18: entCmd = "f18"; break;
                case Rosenholz::EntityType::AKT: entCmd = "akt"; break;
                default: break;
            }
            for (auto& c : registry()) {
                if (c.name == entCmd) {
                    for (auto* s : c.subHints)
                        if (prefix.empty() || std::string(s).find(prefix) == 0)
                            result.push_back(s);
                    return result;
                }
            }
        }
        return result;
    }

    // Sub-command completion: if prev matches a command name, offer its subHints
    for (auto& c : registry()) {
        bool matchName = (prev == c.name);
        bool matchDash = (c.dashName && prev == c.dashName);
        if (matchName || matchDash) {
            for (auto* s : c.subHints)
                if (prefix.empty() || std::string(s).find(prefix) == 0)
                    result.push_back(s);
            return result;
        }
    }

    // Top-level command completion:
    for (auto& c : registry()) {
        if (std::string(c.name).find(prefix) == 0)
            result.push_back(c.name);
        if (c.dashName && std::string(c.dashName).find(prefix) == 0)
            result.push_back(c.dashName);
    }
    // Fixed keywords:
    for (auto* s : {"exit", "quit", "-h", "--help", ".."})
        if (std::string(s).find(prefix) == 0)
            result.push_back(s);

    return result;
}

} // namespace CLI
