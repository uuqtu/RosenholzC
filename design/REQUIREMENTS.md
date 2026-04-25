# Rosenholz PM — Anforderungen (Code-basiert, v3)

Alle Anforderungen spiegeln den tatsächlichen Implementierungsstand wider:
Modell-Felder, Engine-Methoden, SQL-Schemata und CLI-Befehle wurden
vollständig ausgewertet.

---

## System — Globale Anforderungen

Als Benutzer des Rosenholz PM möchte ich:

- Alle Projekte (F16) gesammelt einsehen, nach Status filtern und nach
  Titel oder Registriernummer suchen können.
- Personen anlegen, alle laden, per E-Mail suchen und nach Namensbestandteilen
  durchsuchen können. Jede Person erhält eine DDR-Registriernummer (XV/PER/…).
- Diensteinheiten (Teams) anlegen und hierarchisch gliedern können. Mitglieder
  hinzufügen, verschieben und Rollen zuweisen können.
- Einen Backup aller Datenbanken (`backupDatabases`) und aller MFS-Dateien
  (`backupMFS`) ausführen können — entweder manuell via `rh -backup` oder
  automatisch, wenn das konfigurierte Zeitintervall (`isDue`) abgelaufen ist.
  Ein Vollbackup-Aufruf (`runFull`) kombiniert beide Schritte.
- Den vollständigen MFS-Dateibaum aus dem aktuellen Datenbankstand neu aufbauen
  können (`MFSWriter::rebuildAll`) via `rh -mfs`.
- Eine einzelne Entität (F16, F22, F18, DOK, F77) in den MFS-Baum schreiben
  lassen können via `rh -mfs <id>`.
- Alle Entitäten (F16, F22, F18, DOK, F77) systemweit nach Text durchsuchen
  können via `rh -search <query>`.
- Die Log-Verbosity zur Laufzeit anpassen können: `debug|info|warn|error`
  via `rh -log <level>`.
- Standard-Workflow-Vorlagen automatisch beim ersten Start erzeugen lassen
  können (`F77_Engine::seedDefaultTemplates`).
- Den aktuellen Systemstatus mit Datensatz-Zählungen je Datenbank einsehen
  können via `rh -status`.

### Datenhaltung

Sechs SQLite-Datenbanken, getrennt nach Fachbereich:

| Pool-Name | Datei     | Inhalt                              |
|-----------|-----------|-------------------------------------|
| `core`    | core.db   | Personen, Teams, Reg-Sequenzen      |
| `f16`     | f16.db    | Projekte, QTCS-Dimensionen          |
| `f22`     | f22.db    | Aufgaben                            |
| `f18`     | f18.db    | F18-Operationen, Schritte, Comms    |
| `f77`     | f77.db    | Workflow-Vorlagen und Instanzen     |
| `dok`     | dok.db    | Dokumente, Revisionen               |

LMDB (`archive.lmdb`) wird ausschließlich für binären Dateiinhalt genutzt —
alle Metadaten bleiben in SQLite. Schema-Versionen werden pro Datenbank in
`schema_version` geführt; Basislinie ist v2. Alle Entitäten erhalten eine
DDR-artige Registriernummer aus der Tabelle `reg_number_sequences`.

---

## F16 — Projekt (Vorgang)

### Identität & Registriernummer

- Jedes Projekt erhält automatisch eine Registriernummer nach dem Schema
  `XV/F16/{seq}/{year}`, erzeugt durch `genId("F16")`.
- Felder: `projectId`, `regNumber`, `title`, `codename`, `projectType`,
  `sizeClass` (`large|medium|small`).

### Organisatorische Verknüpfungen

- Einem Projekt können zugewiesen werden: Leiter (`leadId` → Person),
  Auftraggeber (`sponsorId` → Person), Diensteinheit (`ownerTeamId` → Team).
- Jede dieser Verknüpfungen kann separat neu gesetzt werden:
  `reassignLead`, `reassignSponsor`, `reassignTeam`.
- Ein zusätzlicher Workflow-Instanz-Zeiger kann neu gesetzt werden:
  `reassignWorkflowInstance`.

### Status & Lifecycle

- Status: `in_work` (Default) → `released` (nur über F77 Main-Workflow
  End-Schritt via `applyTargetState`). Kein direktes Schreiben erlaubt.
- Steuerung über `releaseWorkflowId` (Main-WFI).
- Beim ersten `save()` erzeugt `ensureReleaseWorkflow()` automatisch einen
  steuernden Main-Workflow (F77_Workflow) für dieses Projekt.
- Status nach `released`: kein Bearbeiten, keine neuen F22, keine neuen
  F18, keine neuen DOK möglich.

### Zeitfelder (QTCS — Zeit)

- Geplant: `startDatePlanned`, `endDatePlanned`, `durationPlannedDays`.
- Ist: `startDateActual`, `endDateActual`, `durationActualDays`.
- Abweichung: `scheduleVarianceDays`.

### Kostenfelder & Earned Value (QTCS — Kosten)

- `budgetPlanned`, `budgetApproved`, `budgetCommitted`, `budgetActual`.
- Earned Value Metriken (werden durch `recalcEarnedValue()` berechnet):
  `earnedValue`, `plannedValue`, `actualCost`, `costVariance`,
  `cpi` (Cost Performance Index), `spi` (Schedule Performance Index),
  `eac` (Estimate At Completion), `etc` (Estimate To Complete),
  `vac` (Variance At Completion).
- Währung: `currency` (Default: `EUR`).

### Scope-Versionierung (QTCS — Scope)

- `scopeStatement`, `scopeVersion`, `scopeLastChanged`, `scopeChangeReason`,
  `scopeChangeCount` (wird bei jeder Scope-Änderung inkrementiert).

### Weitere Felder

- `phase`, `methodology` (`agile|waterfall|kanban`), `classification`,
  `priority`, `complexity`, `strategicAlignment`.
- `qualityGateId`, `communicationPlanId`.
- `milestones` (freier Text), `notes` (JSON), `links`, `externalRef`.

### QTCS-Dimensionen (Multi-assignable)

- Jedes Projekt kann beliebig viele Verknüpfungen zu Qualitäts-, Zeit-,
  Kosten- und Scope-Dimensionsobjekten haben (gespeichert in Junction-Tabellen).
- Methoden: `addQuality/addCost/addTime/addScope`,
  `removeQuality/removeCost/removeTime/removeScope`, `loadQTCSLinks()`.

### Verbundene Objekte

- **F22 Aufgaben**: Beliebig viele Tasks unter diesem Projekt. Jede Task kann
  auch eine Eltern-Task haben. Laden via `TaskF22::loadForProject`.
- **F18 Vorgänge**: Beliebig viele, alle 11 Typen. Optional nach Typ gefiltert
  laden via `F18Operation::loadForProject(projectId, type)`.
- **Dokumente**: Beliebig viele, mit Pflicht zur Projektzuordnung.
  Laden via `Document::loadForProject`.
- **Communications**: Beliebig viele, `ownerType = "project"`.
  Laden via `Communication::loadForOwner`.
- **F77 Instanzen**: Beliebig viele zusätzliche Workflows, die an diesem
  Projekt hängen. Laden via `F77_Workflow::loadForEntity("f16", projectId)`.

### Lifecycle-Prüfungen

- `F77_Engine::canRelease("f16", projectId, releaseWorkflowId, blockerCount)`:
  Prüft, ob offene Sub-WFIs die Freigabe blockieren.
- `F77_Engine::lockAll("f16", projectId, releaseWorkflowId, force)`:
  Setzt alle offenen Sub-WFIs auf `locked` (mit expliziter Bestätigung).

### Konvertierung

- `convertToTask(parentProjectId)`: Erzeugt eine F22-Aufgabe aus diesem
  Projekt (Projekt bleibt erhalten, wird nicht gelöscht).

### Export & Serialisierung

- `writeMFSFile(mfsRoot)`: Schreibt MFS-Akte mit Deckblatt und Querverweisen.
- `toJson()` / `fromJson()`: JSON-Serialisierung.
- `MFSWriter::writeProject`: Schreibt vollständige Hängeregister-Struktur
  mit Unterordnern `F22/`, `F18/`, `DOK/` und `owner_key.txt`-Eintrag.

---

## F22 — Aufgabe (Task)

### Identität & Registriernummer

- Jede Aufgabe erhält eine Registriernummer: `XV/F22/{seq}/{year}`.
- Felder: `taskId`, `regNumber`, `projectId`, `parentTaskId`, `taskCode`,
  `title`, `description`, `taskType`.

### Zuweisung

- `assigneeId` (→ Person), `assignedBy`. Neu zuweisen via `reassignTo`.
- In anderes Projekt verschieben: `reassignToProject`.
- Eltern-Aufgabe wechseln: `reassignParent`.

### Status & Lifecycle

- Status: `in_work` (Default) → `released` (ausschließlich über F77
  Main-Workflow End-Schritt).
- `releaseWorkflowId`: steuernder Main-WFI.
- `ensureReleaseWorkflow()` beim ersten `save()`.

### Zeit (QTCS)

- Geplant: `startDatePlanned`, `dueDatePlanned`, `effortPlannedHrs`.
- Ist: `startDateActual`, `dueDateActual`, `effortActualHrs`, `effortRemainingHrs`.
- `scheduleVarianceDays`, `percentComplete` (0–100).

### Kosten (QTCS)

- `costPlanned`, `costActual`.

### Qualität & Scope (QTCS)

- `qualityCriteria`, `acceptanceCriteria`, `milestones` (freier Text).
- `wbsCode`, `sprintOrPhase`.
- QTCS-Dimensionen: `addQuality/addCost/addTime/addScope`, `loadQTCSLinks()`.

### Verbundene Objekte

- **Teil-Aufgaben**: Beliebig viele F22 mit `parentTaskId = taskId`.
  Laden via `TaskF22::loadChildren(taskId)`.
- **Dokumente**: Laden via `Document::loadForEntity("f22", taskId)`.
- **F18 Vorgänge**: Laden via `F18Operation::loadForTask(taskId, type)`.
- **Communications**: `ownerType = "task"`.

### Konvertierung

- `convertToProject(projectType)`: Erzeugt ein F16-Projekt aus dieser
  Aufgabe (Aufgabe bleibt erhalten).

### Export

- `writeMFSFile(mfsRoot)`: MFS-Karte im übergeordneten F16-Hängeregister.
- `toJson()` / `fromJson()`.

---

## F18 — Vorgang (Operation, 12 Typen)

### Typen

Es gibt 12 Vorgangstypen (11 Domänen-Typen + 1 interner Typ):

| Typ                    | Konstante                          |
|------------------------|------------------------------------|
| Incident               | `F18OperationType::INCIDENT`       |
| Risk                   | `F18OperationType::RISK`           |
| Maßnahme               | `F18OperationType::MEASURE`        |
| Quality Gate           | `F18OperationType::QUALITY_GATE`   |
| Annahme/Beschränkung   | `F18OperationType::ASSUMPTION_CONSTRAINT` |
| Kommunikationsplan     | `F18OperationType::COMMUNICATION_PLAN` |
| Lessons Learned        | `F18OperationType::LESSONS_LEARNED` |
| Entscheidungsprotokoll | `F18OperationType::DECISION_LOG`   |
| Änderungsantrag        | `F18OperationType::CHANGE_REQUEST` |
| Änderungsobjekt        | `F18OperationType::CHANGE_OBJECT`  |
| Generisch              | `F18OperationType::GENERIC`        |
| F77-Schritt            | `F18OperationType::F77_STEP` (intern) |

### Identität & Registriernummer

- `vorgangId`: `XV/F18/{seq}/{year}`.
- Elternbeziehungen: `projectId` (→ F16, verpflichtend), `taskId` (→ F22,
  optional), `parentVorgangId` (→ F18, nur für `changeObject`).

### Gemeinsame Felder (alle Typen)

- `title`, `description`, `status` (`in_work` Default → `released`),
  `releaseWorkflowId`, `ownerId` (→ Person), `priority` (`low|medium|high|critical`).
- `notes` (JSON-Array von Fortschrittsnotizen, via `addNote`), `links`.
- `ensureReleaseWorkflow()` beim ersten `save()`.

### Typ-spezifische Felder

**incident**: `incidentType` (`technical|process|security|quality`), `severity`
(`low|medium|high|critical`), `occurredDate`, `resolvedDate`, `rootCause`,
`immediateAction`, `resolution`, `costImpact`, `scheduleImpactDays`,
`scopeImpact`, `qualityImpact`.

**risk**: `riskLevel` (`low|medium|high|critical`), `probabilityScore` (1–5),
`impactScoreTime/Cost/Quality/Scope` (je 1–5), `overallRiskScore` (automatisch
via `recalcRiskScore()`), `responseStrategy` (`avoid|mitigate|transfer|accept`),
`contingencyPlan`, `triggerCondition`, `residualRiskLevel`, `costReserve`,
`scheduleReserveDays`.

**measure**: `measureCategory` (`corrective|preventive|detective`),
`plannedDate`, `actualDate`, `effectiveness`, `verificationMethod`,
`verifiedDate`, `verifiedBy`.

**qualityGate**: `phase`, `criteria`, `acceptanceCriteria`, `findings`,
`gateResult` (`passed|failed|conditional|pending`), `gateDecision`
(`proceed|hold|stop`).

**assumptionConstraint**: `acType` (`assumption|constraint`), `validatedDate`,
`validatedBy`, `impact`.

**communicationPlan**: `audience`, `frequency`, `channel`, `responsible`.

**lessonsLearned**: `lessonType` (`positive|negative|observation`),
`recommendation`, `applicablePhases`.

**decisionLog**: `decisionType` (`architectural|process|resource|scope`),
`rationale`, `decisionDate`, `decisionBy`, `alternativesConsidered`.

**changeRequest**: `changeType` (`general|scope|budget|schedule|quality`),
`justification`, `crImpact`, `raisedDate`, `crDecisionDate`,
`crDecisionRationale`, `crScheduleImpactDays`.

**changeObject**: `executedBy`, `executionDate`; `parentVorgangId` → zugehöriger
changeRequest.

### F18OperationStep — Workflow-Schritte

Jede F18-Operation hat beim Anlegen automatisch:
- **Init-Schritt** (`isInitialize=true`, `autoApprove=true`, `status="done"`),
- **End-Schritt** (`isFinal=true`, letzter Schritt der Kette).

Zwischen Init und End können beliebig viele Zwischen-Schritte eingefügt werden.

**Schrittfelder**:
`stepId` (`XV/WFS/{seq}/{year}`), `vorgangId`, `tplStepId`,
`title`, `description`, `stepType` (`task|approval|review|notification`),
`sequenceOrder`, `predecessorStepIds` (kommasepariert).

**Bookend-Flags**: `isInitialize`, `isFinal`.

**Freie Schritte** (`isFree = true`):
- Keine Vorgänger-Abhängigkeiten.
- Nicht in die lineare Kette eingebunden.
- Kein Predecessor-Eintrag beim End-Schritt.
- `canStart()` gibt immer `true` zurück (solange nicht terminal).

**Status-Maschine** (neu in v3):
```
pending → in_progress → waiting → blocked → skipped → done
```
- Terminal: `done`, `skipped`.
- Nicht terminal: `waiting`, `blocked` (Schritt kann fortgesetzt werden).
- `isComplete()`: gibt `true` nur bei `done` und `skipped`.
- `canStart(allSteps)`: prüft alle Vorgänger; bei freien Schritten immer `true`
  (außer bei terminalen Zuständen).

**Tracking-Status** (auto-berechnet via `computeTrackingStatus()`):
`planned|focused|due|in_work|archived`.
Berechnung: `done`/`skipped` → `archived`; `inWorkSince` gesetzt → `in_work`;
`dueDate` überschritten → `due`; `focusDate` überschritten → `focused`;
sonst → `planned`.

**Weitere Felder**: `assignedTo`, `requiredRole`, `dueDate`, `startedDate`,
`completedDate`, `slaHours`, `slaBreached`, `autoApprove`, `requiresComment`,
`requiresDocument`, `decision`, `decisionBy`, `decisionDate`, `comment`,
`focusDate`, `inWorkSince`, `priority`, `assignedToGroup`, `progressNote`,
`percentComplete`, `notes` (JSON).

**Schrittmethoden**:
- `addStep(title, stepType, assigneeId, isFree)`: Schritt anhängen.
- `insertAfter(predecessorStepId, title, stepType, assigneeId)`:
  Schritt zwischen zwei bestehenden Schritten einfügen. Der bisherige
  Nachfolger zeigt dann auf den neuen Schritt.
- `loadSteps()`: Schritte aus DB in `steps`-Vektor laden.

---

## Dokument (DOK)

### Identität & Registriernummer

- `documentId`: `XV/DOK/{seq}/{year}`.
- Pflichtfeld: `projectId` (→ F16). Ohne Projektzuordnung kein Dokument.
- Optional: `taskId` (→ F22), `f18OperationId` (→ F18), `f18StepId` (→ F18-Schritt).

### Felder

- `title`, `docType` (`report|specification|contract|correspondence|
  evidence|plan|minutes|archive|other`), `docCategory`.
- `version` (Default: `1.0`), `status` (gespiegelt von aktiver Revision),
  `classification`, `language` (Default: `EN`).
- Datum: `dateCreated`, `dateModified`, `dateApproved`, `dateExpires`.
- `volumeNumber`, `pageCount`.
- `authorId`, `approvedBy`.
- `filePath`, `fileSize`, `fileHash` (SHA-256), `fileUrl`, `format`.
- `checkedOutPath` (lokal ausgecheckte Arbeitsdatei).
- `preCheckoutRevId` (Rev-Nummer beim letzten Checkout, für `revertChanges()`).
- `summary`, `tags`, `links`, `notes` (JSON), `externalRef`.
- `releaseWorkflowId` (Main-WFI), `workflowInstanceId`.

### Anlegen

- `Document::create(title, docType, projectId)`: erzeugt In-Memory-Objekt.
- Nach `save()` erzeugt `ensureRevision1()` Revision 1 (`in_work`) und
  legt den Inhaltsspeicher-Eintrag in LMDB an.
- Drei Quellen beim Erstellen via Wizard:
  1. Lokale Datei (`importLocalFile`): in MFS kopiert, SHA-256 berechnet.
  2. URL (`archiveFromUrl`): heruntergeladen, optional als PDF konvertiert.
  3. Neue leere Datei: Stub-Datei im MFS angelegt.

### Revisionen & 5-State-Lifecycle

Jedes Dokument hat N Revisionen (`DocumentRevision`). Genau eine Revision
hält `superseded = false` — das ist die aktive Revision.

**Zustände** (`DocRevState::*`):

| Zustand       | Bedeutung                                         |
|---------------|---------------------------------------------------|
| `in_work`     | Veränderbar — in Bearbeitung                      |
| `pre_released`| Nur-Lesen — zur Prüfung freigegeben               |
| `released`    | Unveränderlich — freigegeben                      |
| `locked`      | Eingefroren — nur entsperrbar (neueste Rev)       |
| `closed`      | Terminal — kein weiterer Übergang möglich         |

**Erlaubte Übergänge** (`isTransitionAllowed`):
- `in_work` → `pre_released`, `locked`, `closed`
- `pre_released` → `released`, `locked`, `closed`, `in_work`
- `released` → `locked`, `closed`
- `locked` → `pre_released` (nur neueste Rev), `closed`
- `closed` → (keine)

**Superseded-Priorität** (atomar berechnet bei jedem Zustandswechsel):
1. Neueste `released`-Revision
2. Neueste nicht-locked/nicht-closed Revision (falls kein `released`)
3. Neueste `in_work`-Revision (Fallback)

**Revisions-Methoden**:
- `DocumentRevision::createRevision(docId, baseRev, createdBy, note)`:
  Neue Revision (startet in `in_work`), ohne Dateiinhalt zu kopieren.
- `DocumentRevision::currentRevision(docId)`: Aktive Revision.
- `DocumentRevision::loadAllRevisions(docId)`: Alle Revisionen.
- `DocumentRevision::loadByRev(docId, rev)`: Einzelne Revision.
- `rev->transitionState(targetState)`: Zustandswechsel mit atomarer
  `superseded`-Neuberechnung.

**Redundanz**: `Document.status` spiegelt den `revState` der aktiven Revision
— `revState` ist autoritativ. Nach `transitionState()` muss `doc.status` manuell
synchronisiert werden.

### Checkout / Checkin / Revert

- `checkout(destDir)`:
  1. Automatischer Snapshot (`snapshotVersion`) der aktuellen Revision
     **vor** dem Extrahieren — speichert `preCheckoutRevId`.
  2. Inhalt aus LMDB → lokale Datei unter `destDir` (oder Standard-Checkout-Pfad).
  3. Fallback: MFS-Dateipfad.
  4. Fallback: leere Datei.
  5. Setzt `checkedOutPath`.

- `checkin(srcPath)`:
  1. Datei in LMDB staged (`stageContent`).
  2. Neue Revision angelegt (`createRevision`).
  3. Inhalt committed (`commitContent`).
  4. `checkedOutPath` gelöscht, `preCheckoutRevId` = 0.

- `revertChanges()`:
  1. Lädt Revision `preCheckoutRevId` aus LMDB.
  2. Committed deren Inhalt als neue `in_work`-Revision
     (Änderungsnotiz: "Revert auf Checkout-Stand").
  3. Lokale Arbeitskopie (`checkedOutPath`) wird verworfen.
  4. Geschichte wird niemals zerstört — Revert ist eine neue Revision.
  5. Gibt `false` zurück, wenn kein Checkout aktiv oder kein LMDB-Inhalt.

### Weitere Datei-Operationen

- `snapshotVersion(changeNote, createdBy)`: Legt neue Revision in LMDB an.
- `loadVersions()`: Gibt alle Revisionen als `VersionRecord`-Vektor zurück.
- `importLocalFile(srcPath)`: Kopiert Datei in MFS, berechnet Hash/Größe.
- `refreshFromUrl()`: Snapshot zuerst, dann Neu-Download von `fileUrl`.
- `openFile(mode)`: `"read"` → Temp-Kopie öffnen; `"edit"` → Original öffnen.

### LMDB-Inhaltsspeicher (`ArchiveStore`)

- `stageContent(srcPath, outTmp)`: Berechnet SHA-256, legt Inhalt in Temp-Ordner.
  Gibt `ChunkRef` (sha256, size) zurück.
- `commitContent(tmpPath, ref, docId, rev)`: Schreibt staged Inhalt atomar in LMDB.
  Content-Adressierung per SHA-256 verhindert Duplikate.
- `retrieveContent(docId, rev, destPath)`: Exportiert Inhalt aus LMDB.
- `chunkExists(sha256)`: Prüft, ob ein Chunk bereits vorhanden ist.
- `lookupRevChunk(docId, rev)`: Gibt SHA-256 für eine bestimmte Revision zurück.
- Automatische Map-Größen-Verdopplung wenn freier Speicher unter 1 GB.

### Anhänge & Zuweisung

- `attachToEntity(entityType, entityId, relationship)`: Polymorphe Verknüpfung
  zu beliebigen Entitäten via `entity_documents`-Tabelle.
- `reassignAuthor`, `reassignToProject`, `reassignToTask`.

### Export

- `MFSWriter::writeDocument`: MFS-Karte unter F16-Hängeregister im Unterordner
  `DOK/<docId>/`. Physische Datei neben der Karte abgelegt.

---

## F77 — Workflow-Instanz (WFI)

### Klassen

| Klasse                      | Bedeutung                                   |
|-----------------------------|---------------------------------------------|
| `F77_WorkflowTemplate`      | Wiederverwendbare Vorlage (Admin-Zeit)       |
| `F77_WorkflowTemplateStep`  | Schritt-Definition in einer Vorlage          |
| `F77_Workflow`              | Laufende Instanz (an eine Entität gebunden)  |
| `F77_WorkflowStep`          | Schritt-Instanz (Snapshot der Vorlage)       |

### Vorlage (`F77_WorkflowTemplate`)

- Felder: `templateId`, `name`, `version` (Default: `1.0`), `description`,
  `entityTypes` (z.B. `"f16,f22,f18,dok"`), `targetState` (Default: `released`),
  `status` (`active|inactive`), `createdBy`.
- Methoden: `create`, `loadById`, `loadAll`, `loadForEntityType`.
- `addTemplateStep(title, executionMode, isInit, isFinal)`:
  Schritt zu Vorlage hinzufügen.
- `F77_Engine::seedDefaultTemplates()`: Erzeugt Standard-Vorlagen beim ersten Start.

**Vorlagen-Schritt** (`F77_WorkflowTemplateStep`):
`tplStepId`, `templateId`, `title`, `sequenceOrder`, `isInitialize`, `isFinal`,
`executionMode` (`sequential|parallel`), `predecessorTplStepIds`,
`waitConditionF18Type`, `waitConditionTitle`, `requiredRole`, `slaHours`,
`autoApprove`, `requiresComment`, `requiresDocument`.

### Instanz (`F77_Workflow`)

- Felder: `workflowId`, `templateId` (soft ref), `templateName` (Snapshot),
  `entityType` (`f16|f22|f18|dok`), `entityId`, `targetState`, `status`
  (`active|completed|cancelled|locked`), `initiatedBy`, `initiatedDate`,
  `completedDate`, `notes` (JSON).
- Methoden: `loadById`, `loadForEntity(entityType, entityId)`, `loadActive`.

**Instanz-Schritt** (`F77_WorkflowStep`):
`stepId`, `workflowId`, `tplStepId`, `title`, `sequenceOrder`, `isInitialize`,
`isFinal`, `executionMode`, `predecessorStepIds`, `f18OperationId`
(→ F18-Operation, die diesen Schritt ausführt), `waitF18OperationId`,
`waitConditionF18Type`, `status` (`pending|in_progress|approved|rejected|skipped|cancelled`),
`autoApprove`, `requiresComment`, `requiresDocument`, `completedDate`.

- `isComplete()`: `approved|rejected|skipped` (F77-eigene Terminale).
- `canStart(allSteps)`: Alle Vorgänger müssen `isComplete()` sein.
- `syncFromF18()`: Synchronisiert Status vom verknüpften F18-Vorgang.

### Engine (`F77_Engine`)

- `startFromTemplate(templateId, entityType, entityId, initiatedBy)`:
  Instanz aus Vorlage erzeugen, Init-Schritt sofort genehmigen (Tick).
- `startDefault(entityType, entityId, targetState, initiatedBy)`:
  Minimal-Workflow ohne Vorlage (Init + 1 Schritt + End).
- `tick(wf)`: Auto-Approve-Schritte und Sequenz-Logik ausführen.
- `fireStep(wf, stepId, decision, actor, comment)`:
  Schritt ausführen (`approved|rejected|skipped`).
  Nur möglich wenn `canStart` und alle Wait-Conditions erfüllt.
- `validateStep(wf, stepId)`: Trockenlauf — prüft ohne Zustandsänderung.
- `canRelease(entityType, entityId, releaseWorkflowId, blockerCount)`:
  Prüft, ob alle Sub-WFIs abgeschlossen oder gesperrt sind.
- `lockAll(entityType, entityId, releaseWorkflowId, force)`:
  Setzt alle offenen Sub-WFIs auf `locked`. Gibt Anzahl gesperrter WFIs zurück.
- `applyTargetState(wf)`:
  Setzt nach Abschluss des End-Schritts den Zielzustand:
  - DOK: `DocumentRevision::transitionState(targetState)` auf aktive Revision.
  - F16/F22/F18: `UPDATE … SET status = targetState`.
- `checkAndComplete(wf)`: Prüft ob Workflow abgeschlossen ist, ruft dann
  `applyTargetState` auf.
- `spawnWaitConditionF18(step, entityId)`: Erzeugt F18-Operation als
  Wait-Condition für einen Schritt.
- `seedDefaultTemplates()`: Erzeugt Standard-Vorlagen beim ersten Start.

### Main-Workflow

Jedes F16, F22, F18 und DOK erhält beim ersten `save()` automatisch einen
steuernden Main-Workflow via `ensureReleaseWorkflow()`. Dieser kontrolliert
den Lebenszyklus der Entität. Status-Übergang von `in_work` nach `released`
(oder anderem Zielzustand bei DOK) ausschließlich über den End-Schritt dieses
Main-Workflows.

---

## Communication

- Identität: `commId` (`XV/COM/{seq}/{year}`).
- Besitzer: `ownerId` + `ownerType`. Drei gültige Werte:
  - `ownerType = "project"` → `ownerId` ist `F16.projectId`
  - `ownerType = "task"` → `ownerId` ist `F22.taskId`
  - `ownerType = "f18step"` → `ownerId` ist `F18OperationStep.stepId`
- Typ (`commType`): `meeting|message|call|email|report`.
- Inhalt: `title`, `agenda`, `scheduledDate`, `actualDate`, `durationMins`,
  `channel` (`teams|zoom|phone|email|in-person`), `location`, `organiserId`.
- Teilnehmer: `participants` (JSON-Array von `{personId, role}`).
- Ergebnis: `decisions`, `actions` (JSON-Array), `notes`.
- Status: `scheduled|completed|cancelled`.
- `complete(decisions, actions)`: Setzt Status auf `completed`, `actualDate = now()`.
- Laden: `loadForOwner(ownerId, ownerType)`, `loadById`, `loadRecent`.

---

## Person

- Identität: `personId`, `regNumber` (`XV/PER/{seq}/{year}`), `lastName`,
  `firstName`, `preferredName`, `email`, `phone`.
- Organisatorisch: `orgUnit`, `department`, `location`, `country`, `roleTitle`,
  `personType` (`internal|external|contractor|advisor`), `employmentType`,
  `seniorityLevel`, `managerId` (Self-Ref), `clearanceLevel`.
- Fähigkeiten: `skills`, `certifications`, `languages` (je JSON-Array).
- Verfügbarkeit/Kosten: `dayRate`, `monthlyRate`, `availabilityPct`,
  `availabilityFrom`, `availabilityTo`.
- Status: `active|inactive|on-leave|terminated`.
- Audit: `onboardDate`, `offboardDate`, `externalRef`, `links`, `notes` (JSON).
- Hilfsmethoden: `fullName()`, `displayName()` (bevorzugt `preferredName`).
- CRUD: `save`, `load`, `remove`, `update`.
- Factory: `create(firstName, lastName, email, personType)`.
- Laden: `loadById`, `loadByEmail`, `loadAll`, `search(nameFragment)`.
- `reassignManager(newManagerId)`, `setStatus(newStatus)`.
- Serialisierung: `toJson()`, `fromJson()`.
- MFS: Nur `owner_key.txt`-Eintrag — kein eigener `PERSONEN/`-Ordner.

---

## Diensteinheit / Team

### Team

- Identität: `teamId`, `regNumber`, `name`, `abbreviation`, `rosenholzEquiv`,
  `parentTeamId` (Hierarchie), `leadId` (→ Person).
- Felder: `location`, `type` (`delivery|platform|governance|advisory|ops`),
  `headcountPlanned`, `headcountActual`, `budgetAllocated`, `budgetConsumed`,
  `methodology`, `tools`, `status`.
- Methoden: `loadAll`, `loadChildren(parentId)`, `reassignLead`, `reassignParent`.
- Mitglieder: `addMember`, `removeMember`, `loadMembers`.
- MFS: Nur `owner_key.txt`-Eintrag — kein eigener `DIENSTEINHEITEN/`-Ordner.

### Teammitglied (`TeamMember`)

- `membershipId`, `teamId`, `personId`.
- Rolle: `role`, `roleCategory` (`leadership|technical|delivery|advisory|support`),
  `seniorityInTeam`, `memberType` (`internal|contractor|advisor|secondment`).
- Flags: `isLead`, `isDeputy`, `isCoreMemember`, `isExtendedMember`, `isObserver`.
- Zuweisung: `allocationPct`, `fteEquivalent`, `startDate`, `endDate`,
  `assignmentType` (`permanent|temporary|secondment|project-based`).
- Kompetenz: `primarySkill`, `secondarySkills`, `certificationsRelevant`,
  `clearanceLevel`.
- Kosten: `plannedHoursPerWeek`, `actualHoursPerWeek`, `costRate`, `costCenter`.
- Status: `active|inactive|onboarding|offboarding`.
- `reassignRole(newRole, newCategory)`: Rolle innerhalb desselben Teams ändern.
- `moveToTeam(newTeamId)`: In andere Diensteinheit verschieben.

---

## MFS-Dateibaum (physische Ablage)

- **F16 Hängeregister**: `mfs/F16/<reg>/`
  - `00_DECKBLATT.txt`: Deckblatt mit allen Querverweisen.
  - `F22/<reg>.txt`: Karte je Aufgabe.
  - `F18/<id>.txt`: Karte je Vorgang.
  - `DOK/<docId>/`: Unterordner je Dokument mit Karte + physischer Datei.
- **F77-Workflows**: `mfs/F77/<workflowId>/`.
- **`owner_key.txt`** (Berechtigungen: 600): Einzige Datei mit Klarnamen.
  Alle anderen Akten enthalten nur Registriernummern.
- Schreibmethoden: `MFSWriter::writeProject`, `writeTask`, `writeF18`,
  `writeDocument`, `writeF77`, `writePerson`, `writeTeam`.
- `MFSWriter::rebuildAll(mfsRoot)`: Vollständiger Neuaufbau aus DB (F16, F22,
  F18, DOK, F77, Personen, Teams).

---

## CLI — Vollständige Befehlsreferenz

Der Befehl `rh` wird ohne Argumente gestartet → interaktive Shell.
Alle Argumente mit `/` werden als ID interpretiert (Format: `XV/F16/0001/2026`).
Guided-Assistenten (`-n`, `-f16`, `-f22`, `-f18`) zeigen zuerst eine Auswahlliste.

### Globale Flags (vor dem Befehl)

```
-s <settings.json>    Alternative Einstellungsdatei verwenden
-b <basispfad>        Basispfad für Datenhaltung überschreiben
-h, --help            Hilfe anzeigen (ohne Datenbankinitialisierung)
```

### Interaktive Shell

```
rh                    Interaktive Shell starten (kein Argument)
rh> BEFEHL [ARGS]     Befehl wie auf Kommandozeile eingeben
rh> exit              Shell beenden
```

### F16 — Projekt

```
rh -f16                        Alle Projekte tabellarisch auflisten
                               (Spalten: REG-NR, TITEL, STATUS, PHASE, PRIO, CPI)
rh -f16 -n                     Guided Wizard: neues Projekt anlegen
rh -f16 <id>                   Projektmenü öffnen
rh -f16 -s <query>             Suche nach Titel oder Registriernummer
rh -f16 -status <status>       Filter: in_work | released | locked | closed
```

**Projektmenü** (interaktiv, erreichbar via `rh -f16 <id>`):
- Felder bearbeiten (solange nicht `released`): `editMenu`
- Revisionen / Lifecycle: Zustand der Entität einsehen
- F77 Freigabe-Workflow: Main-WFI öffnen, Sub-WFIs sperren
- F22 Aufgaben: anlegen, auflisten
- F18 Vorgänge: anlegen, Browser öffnen
- Dokumente: anlegen, Browser öffnen
- Communications: anlegen, auflisten
- MFS-Datei schreiben
- Projekt löschen (mit Bestätigung)

### F22 — Aufgabe

```
rh -f22                        Letzte 20 Aufgaben tabellarisch auflisten
                               (Spalten: REG-NR, TITEL, STATUS, %, PRIO, ASSIGNEE)
rh -f22 -n                     Guided Wizard: F16 wählen, dann Aufgabe anlegen
rh -f22 <projekt-id>           Alle Aufgaben dieses Projekts auflisten
rh -f22 <aufgabe-id>           Aufgabenmenü öffnen
```

**Aufgabenmenü** (interaktiv, erreichbar via `rh -f22 <id>`):
- Felder bearbeiten: `editMenu` (Titel, Beschreibung, Zuweisung, Priorität,
  Fortschritt %, Fälligkeitsdatum, Aufwand, WBS-Code)
- Main-Workflow öffnen: `mainWorkflowMenu`
- Dokumente, F18 Vorgänge, Communications, Teil-Aufgaben
- MFS-Datei schreiben
- Aufgabe löschen

### F18 — Operation

```
rh -f18                        Letzte 20 F18-Operationen auflisten
                               (Spalten: VORGANG-ID, TYP, STATUS, TITEL)
rh -f18 -n                     Guided Wizard: F16/F22 wählen, dann F18 anlegen
                               Optional: F22-Aufgabe innerhalb des Projekts wählen
rh -f18 <projekt-id>           F18-Browser für dieses Projekt öffnen
rh -f18 <projekt-id> -t <typ>  F18-Operation mit vorgegebenem Typ anlegen
                               Typen: incident | risk | measure | qualityGate |
                                      changeRequest | decisionLog |
                                      lessonsLearned | assumptionConstraint |
                                      communicationPlan | changeObject | generic
rh -f18 <f18-id>               F18-Menü öffnen
```

**F18-Menü** (interaktiv, erreichbar via `rh -f18 <id>`):
- Operationsdetails anzeigen: `printF18Operation`
- Schritte visualisieren: `drawF18Chain` (ASCII-Kettenanzeige)
- Schritt hinzufügen (mit isFree-Wahl), Schritt öffnen: `stepMenu`
- Felder bearbeiten (typspezifisch)
- Main-Workflow öffnen
- Risiko-Score neu berechnen (`recalcRiskScore`, nur bei `risk`)
- Communications, Dokumente
- MFS-Datei schreiben

**Schrittmenü** (`stepMenu`):
- Status setzen: `in_progress | waiting | blocked | skipped | done`
- Tracking aktualisieren: Status, Fortschritt %, Notiz, Priorität
- Notiz anfügen
- Communications (ownerType: `f18step`)
- Dokumente

### DOK — Dokument

```
rh -dok                        Letzte 20 Dokumente auflisten
                               (Spalten: DOK-ID, STATUS, VERSION, TITEL)
rh -dok -n                     Guided Wizard: Entitätstyp wählen, dann anlegen
                               (F16 | F22 | F18)
rh -dok -f16                   Guided: Projekt aus Liste wählen, dann anlegen
rh -dok -f22                   Guided: Projekt → Aufgabe wählen, dann anlegen
rh -dok -f18                   Guided: F18-Operation aus Liste wählen, dann anlegen
rh -dok <projekt-id>           Dokumenten-Browser für dieses Projekt öffnen
rh -dok <dok-id>               Dokumentenmenü öffnen
```

**Dokumentenmenü** (interaktiv, erreichbar via `rh -dok <id>`):
```
1.  Bearbeiten (Felder: Titel, Kategorie, Version, Klassifizierung, Beschreibung,
                Autor-ID, Genehmiger-ID)
2.  Revisionen / 5-State-Workflow  (revisionMenu: Zustand wählen und wechseln)
3.  Main Workflow / Freigabe       (F77-Instanz öffnen)
4.  Datei öffnen (Lesen)           (Temp-Kopie, Original unverändert)
5.  Checkout                       (Snapshot automatisch, dann Datei extrahieren)
6.  Checkin                        (Bearbeitete Datei als neue Revision einpflegen)
7.  Änderungen verwerfen (Revert)  (Checkout-Stand wiederherstellen)
8.  URL neu herunterladen          (Snapshot zuerst, dann refreshFromUrl)
9.  Versionsverlauf                (Alle Revisionen mit Datum und Notiz)
10. MFS-Datei schreiben
11. Dokument löschen
```

**Revisions-Untermenü** (Option 2):
- Aktive Revision und Zustand anzeigen.
- Zielzustand auswählen: `in_work | pre_released | released | locked | closed`.
- Übergang nur wenn `isTransitionAllowed` (sonst Fehlermeldung).
- Nach Übergang: `Document.status` synchronisiert.

### F77 — Workflow

```
rh -f77                        Alle aktiven Workflows auflisten
                               (Spalten: WORKFLOW-ID, VORLAGE, TYP, ZIEL)
rh -f77 -tpl                   Alle Vorlagen auflisten
rh -f77 -new                   Neue Vorlage anlegen (Assistent via workflowMenu)
rh -f77 -start <entität-id>    Workflow auf Entität starten (Typ auto-erkannt:
                               F16 | F22 | F18 | DOK), Zielzustand = released
rh -f77 -start <id> <zustand>  Workflow mit vorgegebenem Zielzustand starten
rh -f77 <workflow-id>          Workflow-Menü öffnen
```

**Workflow-Browser** (`workflowMenu`):
- Vorlagen: auflisten, neue anlegen, bearbeiten.
- Instanzen: alle aktiven anzeigen, per ID öffnen, neue starten.

**Instanz-Menü** (`instanceMenu`, erreichbar via `rh -f77 <id>`):
```
1. Schritt ausführen  (fireStep: approved | rejected | skipped)
2. Schritt validieren (validateStep: Trockenlauf ohne Zustandsänderung)
3. Engine-Tick        (tick: Auto-Approve-Schritte und Sequenz-Logik)
4. Workflow sperren   (status = locked, mit Bestätigung)
5. Zielzustand ändern (targetState neu setzen)
```
Zusätzlich: ASCII-Ketten-Visualisierung aller Schritte mit Status-Markierungen.

### Person

```
rh -per                        Alle Personen tabellarisch auflisten
                               (Spalten: REG-NR, NAME, ROLLE, TYP, STATUS)
rh -per -n                     Guided Wizard: neue Person anlegen
rh -per -s <query>             Suche nach Name (nameFragment)
rh -per <id>                   Personen-Detailkarte anzeigen (printPerson)
```

### Diensteinheit

```
rh -de                         Diensteinheiten-Browser öffnen (teamMenu)
```

**Team-Browser** (`teamMenu`): Alle Teams auflisten, einzelne öffnen,
Mitglieder anzeigen, Mitglieder hinzufügen/verschieben.

### System-Befehle

```
rh -search <query>             Globale Suche über alle Entitätstypen
                               (F16, F22, F18, DOK, F77)
                               Ergebnisse nummeriert, direkt in Menü springbar
rh -backup                     Vollständiges Backup:
                               backupDatabases + backupMFS
rh -status                     Systemstatus: Datensatz-Zählungen je Pool
                               (F16, F22, F18 Ops/Schritte/Comms, F77, DOK,
                                Personen, Teams, Basispfad)
rh -mfs                        MFS-Baum vollständig aus DB neu aufbauen
                               (rebuildAll: F16+F22+F18+DOK+F77+Personen+Teams)
rh -mfs <id>                   MFS-Dateien für eine einzelne Entität schreiben
                               (Auto-Erkennung: F16 | F22 | F18 | DOK | F77)
rh -log <level>                Log-Verbosität: debug | info | warn | error
rh -h, --help                  Hilfe anzeigen (ohne DB-Initialisierung)
```

---

## Was im Original-Dokument nicht mehr stimmt

Die folgenden Punkte aus der ursprünglichen Anforderungsliste (v2) wurden
geändert oder entfernt und spiegeln nicht mehr den Ist-Stand wider:

1. **Datenbanken**: Das Original nennt `projects.db`, `workflow.db`,
   `documents.db`, `tracking.db`, `reporting.db`. Tatsächlicher Stand:
   `f16.db`, `f22.db`, `f77.db`, `dok.db`. `tracking.db` und `reporting.db`
   wurden entfernt.

2. **F18-Schritte: Status-Maschine**: Das Original beschreibt
   `pending → in_progress → approved|rejected|skipped`. Aktuell (v3):
   `pending → in_progress → waiting → blocked → skipped → done`.
   Terminal: nur `done` und `skipped`. `waiting` und `blocked` sind nicht terminal.

3. **F18-Schritte: Freie Schritte**: Im Original nicht vorhanden. Neu in v3:
   `isFree = true` — Schritte ohne Predecessor-Abhängigkeiten, jederzeit
   startbar und transitierbar unabhängig vom Kettenstand.

4. **Dokument-Checkout**: Das Original erwähnt keinen automatischen Snapshot
   beim Checkout. Aktuell: `checkout()` nimmt automatisch einen Snapshot
   (`snapshotVersion`) vor dem Extrahieren. `preCheckoutRevId` wird gespeichert.

5. **Dokument-Revert**: Im Original nicht vorhanden. Neu in v3:
   `revertChanges()` stellt den Checkout-Stand als neue Revision wieder her
   (Geschichte bleibt erhalten).

6. **F18 Init-Schritt Status**: Das Original beschreibt `status = "approved"`
   für den Init-Schritt. Aktuell: `status = "done"` (da `approved` kein gültiger
   Wert mehr in der neuen Status-Maschine ist).

7. **Communication ownerType**: Das Original nennt `project/task`. Aktuell:
   `"project" | "task" | "f18step"` (nicht `"f16"/"f22"/"f18"`).

8. **CLI — Shell-Modus**: Das Original beschreibt kein interaktives Shell-Modus.
   Aktuell: `rh` ohne Argumente startet eine interaktive Shell mit `rh>`-Prompt
   (CLion-Debugger-kompatibel).

9. **CLI — rh -f16 "Title"**: Dieser Pfad existiert in der aktuellen CLI nicht.
   Anlegen erfolgt via `rh -f16 -n` (Guided Wizard) oder direkt im Projektmenü.

10. **F12-Typen**: Das Original beschreibt 11 Typen. Aktuell gibt es 12 Konstanten
    in `F18OperationType::*`, darunter `F77_STEP` als interner Typ für die
    Workflow-Engine.

11. **ensureReleaseWorkflow im CLI-Wizard**: Früher wurde `ensureReleaseWorkflow()`
    automatisch in CLI-Wizards aufgerufen (verletzt v3-Prinzip: kein Auto-Workflow).
    In v3 entfernt — Workflows werden nur explizit via `rh -f77 -start <id>`
    gestartet oder durch das Modell selbst beim ersten `save()`.
