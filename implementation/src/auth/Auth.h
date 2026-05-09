#pragma once
// ============================================================
// Auth.h  —  User Authentication and Session Management
// Rosenholz PM v10.0
// ============================================================
#include <string>
#include <vector>
#include <memory>

namespace Rosenholz {

std::string sha256hex(const std::string& input);

struct RhUser {
    std::string userId;
    std::string username;
    std::string passwordHash;
    std::string fullName;
    std::string email;
    bool        isActive { true };
    std::vector<std::string> roles;

    bool hasRole(const std::string& role) const;
    bool isAdmin()  const { return hasRole("admin"); }
    bool isUser()   const { return hasRole("user");  }
    bool save()   const;
    bool remove() const;
    bool setPassword(const std::string& newPlain);

    static std::shared_ptr<RhUser> loadByUsername(const std::string& u);
    static std::shared_ptr<RhUser> loadById(const std::string& id);
    static std::vector<std::shared_ptr<RhUser>> loadAll();
    static std::shared_ptr<RhUser> create(const std::string& username,
                                          const std::string& plainPassword,
                                          const std::string& fullName = "");
};

struct RhRole {
    std::string roleId;
    std::string roleName;
    std::string description;
    bool save()   const;
    bool remove() const;
    static std::vector<RhRole> loadAll();
};

class AuthSession {
public:
    static AuthSession& instance();
    bool login(const std::string& username, const std::string& plain);
    void loginDirect(std::shared_ptr<RhUser> user);
    void logout();
    bool isLoggedIn() const { return m_user != nullptr; }
    bool isAdmin()    const { return m_user && m_user->isAdmin(); }
    std::string userId()   const { return m_user ? m_user->userId   : ""; }
    std::string username() const { return m_user ? m_user->username : ""; }
    const RhUser* user()   const { return m_user.get(); }
    static void ensureSchema();
private:
    AuthSession() = default;
    std::shared_ptr<RhUser> m_user;
};

bool cliLogin();
void cliUserManager();

} // namespace Rosenholz
