// ============================================================
// Auth.cpp  —  User Authentication and Session Management
// Rosenholz PM v10.0
// ============================================================
#include "Auth.h"
#include "../core/Database.h"
#include "../model/Utils.h"
#include "../core/Logger.h"
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <algorithm>

namespace Rosenholz {

// ── SHA-256 (portable, no external dependency) ───────────────────────────
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
    0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
    0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
    0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
    0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
    0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static inline uint32_t rotr(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }

std::string sha256hex(const std::string& msg) {
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    std::vector<uint8_t> data(msg.begin(), msg.end());
    uint64_t bitlen = (uint64_t)data.size() * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i)
        data.push_back((bitlen >> (i*8)) & 0xFF);

    for (size_t i = 0; i < data.size(); i += 64) {
        uint32_t w[64];
        for (int j = 0; j < 16; ++j)
            w[j] = ((uint32_t)data[i+j*4]<<24)|((uint32_t)data[i+j*4+1]<<16)|
                   ((uint32_t)data[i+j*4+2]<<8)|(uint32_t)data[i+j*4+3];
        for (int j = 16; j < 64; ++j) {
            uint32_t s0 = rotr(w[j-15],7)^rotr(w[j-15],18)^(w[j-15]>>3);
            uint32_t s1 = rotr(w[j-2],17)^rotr(w[j-2],19)^(w[j-2]>>10);
            w[j] = w[j-16]+s0+w[j-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int j = 0; j < 64; ++j) {
            uint32_t S1 = rotr(e,6)^rotr(e,11)^rotr(e,25);
            uint32_t ch = (e&f)^(~e&g);
            uint32_t t1 = hh+S1+ch+K[j]+w[j];
            uint32_t S0 = rotr(a,2)^rotr(a,13)^rotr(a,22);
            uint32_t maj= (a&b)^(a&c)^(b&c);
            uint32_t t2 = S0+maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    std::ostringstream oss;
    for (int i = 0; i < 8; ++i)
        oss << std::hex << std::setw(8) << std::setfill('0') << h[i];
    return oss.str();
}

// ── RhUser ────────────────────────────────────────────────────────────────
bool RhUser::hasRole(const std::string& role) const {
    return std::find(roles.begin(), roles.end(), role) != roles.end();
}

bool RhUser::save() const {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    db->exec(
        "INSERT OR REPLACE INTO users (user_id,username,password_hash,"
        "full_name,email,is_active,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?);",
        std::vector<BindParam>{
          BindParam::text(userId), BindParam::text(username),
          BindParam::text(passwordHash), BindParam::text(fullName),
          BindParam::text(email), BindParam::int64(isActive?1:0),
          BindParam::text(nowIso()), BindParam::text(nowIso())});
    db->exec("DELETE FROM user_roles WHERE user_id=?;", std::vector<BindParam>{BindParam::text(userId)});
    for (auto& r : roles) {
        db->exec(
            "INSERT OR IGNORE INTO user_roles (user_id,role_id,granted_at,granted_by) "
            "SELECT ?,role_id,?,? FROM roles WHERE role_name=?;",
            std::vector<BindParam>{BindParam::text(userId), BindParam::text(nowIso()),
             BindParam::text("system"), BindParam::text(r)});
    }
    return true;
}

bool RhUser::remove() const {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    db->exec("DELETE FROM user_roles WHERE user_id=?;", std::vector<BindParam>{BindParam::text(userId)});
    db->exec("DELETE FROM users WHERE user_id=?;",      std::vector<BindParam>{BindParam::text(userId)});
    return true;
}

bool RhUser::setPassword(const std::string& newPlain) {
    const_cast<RhUser*>(this)->passwordHash = sha256hex(newPlain);
    return save();
}

static std::shared_ptr<RhUser> userFromRow(const std::map<std::string,std::string>& row) {
    auto u = std::make_shared<RhUser>();
    auto g = [&](const std::string& k) {
        auto it = row.find(k); return it != row.end() ? it->second : "";
    };
    u->userId       = g("user_id");
    u->username     = g("username");
    u->passwordHash = g("password_hash");
    u->fullName     = g("full_name");
    u->email        = g("email");
    u->isActive     = (g("is_active") != "0");
    return u;
}

std::shared_ptr<RhUser> RhUser::loadByUsername(const std::string& name) {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return nullptr;
    auto rows = db->query("SELECT * FROM users WHERE username=?;", std::vector<BindParam>{BindParam::text(name)});
    if (rows.empty()) return nullptr;
    auto u = userFromRow(rows[0]);
    // Load roles:
    auto rrows = db->query(
        "SELECT r.role_name FROM roles r JOIN user_roles ur ON r.role_id=ur.role_id "
        "WHERE ur.user_id=?;", std::vector<BindParam>{BindParam::text(u->userId)});
    for (auto& r : rrows)
        if (r.count("role_name")) u->roles.push_back(r.at("role_name"));
    return u;
}

std::shared_ptr<RhUser> RhUser::loadById(const std::string& id) {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return nullptr;
    auto rows = db->query("SELECT * FROM users WHERE user_id=?;", std::vector<BindParam>{BindParam::text(id)});
    if (rows.empty()) return nullptr;
    auto u = userFromRow(rows[0]);
    auto rrows = db->query(
        "SELECT r.role_name FROM roles r JOIN user_roles ur ON r.role_id=ur.role_id "
        "WHERE ur.user_id=?;", std::vector<BindParam>{BindParam::text(u->userId)});
    for (auto& r : rrows)
        if (r.count("role_name")) u->roles.push_back(r.at("role_name"));
    return u;
}

std::vector<std::shared_ptr<RhUser>> RhUser::loadAll() {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return {};
    auto rows = db->query("SELECT * FROM users ORDER BY username;", {});
    std::vector<std::shared_ptr<RhUser>> result;
    for (auto& row : rows) {
        auto u = userFromRow(row);
        auto rrows = db->query(
            "SELECT r.role_name FROM roles r JOIN user_roles ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=?;", std::vector<BindParam>{BindParam::text(u->userId)});
        for (auto& r : rrows)
            if (r.count("role_name")) u->roles.push_back(r.at("role_name"));
        result.push_back(u);
    }
    return result;
}

std::shared_ptr<RhUser> RhUser::create(const std::string& username,
                                        const std::string& plain,
                                        const std::string& fullName) {
    auto u = std::make_shared<RhUser>();
    u->userId   = "usr-" + username;  // simple deterministic ID
    u->username = username;
    u->passwordHash = sha256hex(plain);
    u->fullName     = fullName.empty() ? username : fullName;
    u->isActive     = true;
    u->roles        = {"user"};  // default role
    u->save();
    return u;
}

// ── RhRole ────────────────────────────────────────────────────────────────
std::vector<RhRole> RhRole::loadAll() {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return {};
    auto rows = db->query("SELECT * FROM roles ORDER BY role_name;", {});
    std::vector<RhRole> result;
    for (auto& row : rows) {
        RhRole r;
        auto g = [&](const std::string& k) { auto it=row.find(k); return it!=row.end()?it->second:""; };
        r.roleId      = g("role_id");
        r.roleName    = g("role_name");
        r.description = g("description");
        result.push_back(r);
    }
    return result;
}

bool RhRole::save() const {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    return db->exec(
        "INSERT OR REPLACE INTO roles (role_id,role_name,description,created_at) VALUES (?,?,?,?);",
        std::vector<BindParam>{BindParam::text(roleId), BindParam::text(roleName),
         BindParam::text(description), BindParam::text(nowIso())});
}

bool RhRole::remove() const {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    db->exec("DELETE FROM user_roles WHERE role_id=?;", std::vector<BindParam>{BindParam::text(roleId)});
    return db->exec("DELETE FROM roles WHERE role_id=?;", std::vector<BindParam>{BindParam::text(roleId)});
}

// ── AuthSession ───────────────────────────────────────────────────────────
AuthSession& AuthSession::instance() {
    static AuthSession s;
    return s;
}

bool AuthSession::login(const std::string& name, const std::string& plain) {
    auto u = RhUser::loadByUsername(name);
    if (!u || !u->isActive) return false;
    if (u->passwordHash != sha256hex(plain)) return false;
    m_user = u;
    return true;
}

void AuthSession::loginDirect(std::shared_ptr<RhUser> user) {
    m_user = user;
}

void AuthSession::logout() {
    m_user = nullptr;
}

void AuthSession::ensureSchema() {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return;
    // Tables are created by core.sql via DatabasePool::initAll().
    // This function only seeds the default roles and users.

    // Seed roles:
    db->exec("INSERT OR IGNORE INTO roles (role_id,role_name,description,created_at) VALUES (?,?,?,?);",
             std::vector<BindParam>{BindParam::text("role-admin"), BindParam::text("admin"),
              BindParam::text("Vollzugriff inkl. Admin-Funktionen"), BindParam::text(nowIso())});
    db->exec("INSERT OR IGNORE INTO roles (role_id,role_name,description,created_at) VALUES (?,?,?,?);",
             std::vector<BindParam>{BindParam::text("role-user"), BindParam::text("user"),
              BindParam::text("Standard-Benutzer"), BindParam::text(nowIso())});

    // Seed users: rh/rh, admin/admin
    std::string rhHash    = sha256hex("rh");
    std::string adminHash = sha256hex("admin");
    std::string now       = nowIso();

    db->exec("INSERT OR IGNORE INTO users (user_id,username,password_hash,full_name,is_active,created_at,updated_at) VALUES (?,?,?,?,?,?,?);",
             std::vector<BindParam>{BindParam::text("usr-rh"), BindParam::text("rh"), BindParam::text(rhHash),
              BindParam::text("Rosenholz User"), BindParam::int64(1), BindParam::text(now), BindParam::text(now)});
    db->exec("INSERT OR IGNORE INTO users (user_id,username,password_hash,full_name,is_active,created_at,updated_at) VALUES (?,?,?,?,?,?,?);",
             std::vector<BindParam>{BindParam::text("usr-admin"), BindParam::text("admin"), BindParam::text(adminHash),
              BindParam::text("Administrator"), BindParam::int64(1), BindParam::text(now), BindParam::text(now)});

    // Seed user_roles:
    db->exec("INSERT OR IGNORE INTO user_roles (user_id,role_id,granted_at,granted_by) VALUES (?,?,?,?);",
             std::vector<BindParam>{BindParam::text("usr-rh"),    BindParam::text("role-user"),  BindParam::text(now), BindParam::text("system")});
    db->exec("INSERT OR IGNORE INTO user_roles (user_id,role_id,granted_at,granted_by) VALUES (?,?,?,?);",
             std::vector<BindParam>{BindParam::text("usr-admin"), BindParam::text("role-admin"), BindParam::text(now), BindParam::text("system")});
    db->exec("INSERT OR IGNORE INTO user_roles (user_id,role_id,granted_at,granted_by) VALUES (?,?,?,?);",
             std::vector<BindParam>{BindParam::text("usr-admin"), BindParam::text("role-user"),  BindParam::text(now), BindParam::text("system")});
}

// ── CLI ───────────────────────────────────────────────────────────────────
bool cliLogin() {
    std::cout << "\n  Rosenholz PM v10  —  Anmeldung\n"
              << "  " << std::string(40, '-') << "\n";
    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::cout << "  Benutzername: ";
        std::string user; std::getline(std::cin, user);
        std::cout << "  Passwort: ";
        // Simple read (no echo suppression in terminal — add if needed):
        std::string pass; std::getline(std::cin, pass);
        if (AuthSession::instance().login(user, pass)) {
            std::cout << "  >> Angemeldet als: " << user;
            if (AuthSession::instance().isAdmin())
                std::cout << " [admin]";
            std::cout << "\n\n";
            return true;
        }
        std::cout << "  >> Ungueltige Anmeldedaten";
        if (attempt < 3) std::cout << " (noch " << (3-attempt) << " Versuch(e))";
        std::cout << "\n";
    }
    std::cout << "\n  Anmeldung fehlgeschlagen.\n";
    return false;
}

void cliUserManager() {
    auto& session = AuthSession::instance();
    if (!session.isAdmin()) {
        std::cout << "  >> Keine Berechtigung (Admin erforderlich).\n";
        return;
    }
    while (true) {
        auto users = RhUser::loadAll();
        auto roles = RhRole::loadAll();
        std::cout << "\n  -- BENUTZERVERWALTUNG --\n\n";
        std::cout << "  " << std::left << std::setw(12) << "BENUTZER"
                  << std::setw(30) << "NAME"
                  << std::setw(20) << "ROLLEN"
                  << "AKTIV\n  " << std::string(65,'-') << "\n";
        int n = 1;
        for (auto& u : users) {
            std::string roleStr;
            for (auto& r : u->roles) { if (!roleStr.empty()) roleStr+=","; roleStr+=r; }
            std::cout << "  " << std::setw(3) << n++
                      << std::setw(12) << u->username
                      << std::setw(30) << u->fullName.substr(0,28)
                      << std::setw(20) << roleStr
                      << (u->isActive ? "ja" : "nein") << "\n";
        }
        std::cout << "\n  1.Neu  2.Passwort ändern  3.Rolle(n) setzen  4.Aktivieren/Sperren  0.Zurück\n";

        // Simple int read:
        std::string line; std::cout << "  Wahl: "; std::getline(std::cin, line);
        int ch = 0; try { ch = std::stoi(line); } catch(...) {}

        if (ch == 0) return;

        if (ch == 1) {
            std::cout << "  Benutzername: "; std::string u; std::getline(std::cin, u);
            if (u.empty()) continue;
            std::cout << "  Passwort: "; std::string p; std::getline(std::cin, p);
            if (p.empty()) continue;
            std::cout << "  Vollname (leer=Benutzername): "; std::string fn; std::getline(std::cin, fn);
            auto nu = RhUser::create(u, p, fn);
            std::cout << "  >> Benutzer angelegt: " << nu->userId << "\n";
        }
        else if (ch == 2) {
            std::cout << "  Benutzername: "; std::string u; std::getline(std::cin, u);
            auto pu = RhUser::loadByUsername(u);
            if (!pu) { std::cout << "  >> Nicht gefunden.\n"; continue; }
            std::cout << "  Neues Passwort: "; std::string p; std::getline(std::cin, p);
            if (p.empty()) continue;
            pu->setPassword(p);
            std::cout << "  >> Passwort geändert.\n";
        }
        else if (ch == 3) {
            std::cout << "  Benutzername: "; std::string u; std::getline(std::cin, u);
            auto pu = RhUser::loadByUsername(u);
            if (!pu) { std::cout << "  >> Nicht gefunden.\n"; continue; }
            std::cout << "  Rollen (kommasepariert, z.B. admin,user): ";
            std::string rline; std::getline(std::cin, rline);
            pu->roles.clear();
            std::istringstream rs(rline);
            std::string tok;
            while (std::getline(rs, tok, ',')) {
                if (!tok.empty()) pu->roles.push_back(tok);
            }
            pu->save();
            std::cout << "  >> Rollen aktualisiert.\n";
        }
        else if (ch == 4) {
            std::cout << "  Benutzername: "; std::string u; std::getline(std::cin, u);
            auto pu = RhUser::loadByUsername(u);
            if (!pu) { std::cout << "  >> Nicht gefunden.\n"; continue; }
            pu->isActive = !pu->isActive;
            pu->save();
            std::cout << "  >> " << u << " ist jetzt " << (pu->isActive ? "aktiv" : "gesperrt") << ".\n";
        }
    }
}

} // namespace Rosenholz
