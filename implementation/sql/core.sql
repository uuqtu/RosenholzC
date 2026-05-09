CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS persons (
    person_id          TEXT PRIMARY KEY,
    reg_number         TEXT,
    last_name          TEXT NOT NULL,
    first_name         TEXT NOT NULL,
    preferred_name     TEXT,
    email              TEXT,
    phone              TEXT,
    org_unit           TEXT,
    department         TEXT,
    location           TEXT,
    country            TEXT,
    role_title         TEXT,
    person_type        TEXT,
    employment_type    TEXT,
    seniority_level    TEXT,
    skills             TEXT,
    certifications     TEXT,
    languages          TEXT,
    day_rate           REAL,
    monthly_rate       REAL,
    availability_pct   REAL,
    availability_from  TEXT,
    availability_to    TEXT,
    manager_id         TEXT,   -- soft-ref, no FK (self-ref causes insert-order issues)
    status             TEXT DEFAULT 'active',
    onboard_date       TEXT,
    offboard_date      TEXT,
    clearance_level    TEXT,
    external_ref       TEXT,
    links              TEXT,
    notes              TEXT,  -- JSON
    created_at         TEXT DEFAULT (datetime('now')),
    updated_at         TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS teams (
    team_id            TEXT PRIMARY KEY,
    name               TEXT NOT NULL,
    abbreviation       TEXT,
    rosenholz_equiv    TEXT,
    parent_team_id     TEXT,   -- soft-ref to teams(team_id)
    lead_id            TEXT,
    location           TEXT,
    type               TEXT,
    headcount_planned  INTEGER,
    headcount_actual   INTEGER,
    budget_allocated   REAL,
    budget_consumed    REAL,
    methodology        TEXT,
    tools              TEXT,
    status             TEXT DEFAULT 'active',
    external_ref       TEXT,
    links              TEXT,
    notes              TEXT  -- JSON
);

CREATE TABLE IF NOT EXISTS team_members (
    membership_id           TEXT PRIMARY KEY,
    team_id                 TEXT NOT NULL,
    person_id               TEXT NOT NULL,
    -- Role & classification
    role                    TEXT,
    role_category           TEXT,
    seniority_in_team       TEXT,
    member_type             TEXT,   -- 'internal','contractor','advisor'
    is_lead                 INTEGER DEFAULT 0,
    is_deputy               INTEGER DEFAULT 0,
    is_core_member          INTEGER DEFAULT 0,
    is_extended_member      INTEGER DEFAULT 0,
    is_observer             INTEGER DEFAULT 0,
    -- Assignment
    allocation_pct          REAL,
    fte_equivalent          REAL,
    start_date              TEXT,
    end_date                TEXT,
    assignment_type         TEXT,   -- 'permanent','temporary','secondment'
    -- Competence
    primary_skill           TEXT,
    secondary_skills        TEXT,
    certifications_relevant TEXT,
    clearance_level         TEXT,
    -- Workload & cost
    planned_hours_per_week  REAL,
    actual_hours_per_week   REAL,
    cost_rate               REAL,
    cost_center             TEXT,
    -- Status
    status                  TEXT DEFAULT 'active',
    onboarded_date          TEXT,
    offboarded_date         TEXT,
    offboarding_reason      TEXT,
    notes                   TEXT    -- JSON
);

CREATE TABLE IF NOT EXISTS reg_number_sequences (
    reg_dept    TEXT NOT NULL,
    reg_year    INTEGER NOT NULL,
    next_seq    INTEGER DEFAULT 1,
    PRIMARY KEY (reg_dept, reg_year)
);

-- ── User/Role Management (v10) ──────────────────────────
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
