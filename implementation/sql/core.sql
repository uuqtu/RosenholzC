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

CREATE TABLE IF NOT EXISTS project_types (
    type_code              TEXT PRIMARY KEY,
    label                  TEXT NOT NULL,
    rosenholz_equivalent   TEXT,
    pm_equivalent          TEXT,
    description            TEXT,
    requires_au_archive    INTEGER DEFAULT 0,
    default_methodology    TEXT,
    default_workflow_def_id TEXT
);

-- Pre-populate project types
INSERT OR IGNORE INTO project_types VALUES
  ('IM','IM-Vorgang','F16/IM','Ongoing contributor engagement','Inoffizieller Mitarbeiter file',0,'agile',NULL),
  ('OV','Operativer Vorgang','F16/OV','Active investigation / audit','Full operational case',1,'waterfall',NULL),
  ('OPK','Operative Personenkontrolle','F16/OPK','Due diligence / review','Person check file',0,'kanban',NULL),
  ('GMS','GMS-Akte','F16/GMS','Advisory relationship','Gesellschaftlicher Mitarbeiter',0,'agile',NULL),
  ('AU','Untersuchungsvorgang','F16/AU','Formal inquiry / closed case','Archived investigation',1,'waterfall',NULL),
  ('SVG','Sicherungsvorgang','F16/SVG','Monitoring / watch brief','Security watch file',0,'kanban',NULL);
