#pragma once
// ============================================================
// F77Engine.h  —  Workflow Engine (single responsibility)
//
// The engine owns ALL workflow lifecycle logic:
//   start / tick / fireStep / complete / cancel / propagate
//
// Nothing outside this file should contain workflow decision
// logic. The CLI calls engine methods only — no IF-chains
// on workflow state in UI code.
//
// DATA CLASSES (F77W, F77Task, F77W_Template, etc.) remain
// in F77Workflow.h for backwards compat. The engine methods
// are all on F77Engine which is declared in F77Workflow.h.
// This file is a documentation-only alias that includes it.
// ============================================================
#include "F77Workflow.h"
