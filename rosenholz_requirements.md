# Rosenholz PM v2 — Anforderungen je Objekt (Code-basiert)

Alle Anforderungen basieren auf den tatsächlichen Modell-Methoden, Engine-Funktionen
und Beziehungen im Code — nicht auf dem CLI-Menü.

---

## System — Globale Anforderungen

Als Benutzer des Rosenholz PM möchte ich:

- Alle Projekte (F16) gesammelt einsehen können.
- Personen anlegen und nach Namen suchen können.
- Diensteinheiten (Teams) anlegen und hierarchisch gliedern können.
- Einen Backup aller Datenbanken und MFS-Dateien ausführen können — entweder manuell
  oder automatisch nach Ablauf eines konfigurierten Zeitintervalls.
- Den vollständigen MFS-Dateibaum aus dem aktuellen Datenbankstand neu aufbauen können.
- Alle Entitäten systemweit nach Text durchsuchen können.
- Die Log-Verbosity zur Laufzeit anpassen können.
- Standard-Workflow-Vorlagen automatisch beim ersten Start erzeugen lassen können.

---

## F16 — Projekt (Vorgang)

Als Benutzer möchte ich für ein F16-Projekt:

### Anlegen & Grunddaten
- Ein neues Projekt anlegen mit Titel, Vorgangsart und Größenklasse. Dabei werden
  Registriernummer und Jahr automatisch vergeben.
- Beim ersten Speichern automatisch einen steuernden Main-Workflow (WFI) erhalten,
  der den Lebenszyklus des Projekts kontrolliert.
- Den Status des Projekts von `in_work` nach `released` überführen — ausschließlich
  über den End-Schritt des Main-Workflows.

### Bearbeiten
- Titel und Codename jederzeit ändern können (solange nicht `released`).
- Status und Phase setzen können.
- Geplante Start- und Endtermine sowie tatsächliche Termine pflegen können.
- Budget, tatsächliche Kosten und Währung erfassen können.
- Das Scope-Statement erfassen und versionieren können (scope_version, scope_last_changed,
  scope_change_reason, scope_change_count).
- Leiter, Diensteinheit und Auftraggeber neu zuweisen können.
- Earned Value, CPI, SPI, EAC, ETC und VAC neu berechnen lassen können.
- QTCS-Dimensionsverknüpfungen (Qualität, Zeit, Kosten, Scope) hinzufügen und entfernen.
- Meilenstein-Notizen als freien Text pflegen können.
- Eine Notiz als JSON-Feld speichern können.

### Verbundene Objekte
- Beliebig viele Aufgaben (F22) anlegen, die unter diesem Projekt liegen. Dabei kann
  jede Aufgabe auch eine Eltern-Aufgabe bekommen (Teilaufgaben).
- Aufgaben nach Projekt abfragen und auflisten können.
- Beliebig viele Dokumente anlegen, die diesem Projekt zugeordnet sind. Ohne
  Projektzuordnung kann kein Dokument angelegt werden.
- Dokumente für dieses Projekt auflisten können.
- Beliebig viele F18-Vorgänge (alle 11 Typen) anlegen, die diesem Projekt gehören.
- F18-Vorgänge gefiltert nach Typ abfragen können (z. B. nur Risiken, nur Incidents).
- Beliebig viele Communications (Meetings, Calls, Emails, Reports, Messages) anlegen,
  die diesem Projekt zugeordnet sind.
- Beliebig viele zusätzliche Workflow-Instanzen (WFI) starten können — entweder
  ad-hoc oder auf Basis einer Vorlage. Diese können durch den Main-Workflow blockiert werden.

### Lifecycle
- Den Main-Workflow öffnen und dessen Schritte einsehen können.
- Prüfen lassen, ob noch offene Sub-WFIs die Freigabe blockieren.
- Alle offenen Sub-WFIs mit expliziter Bestätigung auf `locked` setzen können,
  um die Freigabe zu ermöglichen.
- Das Projekt nach `released` überführen — dann sind keine Änderungen, keine neuen
  Aufgaben, keine neuen Dokumente und keine neuen F18-Vorgänge mehr möglich.

### Export
- Eine MFS-Akte (DDR-Aktenstruktur) für dieses Projekt schreiben lassen können, die
  alle Unterordner (F22/, F18/, DOK/) und ein Deckblatt mit Querverweisen enthält.
- Den Realnamen und alle Verbindungen in die `owner_key.txt` eintragen lassen können.
- Das Projekt als JSON serialisieren können.

---

## F22 — Aufgabe (Task)

Als Benutzer möchte ich für eine F22-Aufgabe:

### Anlegen & Grunddaten
- Eine neue Aufgabe unter einem bestehenden F16-Projekt anlegen mit Titel, optionalem
  Bearbeiter und optionaler Eltern-Aufgabe.
- Beim ersten Speichern automatisch einen steuernden Main-Workflow erhalten.
- Den Status von `in_work` nach `released` überführen — ausschließlich über den
  End-Schritt des Main-Workflows.
- Die Aufgabe in ein neues Projekt umwandeln können (`convertToProject`).

### Bearbeiten
- Titel und Beschreibung ändern können.
- Status, Priorität und Fortschritt in % setzen können.
- Geplante Start- und Endtermine sowie Ist-Termine pflegen können.
- Geplanten und tatsächlichen Aufwand in Stunden sowie geplante und tatsächliche
  Kosten erfassen können.
- Den Bearbeiter (Person) neu zuweisen können.
- Die Aufgabe einem anderen Projekt zuweisen können (`reassignToProject`).
- Eine Eltern-Aufgabe setzen oder wechseln können (`reassignParent`).
- WBS-Code und Sprint/Phase setzen können.
- Qualitätskriterien und Abnahmekriterien erfassen können.
- QTCS-Dimensionsverknüpfungen hinzufügen können.
- Meilenstein-Notizen als freien Text pflegen können.

### Verbundene Objekte
- Beliebig viele Teilaufgaben (F22 mit parentTaskId) anlegen können.
- Alle direkten Teilaufgaben laden können (`loadChildren`).
- Beliebig viele Dokumente anlegen können, die dieser Aufgabe zugeordnet sind.
- Dokumente für diese Aufgabe auflisten können.
- Beliebig viele F18-Vorgänge (alle 11 Typen) anlegen können, die dieser Aufgabe gehören.
- F18-Vorgänge gefiltert nach Typ abfragen können.
- Beliebig viele Communications anlegen können, die dieser Aufgabe zugeordnet sind.
- Zusätzliche Workflow-Instanzen starten können.

### Lifecycle
- Main-Workflow öffnen und Freigabe-Status prüfen können.
- Offene Sub-WFIs sperren lassen können.
- Nach `released` kann die Aufgabe nicht mehr geändert werden.

### Export
- Eine MFS-Akte unter dem übergeordneten F16-Hängeregister schreiben lassen können,
  mit Rückverweis auf das Projekt, die Eltern-Aufgabe und den Bearbeiter.

---

## F18 — Vorgang (alle 11 Typen)

Als Benutzer möchte ich für einen F18-Vorgang:

### Typen
Es gibt 11 Vorgangstypen — jeder teilt die Basis-Felder, hat aber eigene Zusatzfelder:
- `incident` — Schwere, Auftrittsdatum, Ursache, Sofortmaßnahme, Lösung, Kostenauswirkung,
  Terminauswirkung, Scope- und Qualitätsauswirkung.
- `risk` — Eintrittswahrscheinlichkeit (1–5), Auswirkung auf Zeit/Kosten/Qualität/Scope (je 1–5),
  Gesamtrisikoscore (automatisch berechnet), Risikolevel, Antwortstrategie, Notfallplan,
  Auslösebedingung, Restrisikostufe, Kostenreserve, Zeitreserve.
- `measure` — Maßnahmenkategorie, geplantes/tatsächliches Datum, Wirksamkeit,
  Verifikationsmethode, Verifikationsdatum und -person.
- `qualityGate` — Phase, Kriterien, Abnahmekriterien, Befunde, Ergebnis (passed/failed/
  conditional/pending), Entscheidung (proceed/hold/stop), Validierungsdatum.
- `assumptionConstraint` — Typ (Annahme oder Beschränkung), Auswirkung, Validierungsdatum.
- `communicationPlan` — Zielgruppe, Häufigkeit, Kanal, Verantwortlicher.
- `lessonsLearned` — Lerntyp, Empfehlung, anwendbare Phasen.
- `decisionLog` — Entscheidungstyp, Begründung, Entscheidungsdatum und -person,
  betrachtete Alternativen.
- `changeRequest` — Änderungstyp, Begründung, Auswirkung, Einreichungsdatum,
  Entscheidungsdatum, Entscheidungsbegründung, Terminauswirkung.
- `changeObject` — Ausführender, Ausführungsdatum; referenziert den zugehörigen
  ChangeRequest über `parentVorgangId`.
- `generic` — nur Basisfelder, für alle anderen Anwendungsfälle.

### Anlegen & Grunddaten
- Einen neuen F18-Vorgang anlegen — verknüpft mit einem F16-Projekt und optional
  mit einer F22-Aufgabe.
- Beim Anlegen werden automatisch ein Init-Schritt (sofort genehmigt) und ein End-Schritt
  erzeugt. Dazu wird automatisch ein steuernder Main-Workflow angelegt.
- Den Status von `in_work` nach `released` überführen — ausschließlich über den
  End-Schritt des Main-Workflows.

### Bearbeiten
- Titel, Beschreibung, Priorität und Besitzer-Person bearbeiten können.
- Alle typspezifischen Felder bearbeiten können.
- Den Gesamtrisikoscore für `risk`-Vorgänge automatisch neu berechnen lassen können
  (`recalcRiskScore`).
- Eine Textnotiz anfügen können (`addNote`).

### Schritte (F18WorkflowStep)
- Beliebig viele Zwischen-Schritte zwischen Init und End einfügen können.
- Jeden Schritt kann man ausführen (genehmigen, ablehnen, überspringen) — jedoch nur,
  wenn alle Vorgänger-Schritte abgeschlossen sind (`canStart`).
- Tracking-Felder pro Schritt pflegen können: Status (planned/focused/due/archived),
  Fortschritt in %, Fortschrittsnotiz, Priorität, zugewiesene Gruppe.
- Communications pro Schritt anlegen können (`ownerType=f18step`).

### Verbundene Objekte
- Beliebig viele Communications anlegen können.
- Dokumente an Workflow-Schritte anhängen können.

### Lifecycle
- Main-Workflow öffnen und Freigabe-Status prüfen können.
- Nach `released` sind keine Änderungen mehr möglich.

### Export
- Eine MFS-Karte unter dem F16-Hängeregister im Unterordner `F18/` schreiben lassen,
  mit Verweisen auf Projekt, Aufgabe und übergeordneten F18 (bei ChangeObject).

---

## Dokument (DOK)

Als Benutzer möchte ich für ein Dokument:

### Anlegen
- Ein Dokument anlegen können — es muss zwingend einem F16-Projekt zugeordnet sein
  (optional zusätzlich einer F22-Aufgabe). Ohne Projektzuordnung ist kein Dokument möglich.
- Beim Anlegen wird automatisch **Revision 1** erzeugt (Zustand: `in_work`) und ein
  steuernder Main-Workflow angelegt.
- Drei Quellen beim Anlegen wählen können:
  - Lokale Datei hochladen (wird in den MFS-Baum kopiert, SHA-256 berechnet).
  - URL/Link angeben und automatisch herunterladen lassen.
  - Neue leere Datei im MFS anlegen.
- Eine URL als Dokument archivieren können, einschließlich optionaler PDF-Konvertierung
  von Webseiten.

### Revisionen & 5-State-Lifecycle
Das Dokument durchläuft Revisionen — jede Revision hat exakt einen der fünf Zustände:

| Zustand | Bedeutung |
|---|---|
| `in_work` | Mutable — in Bearbeitung |
| `pre_released` | Read-only — zur Prüfung freigegeben |
| `released` | Immutable — freigegeben, kann nicht geändert werden |
| `locked` | Eingefroren — kann nur entsperrt werden (nur neueste Revision) |
| `closed` | Terminal — kein Übergang möglich |

Erlaubte Übergänge:
- `in_work` → `pre_released`, `locked`, `closed`
- `pre_released` → `released`, `locked`, `closed`, `in_work`
- `released` → `locked`, `closed`
- `locked` → `pre_released` (nur neueste Revision), `closed`
- `closed` → (keine weiteren Übergänge)

Exakt eine Revision pro Dokument hält `superseded = false` — das ist die aktive Revision.
Priorität: neueste `released` > neueste nicht-locked/nicht-closed > neueste `in_work`.
Das Flag wird atomar bei jedem Zustandswechsel neu berechnet.

- Den Zustand der aktiven Revision über den End-Schritt des Main-Workflows setzen lassen
  können — alle 5 Zustände sind als Ziel wählbar.
- Manuell eine neue Revision anlegen können (basierend auf der aktuellen aktiven Revision).
  Revisionen auf Basis von `released` oder `locked` erfordern eine Bestätigung.
- Alle Revisionen eines Dokuments laden und einsehen können.
- Die aktive Revision (aktuelle) abrufen können.

### Bearbeiten
- Titel, Kategorie und Version bearbeiten können.
- Status, Klassifizierung, Erstellungs-, Genehmigungs- und Ablaufdaten setzen können.
- Zusammenfassung und Tags pflegen können.
- Autor und Genehmiger neu zuweisen können.
- Dem Dokument einen neuen Projektbezug oder Aufgabenbezug geben können.
- Das Dokument an beliebige Entitäten (Projekt, Aufgabe, Incident, Risk, …) polymorphisch
  anhängen können (`entity_documents`).

### Datei-Operationen
- Die aktuelle Datei vor Änderungen als Version sichern können (`snapshotVersion`).
  Dabei wird eine nummerierte Kopie erstellt und in `document_versions` eingetragen.
- Den Versionsverlauf aller gesicherten Versionen laden und einsehen können.
- Eine lokale Datei in den MFS-Baum importieren können (kopiert, Hash berechnet,
  Größe ermittelt).
- Die Quelldatei über die gespeicherte URL neu herunterladen können (Snapshot vorher).
- Die Datei zum Lesen öffnen (schreibgeschützte Kopie in Temp-Ordner) oder direkt
  zum Bearbeiten öffnen können.

### LMDB-Inhaltsspeicher
- Den Dateiinhalt (binär) in LMDB für eine bestimmte Revision staged ablegen können
  (`stageContent` → Temp-Ordner mit SHA-256).
- Den gestagten Inhalt nach einem erfolgreichen Zustandsübergang atomar in LMDB
  committen können (`commitContent`). Content-Addressierung per SHA-256 verhindert
  Duplikate.
- Den Inhalt einer Revision aus LMDB in eine Zieldatei exportieren können (`retrieveContent`).
- Den LMDB-Füllstand prüfen lassen: bei weniger als 1 GB freiem Speicher wird die
  Map-Größe automatisch verdoppelt.

### Export
- Eine MFS-Karte unter dem F16-Hängeregister im Unterordner `DOK/` schreiben lassen,
  mit Verweisen auf Projekt, Aufgabe, Verfasser und Quell-Datei. Die physische Datei
  wird neben der Karteikarte abgelegt.

---

## Workflow-Instanz (WFI)

Als Benutzer möchte ich für eine Workflow-Instanz:

### Anlegen & Starten
- Eine Instanz ad-hoc für jede beliebige Entität (Projekt, Aufgabe, Dokument, F18) starten
  können, ohne Vorlage.
- Eine Instanz auf Basis einer gespeicherten Vorlage starten können.
- Drei Ausführungstypen wählen können: sequentiell, parallel, frei.
- Beim Start wird automatisch ein Init-Schritt angelegt und sofort genehmigt (Tick).

### Schritte verwalten
- Beliebig viele Schritte hinzufügen können — mit Titel, Ausführungstyp, Vorgänger-IDs,
  Rolle, SLA-Stunden, Auto-Approve-Flag.
- Einen Schritt ausführen können (genehmigen, ablehnen, überspringen) — nur wenn alle
  Vorgänger abgeschlossen sind.
- Für den End-Schritt eines Dokument-WFIs den Zielzustand (einer der 5 Dokumentzustände)
  setzen können, bevor der Schritt ausgeführt wird.
- Den Engine-Tick manuell auslösen können (Auto-Approve-Schritte und Sequenz-Logik).

### Tracking
- Tracking-Felder pro Schritt pflegen: Status (planned/focused/due/archived),
  Fortschritt in %, Fortschrittsnotiz, Priorität, zugewiesene Gruppe.
- SLA-Verletzungen erkennen und protokollieren lassen können.

### Dokumente & Entscheidungen
- Dokumente an die gesamte Instanz oder einen einzelnen Schritt anhängen können.
- Alle angehängten Dokumente einer Instanz oder eines Schritts laden können.
- Einen Entscheidungslog-Eintrag für einen Schritt erzeugen können.
- Einen Lessons-Learned-Eintrag für einen Schritt erzeugen können.

### Teilnehmer & Eskalation
- Beliebig viele Teilnehmer mit verschiedenen Rollen anlegen können (Initiator, Genehmiger,
  Prüfer, Beobachter, Informiert, Delegierter, Eskalationsziel).
- Eine Eskalation mit Ziel-Person und Begründung auslösen können.

### Main-Workflow (Lifecycle)
- Für F16, F22, F18 und Dokumente jeweils einen steuernden Main-Workflow automatisch
  anlegen lassen (wird beim ersten Speichern erzeugt).
- Prüfen lassen, ob alle Sub-WFIs einer Entität abgeschlossen oder gesperrt sind,
  bevor der End-Schritt des Main-Workflows ausgeführt werden darf.
- Alle offenen Sub-WFIs einer Entität mit expliziter Bestätigung auf `locked` setzen.
- Eine Entität nach Abschluss des Main-WFI-End-Schritts automatisch auf `released`
  setzen lassen (F16/F22/F18) oder auf den gewählten Dokumentzustand (Dokument).

### Suchen & Filtern
- WFIs nach Entitätstyp, Status, Namenstext und SLA-Verletzung filtern können.
- Alle aktiven WFIs systemweit laden können.
- Alle WFIs mit SLA-Verletzung laden können.

### Vorlagen
- Workflow-Vorlagen anlegen, benennen und versionieren können.
- Vorlagen-Schritte definieren (inkl. sequentiell/parallel/frei, Vorgänger, Auto-Approve).
- Vorlagen nach Entitätstyp filtern können.

---

## Communication

Als Benutzer möchte ich für eine Communication:

- Eine Communication anlegen können — verknüpft mit einem Projekt (`project`),
  einer Aufgabe (`task`) oder einem F18-Schritt (`f18step`). Exakt einer dieser
  drei Besitzer-Typen muss gesetzt sein.
- Fünf Typen wählen können: `meeting`, `message`, `call`, `email`, `report`.
- Datum, Dauer, Kanal, Ort, Organisator und Teilnehmerliste pflegen können.
- Agenda erfassen können.
- Eine Communication als abgeschlossen markieren können (`complete`) — dabei werden
  Beschlüsse und Aktionspunkte gespeichert und der Status auf `completed` gesetzt.
- Status: `scheduled`, `completed`, `cancelled`.
- Alle Communications für einen bestimmten Besitzer laden können.

---

## Person

Als Benutzer möchte ich für eine Person:

- Eine Person anlegen können mit Vorname, Nachname, E-Mail und Beschäftigungstyp.
- Alle Personen laden können.
- Eine Person per E-Mail suchen oder nach Namensbestandteilen durchsuchen können.
- Organisations-Einheit, Abteilung, Rolle, Gehaltssatz, Verfügbarkeit und Seniority
  pflegen können.
- Eine Person als `active` oder `inactive` führen können.
- Den Realnamen und Verbindungen in die `owner_key.txt` schreiben lassen können —
  kein eigener PERSONEN/-Ordner existiert, da eine Personendatei allein sinnlos wäre.
- Eine Person als JSON serialisieren können.

---

## Diensteinheit / Team

Als Benutzer möchte ich für eine Diensteinheit (Team):

- Eine Diensteinheit anlegen können mit Name, Typ und optionaler übergeordneter Einheit.
- Alle Diensteinheiten laden können.
- Untergeordnete Einheiten einer Einheit laden können (`loadChildren`).
- Mitglieder hinzufügen können — mit Rolle, Kategorie, Allokationsprozent, Startdatum
  und Enddatum.
- Mitglieder einer Einheit oder alle Einheiten einer Person laden können.
- Ein Mitglied in eine andere Einheit verschieben können (`moveToTeam`).
- Die Rolle eines Mitglieds ändern können (`reassignRole`).
- Realnamen und Verbindungen in `owner_key.txt` schreiben lassen — kein eigener
  DIENSTEINHEITEN/-Ordner existiert.

---

## MFS-Dateibaum (physische Ablage)

Als Benutzer möchte ich:

- Für jedes F16-Projekt eine vollständige Hängeregister-Struktur schreiben lassen können:
  - Deckblatt mit Querverweisen auf alle verbundenen Objekte und Personen.
  - Unterordner `F22/` für alle Aufgabenkarten.
  - Unterordner `F18/` für alle Vorgangskarten.
  - Unterordner `DOK/` für alle Dokumentkarten und physischen Dateien.
- Für jede F22-Aufgabe eine Karte im F16-Hängeregister schreiben lassen, mit
  Rückverweis auf Projekt, Eltern-Aufgabe und Bearbeiter.
- Für jeden F18-Vorgang eine Karte im F16-Hängeregister schreiben lassen, mit
  Rückverweis auf Projekt, Aufgabe und übergeordneten Vorgang.
- Für jedes Dokument eine Karte im F16-Hängeregister schreiben lassen sowie die
  physische Datei neben der Karte ablegen.
- Alle Klarnamen (Projekt-Titel, Personen-Namen, Dokument-Titel) ausschließlich in der
  `owner_key.txt` (Berechtigungen 600) speichern lassen. Alle anderen Akten enthalten
  nur Registriernummern — eine einzelne Akte ist ohne `owner_key.txt` nicht vollständig.
- Den gesamten MFS-Baum aus dem Datenbankstand neu aufbauen können (`rebuildAll`).

---

## Systemweit — Datenhaltung

- SQLite-Datenbanken: `core.db`, `projects.db`, `workflow.db`, `documents.db`,
  `f18.db`, `tracking.db`, `reporting.db` — getrennt nach Fachbereich.
- Schema-Versionen werden pro Datenbank geführt. Neue Deltas können in der
  `MigrationEngine` als Registry-Einträge hinterlegt werden; Basislinie ist v2.
- LMDB (`archive.lmdb`) wird ausschließlich für binären Dateiinhalt genutzt —
  alle Metadaten bleiben in SQLite.
- Alle Entitäten erhalten eine DDR-artige Registriernummer (XV/F16/0001/2026 usw.)
  aus einer zentralen Sequenzgeneratortabelle (`reg_number_sequences`).
