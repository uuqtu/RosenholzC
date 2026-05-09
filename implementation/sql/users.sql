-- User and Role Management (core.db)
-- Added: Rosenholz PM v10.0 (non-backwards-compat)

CREATE TABLE IF NOT EXISTS users (
    user_id      TEXT PRIMARY KEY,           -- XV/USR/000001/26
    username     TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,             -- SHA-256 hex
    full_name    TEXT,
    email        TEXT,
    is_active    INTEGER NOT NULL DEFAULT 1,
    created_at   TEXT NOT NULL,
    updated_at   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS roles (
    role_id      TEXT PRIMARY KEY,
    role_name    TEXT NOT NULL UNIQUE,       -- 'admin', 'user'
    description  TEXT,
    created_at   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS user_roles (
    user_id      TEXT NOT NULL REFERENCES users(user_id),
    role_id      TEXT NOT NULL REFERENCES roles(role_id),
    granted_at   TEXT NOT NULL,
    granted_by   TEXT,
    PRIMARY KEY (user_id, role_id)
);

-- Seed data: roles
INSERT OR IGNORE INTO roles (role_id, role_name, description, created_at) VALUES
    ('role-admin', 'admin', 'Vollzugriff inkl. Admin-Funktionen', datetime('now')),
    ('role-user',  'user',  'Standard-Benutzer',                  datetime('now'));

-- Seed data: users (passwords: rh=rh, admin=admin, SHA-256)
-- SHA-256("rh")    = a5785c8e4cb0f694bb37e0b073c98d9f8e42ef67f52c2f8b5d543f1fbf95ed66
-- SHA-256("admin") = 8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918
INSERT OR IGNORE INTO users (user_id, username, password_hash, full_name, is_active, created_at, updated_at) VALUES
    ('usr-rh',    'rh',    'a5785c8e4cb0f694bb37e0b073c98d9f8e42ef67f52c2f8b5d543f1fbf95ed66', 'Rosenholz User',  1, datetime('now'), datetime('now')),
    ('usr-admin', 'admin', '8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918', 'Administrator',   1, datetime('now'), datetime('now'));

-- Seed data: user_roles
INSERT OR IGNORE INTO user_roles (user_id, role_id, granted_at, granted_by) VALUES
    ('usr-rh',    'role-user',  datetime('now'), 'system'),
    ('usr-admin', 'role-admin', datetime('now'), 'system'),
    ('usr-admin', 'role-user',  datetime('now'), 'system');
