// Thinker's governor: what a base builds and what it hurries (its build.cpp and the production
// choice of its base.cpp), on OpenSMAC (thinker.h).
#pragma once
#include "thinker/compat.h"

namespace thinker {

// Thinker's choice of production for the base (its select_build).
int select_build(int base_id);
// Thinker's mod_base_build: the production the governor settles on for the base, select_build's
// choice when the base finished something, the governor asked, or the current item cannot be
// built, else the current item (a defender first when the base is empty); asks the player before
// a secret project, as the original does.
int base_build_choice(int base_id, int has_gov);
// Thinker's mod_base_hurry for the player's governor: hurries the current base's production out
// of the faction's credits when worth it. Returns whether anything was hurried.
int governor_hurry();

}  // namespace thinker
