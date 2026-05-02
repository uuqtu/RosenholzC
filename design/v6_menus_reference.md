# Rosenholz PM v6 — Alte Menü-Referenz (entfernt in v7)
# Prüfliste: alle Aktionen müssen in v7 via cd/ls/lo/Kurzbefehle erreichbar sein

## F16 — projectMenu (1–11, 0=Zurück)

  1.Bearbeiten
  F22: 2.listen | 3.<#>öffnen | 4.neu
  AKT: 5.listen | 6.<#>öffnen | 7.neu
  KOM: 8.listen | 9.<#>öffnen | 10.neu
  11.Notizen
  0.Zurück

v7-Equivalent:
  f16 -e             → Bearbeiten
  ls                 → F22+AKT listen
  cd <F22-ID>        → F22 öffnen
  f22 -n             → Neue F22 im Kontext
  cd <AKT-ID>        → AKT öffnen
  akt -n             → Neue AKT im Kontext
  kom -n             → Neue Kommunikation
  kom -l             → Kommunikation listen
  note <text>        → Notiz (auch: f16 -note <text>)
  ..                 → Zurück


## F22 — taskMenu (1–13, 0=Zurück)

  1.Bearbeiten
  AKT: 2.listen | 3.<#>öffnen | 4.neu
  F18: 5.listen | 6.<#>öffnen | 7.neu
  KOM: 8.listen | 9.<#>öffnen | 10.neu
  11.F77 | 12.Nacherfassen
  13.Notizen
  0.Zurück

  Untermenü F77 (kein WF):
    1. F77 starten
    0. Zurück

  Untermenü F77 (WF aktiv):
    1. F77 öffnen
    0. Zurück

  Nacherfassen-Dialog:
    1. Neue Akte anlegen und Datei hinzufügen
    2. Vorhandener Akte hinzufügen   [nur verknüpfte Akten]
    3. Ignorieren
    0. Abbrechen

v7-Equivalent:
  f22 -e             → Bearbeiten
  ls                 → AKT+F18 listen
  cd <AKT-ID>        → AKT öffnen
  akt -n             → Neue AKT im Kontext
  cd <F18-ID>        → F18 öffnen
  f18 -n             → Neuer F18 im Kontext
  kom -n / kom -l    → Kommunikation neu/listen
  f77 -s             → F77 starten
  f77 -d             → F77 anzeigen
  f22 -ind           → Nacherfassung (Task aus -tasks)
  note <text>        → Notiz (auch: f22 -note <text>)
  ..                 → Zurück


## F18 — f18Menu (1–12, 0=Zurück)

  1.Bearbeiten | 2.Notiz+
  F18S: 3.listen | 4.<#>öffnen | 5.neu
  KOM:  6.listen | 7.<#>öffnen | 8.neu
  AKT:  9.listen | 10.<#>öffnen | 11.neu
  12.F77
  0.Zurück

  Schritt-Untermenü:
    1.Ausführen | 2.Tracking | 3.Notiz
    KOM: 4.listen | 5.<#> | 6.neu
    AKT: 7.listen | 8.<#> | 9.neu
    0.Zurück
    Entscheidung: 1.Genehmigen  2.Ablehnen  3.Überspringen

v7-Equivalent:
  f18 -e             → Bearbeiten
  f18 -note <text>   → Notiz
  ls                 → Schritte + AKT listen
  f18 -stp           → Neuen Schritt hinzufügen
  kom -n / kom -l    → Kommunikation
  akt -n             → Neue AKT im Kontext
  cd <AKT-ID>        → AKT öffnen
  f77 -s / f77 -d    → F77 starten/anzeigen
  ..                 → Zurück


## AKT — documentMenu (0–19)

  AKT: 1.Bearbeiten | 2.Revisionieren | 15.Verlauf | 16.MFS | 18.Drucken | 19.RevWechsel
  F77: 3.F77
  OBJ: 4.listen(<n>) | 5.<#>→Untermenü (öffnen/checkout/URL)
  OBJ: 6.OBJ+ | 13.URL-Update <#> | 14.alle URL
  [Admin] 17.Löschen
  0.Zurück

  OBJ-Untermenü (pro Objekt):
    1.Öffnen (lesen)
    2.checkout | 3.checkin | 4.revert | 5.URL aktualisieren | 6.entfernen
    0.Zurück

  Objekt hinzufügen (OBJ+):
    1. Lokal kopieren
    2. URL herunterladen
    3. Leere Datei anlegen (Formate: txt/docx/pptx/xlsx/...)
    0. Abbrechen

v7-Equivalent:
  akt -v / akt -e    → Bearbeiten (öffnet documentMenu)
  rev                → Neue Revision anlegen (Option 2)
  f77 -s / f77 -d    → F77 starten/anzeigen (Option 3)
  ls                 → Objekte listen (Option 4)
  akt -co <n>        → Objekt öffnen/auschecken (Option 5)
  akt -obj           → Neues Objekt hinzufügen (Option 6)
  akt -url           → URL-Objekte aktualisieren (Option 14)
  akt -rv <n>        → Revision wechseln (Option 19)
  note <text>        → Notiz (auch: akt -note <text>)
  ..                 → Zurück


## F77 — instanceMenu

  Nicht-Admin:
    1.Schritt hinzufügen
    2.Schritt simulieren
    3.Engine-Tick (automatisch)
    4.F77 abbrechen
    0.Zurück

  Admin-Zusatz:
    [ADMIN] 1.Schritt manuell ausführen  2.Schritt simulieren
            3.Schritt hinzufügen  4.Engine-Tick  5.F77 abbrechen

v7-Equivalent:
  f77 -d             → instanceMenu weiterhin vollständig erreichbar
  f77 -s             → Workflow starten (startWfInstanceWizard)
  f77 -tpl           → Vorlagen anzeigen


## F77-Task — executeTask (via tsk)

  Nacherfassen:
    1. Neue Akte anlegen und Datei hinzufügen
    2. Vorhandener Akte hinzufügen  [nur zur Entity verknüpfte Akten]
    3. Datei ignorieren (Task überspringen)
    0. Später entscheiden (Task offen lassen)

  Generic Task:
    1. Aufgabe abschließen
    2. Überspringen
    0. Später

v7-Equivalent: tsk → executeTask-Dialoge unverändert


## DE — teamMenu (unverändertes Untermenü, kein Kontext)

  Team-Menü:
    1.Name/Typ bearbeiten
    2.Lead zuweisen
    3.Parent setzen
    4.Mitglied hinzufügen
    5.Mitglied entfernen
    6.Budget setzen
    7.Status ändern
    0.Zurück

  DE-Liste:
    1.Neue Diensteinheit  2.Öffnen  0.Zurück

v7-Equivalent: -de (global, kein Kontextbezug — bleibt unverändertes interaktives Menü)


## Wizards (bleiben alle vollständig in v7 erhalten)

### F16-Wizard  (f16 -n)
  Titel
  Typ: 1.OV  2.IM  3.OPK  4.GMS  5.AU  6.SVG
  Größe: 1.large  2.medium  3.small
  Codename (optional)
  Priorität (high/medium/low)
  Komplexität (complex/moderate/simple)
  Methodik (agile/waterfall/kanban)
  Scope-Beschreibung
  Geplanter Start  (YYYY-MM-DD / . +Nd +Nw +Nm +Ny)
  Geplantes Ende   (YYYY-MM-DD / . +Nd +Nw +Nm +Ny)
  Budget geplant EUR

### F22-Wizard  (f22 -n  oder  -f22 -n global)
  Titel
  Beschreibung (optional)
  Bearbeiter Person-ID (optional)
  Priorität (high/medium/low, leer=medium)
  WBS-Code (optional)
  Geplanter Start  (YYYY-MM-DD / . +Nd ...)
  Fälligkeitsdatum (YYYY-MM-DD / . +Nd ...)
  Geplanter Aufwand Stunden (optional)
  → Allgemeine Akte wird automatisch angelegt

### F18-Wizard  (f18 -n  oder  -f18 -n global)
  Titel
  Typ (1–11):
    1.Incident  2.Risk  3.Measure  4.QualityGate
    5.AssumptionConstraint  6.CommunicationPlan
    7.LessonsLearned  8.DecisionLog
    9.ChangeRequest  10.ChangeObject  11.Generic
  Verantwortlich Person-ID (optional)
  Priorität (low/medium/high/critical, leer=medium)
  → Allgemeine Akte wird automatisch angelegt
  → Init- und End-Schritt werden automatisch angelegt

### AKT-Wizard  (akt -n  oder  -akt -n global)
  Elternteil: 1.F22  2.F18  0.Abbrechen
  [F22/F18 auswählen]
  Titel
  Typ (1–10):
    1.Bericht  2.Spezifikation  3.Vertrag  4.Schriftverkehr
    5.Nachweis  6.Plan  7.Protokoll  8.Archiv  9.Sonstiges  10.Analyse
  Kategorie (optional)
  Sprache (EN/DE)
  Format (pdf/docx/...)
  Zusammenfassung (optional)
  Tags (optional)
  Externe Referenz (optional)
  → Rev 1 (in_work) wird automatisch angelegt

### AKT-Objekt-Wizard  (akt -obj)
  1. Lokal kopieren   → Pfad eingeben
  2. URL herunterladen→ URL eingeben
  3. Leere Datei      → Format wählen:
       1.txt  2.txt-Template  3.docx  4.docx-Template
       5.pptx  6.pptx-Template  7.xlsx  8.Anderes
  0. Abbrechen
  Bezeichnung (optional, leer=Dateiname)
  Beschreibung (optional)

### F77-Wizard  (f77 -s)
  Gestartet von Person-ID (leer=System)
  Verfügbare Vorlagen listen → auswählen
  ODER: Zielzustand 1.in_work 2.pre_released 3.released 4.locked 5.closed
  Manuelle Operationen hinzufügen? ja/nein
    Titel
    Beschreibung (optional)
    Zugewiesen an Person-ID (leer=offen)
