# Rosenholz PM v2 — Anforderungen je Objekt (vollständig)
## Basis: tatsächliche Code-Verbindungen, Methoden und Datenbankstruktur

---

## Namenskonventionen (vollständig konsistent)

| Kürzel | Bedeutung | Klasse | DB-Pool | DB-Datei | SQL-Schema | entityType | src-Ordner |
|---|---|---|---|---|---|---|---|
| **F16** | Projekt | `ProjectF16` | `f16` | `f16.db` | `f16.sql` | `"f16"` | `model/f16/` |
| **F22** | Aufgabe | `TaskF22` | `f16` | `f16.db` | `f16.sql` | `"f22"` | `model/f22/` |
| **F18** | Operation | `F18Operation` | `f18` | `f18.db` | `f18.sql` | `"f18"` | `model/f18/` |
| **F18-Step** | Op.-Schritt | `F18OperationStep` | `f18` | `f18.db` | `f18.sql` | — | `model/f18/` |
| **F77** | Freigabe-WFI | `WorkflowInstance` | `f77` | `f77.db` | `f77.sql` | — | `workflow/` |
| **DOK** | Dokument | `Document` | `dok` | `dok.db` | `dok.sql` | `"dok"` | `model/dok/` |
| **PER** | Person | `Person` | `core` | `core.db` | `core.sql` | — | `model/` |
| **DE** | Diensteinheit | `Team` | `core` | `core.db` | `core.sql` | — | `model/` |

F16 und F22 teilen sich `f16.db` (projektbezogene Daten). F77 (Freigabe-Workflow-Engine) hat die eigene `f77.db`. DOK hat `dok.db`.

---

## Quellcode-Struktur

```
src/
├── app/                    AppController — Bootstrap, Lifecycle
├── cli/                    Alle CLI-Menüs und Wizards
│   ├── main_cli.cpp        Hauptmenü
│   ├── ProjectMenu.cpp     F16-Menü
│   ├── TaskMenu.cpp        F22-Menü
│   ├── F18Menu.cpp         F18-Operations-Menü und -Browser
│   ├── DocumentMenu.cpp    DOK-Menü
│   ├── WorkflowMenu.cpp    F77/WFI-Menü
│   ├── Utilities.cpp       Wizards (createDocumentWizard, createF18Wizard, ...)
│   └── cli_common.h        Shared-Deklarationen
├── core/                   Infrastruktur (kein Business-Logic)
│   ├── Database.cpp/h      SQLite-Pool: core/f16/f77/dok/f18/tracking
│   ├── Config.cpp/h        settings.json (Pfade, Registratur, Backup)
│   ├── Migration.cpp/h     Schema-Versionen und Delta-Migrationen
│   ├── RegNumber.cpp/h     Registriernummer-Generator (XV/F16/0001/2026)
│   ├── FileOps.cpp/h       Dateisystem-Operationen und MFS-Baum
│   ├── Logger.cpp/h        Logging (DEBUG/INFO/WARN/ERR)
│   └── BackupManager.cpp/h SQLite- und MFS-Backup
├── mfs/                    Physische Ablage (DDR-Aktenstruktur)
│   └── MFSWriter.cpp/h     writeProject/writeTask/writeF18/writeF77/writeDocument
├── model/
│   ├── f16/                F16 Projekt
│   │   ├── ProjectF16.h    Felder, Methoden, Statische Factory
│   │   └── ProjectF16.cpp
│   ├── f22/                F22 Aufgabe
│   │   ├── TaskF22.h
│   │   └── TaskF22.cpp
│   ├── f18/                F18 Operationen und Kommunikation
│   │   ├── F18Operation.h/cpp
│   │   ├── F18OperationStep.h/cpp
│   │   └── Communication.h/cpp
│   ├── dok/                Dokument
│   │   ├── Document.h
│   │   └── Document.cpp
│   ├── Person.h/cpp        Person (in core.db)
│   ├── Team.h/cpp          Diensteinheit/TeamMember (in core.db)
│   └── Utils.h             Shared-Utilities (genId, nowIso, sanitiseRegNr, ...)
├── repository/             LMDB-Inhaltsspeicher und Dokument-Revisionen
│   ├── ArchiveStore.h/cpp  LMDB binärer Content-Store
│   └── DocumentRevision.h/cpp  Revisions-Lifecycle (5 Zustände)
└── workflow/               F77 Freigabe-Workflow-Engine
    └── WorkflowEngine.h/cpp  Alle WFI/WFA-Operationen (stateless)
```

---

## F16 — Projekt

### Anlegen
- `ProjectF16::create(title, projectType="OV", sizeClass="medium", createdBy="")` — neue Instanz; DDR-Registriernummer (`XV/F16/nnnn/yyyy`) automatisch vergeben; `ensureReleaseWorkflow()` legt sofort den F77-WFI an.

### Felder
| Gruppe | Felder |
|---|---|
| Identifikation | `projectId`, `regNumber`, `title`, `codename`, `projectType`, `sizeClass` |
| Steuerung | `status`, `phase`, `priority`, `complexity`, `strategicAlignment`, `classification` |
| Termine | `startDatePlanned/Actual`, `endDatePlanned/Actual`, `durationPlannedDays`, `durationActualDays`, `scheduleVarianceDays` |
| Finanzen | `budgetPlanned/Approved/Committed/Actual`, `costVariance`, `currency` |
| Earned Value | `earnedValue`, `plannedValue`, `actualCost`, `cpi`, `spi`, `eac`, `etc`, `vac` |
| Scope | `scopeStatement`, `scopeVersion`, `scopeLastChanged`, `scopeChangeReason`, `scopeChangeCount` |
| F77-Bezug | `releaseWorkflowId`, `workflowInstanceId`, `workflowStatus`, `workflowCurrentState` |
| Sonstige | `qualityGateId`, `communicationPlanId`, `externalRef`, `links`, `milestones` |
| QTCS | `qualityIds[]`, `costIds[]`, `timeIds[]`, `scopeIds[]` |

### Methoden
- `save()`, `update()`, `load(id)`, `remove()`
- `reassignLead(id)`, `reassignTeam(id)`, `reassignSponsor(id)`, `reassignWorkflowInstance(id)`
- `recalcEarnedValue()` — CPI, SPI, EAC, ETC, VAC neu berechnen
- `addQuality/Cost/Time/Scope(id)`, `removeQuality/Cost/Time/Scope(id)` — QTCS-Verknüpfungen
- `convertToTask(parentProjectId)` — F16 → neue F22 umwandeln
- `ensureReleaseWorkflow()` — F77-WFI anlegen falls fehlend
- `writeMFSFile(mfsRoot)` — `mfs/F16/<reg>/00_DECKBLATT.txt` schreiben
- `fromJson(j)` — JSON-Deserialisierung

### Abfragen (statisch)
- `loadById(projectId)`, `loadAll()`, `loadRecent(n=20)`, `loadByStatus(status)`, `fromJson(j)`

### Verbundene Objekte
- **F22** anlegen: `TaskF22::create(projectId, ...)` — beliebig viele, hierarchisch
- **DOK** anlegen: `Document::create(title, type, projectId)` — Pflichtbezug auf dieses Projekt
- **F18** anlegen: `F18Operation::create(projectId, title, type)` — alle 11 Typen
- **Communication** anlegen: `Communication::create(projectId, "f16", title, type)`
- **Zusätzliche F77/WFIs** starten: `WorkflowEngine::startAdHoc(...)` oder `startFromTemplate(...)`

### Lifecycle (F77)
- `canReleaseEntity("f16", id, releaseWfiId, count)` — prüft Blocker
- `lockAllOpenWorkflows("f16", id, releaseWfiId, true)` — alle offenen WFIs sperren
- End-Schritt des F77 → `releaseEntity("f16", id)` → `status = "released"`
- Nach `released`: keine neuen Unterentitäten oder WFIs mehr möglich

### MFS
- `mfs/F16/<reg>/00_DECKBLATT.txt` mit Querverweisen auf alle F22/F18/DOK und F77

---

## F22 — Aufgabe

### Anlegen
- `TaskF22::create(projectId, title, assigneeId="", parentTaskId="")` — immer unter F16; Registriernummer `XV/F22/nnnn/yyyy`; `ensureReleaseWorkflow()` legt F77-WFI an.

### Felder
| Gruppe | Felder |
|---|---|
| Identifikation | `taskId`, `regNumber`, `projectId`, `parentTaskId`, `taskCode`, `title`, `description`, `taskType` |
| Zuweisung | `assigneeId`, `assignedBy` |
| Steuerung | `status`, `priority`, `percentComplete` |
| F77-Bezug | `releaseWorkflowId`, `workflowInstanceId`, `workflowStatus`, `workflowCurrentState` |
| Termine | `startDatePlanned/Actual`, `dueDatePlanned/Actual`, `scheduleVarianceDays` |
| Aufwand | `effortPlannedHrs`, `effortActualHrs`, `costPlanned`, `costActual` |
| Qualität | `qualityCriteria`, `acceptanceCriteria`, `wbsCode`, `sprintOrPhase`, `milestones` |
| QTCS | `qualityIds[]`, `costIds[]`, `timeIds[]`, `scopeIds[]` |

### Methoden
- `save()`, `update()`, `load(id)`, `remove()`
- `reassignTo(personId)`, `reassignToProject(projectId)`, `reassignParent(parentTaskId)`
- `addQuality/Cost/Time/Scope(id)` — QTCS
- `convertToProject(projectType="OV")` — F22 → neues F16 umwandeln
- `ensureReleaseWorkflow()`
- `writeMFSFile(mfsRoot)`

### Abfragen (statisch)
- `loadById(taskId)`, `loadForProject(projectId)`, `loadChildren(parentTaskId)`, `loadRecent(n=20)`, `fromJson(j)`

### Verbundene Objekte
- **Teilaufgaben** (weitere F22 mit `parentTaskId`) — beliebige Tiefe
- **DOK** anlegen: `Document::create(title, type, projectId)` + `doc->taskId = taskId`
- **F18** anlegen: `F18Operation::create(projectId, title, type, taskId)`
- **Communication** anlegen: `Communication::create(taskId, "f22", title, type)`
- **Zusätzliche WFIs** starten

### Lifecycle (F77)
- Identisch zu F16: `canReleaseEntity("f22", ...)`, `lockAllOpenWorkflows("f22", ...)`, End-Schritt → `released`

### MFS
- `mfs/F22/<reg>/00_KARTE.txt` — Rückverweis auf F16, Eltern-F22, Bearbeiter

---

## F18 — Operation (alle 11 Typen)

### Anlegen
- `F18Operation::create(projectId, title, type=F18OperationType::GENERIC, taskId="")` — Pflichtbezug F16; Init-Step sofort `approved`; End-Step `pending`; F77-WFI automatisch angelegt.

### 11 Typen (`F18OperationType::*`)

| Konstante | Wert | Domäne |
|---|---|---|
| `INCIDENT` | `"incident"` | Vorfälle, Probleme |
| `RISK` | `"risk"` | Risiken (Score automatisch berechnet) |
| `MEASURE` | `"measure"` | Maßnahmen |
| `QUALITY_GATE` | `"qualityGate"` | Qualitätstore |
| `ASSUMPTION_CONSTRAINT` | `"assumptionConstraint"` | Annahmen & Beschränkungen |
| `COMMUNICATION_PLAN` | `"communicationPlan"` | Kommunikationspläne |
| `LESSONS_LEARNED` | `"lessonsLearned"` | Lernerkenntnisse |
| `DECISION_LOG` | `"decisionLog"` | Entscheidungsprotokoll |
| `CHANGE_REQUEST` | `"changeRequest"` | Änderungsanträge |
| `CHANGE_OBJECT` | `"changeObject"` | Ausgeführte Änderungen (→ ChangeRequest via `parentVorgangId`) |
| `GENERIC` | `"generic"` | Allgemein |

### Basisfelder (alle Typen)
`vorgangId`, `vorgangType`, `projectId`, `taskId`, `parentVorgangId`, `title`, `description`, `status`, `priority`, `ownerId`, `releaseWorkflowId`, `links`, `createdAt`, `updatedAt`

### Typspezifische Felder

**incident:** `severity`, `occurredDate`, `resolvedDate`, `rootCause`, `immediateAction`, `resolution`, `costImpact`, `scheduleImpactDays`, `scopeImpact`, `qualityImpact`

**risk:** `probabilityScore` (1–5), `impactScoreTime/Cost/Quality/Scope` (je 1–5), `overallRiskScore` (automatisch: `probabilityScore × max(impactScores)`), `riskLevel` (auto), `responseStrategy`, `contingencyPlan`, `triggerCondition`, `residualRiskLevel`, `costReserve`, `scheduleReserveDays`

**measure:** `measureCategory`, `plannedDate`, `actualDate`, `effectiveness`, `verificationMethod`, `verifiedDate`, `verifiedBy`

**qualityGate:** `phase`, `criteria`, `acceptanceCriteria`, `findings`, `gateResult` (passed/failed/conditional/pending), `gateDecision` (proceed/hold/stop), `validatedDate`, `validatedBy`

**assumptionConstraint:** `constraintType` (assumption/constraint), `impact`, `validatedDate`

**communicationPlan:** `audience`, `frequency`, `channel`, `responsible`

**lessonsLearned:** `learnType`, `recommendation`, `applicablePhases`

**decisionLog:** `decisionType`, `rationale`, `decisionDate`, `decisionBy`, `alternativesConsidered`

**changeRequest:** `changeType`, `crImpact`, `raisedDate`, `crDecisionDate`, `crDecisionRationale`, `crScheduleImpactDays`

**changeObject:** `executedBy`, `executionDate` (+ `parentVorgangId` → zugehöriger ChangeRequest)

### Methoden
- `save()`, `update()`, `remove()`, `load(id)`
- `addNote(authorId, text)` — Notiz anfügen (JSON-Array in `notes`)
- `recalcRiskScore()` — für `risk`: Score und Level neu berechnen
- `ensureReleaseWorkflow()`
- `loadSteps()` — `steps[]` aus DB laden

### Abfragen (statisch)
- `loadById(vorgangId)`, `loadForProject(projectId, typeFilter="")`, `loadForTask(taskId, typeFilter="")`, `loadRecent(n=20)`

### Verbundene Objekte
- **DOK** anlegen: `doc->f18OperationId = vorgangId` oder `doc->f18StepId = stepId`
- **Communication** anlegen: `Communication::create(vorgangId, "f18", ...)` oder `Communication::create(stepId, "f18step", ...)`

### F18OperationStep — Schrittkette (ausschließlich sequentiell)

**Struktur:** `[Init ✓] → [Step 1] → [Step 2] → … → [End]`

**Init** ist immer `approved` bei Anlage. **End** wartet auf den letzten Zwischen-Schritt.

**Schritte hinzufügen — zwei Modi:**
- `addStep(title, stepType, assigneeId)` — **Standard**: neuer Schritt vor dem End-Schritt; Vorgänger = letzter Zwischen-Schritt (oder Init)
- `insertAfter(predecessorStepId, title, stepType, assigneeId)` — **Custom**: exakt zwischen Vorgänger und dessen bisherigem Nachfolger einfügen; Nachfolger zeigt danach auf neuen Schritt

**Schritt-Felder:**
`stepId`, `vorgangId`, `title`, `description`, `stepType`, `sequenceOrder`, `predecessorStepIds`, `isInitialize`, `isFinal`, `assignedTo`, `requiredRole`, `assignedToGroup`, `dueDate`, `focusDate`, `inWorkSince`, `startedDate`, `completedDate`, `slaHours`, `slaBreached`, `status`, `autoApprove`, `requiresComment`, `requiresDocument`, `decision`, `decisionBy`, `decisionDate`, `comment`, `trackingStatus`, `percentComplete`, `progressNote`, `priority`, `notes`

**Status-Sequenz:** `pending → in_progress → approved | rejected | skipped`

**Tracking — vollautomatisch** via `computeTrackingStatus()`:

| `trackingStatus` | Bedingung |
|---|---|
| `archived` | `status` ∈ {approved, rejected, skipped, cancelled} |
| `in_work` | `inWorkSince` ≠ leer |
| `due` | Heute > `dueDate` |
| `focused` | Heute > `focusDate` (und `dueDate` noch nicht überschritten) |
| `planned` | Keines der obigen |

Regel: `focusDate` muss vor `dueDate` liegen. Tracking-Felder nie manuell setzen.

**Abfragen (statisch):** `loadById(stepId)`, `loadForVorgang(vorgangId)`

**`canStart(allSteps)`** — Vorgänger-Prüfung: alle Schritte in `predecessorStepIds` müssen `isComplete()` sein.

### Lifecycle (F77)
- `ensureReleaseWorkflow()`, `canReleaseEntity("f18", ...)`, End-Schritt → `releaseEntity("f18", id)` → `status = "released"`

### MFS
- `mfs/F18/<vorgangId>/00_KARTE.txt` — Rückverweis auf F16, F22, Eltern-F18; `mfs/F18/<vorgangId>/DOK/<docId>/` für Dokumente

---

## DOK — Dokument

### Anlegen — Pflichtbezug (mindestens eines)
- `projectId` → F16-Projekt
- `taskId` → F22-Aufgabe
- `f18OperationId` → F18-Operation
- `f18StepId` → F18-Schritt

Ohne Bezug schlägt MFS-Filing fehl. Bei Anlage automatisch: **Revision 1** (Zustand `in_work`) + **F77-WFI** (`ensureReleaseWorkflow()`).

### Felder
| Gruppe | Felder |
|---|---|
| Identifikation | `documentId`, `releaseWorkflowId`, `workflowInstanceId/Status/CurrentState` |
| Bezüge | `projectId`, `taskId`, `f18OperationId`, `f18StepId` |
| Personen | `authorId`, `approvedBy` |
| Klassifikation | `docType`, `docCategory`, `classification` |
| Metadaten | `title`, `version`, `language`, `volumeNumber`, `pageCount` |
| Daten | `dateCreated/Modified/Approved/Expires` |
| Datei | `filePath`, `fileSize`, `fileHash` (SHA-256), `fileUrl`, `format` |
| Inhalt | `summary`, `tags`, `links`, `notes` |
| Checkout | `checkedOutPath` (leer wenn nicht ausgecheckt) |

### Methoden
- `save()`, `update()`, `load(id)`, `remove()`
- `attachToEntity(entityType, entityId, relationship)` — an beliebige Entität hängen
- `reassignAuthor(id)`, `reassignToProject(id)`, `reassignToTask(id)`
- `importLocalFile(srcPath)` — Datei in MFS kopieren, SHA-256 berechnen
- `refreshFromUrl()` — URL neu herunterladen (Checkout → Download → Checkin)
- `openFile(mode="read")` — Datei in Temp-Verzeichnis öffnen
- `ensureRevision1()` — Rev 1 in `in_work` anlegen (idempotent)
- `ensureReleaseWorkflow()`

### Checkout / Checkin

**`checkout(destDir="")`** → lokale Arbeitskopie:
1. Sucht Inhalt in LMDB (per SHA-256 aktiver Revision)
2. Fallback: kopiert `filePath` aus MFS
3. Fallback: erstellt leere Datei
4. Setzt `checkedOutPath`, gibt lokalen Pfad zurück

**`checkin(srcPath="")`** → Datei in Datenbank:
1. Liest von `srcPath` (oder `checkedOutPath`)
2. `ArchiveStore::stageContent()` → SHA-256-Staging in Temp
3. `DocumentRevision::createRevision()` → neue Revision in `in_work`
4. `ArchiveStore::commitContent()` → atomar in LMDB schreiben
5. Aktualisiert `fileHash`, `fileSize`, `filePath`
6. Löscht lokale ausgecheckte Datei, leert `checkedOutPath`

### Quellen beim Anlegen
- Lokale Datei (via `importLocalFile`)
- URL (via `archiveFromUrl` oder `refreshFromUrl`)
- Neue Datei — Formate: `.txt`, `.docx`, `.xlsx`, `.pptx`, `.pdf`, Weiteres

### Revisionen und 5-State-Lifecycle

Exakt eine Revision hält `superseded = false` (= aktiv). Priorität: neueste `released` > neuestes nicht-closed > neuestes `in_work`.

**Erlaubte Übergänge:**

| Von | Nach (erlaubt) |
|---|---|
| `in_work` | `pre_released`, `locked`, `closed` |
| `pre_released` | `released`, `locked`, `closed`, `in_work` |
| `released` | `locked`, `closed` |
| `locked` | `pre_released` (nur neueste Revision), `closed` |
| `closed` | — (terminal) |

**Revisions-API:**
- `DocumentRevision::createRevision(documentId, baseRev, createdBy, note)`
- `DocumentRevision::currentRevision(documentId)` — aktive Revision
- `DocumentRevision::loadAllRevisions(documentId)` — alle, neueste zuerst
- `DocumentRevision::loadByRev(documentId, rev)`
- `DocumentRevision::latestRevNumber(documentId)`
- `DocumentRevision::isTransitionAllowed(from, to)`
- `rev->transitionState(targetState)` — `superseded`-Flags atomar aktualisieren
- `rev->recomputeSuperseded()`

**Revisions-Felder:** `revId`, `documentId`, `rev` (uint32), `parentRev`, `revState`, `superseded`, `contentHash`, `contentSize`, `createdBy`, `changeNote`, `createdAt`, `updatedAt`

### LMDB-Inhaltsspeicher (ArchiveStore)
- `stageContent(srcPath, outTmp)` → `ChunkRef{sha256, size}` (temporär)
- `commitContent(tmpPath, ref, docId, rev)` → atomar in LMDB
- `retrieveContent(docId, rev, destPath)` → Inhalt exportieren
- `chunkExists(sha256)` → Deduplizierung prüfen
- `lookupRevChunk(docId, rev)` → SHA-256 nachschlagen
- `deleteRevMapping(docId, rev)` → Mapping löschen
- `ensureFreeSpace()` → bei < 1 GB LMDB-Map verdoppeln

### Abfragen (statisch)
- `loadById(documentId)`, `loadForProject(projectId)`, `loadForEntity(entityType, entityId)`, `loadRecent(n=20)`, `archiveFromUrl(url, projectId, authorId)`

### Lifecycle (F77)
- End-Schritt des F77 → Benutzer setzt `targetState` auf End-WFA → `transitionState(targetState)` auf aktiver Revision

### MFS
- `mfs/<ParentTyp>/<ParentId>/DOK/<docId>/<docId>.txt` + physische Datei — jedes Dokument in eigenem Unterordner

---

## F77 — Freigabe-Workflow (WorkflowInstance)

### Was ist der F77?
Der F77 Freigabe-Workflow ist eine `WorkflowInstance` mit Name `"F77 — <Entitätstitel>"`. Er ist der **einzige Weg** um den Status einer Entität (F16/F22/F18/DOK) auf `released` zu setzen. Jede dieser Entitäten hat exakt einen F77-WFI, der in `releaseWorkflowId` vermerkt ist.

### Erzeugen
- `WorkflowEngine::createReleaseWorkflow(entityType, entityId, entityTitle)` — wird automatisch von `ensureReleaseWorkflow()` aufgerufen:
  1. `startAdHoc(entityType, entityId, "F77 — "+title, "sequential", "system")`
  2. Init-WFA (autoApprove=true) → sofort per `tick()` genehmigt
  3. Zwischen-Schritt "Freigabe vorbereiten" (manuell zu feuern)
  4. End-WFA (isFinal=true) → auto-approved durch `tick()` wenn alle Zwischen-Schritte done

### Abschluss-Logik
`tick()` prüft nach jedem `fireAction()`:
- Alle Zwischen-Schritte abgeschlossen? → End-WFA auto-approved
- `checkAndCompleteInstance()` → `inst.status = "completed"`
- `syncEntityWorkflowFields()`:
  - F16/F22/F18: `releaseEntity()` → `status = "released"`
  - DOK: `rev->transitionState(targetState)` → Revisions-State wechselt

### Freigabe-Voraussetzungen
- `canReleaseEntity(entityType, entityId, releaseWfiId, blockerCount)` — alle Sub-WFIs completed oder locked?
- `lockAllOpenWorkflows(entityType, entityId, releaseWfiId, confirmLock=true)` — erzwingen
- `releaseEntity(entityType, entityId)` — direkt aufrufen (nach End-Schritt automatisch)

### MFS
- `MFSWriter::writeF77(wfiId, entityType, entityTitle, mfsRoot)` → `mfs/F77/<wfiId>/00_KARTE.txt`

---

## WFI / WFA — Allgemeine Workflow-Instanzen

Zusätzlich zum F77 können beliebig viele weitere `WorkflowInstance`-Objekte an eine Entität gehängt werden. Diese steuern keine Lifecycle-Zustände, können aber Genehmigungsprozesse, Prüfzyklen, Eskalationen etc. modellieren.

### Anlegen
- `WorkflowEngine::startAdHoc(entityType, entityId, name, execType="sequential", initiatedBy="")` — ohne Vorlage
- `WorkflowEngine::startFromTemplate(templateId, entityType, entityId, name, initiatedBy="")` — mit Vorlage

### WorkflowInstance-Felder
`instanceId`, `templateId`, `name`, `entityType`, `entityId`, `executionType`, `status`, `initiatedBy`, `initiatedDate`, `dueDate`, `completedDate`, `slaHours`, `slaBreached`, `slaBreachDate`, `escalatedTo`, `escalatedDate`, `priority`, `outcome`, `notes`, `actions[]`, `participants[]`

### WorkflowAction (WFA)-Felder
`actionId`, `instanceId`, `tplActionId`, `title`, `description`, `sequenceOrder`, `executionType`, `predecessorActionIds`, `status`, `isInitialize`, `isFinal`, `assignedTo`, `requiredRole`, `assignedToGroup`, `dueDate`, `startedDate`, `completedDate`, `slaHours`, `slaBreached`, `decision`, `decisionBy`, `decisionDate`, `comment`, `autoApprove`, `requiresComment`, `requiresDocument`, `requiresDecisionLogEntry`, `requiresLessonLearnedEntry`, `targetState` (nur End-DOK-WFI), `trackingStatus`, `plannedDate`, `focusDate`, `archivedDate`, `priority`, `progressNote`, `percentComplete`, `notes`

**Tracking auf WFA ist manuell** — im Gegensatz zu F18OperationStep ist kein `computeTrackingStatus()` vorhanden.

### Ausführungstypen

| Typ | Verhalten |
|---|---|
| `sequential` | Jeder WFA hat expliziten Vorgänger; strenge Reihenfolge |
| `parallel` | Alle WFAs gleichzeitig startbar; WFI completed wenn alle done |
| `free` | Beliebige Reihenfolge, keine Vorgänger-Einschränkung |

### Engine-Methoden
- `addAction(inst, title, execType, seqOrder, predecessors, assignedTo, dueDate, slaHours)`
- `fireAction(inst, actionId, decision, actor, comment)` — `canStart()` → Transaktion → `tick()`
- `tick(inst)` — auto-approved Init/autoApprove-WFAs, End wenn alle done
- `escalate(inst, escalateTo, reason)` — Eskalation
- `addParticipant(inst, personId, role)` — Rollen: `approver`, `reviewer`, `watcher`, `informed`, `delegate`

### Dokumente & Protokolle
- `attachDocumentToInstance(instanceId, documentId, relationship, notes)`
- `attachDocumentToAction(actionId, documentId, relationship)`
- `loadDocumentsForInstance(instanceId)`, `loadDocumentsForAction(actionId)`
- `createDecisionLogEntry(actionId, entityType, entityId, title, rationale)`
- `createLessonLearnedEntry(actionId, entityType, entityId, title, description)`

### Abfragen
- `WorkflowInstance::loadById(id)`, `loadForEntity(entityType, entityId)`, `loadActive()`, `loadBreached()`
- `searchInstances(entityType, status, nameContains, slaOnly)` — mehrdimensional

### Vorlagen (WorkflowTemplate)
- `WorkflowTemplate::create(name, execType)` — Vorlage anlegen
- Vorlage hat `templateActions[]` mit denselben Feldern wie WFA
- `loadAll()`, `loadForEntityType(entityType)`, `loadById(id)`
- `createStandardTemplates()` — Standard-Vorlagen seeden (idempotent):
  - `"Standardgenehmigung"` — 4-Schritt sequential
  - `"Projektabschluss"` — 4-Schritt mit auto-LL und auto-DL

---

## Communication

### Anlegen
- `Communication::create(ownerId, ownerType, title, commType="meeting")`
- `ownerType` ∈ `{"f16", "f22", "f18step"}` — genau einer

### Typen: `meeting`, `message`, `call`, `email`, `report`

### Felder
`commId`, `ownerId`, `ownerType`, `commType`, `title`, `agenda`, `scheduledDate`, `actualDate`, `durationMins`, `channel`, `location`, `organizer`, `participants`, `status` (`scheduled`/`completed`/`cancelled`), `decisions`, `actionItems`, `notes`

### Methoden
- `complete(decisions, actionItems)` → `status = "completed"`
- `update()`, `remove()`
- `loadForOwner(ownerId, ownerType)`, `loadRecent(n=20)`, `loadById(id)`

---

## Person

### Anlegen
- `Person::create(lastName, firstName, email, employmentType)` — Registriernummer `HVA/nnnn/yyyy`

### Felder
`personId`, `regNumber`, `lastName`, `firstName`, `preferredName`, `email`, `phone`, `orgUnit`, `department`, `location`, `country`, `roleTitle`, `employmentType`, `seniorityLevel`, `status`, `clearanceLevel`, `dayRate`, `monthlyRate`, `availabilityPct/From/To`, `onboardDate`, `offboardDate`, `externalRef`, `links`

### Abfragen
- `loadById(id)`, `loadByEmail(email)`, `loadAll()`, `search(nameFragment)`

### Persistenz
- `writePerson(p, mfsRoot)` → ausschließlich `owner_key.txt` (chmod 600), kein eigener Ordner

---

## Diensteinheit / Team

### Anlegen
- `Team::create(name, type, parentTeamId="")` — hierarchisch, Registriernummer `DE/nnnn/yyyy`

### Team-Felder
`teamId`, `regNumber`, `name`, `type`, `parentTeamId`, `status`, `description`, `missionStatement`, `location`, `headCountPlanned/Actual`

### Mitgliedschaft (TeamMember)
- `TeamMember::create(teamId, personId, role, startDate, endDate, allocPct)`
- Flags: `isLead`, `isDeputy`, `isCoreMemember`, `isExtendedMember`, `isObserver`
- Felder: `allocationPct`, `fteEquivalent`, `plannedHoursPerWeek`, `actualHoursPerWeek`, `costRate`, `costCenter`, `primarySkill`, `clearanceLevel`, `onboardedDate`, `offboardedDate`, `offboardingReason`
- `moveToTeam(newTeamId)`, `reassignRole(newRole, category)`

### Abfragen
- `loadById(id)`, `loadAll()`, `loadChildren(parentId)`, `TeamMember::loadForTeam(teamId)`, `loadForPerson(personId)`

### Persistenz
- `writeTeam(t, mfsRoot)` → ausschließlich `owner_key.txt`, kein eigener Ordner

---

## MFS-Dateibaum

### Struktur
```
mfs/
├── F16/<sanitised_regNr>/           Projektakte
│   ├── 00_DECKBLATT.txt             mit Querverweisen
│   └── DOK/<docId>/                 je Dokument eigener Ordner
│       ├── <docId>.txt              Metadaten-Karte
│       └── <filename>.<ext>         physische Datei
├── F22/<sanitised_regNr>/           Aufgabenakte
│   ├── 00_KARTE.txt
│   └── DOK/<docId>/
├── F18/<vorgangId>/                 Vorgangsakte (F18 Operation)
│   ├── 00_KARTE.txt
│   └── DOK/<docId>/
├── F77/<wfiId>/                     Freigabe-Workflow-Akte
│   └── 00_KARTE.txt
└── owner_key.txt                    Klarnamendatei (chmod 600)
```

### Regeln
- Jede Entität hat einen eigenen Unterordner
- Jedes Dokument hat einen eigenen Unterordner innerhalb der Elternentität
- Alle Karten enthalten Querverweise auf alle verbundenen Entitäten
- **Kein Ordner ergibt allein Sinn** — `owner_key.txt` ist zum Auflösen aller IDs nötig
- Klarnamen nur in `owner_key.txt` (Berechtigungen 600) — alle Akten enthalten nur IDs

### Write-Methoden
- `writeProject(p, mfsRoot)` — F16-Deckblatt
- `writeTask(t, mfsRoot)` — F22-Karte mit F16-Rückverweis
- `writeF18(v, mfsRoot)` — F18-Karte mit F16/F22/Eltern-F18-Verweis
- `writeF77(wfiId, entityType, title, mfsRoot)` — F77-Karte
- `writeDocument(d, mfsRoot)` — DOK-Karte + physische Datei
- `writePerson(p, mfsRoot)` — nur `owner_key.txt`
- `writeTeam(t, mfsRoot)` — nur `owner_key.txt`
- `appendOwnerKey(regNr, klarname, connections, mfsRoot)` — Eintrag in `owner_key.txt`
- `rebuildAll(mfsRoot)` — vollständiger Rebuild: alle F16/F22/F18; erstellt F16/F22/F18/F77-Rootordner

---

## System / Datenhaltung

### Datenbanken

| Pool-Name | Datei | SQL-Schema | Inhalt |
|---|---|---|---|
| `core` | `core.db` | `core.sql` | Registriernummern, Personen, Teams, Schema-Versionen |
| `f16` | `f16.db` | `f16.sql` | F16 Projekte, F22 Aufgaben, QTCS-Verknüpfungen |
| `f18` | `f18.db` | `f18.sql` | F18 Operationen, F18 Schritte, Communications |
| `f77` | `f77.db` | `f77.sql` | WFI, WFA, Vorlagen, Teilnehmer, SLA-Log |
| `dok` | `dok.db` | `dok.sql` | Dokument-Metadaten, Revisionen, entity_documents |
| `tracking` | `tracking.db` | `tracking.sql` | Reporting-Hilfsmodelle |
| LMDB | `archive.lmdb` | — | Binärer Dateiinhalt (content-addressiert per SHA-256) |

### Registriernummern
Format: `<DE-Kürzel>/<TypeCode>/<seq:04d>/<year>` — z.B. `XV/F16/0042/2026`

| Typ | Code (`RegDept::*`) |
|---|---|
| F16 Projekt | `F16` |
| F22 Aufgabe | `F22` |
| F18 Operation | `F18` |
| F77 Freigabe | `F77` |
| Person | `HVA` |
| Diensteinheit | `DE` |

### Schema-Versionen (`SchemaVersions::*`)
`core=2`, `f16=2`, `f77=2`, `dok=2`, `f18=2`, `tracking=2` — Basis v2; Deltas via `MigrationEngine::registry()`

### Backup
- `BackupManager::backupDatabases(basePath, dest, incremental)` — alle SQLite-DBs
- `BackupManager::backupMFS(mfsRoot, dest)` — MFS-Baum
- `BackupManager::runFull(basePath, dest)` — vollständig
- `BackupManager::isDue(dest, intervalHours)` — zeitbasierte Prüfung
