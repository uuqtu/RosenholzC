# Rosenholz PM — C++ Project Management System

A modular C++17 project management system inspired by the DDR MfS Rosenholz 
archive card structure, with SQLite persistence, MFS-style file output, 
and a clean model layer ready for Qt/QML UI integration.

## Build

```bash
# Ubuntu/Debian
sudo apt install libsqlite3-dev nlohmann-json3-dev cmake g++

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Run tests

```bash
./rosenholz --basepath /path/to/data
# or for verbose debug output:
./rosenholz --basepath /path/to/data --debug
```

## Architecture

```
src/
├── core/
│   ├── Logger.h/.cpp       Verbosity-controlled logging (DEBUG/INFO/WARN/ERR)
│   ├── Config.h/.cpp       JSON settings + .rh project file support
│   ├── FileOps.h/.cpp      All filesystem ops (Linux + Windows)
│   ├── Database.h/.cpp     SQLite abstraction — 6 databases, all schemas
│   ├── RegNumber.h/.cpp    Structured reg number generator (F16/F22/F18/...)
│   └── BackupManager.h/.cpp  Configurable DB + MFS backup with pruning
├── model/
│   ├── Utils.h             Shared nowIso(), genId(), ton() helpers
│   ├── Trackable.h/.cpp    ise-cobra state machine (planned/focused/due/archived)
│   ├── Person.h/.cpp       Person entity (core.db)
│   ├── Team.h/.cpp         Team + TeamMember with categorisation (core.db)
│   ├── ProjectF16.h/.cpp   Project entity with EV metrics, QTCS (projects.db)
│   ├── TaskF22.h/.cpp      Task with subtask hierarchy (projects.db)
│   ├── IncidentF18.h/.cpp  Incident record (projects.db)
│   ├── Risk.h/.cpp         Risk register (reporting.db)
│   └── Document.h/.cpp     Documents + URL archiving (documents.db)
├── mfs/
│   └── MFSWriter.h/.cpp    MFS-style plaintext output (chmod 600, owner key)
└── app/
    └── AppController.h/.cpp  CLI/UI mode switch, Qt log hook
```

## Databases

| File | Contents |
|------|----------|
| `core.db` | persons, teams, team_members, reg_number_sequences, project_types |
| `projects.db` | F16 projects, F22 tasks, F18 incidents, milestones, meetings, dependencies |
| `workflow.db` | workflow engine: definitions, states, transitions, instances, actions |
| `documents.db` | documents, entity_documents, communication plans |
| `tracking.db` | trackable items, notes, reminders, QTCS dimensions, change requests |
| `reporting.db` | risks, measures, quality gates, KPIs, lessons learned, decisions |

## Key design decisions

- **QTCS dimensions** (Quality/Time/Cost/Scope) are multi-assignable via junction 
  tables to F16/F22/F18 — never mandatory
- **Trackable items** follow the ise-cobra model: planned → focused → due → archived
- **Notes** are stored as JSON arrays on all entities
- **MFS files** are owner-only (chmod 600); only `owner_key.txt` maps reg numbers 
  to real names
- All 6 databases use **WAL mode + mmap** for OneDrive concurrent access safety
- **No Qt dependencies** in the model layer — ready to wrap in QML

## Qt/QML integration (next step)

The model is pure C++17. To add QML:
1. Create a `QmlBridge` layer that wraps model classes as `QObject` subclasses
2. Call `AppController::instance().setQtLogCallback(...)` to route logs to QML
3. `AppController::init(settingsPath, rhFile, AppMode::UI)` before `QApplication`
