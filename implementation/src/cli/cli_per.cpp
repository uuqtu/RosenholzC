// ============================================================
// cli_per.cpp  —  Person: Befehlshandler, Wizard, Anzeige
//
// Public functions:
//   cmdPer(args)         — dispatch for 'rh -per ...'
//   listPersons()        — tabular list of all persons
//   printPerson(p)       — detail card for one person
//   createPersonWizard() — step-by-step person creation
// ============================================================
#include "cli_common.h"
#include "../core/OperationResult.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include "../mfs/MFSWriter.h"
#include <algorithm>
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── -per ──────────────────────────────────────────────────────

void cmdPer(const std::vector<std::string>& args) {
    if (args.empty()) { CLI::listPersons(); return; }

    if (args[0] == "-s" || args[0] == "--search") {
        std::string q = args.size() > 1 ? args[1] : CLI::readLine("Suche: ");
        auto res = Person::search(q);
        if (res.empty()) { std::cout << "  (keine Treffer)\n"; return; }
        for (auto& p : res)
            std::cout << "  " << std::left << std::setw(28) << p->personId
                      << "  " << p->fullName() << "\n";
        return;
    }

    if (isId(args[0])) {
        auto p = Person::loadById(args[0]);
        if (!p) { printErr("Person nicht gefunden: " + args[0]); return; }
        CLI::printPerson(*p);
        return;
    }

    // Non-ID → create wizard
    auto p = CLI::createPersonWizard();
    if (p) printOk("  >> Person angelegt: " + p->regNumber.toString() + "  " + p->fullName());
}

void printPerson(const Rosenholz::Person& p) {
    hdr("PERSON  " + p.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",           p.personId);
    row("Name",         p.fullName());
    row("Email",        fval(p.email));
    row("Phone",        fval(p.phone));
    row("Role",         fval(p.roleTitle));
    row("Dept",         fval(p.department));
    row("Type",         fval(p.personType));
    row("Status",       fval(p.status));
    row("Day-rate",     std::to_string((int)p.dayRate) + " EUR");
    row("Avail.%",      std::to_string((int)p.availabilityPct));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}



void listPersons() {
    auto all = Rosenholz::Person::loadAll();
    if (all.empty()) { std::cout << "  (keine Personen)\n"; return; }
    std::cout << "  " << std::left
              << std::setw(20) << "REG-NR"
              << std::setw(24) << "NAME"
              << std::setw(16) << "ROLLE"
              << std::setw(14) << "TYP"
              << "STATUS\n"
              << "  " << std::string(76,'-') << "\n";
    for (auto& p : all) {
        std::string name = p->fullName().size()>22 ? p->fullName().substr(0,21)+"~" : p->fullName();
        std::string role = p->roleTitle.empty() ? "-" :
                           (p->roleTitle.size()>14 ? p->roleTitle.substr(0,13)+"~" : p->roleTitle);
        std::cout << "  " << std::left
                  << std::setw(20) << p->regNumber.toString()
                  << std::setw(24) << name
                  << std::setw(16) << role
                  << std::setw(14) << (p->employmentType.empty() ? "-" : p->employmentType)
                  << p->status << "\n";
    }
    std::cout << "  " << all.size() << " Person(en)\n";
}



std::shared_ptr<Rosenholz::Person> createPersonWizard() {
    hdr("CREATE PERSON");
    std::string first  = readLine("First name: ");
    std::string last   = readLine("Last name: ");
    std::string email  = readOpt("Email (optional): ");
    std::string role   = readOpt("Role title (optional): ");
    std::string dept   = readOpt("Department (optional): ");
    std::cout << "  Person type:\n"
              << "    1. internal  2. external  3. contractor  4. advisor\n";
    int tc = readInt("Choose type", 1, 4);
    static const char* ptypes[] = {"internal","external","contractor","advisor"};

    std::string rateStr = readOpt("Day rate EUR (optional): ");
    double rate = 0.0;
    if (!rateStr.empty()) try { rate = std::stod(rateStr); } catch(...) {}

    auto p = Rosenholz::Person::create(first, last, email, ptypes[tc-1]);
    p->roleTitle  = role;
    p->department = dept;
    p->dayRate    = rate;

    if (opOk(p->save())) {
        std::cout << "\n  >> Person created: " << p->regNumber.toString()
                  << " (" << p->personId << ")\n\n";
        return p;
    } else {
        std::cout << "\n  >> ERROR: Person could not be saved.\n\n";
        return nullptr;
    }
}

} // namespace CLI
