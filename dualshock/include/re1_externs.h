/*
 * re1_externs.h - typed declarations of Capcom globals (reconstructed).
 *
 * NOT included by the src files directly: older integrated functions declare these globals
 * in block scope with m2c's guessed types, which would conflict with typed file-scope
 * declarations. Used as m2c context (autodecomp.py --psyq) and copied into function bodies
 * by integrate.py when a new function uses them (like psyq_protos.h).
 */
#ifndef RE1_EXTERNS_H
#define RE1_EXTERNS_H

#include "re1_types.h"

extern Entity *g_CurrentEntity;      /* current game object */

#endif /* RE1_EXTERNS_H */
