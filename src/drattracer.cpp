#include "internal.hpp"

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

DratTracer::DratTracer (Internal *i, File *f, bool b)
    : internal (i), file (f), binary (b)
#ifndef QUIET
      ,
      added (0), deleted (0)
#endif
{
  (void) internal;
}

void DratTracer::connect_internal (Internal *i) {
  internal = i;
  file->connect_internal (internal);
  LOG ("DRAT TRACER connected to internal");
}

DratTracer::~DratTracer () {
  LOG ("DRAT TRACER delete");
  delete file;
}

/*------------------------------------------------------------------------*/

inline void DratTracer::put_binary_zero () {
  assert (binary);
  assert (file);
  file->put ((unsigned char) 0);
}

inline void DratTracer::put_binary_lit (int lit) {
  assert (binary);
  assert (file);
  assert (lit != INT_MIN);
  unsigned idx = abs (lit);
  assert (idx < (1u << 31));
  unsigned x = 2u * idx + (lit < 0);
  unsigned char ch;
  while (x & ~0x7f) {
    ch = (x & 0x7f) | 0x80;
    file->put (ch);
    x >>= 7;
  }
  ch = x;
  file->put (ch);
}

/*------------------------------------------------------------------------*/

void DratTracer::drat_add_clause (const vector<int> &clause) {
  if (binary)
    file->put ('a');
  for (const auto &external_lit : clause)
    if (binary)
      put_binary_lit (external_lit);
    else
      file->put (external_lit), file->put (' ');
  if (binary)
    put_binary_zero ();
  else
    file->put ("0\n");
}
void DratTracer::drat_delete_clause (const vector<int> &clause) {
  if (binary)
    file->put ('d');
  else
    file->put ("d ");
  for (const auto &external_lit : clause)
    if (binary)
      put_binary_lit (external_lit);
    else
      file->put (external_lit), file->put (' ');
  if (binary)
    put_binary_zero ();
  else
    file->put ("0\n");
}

/*------------------------------------------------------------------------*/

void DratTracer::add_derived_clause (int64_t, bool, int,
                                     const vector<int> &clause,
                                     const vector<int64_t> &) {
  if (file->closed ())
    return;
  LOG ("DRAT TRACER tracing addition of derived clause");
  drat_add_clause (clause);
#ifndef QUIET
  added++;
#endif
}

void DratTracer::delete_clause (int64_t id, bool,
                                const vector<int> &clause) {
  if (file->closed ())
    return;
  if (weakened.erase (id)) {
    LOG ("DRAT TRACER skipping deletion of weakened clause[%" PRId64 "]",
         id);
    return;
  }
  LOG ("DRAT TRACER tracing deletion of clause");
  drat_delete_clause (clause);
#ifndef QUIET
  deleted++;
#endif
}

void DratTracer::weaken_minus (int64_t id, const vector<int> &) {
  if (file->closed ())
    return;
  LOG ("DRAT TRACER remembering weakened clause[%" PRId64 "]", id);
  weakened.insert (id);
}

void DratTracer::solve_query () { assumption_clauses.clear (); }

// The negation of a core of failing assumptions is implied by the formula
// and actually RUP.  It is traced since users (for instance the stand-alone
// solver on incremental 'p inccnf' files) might add it (or a superset of
// it) as original clause afterwards, which is then used in derivations.

void DratTracer::add_assumption_clause (int64_t, const vector<int> &clause,
                                        const vector<int64_t> &) {
  if (file->closed ())
    return;
  LOG (clause, "DRAT TRACER tracing addition of assumption clause");
  drat_add_clause (clause);
#ifndef QUIET
  added++;
#endif
  assumption_clauses.push_back (sorted_without_duplicates (clause));
}

// Original clauses are not traced in DRAT.  However, an original clause
// added after a query which contains a traced assumption clause is traced
// as derived (it is RUP) and the assumption clause is deleted, such that
// the clauses known to the checker match those of the solver and a later
// deletion of the original clause can not remove another identical clause.
// It might be a strict superset since 'failed' reports all assumptions
// mapped to the same internal literal, while the assumption clause only
// contains one of them (for instance after 'compact' maps all fixed
// variables to the same internal variable).

void DratTracer::add_original_clause (int64_t, bool,
                                      const vector<int> &clause,
                                      bool restored) {
  if (file->closed ())
    return;
  if (restored || assumption_clauses.empty ())
    return;
  const vector<int> sorted = sorted_without_duplicates (clause);
  for (auto i = assumption_clauses.begin (); i != assumption_clauses.end ();
       i++) {
    if (!includes (sorted.begin (), sorted.end (), i->begin (), i->end ()))
      continue;
    LOG (clause, "DRAT TRACER tracing original clause as derived");
    drat_add_clause (sorted);
    drat_delete_clause (*i);
#ifndef QUIET
    added++, deleted++;
#endif
    assumption_clauses.erase (i);
    return;
  }
}

/*------------------------------------------------------------------------*/

bool DratTracer::closed () { return file->closed (); }

#ifndef QUIET

void DratTracer::print_statistics () {
  uint64_t bytes = file->bytes ();
  uint64_t total = added + deleted;
  MSG ("DRAT %" PRId64 " added clauses %.2f%%", added,
       percent (added, total));
  MSG ("DRAT %" PRId64 " deleted clauses %.2f%%", deleted,
       percent (deleted, total));
  MSG ("DRAT %" PRId64 " bytes (%.2f MB)", bytes,
       bytes / (double) (1 << 20));
}

#endif

void DratTracer::close (bool print) {
  assert (!closed ());
  file->close ();
#ifndef QUIET
  if (print) {
    MSG ("DRAT proof file '%s' closed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

void DratTracer::flush (bool print) {
  assert (!closed ());
  file->flush ();
#ifndef QUIET
  if (print) {
    MSG ("DRAT proof file '%s' flushed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

} // namespace CaDiCaL
