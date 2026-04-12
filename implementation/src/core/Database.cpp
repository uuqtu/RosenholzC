// ============================================================
// Database.cpp  —  SQLite abstraction layer implementation
// ============================================================

#include "Database.h"
#include "Logger.h"
#include "FileOps.h"
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace RH {

// ── Database ─────────────────────────────────────────────────
Database::Database(const std::string& path) : m_path(path) {
    LOG_DEBUG("Opening database: " + path);
    FileOps::makeDirs(FileOps::dirName(path));

    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Failed to open DB " + path + ": " + m_lastError);
        sqlite3_close(m_db);
        m_db = nullptr;
    } else {
        LOG_INFO("Database opened: " + path);
    }
}

Database::~Database() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
        LOG_DEBUG("Database closed: " + m_path);
    }
}

bool Database::isOpen() const { return m_db != nullptr; }

std::string Database::lastError() const { return m_lastError; }

// ── Performance settings ─────────────────────────────────────
void Database::applyPerformanceSettings(bool walMode, int cacheSize) {
    if (!m_db) return;
    if (walMode)    execute("PRAGMA journal_mode=WAL;");
    execute("PRAGMA synchronous=NORMAL;");   // safe with WAL
    execute("PRAGMA cache_size=" + std::to_string(cacheSize) + ";");
    execute("PRAGMA temp_store=MEMORY;");
    execute("PRAGMA mmap_size=268435456;");  // 256 MB mmap
    LOG_DEBUG("Performance settings applied to: " + m_path);
}

// ── Schema version ───────────────────────────────────────────
int Database::schemaVersion() {
    auto v = queryScalar("PRAGMA user_version;");
    return v.empty() ? 0 : std::stoi(v);
}

void Database::setSchemaVersion(int v) {
    execute("PRAGMA user_version = " + std::to_string(v) + ";");
}

// ── DDL ──────────────────────────────────────────────────────
bool Database::execute(const std::string& sql) {
    if (!m_db) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        m_lastError = err ? err : "unknown";
        LOG_ERROR("SQL execute error [" + m_path + "]: " + m_lastError + " | SQL: " + sql.substr(0,120));
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::executeBatch(const std::string& sql) {
    return execute(sql);
}

// ── DML helpers ──────────────────────────────────────────────
bool Database::bindParams(sqlite3_stmt* stmt, const std::vector<BindParam>& params) {
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        const auto& p = params[i];
        int idx = i + 1;
        int rc  = SQLITE_OK;
        switch (p.type) {
            case BindParam::Type::TEXT:
                rc = sqlite3_bind_text(stmt, idx, p.sval.c_str(), -1, SQLITE_TRANSIENT);
                break;
            case BindParam::Type::INT64:
                rc = sqlite3_bind_int64(stmt, idx, p.ival);
                break;
            case BindParam::Type::DOUBLE:
                rc = sqlite3_bind_double(stmt, idx, p.dval);
                break;
            case BindParam::Type::NULL_:
                rc = sqlite3_bind_null(stmt, idx);
                break;
        }
        if (rc != SQLITE_OK) {
            m_lastError = sqlite3_errmsg(m_db);
            LOG_ERROR("Bind param " + std::to_string(idx) + " failed: " + m_lastError);
            return false;
        }
    }
    return true;
}

bool Database::exec(const std::string& sql, const std::vector<BindParam>& params) {
    if (!m_db) return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Prepare failed [" + m_path + "]: " + m_lastError);
        return false;
    }
    bindParams(stmt, params);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("exec step failed: " + m_lastError + " | SQL: " + sql.substr(0,120));
        return false;
    }
    return true;
}

int64_t Database::insert(const std::string& sql, const std::vector<BindParam>& params) {
    if (!exec(sql, params)) return -1;
    return sqlite3_last_insert_rowid(m_db);
}

ResultSet Database::query(const std::string& sql, const std::vector<BindParam>& params) {
    ResultSet result;
    if (!m_db) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Query prepare failed: " + m_lastError);
        return result;
    }
    bindParams(stmt, params);

    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row;
        for (int c = 0; c < cols; ++c) {
            const char* name = sqlite3_column_name(stmt, c);
            const char* val  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
            row[name ? name : ""] = val ? val : "";
        }
        result.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    LOG_DEBUG("Query returned " + std::to_string(result.size()) + " rows");
    return result;
}

std::string Database::queryScalar(const std::string& sql, const std::vector<BindParam>& params) {
    auto rs = query(sql, params);
    if (rs.empty() || rs[0].empty()) return "";
    return rs[0].begin()->second;
}

// ── Transactions ─────────────────────────────────────────────
bool Database::beginTransaction()    { return execute("BEGIN TRANSACTION;"); }
bool Database::commitTransaction()   { return execute("COMMIT;"); }
bool Database::rollbackTransaction() { return execute("ROLLBACK;"); }

// ── Utility ──────────────────────────────────────────────────
bool Database::tableExists(const std::string& tableName) {
    auto v = queryScalar(
        "SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?;",
        {BindParam::text(tableName)});
    return v == "1";
}

int64_t Database::lastInsertRowId() {
    return m_db ? sqlite3_last_insert_rowid(m_db) : -1;
}

int64_t Database::rowCount(const std::string& tableName) {
    auto v = queryScalar("SELECT count(*) FROM " + tableName + ";");
    return v.empty() ? 0 : std::stoll(v);
}

// ── DatabasePool ─────────────────────────────────────────────
DatabasePool& DatabasePool::instance() {
    static DatabasePool inst;
    return inst;
}

bool DatabasePool::initAll(const std::string& basePath, bool walMode, int cacheSize) {
    std::lock_guard<std::mutex> lk(m_mutex);
    LOG_INFO("Initialising database pool under: " + basePath);

    struct DBDef { std::string name; std::string file; };
    const std::vector<DBDef> defs = {
        {"core",      "core.db"},
        {"projects",  "projects.db"},
        {"workflow",  "workflow.db"},
        {"documents", "documents.db"},
        {"tracking",  "tracking.db"},
        {"reporting", "reporting.db"},
    };

    bool ok = true;
    for (auto& def : defs) {
        std::string path = FileOps::joinPath(basePath, "db", def.file);
        auto db = std::make_unique<Database>(path);
        if (!db->isOpen()) { ok = false; continue; }
        db->applyPerformanceSettings(walMode, cacheSize);
        ok &= applySchema(db.get(), def.name);
        m_dbs[def.name] = std::move(db);
    }
    LOG_INFO("Database pool ready. All OK: " + std::string(ok ? "yes" : "NO"));
    return ok;
}

Database* DatabasePool::get(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_dbs.find(name);
    if (it == m_dbs.end()) {
        LOG_ERROR("DatabasePool::get — unknown db name: " + name);
        return nullptr;
    }
    return it->second.get();
}

void DatabasePool::closeAll() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_dbs.clear();
    LOG_INFO("All databases closed.");
}

bool DatabasePool::applySchema(Database* db, const std::string& schemaName) {
    std::string sql;
    if      (schemaName == "core")      sql = Schema::coreSchema();
    else if (schemaName == "projects")  sql = Schema::projectsSchema();
    else if (schemaName == "workflow")  sql = Schema::workflowSchema();
    else if (schemaName == "documents") sql = Schema::documentsSchema();
    else if (schemaName == "tracking")  sql = Schema::trackingSchema();
    else if (schemaName == "reporting") sql = Schema::reportingSchema();
    else { LOG_WARN("Unknown schema name: " + schemaName); return false; }

    bool ok = db->executeBatch(sql);
    LOG_INFO("Schema '" + schemaName + "' applied: " + std::string(ok ? "OK" : "FAILED"));
    return ok;
}

bool DatabasePool::migrateAll() {
    // Future: check schema version per DB and run delta scripts
    LOG_INFO("migrateAll — no pending migrations.");
    return true;
}

// ── Schema definitions ────────────────────────────────────────
namespace Schema {

std::string coreSchema() { return R"SQL(
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
)SQL"; }

std::string projectsSchema() { return R"SQL(
CREATE TABLE IF NOT EXISTS projects (
    project_id                  TEXT PRIMARY KEY,
    workflow_instance_id        TEXT,
    workflow_status             TEXT,
    workflow_current_state      TEXT,
    reg_number                  TEXT,
    reg_dept                    TEXT,
    reg_sequence                INTEGER,
    reg_year                    INTEGER,
    title                       TEXT NOT NULL,
    codename                    TEXT,
    project_type                TEXT,
    size_class                  TEXT,   -- 'large','medium','small'
    owner_team_id               TEXT,
    lead_id                     TEXT,
    sponsor_id                  TEXT,
    status                      TEXT DEFAULT 'draft',
    phase                       TEXT,
    start_date_planned          TEXT,
    start_date_actual           TEXT,
    end_date_planned            TEXT,
    end_date_actual             TEXT,
    duration_planned_days       INTEGER,
    duration_actual_days        INTEGER,
    schedule_variance_days      INTEGER,
    budget_planned              REAL,
    budget_approved             REAL,
    budget_committed            REAL,
    budget_actual               REAL,
    cost_variance               REAL,
    cost_performance_index      REAL,
    schedule_performance_index  REAL,
    earned_value                REAL,
    planned_value               REAL,
    actual_cost                 REAL,
    estimate_at_completion      REAL,
    estimate_to_complete        REAL,
    variance_at_completion      REAL,
    scope_statement             TEXT,
    scope_version               TEXT,
    scope_last_changed          TEXT,
    scope_change_reason         TEXT,
    scope_change_count          INTEGER DEFAULT 0,
    priority                    TEXT,
    complexity                  TEXT,
    strategic_alignment         TEXT,
    quality_gate_id             TEXT,
    communication_plan_id       TEXT,
    currency                    TEXT DEFAULT 'EUR',
    external_ref                TEXT,
    methodology                 TEXT,
    classification              TEXT,
    links                       TEXT,
    notes                       TEXT,   -- JSON
    created_at                  TEXT DEFAULT (datetime('now')),
    updated_at                  TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS tasks (
    task_id                     TEXT PRIMARY KEY,
    workflow_instance_id        TEXT,
    workflow_status             TEXT,
    workflow_current_state      TEXT,
    reg_number                  TEXT,
    project_id                  TEXT NOT NULL,   -- soft ref to projects
    parent_task_id              TEXT,               -- soft ref to tasks (self-ref)
    assignee_id                 TEXT,
    assigned_by                 TEXT,
    task_code                   TEXT,
    title                       TEXT NOT NULL,
    description                 TEXT,
    task_type                   TEXT,
    status                      TEXT DEFAULT 'draft',
    priority                    TEXT,
    effort_planned_hrs          REAL,
    effort_actual_hrs           REAL,
    effort_remaining_hrs        REAL,
    cost_planned                REAL,
    cost_actual                 REAL,
    start_date_planned          TEXT,
    start_date_actual           TEXT,
    due_date_planned            TEXT,
    due_date_actual             TEXT,
    schedule_variance_days      INTEGER,
    percent_complete            INTEGER DEFAULT 0,
    quality_criteria            TEXT,
    acceptance_criteria         TEXT,
    is_milestone                INTEGER DEFAULT 0,
    wbs_code                    TEXT,
    sprint_or_phase             TEXT,
    links                       TEXT,
    notes                       TEXT,   -- JSON
    created_at                  TEXT DEFAULT (datetime('now')),
    updated_at                  TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS incidents (
    incident_id                 TEXT PRIMARY KEY,
    workflow_instance_id        TEXT,
    workflow_status             TEXT,
    workflow_current_state      TEXT,
    project_id                  TEXT,   -- soft ref to projects
    task_id                     TEXT,   -- soft ref to tasks
    risk_id                     TEXT,   -- FK to reporting.db risks
    reported_by                 TEXT,
    owner_id                    TEXT,
    title                       TEXT NOT NULL,
    description                 TEXT,
    incident_type               TEXT,
    category                    TEXT,
    severity                    TEXT,
    impact_area                 TEXT,
    cost_impact                 REAL,
    schedule_impact_days        INTEGER,
    scope_impact                TEXT,
    quality_impact              TEXT,
    occurred_date               TEXT,
    reported_date               TEXT,
    resolved_date               TEXT,
    status                      TEXT DEFAULT 'open',
    root_cause                  TEXT,
    immediate_action            TEXT,
    corrective_action           TEXT,
    resolution                  TEXT,
    escalated                   INTEGER DEFAULT 0,
    escalated_to                TEXT,
    links                       TEXT,
    notes                       TEXT,   -- JSON
    created_at                  TEXT DEFAULT (datetime('now')),
    updated_at                  TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS milestones (
    milestone_id           TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT NOT NULL,   -- soft ref to projects
    task_id                TEXT,               -- soft ref to tasks
    owner_id               TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    milestone_type         TEXT,
    planned_date           TEXT,
    actual_date            TEXT,
    variance_days          INTEGER,
    status                 TEXT DEFAULT 'pending',
    criteria               TEXT,
    contractual            INTEGER DEFAULT 0,
    payment_trigger        INTEGER DEFAULT 0,
    links                  TEXT,
    notes                  TEXT,   -- JSON
    created_at             TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS dependencies (
    dependency_id        TEXT PRIMARY KEY,
    source_project_id    TEXT,
    target_project_id    TEXT,
    source_task_id       TEXT,
    target_task_id       TEXT,
    dependency_type      TEXT,   -- 'FS','SS','FF','SF'
    rosenholz_equivalent TEXT,
    lag_days             INTEGER DEFAULT 0,
    status               TEXT DEFAULT 'active',
    notes                TEXT
);

-- QTCS assignment tables (multi-assignable, not mandatory)
CREATE TABLE IF NOT EXISTS project_quality  (project_id TEXT, quality_id  TEXT, PRIMARY KEY(project_id,quality_id));
CREATE TABLE IF NOT EXISTS project_cost     (project_id TEXT, cost_id     TEXT, PRIMARY KEY(project_id,cost_id));
CREATE TABLE IF NOT EXISTS project_time     (project_id TEXT, time_id     TEXT, PRIMARY KEY(project_id,time_id));
CREATE TABLE IF NOT EXISTS project_scope    (project_id TEXT, scope_id    TEXT, PRIMARY KEY(project_id,scope_id));

CREATE TABLE IF NOT EXISTS task_quality     (task_id TEXT, quality_id  TEXT, PRIMARY KEY(task_id,quality_id));
CREATE TABLE IF NOT EXISTS task_cost        (task_id TEXT, cost_id     TEXT, PRIMARY KEY(task_id,cost_id));
CREATE TABLE IF NOT EXISTS task_time        (task_id TEXT, time_id     TEXT, PRIMARY KEY(task_id,time_id));
CREATE TABLE IF NOT EXISTS task_scope       (task_id TEXT, scope_id    TEXT, PRIMARY KEY(task_id,scope_id));

CREATE TABLE IF NOT EXISTS incident_quality (incident_id TEXT, quality_id  TEXT, PRIMARY KEY(incident_id,quality_id));
CREATE TABLE IF NOT EXISTS incident_cost    (incident_id TEXT, cost_id     TEXT, PRIMARY KEY(incident_id,cost_id));
CREATE TABLE IF NOT EXISTS incident_time    (incident_id TEXT, time_id     TEXT, PRIMARY KEY(incident_id,time_id));
CREATE TABLE IF NOT EXISTS incident_scope   (incident_id TEXT, scope_id    TEXT, PRIMARY KEY(incident_id,scope_id));

-- Meetings belong to tasks
CREATE TABLE IF NOT EXISTS meetings (
    meeting_id             TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    task_id                TEXT NOT NULL,   -- soft ref to tasks
    project_id             TEXT,               -- soft ref to projects
    organiser_id           TEXT,
    title                  TEXT NOT NULL,
    meeting_type           TEXT,
    status                 TEXT DEFAULT 'scheduled',
    scheduled_date         TEXT,
    actual_date            TEXT,
    duration_mins          INTEGER,
    location               TEXT,
    channel                TEXT,
    agenda                 TEXT,
    decisions              TEXT,
    actions                TEXT,
    next_meeting_id        TEXT,   -- soft ref to next meeting
    links                  TEXT,
    notes                  TEXT,   -- JSON
    created_at             TEXT DEFAULT (datetime('now'))
);

-- Archived projects (AU)
CREATE TABLE IF NOT EXISTS archived_projects (
    archive_id             TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT NOT NULL,   -- soft ref to projects
    archived_date          TEXT,
    archive_reason         TEXT,
    archiver_id            TEXT,
    outcome_summary        TEXT,
    final_cost             REAL,
    final_duration_days    INTEGER,
    final_cost_variance    REAL,
    final_schedule_variance INTEGER,
    quality_assessment     TEXT,
    scope_delivered_pct    TEXT,
    court_file_ref         TEXT,
    supplementary_file_ref TEXT,
    case_officer_notes_ref TEXT,
    witness_statements_ref TEXT,
    expert_opinions_ref    TEXT,
    evidence_ref           TEXT,
    storage_location       TEXT,
    retention_period       TEXT,
    links                  TEXT,
    notes                  TEXT    -- JSON
);

-- INDEXES for performance
CREATE INDEX IF NOT EXISTS idx_tasks_project    ON tasks(project_id);
CREATE INDEX IF NOT EXISTS idx_tasks_parent     ON tasks(parent_task_id);
CREATE INDEX IF NOT EXISTS idx_tasks_assignee   ON tasks(assignee_id);
CREATE INDEX IF NOT EXISTS idx_incidents_project ON incidents(project_id);
CREATE INDEX IF NOT EXISTS idx_meetings_task    ON meetings(task_id);
CREATE INDEX IF NOT EXISTS idx_milestones_project ON milestones(project_id);
)SQL"; }

std::string workflowSchema() { return R"SQL(
CREATE TABLE IF NOT EXISTS workflow_definitions (
    workflow_def_id             TEXT PRIMARY KEY,
    name                        TEXT NOT NULL,
    version                     TEXT,
    entity_type                 TEXT,
    description                 TEXT,
    initial_state               TEXT,
    terminal_states             TEXT,
    parallel_approval_allowed   INTEGER DEFAULT 0,
    sla_enforced                INTEGER DEFAULT 0,
    default_sla_hours           INTEGER,
    escalation_policy           TEXT,
    created_by                  TEXT,
    created_date                TEXT,
    effective_from              TEXT,
    effective_to                TEXT,
    status                      TEXT DEFAULT 'active',
    notes                       TEXT
);

CREATE TABLE IF NOT EXISTS workflow_states (
    state_id            TEXT PRIMARY KEY,
    workflow_def_id     TEXT,
    name                TEXT NOT NULL,
    label               TEXT,
    state_type          TEXT,
    is_initial          INTEGER DEFAULT 0,
    is_terminal         INTEGER DEFAULT 0,
    requires_approval   INTEGER DEFAULT 0,
    notifies_on_entry   INTEGER DEFAULT 0,
    notifies_on_exit    INTEGER DEFAULT 0,
    sla_hours           INTEGER,
    allowed_roles       TEXT,
    notes               TEXT
);

CREATE TABLE IF NOT EXISTS workflow_transitions (
    transition_id   TEXT PRIMARY KEY,
    workflow_def_id TEXT,
    from_state_id   TEXT,
    to_state_id     TEXT,
    trigger_event   TEXT,
    condition       TEXT,
    required_role   TEXT,
    requires_comment INTEGER DEFAULT 0,
    auto_trigger    INTEGER DEFAULT 0,
    notes           TEXT
);

CREATE TABLE IF NOT EXISTS workflow_instances (
    instance_id         TEXT PRIMARY KEY,
    workflow_def_id     TEXT,
    entity_type         TEXT NOT NULL,  -- 'project','task','incident', etc.
    entity_id           TEXT NOT NULL,
    current_state_id    TEXT,
    previous_state_id   TEXT,
    initiated_by        TEXT,
    initiated_date      TEXT,
    due_date            TEXT,
    completed_date      TEXT,
    sla_hours           INTEGER,
    sla_breached        INTEGER DEFAULT 0,
    sla_breach_date     TEXT,
    priority            TEXT,
    status              TEXT DEFAULT 'active',
    escalation_level    TEXT,
    escalated_to        TEXT,
    escalated_date      TEXT,
    outcome             TEXT,
    notes               TEXT
);

CREATE TABLE IF NOT EXISTS workflow_participants (
    participant_id  TEXT PRIMARY KEY,
    instance_id     TEXT NOT NULL,
    person_id       TEXT NOT NULL,
    role            TEXT NOT NULL,  -- initiator|approver|co-approver|reviewer|watcher|informed|delegate|escalation-target
    delegation_from TEXT,
    delegated_from  TEXT,
    delegated_to    TEXT,
    active          INTEGER DEFAULT 1,
    notes           TEXT
);

CREATE TABLE IF NOT EXISTS workflow_actions (
    action_id       TEXT PRIMARY KEY,
    instance_id     TEXT NOT NULL,
    transition_id   TEXT,
    actor_id        TEXT NOT NULL,
    acting_as_role  TEXT,
    action_type     TEXT,
    decision        TEXT,
    comment         TEXT,
    action_date     TEXT,
    on_behalf_of    INTEGER DEFAULT 0,
    on_behalf_of_id TEXT,
    attachments     TEXT,
    notes           TEXT
);

CREATE TABLE IF NOT EXISTS workflow_notifications (
    notification_id   TEXT PRIMARY KEY,
    instance_id       TEXT NOT NULL,
    recipient_id      TEXT,
    notification_type TEXT,
    channel           TEXT,
    subject           TEXT,
    body              TEXT,
    scheduled_date    TEXT,
    sent_date         TEXT,
    sent              INTEGER DEFAULT 0,
    acknowledged      INTEGER DEFAULT 0,
    acknowledged_date TEXT,
    notes             TEXT
);

CREATE TABLE IF NOT EXISTS workflow_sla_log (
    sla_log_id        TEXT PRIMARY KEY,
    instance_id       TEXT NOT NULL,
    state_id          TEXT,
    entered_date      TEXT,
    exited_date       TEXT,
    sla_hours_allowed INTEGER,
    actual_hours_spent INTEGER,
    breached          INTEGER DEFAULT 0,
    breach_reason     TEXT,
    notes             TEXT
);

CREATE TABLE IF NOT EXISTS approvals (
    approval_id          TEXT PRIMARY KEY,
    workflow_instance_id TEXT,
    workflow_action_id   TEXT,
    project_id           TEXT,
    document_id          TEXT,
    change_request_id    TEXT,
    milestone_id         TEXT,
    quality_gate_id      TEXT,
    requested_by         TEXT,
    approver_id          TEXT,
    title                TEXT,
    approval_type        TEXT,
    status               TEXT DEFAULT 'pending',
    requested_date       TEXT,
    due_date             TEXT,
    decided_date         TEXT,
    decision             TEXT,
    conditions           TEXT,
    rationale            TEXT,
    links                TEXT,
    notes                TEXT
);

CREATE INDEX IF NOT EXISTS idx_wf_instances_entity ON workflow_instances(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_wf_participants_inst ON workflow_participants(instance_id);
CREATE INDEX IF NOT EXISTS idx_wf_actions_inst      ON workflow_actions(instance_id);
)SQL"; }

std::string documentsSchema() { return R"SQL(
CREATE TABLE IF NOT EXISTS documents (
    document_id            TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    author_id              TEXT,
    approved_by            TEXT,
    doc_type               TEXT,
    doc_category           TEXT,
    title                  TEXT NOT NULL,
    version                TEXT DEFAULT '1.0',
    date_created           TEXT,
    date_modified          TEXT,
    date_approved          TEXT,
    date_expires           TEXT,
    status                 TEXT DEFAULT 'draft',
    classification         TEXT,
    volume_number          INTEGER DEFAULT 1,
    page_count             INTEGER,
    language               TEXT DEFAULT 'EN',
    format                 TEXT,
    file_path              TEXT,
    file_url               TEXT,
    external_ref           TEXT,
    storage_system         TEXT,
    tags                   TEXT,
    summary                TEXT,
    links                  TEXT,
    notes                  TEXT,   -- JSON
    created_at             TEXT DEFAULT (datetime('now')),
    updated_at             TEXT DEFAULT (datetime('now'))
);

-- Polymorphic attachment: any entity can reference any document
CREATE TABLE IF NOT EXISTS entity_documents (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    document_id TEXT NOT NULL,
    relationship TEXT DEFAULT 'attached'
);

CREATE TABLE IF NOT EXISTS communication_plans (
    plan_id                TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    owner_id               TEXT,
    version                TEXT DEFAULT '1.0',
    created_date           TEXT,
    approved_date          TEXT,
    objectives             TEXT,
    escalation_path        TEXT,
    links                  TEXT,
    notes                  TEXT
);

CREATE TABLE IF NOT EXISTS communication_items (
    item_id              TEXT PRIMARY KEY,
    plan_id              TEXT NOT NULL,
    sender_id            TEXT,
    audience             TEXT,
    title                TEXT,
    content_description  TEXT,
    format               TEXT,
    channel              TEXT,
    frequency            TEXT,
    next_due_date        TEXT,
    last_sent_date       TEXT,
    status               TEXT DEFAULT 'active',
    links                TEXT,
    notes                TEXT
);

CREATE INDEX IF NOT EXISTS idx_docs_project  ON documents(project_id);
CREATE INDEX IF NOT EXISTS idx_docs_task     ON documents(task_id);
CREATE INDEX IF NOT EXISTS idx_ent_docs_key  ON entity_documents(entity_type, entity_id);
)SQL"; }

std::string trackingSchema() { return R"SQL(
-- Trackable items (ise-cobra model):
-- States: planned | focused | due | archived
-- A trackable item can be attached to any entity via entity_type + entity_id.
-- Trackable items can have child trackable items (recursive tasks).

CREATE TABLE IF NOT EXISTS trackable_items (
    trackable_id      TEXT PRIMARY KEY,
    entity_type       TEXT NOT NULL,  -- 'project','task','incident','risk', etc.
    entity_id         TEXT NOT NULL,
    parent_trackable_id TEXT,
    title             TEXT NOT NULL,
    description       TEXT,
    status            TEXT DEFAULT 'planned',  -- planned|focused|due|archived
    priority          TEXT,
    focus_date        TEXT,      -- date this should be actively worked (ise-cobra: focused)
    due_date          TEXT,      -- hard deadline
    archived_date     TEXT,      -- when completed/archived (ise-cobra: archived)
    planned_date      TEXT,      -- when it was planned to start (ise-cobra: planned)
    assignee_id       TEXT,
    created_by        TEXT,
    created_at        TEXT DEFAULT (datetime('now')),
    updated_at        TEXT DEFAULT (datetime('now')),
    notes             TEXT       -- JSON (array of note objects)
);

CREATE TABLE IF NOT EXISTS notes (
    note_id      TEXT PRIMARY KEY,
    entity_type  TEXT NOT NULL,  -- any entity type
    entity_id    TEXT NOT NULL,
    author_id    TEXT,
    content      TEXT NOT NULL,  -- stored as JSON: {text, format, attachments[]}
    note_type    TEXT DEFAULT 'general',  -- general|decision|action|reminder
    created_at   TEXT DEFAULT (datetime('now')),
    updated_at   TEXT DEFAULT (datetime('now')),
    is_pinned    INTEGER DEFAULT 0,
    is_private   INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS reminders (
    reminder_id      TEXT PRIMARY KEY,
    trackable_id     TEXT,
    entity_type      TEXT,
    entity_id        TEXT,
    assignee_id      TEXT,
    title            TEXT NOT NULL,
    message          TEXT,
    remind_at        TEXT NOT NULL,
    is_recurring     INTEGER DEFAULT 0,
    recurrence_rule  TEXT,    -- e.g. 'DAILY','WEEKLY','MONTHLY'
    last_triggered   TEXT,
    next_trigger     TEXT,
    is_dismissed     INTEGER DEFAULT 0,
    dismissed_at     TEXT,
    channel          TEXT DEFAULT 'console',  -- console|email|qt-notification
    notes            TEXT    -- JSON
);

-- QTCS dimension tables (multi-assignable to F16/F22/F18)
CREATE TABLE IF NOT EXISTS quality_dimensions (
    quality_id     TEXT PRIMARY KEY,
    title          TEXT NOT NULL,
    description    TEXT,
    standard       TEXT,
    criteria       TEXT,
    target         TEXT,
    actual         TEXT,
    status         TEXT DEFAULT 'draft',
    owner_id       TEXT,
    review_date    TEXT,
    notes          TEXT    -- JSON
);

CREATE TABLE IF NOT EXISTS time_dimensions (
    time_id              TEXT PRIMARY KEY,
    title                TEXT NOT NULL,
    description          TEXT,
    planned_start        TEXT,
    planned_end          TEXT,
    actual_start         TEXT,
    actual_end           TEXT,
    duration_planned     INTEGER,
    duration_actual      INTEGER,
    variance_days        INTEGER,
    schedule_reserve     INTEGER,
    status               TEXT DEFAULT 'draft',
    owner_id             TEXT,
    notes                TEXT    -- JSON
);

CREATE TABLE IF NOT EXISTS cost_dimensions (
    cost_id         TEXT PRIMARY KEY,
    title           TEXT NOT NULL,
    description     TEXT,
    budget_planned  REAL,
    budget_actual   REAL,
    budget_reserve  REAL,
    cost_type       TEXT,  -- 'labour','material','external','travel','contingency'
    currency        TEXT DEFAULT 'EUR',
    cost_center     TEXT,
    status          TEXT DEFAULT 'draft',
    owner_id        TEXT,
    notes           TEXT   -- JSON
);

CREATE TABLE IF NOT EXISTS scope_dimensions (
    scope_id        TEXT PRIMARY KEY,
    title           TEXT NOT NULL,
    description     TEXT,
    in_scope        TEXT,
    out_of_scope    TEXT,
    assumptions     TEXT,
    constraints     TEXT,
    change_count    INTEGER DEFAULT 0,
    version         TEXT DEFAULT '1.0',
    status          TEXT DEFAULT 'draft',
    owner_id        TEXT,
    last_changed    TEXT,
    notes           TEXT   -- JSON
);

-- Change requests
CREATE TABLE IF NOT EXISTS change_requests (
    cr_id                  TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    raised_by              TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    change_type            TEXT,
    status                 TEXT DEFAULT 'draft',
    raised_date            TEXT,
    decision_date          TEXT,
    implemented_date       TEXT,
    cost_impact            REAL,
    schedule_impact_days   INTEGER,
    scope_impact           TEXT,
    quality_impact         TEXT,
    justification          TEXT,
    decision_rationale     TEXT,
    links                  TEXT,
    notes                  TEXT   -- JSON
);

-- Assumption & Constraint
CREATE TABLE IF NOT EXISTS assumption_constraints (
    ac_id                  TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    owner_id               TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    ac_type                TEXT,  -- 'assumption' | 'constraint'
    category               TEXT,
    dimension              TEXT,  -- 'scope'|'time'|'cost'|'quality'|'resource'
    status                 TEXT DEFAULT 'active',
    identified_date        TEXT,
    review_date            TEXT,
    validation_method      TEXT,
    validated_date         TEXT,
    validated_by           TEXT,
    breached               INTEGER DEFAULT 0,
    breached_date          TEXT,
    impact_if_wrong        TEXT,
    mitigation             TEXT,
    links                  TEXT,
    notes                  TEXT   -- JSON
);

CREATE INDEX IF NOT EXISTS idx_trackable_entity  ON trackable_items(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_trackable_parent  ON trackable_items(parent_trackable_id);
CREATE INDEX IF NOT EXISTS idx_notes_entity      ON notes(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_reminders_entity  ON reminders(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_reminders_trigger ON reminders(next_trigger);
)SQL"; }

std::string reportingSchema() { return R"SQL(
CREATE TABLE IF NOT EXISTS risks (
    risk_id                TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    owner_id               TEXT,
    identified_by          TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    category               TEXT,
    subcategory            TEXT,
    risk_type              TEXT,
    status                 TEXT DEFAULT 'open',
    identified_date        TEXT,
    review_date            TEXT,
    closed_date            TEXT,
    probability_score      INTEGER,
    impact_score_time      INTEGER,
    impact_score_cost      INTEGER,
    impact_score_quality   INTEGER,
    impact_score_scope     INTEGER,
    overall_risk_score     INTEGER,
    risk_level             TEXT,
    response_strategy      TEXT,
    contingency_plan       TEXT,
    cost_reserve           REAL,
    schedule_reserve_days  INTEGER,
    trigger_condition      TEXT,
    early_warning          TEXT,
    residual_risk_level    TEXT,
    escalated              INTEGER DEFAULT 0,
    escalated_to           TEXT,
    links                  TEXT,
    notes                  TEXT   -- JSON
);

CREATE TABLE IF NOT EXISTS measures (
    measure_id             TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    risk_id                TEXT,   -- soft ref to risks
    incident_id            TEXT,
    project_id             TEXT,
    task_id                TEXT,
    owner_id               TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    measure_type           TEXT,  -- preventive|corrective|detective|directive
    measure_category       TEXT,
    status                 TEXT DEFAULT 'planned',
    planned_date           TEXT,
    actual_date            TEXT,
    cost_planned           REAL,
    cost_actual            REAL,
    effort_hrs             REAL,
    effectiveness          TEXT,
    verification_method    TEXT,
    verified_date          TEXT,
    verified_by            TEXT,
    outcome                TEXT,
    links                  TEXT,
    notes                  TEXT   -- JSON
);

CREATE TABLE IF NOT EXISTS quality_gates (
    gate_id                TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    reviewer_id            TEXT,
    title                  TEXT NOT NULL,
    phase                  TEXT,
    planned_date           TEXT,
    actual_date            TEXT,
    criteria               TEXT,
    standards_applied      TEXT,
    quality_objectives     TEXT,
    acceptance_criteria    TEXT,
    audit_schedule         TEXT,
    tools_methods          TEXT,
    result                 TEXT,
    decision               TEXT,
    findings               TEXT,
    links                  TEXT,
    notes                  TEXT   -- JSON
);

CREATE TABLE IF NOT EXISTS kpis (
    kpi_id                  TEXT PRIMARY KEY,
    project_id              TEXT,
    task_id                 TEXT,
    owner_id                TEXT,
    title                   TEXT NOT NULL,
    description             TEXT,
    category                TEXT,
    dimension               TEXT,
    unit                    TEXT,
    target_value            REAL,
    actual_value            REAL,
    baseline_value          REAL,
    threshold_green         REAL,
    threshold_amber         REAL,
    threshold_red           REAL,
    rag_status              TEXT,
    measurement_method      TEXT,
    measurement_frequency   TEXT,
    last_measured_date      TEXT,
    next_measurement_date   TEXT,
    trend                   TEXT,
    links                   TEXT,
    notes                   TEXT   -- JSON
);

CREATE TABLE IF NOT EXISTS lessons_learned (
    lesson_id              TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    incident_id            TEXT,
    submitted_by           TEXT,
    reviewed_by            TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    category               TEXT,
    dimension              TEXT,
    identified_date        TEXT,
    reviewed_date          TEXT,
    status                 TEXT DEFAULT 'draft',
    impact                 TEXT,
    recommendation         TEXT,
    action_taken           TEXT,
    added_to_kb            INTEGER DEFAULT 0,
    tags                   TEXT,
    links                  TEXT,
    notes                  TEXT   -- JSON
);

CREATE TABLE IF NOT EXISTS decision_log (
    decision_id            TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    meeting_id             TEXT,
    decided_by             TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    decision_type          TEXT,
    status                 TEXT DEFAULT 'open',
    decision_date          TEXT,
    review_date            TEXT,
    options_considered     TEXT,
    rationale              TEXT,
    impact_cost            TEXT,
    impact_schedule        TEXT,
    impact_scope           TEXT,
    impact_quality         TEXT,
    assumptions_made       TEXT,
    links                  TEXT,
    notes                  TEXT   -- JSON
);

CREATE INDEX IF NOT EXISTS idx_risks_project   ON risks(project_id);
CREATE INDEX IF NOT EXISTS idx_measures_risk   ON measures(risk_id);
CREATE INDEX IF NOT EXISTS idx_kpis_project    ON kpis(project_id);
CREATE INDEX IF NOT EXISTS idx_decisions_project ON decision_log(project_id);
CREATE INDEX IF NOT EXISTS idx_lessons_project ON lessons_learned(project_id);
)SQL"; }

} // namespace Schema
} // namespace RH
