#include "internal.hpp"

#include <limits.h>

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

LratTracer::LratTracer (Internal *i, File *f, bool b)
    : internal (i), file (f), binary (b)
#ifndef QUIET
      ,
      added (0), deleted (0)
#endif
      ,
      latest_id (0) {
  (void) internal;
}

void LratTracer::connect_internal (Internal *i) {
  internal = i;
  file->connect_internal (internal);
  LOG ("LRAT TRACER connected to internal");
}

LratTracer::~LratTracer () {
  LOG ("LRAT TRACER delete");
  delete file;
}

/*------------------------------------------------------------------------*/

inline void LratTracer::put_binary_zero () {
  assert (binary);
  assert (file);
  file->put ((unsigned char) 0);
}

inline void LratTracer::put_binary_lit (int lit) {
  assert (binary);
  assert (file);
  assert (lit != INT_MIN);
  unsigned idx = abs (lit);
  assert (idx < (1u << 31));
  unsigned x = 2 * idx + (lit < 0);
  unsigned char ch;
  while (x & ~0x7f) {
    ch = (x & 0x7f) | 0x80;
    file->put (ch);
    x >>= 7;
  }
  ch = x;
  file->put (ch);
}

inline void LratTracer::put_binary_id (int64_t id) {
  assert (binary);
  assert (file);
  uint64_t x = abs (id);
  x = 2 * x + (id < 0);
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

void LratTracer::lrat_add_clause (int64_t id, const vector<int> &clause,
                                  const vector<int64_t> &chain) {
  if (delete_ids.size ()) {
    if (!binary)
      file->put (latest_id), file->put (" ");
    if (binary)
      file->put ('d');
    else
      file->put ("d ");
    for (auto &did : delete_ids) {
      if (binary)
        put_binary_id (did);
      else
        file->put (did), file->put (" ");
    }
    if (binary)
      put_binary_zero ();
    else
      file->put ("0\n");
    delete_ids.clear ();
  }
  latest_id = id;

  if (binary)
    file->put ('a'), put_binary_id (id);
  else
    file->put (id), file->put (" ");
  for (const auto &external_lit : clause)
    if (binary)
      put_binary_lit (external_lit);
    else
      file->put (external_lit), file->put (' ');
  if (binary)
    put_binary_zero ();
  else
    file->put ("0 ");
  for (const auto &c : chain)
    if (binary)
      put_binary_id (c);
    else
      file->put (c), file->put (' '); // in proof chain, so they get
  if (binary)
    put_binary_zero (); // since cadical has no rat-steps
  else
    file->put ("0\n"); // this is just 2c here
}

void LratTracer::lrat_delete_clause (int64_t id) {
  delete_ids.push_back (id); // pushing off deletion for later
}

/*------------------------------------------------------------------------*/

void LratTracer::add_derived_clause (int64_t id, bool, int,
                                     const vector<int> &clause,
                                     const vector<int64_t> &chain) {
  if (file->closed ())
    return;
  LOG ("LRAT TRACER tracing addition of derived clause");
  lrat_add_clause (id, clause, chain);
#ifndef QUIET
  added++;
#endif
}

void LratTracer::delete_clause (int64_t id, bool, const vector<int> &) {
  if (file->closed ())
    return;
  if (weakened.erase (id)) {
    LOG ("LRAT TRACER skipping deletion of weakened clause[%" PRId64 "]",
         id);
    return;
  }
  LOG ("LRAT TRACER tracing deletion of clause");
  lrat_delete_clause (id);
#ifndef QUIET
  deleted++;
#endif
}

void LratTracer::weaken_minus (int64_t id, const vector<int> &) {
  if (file->closed ())
    return;
  LOG ("LRAT TRACER remembering weakened clause[%" PRId64 "]", id);
  weakened.insert (id);
}

void LratTracer::solve_query () { assumption_clauses.clear (); }

// The negation of a core of failing assumptions is implied by the formula
// and comes with antecedents.  It is traced since users (for instance the
// stand-alone solver on incremental 'p inccnf' files) might add it as
// original clause afterwards, which then gets a new identifier and is used
// in later derivations (see 'add_original_clause' below).

void LratTracer::add_assumption_clause (int64_t id,
                                        const vector<int> &clause,
                                        const vector<int64_t> &chain) {
  if (file->closed ())
    return;
  LOG (clause, "LRAT TRACER tracing addition of assumption clause");
  lrat_add_clause (id, clause, chain);
#ifndef QUIET
  added++;
#endif
  assumption_clauses.push_back ({sorted_without_duplicates (clause), id});
}

// Original clauses are not traced in LRAT.  Restored clauses keep their
// identifier (their deletion was not traced).  An original clause added
// after a query which contains a traced assumption clause however gets a
// new identifier unknown to the checker.  It is traced (without duplicated
// literals) as derived from the assumption clause, which is then deleted.
// It might be a strict superset since 'failed' reports all assumptions
// mapped to the same internal literal, while the assumption clause only
// contains one of them (for instance after 'compact' maps all fixed
// variables to the same internal variable).

void LratTracer::add_original_clause (int64_t id, bool,
                                      const vector<int> &clause,
                                      bool restored) {
  if (file->closed ())
    return;
  if (restored || assumption_clauses.empty ())
    return;
  // The user might have added duplicated literals (for instance cubes in
  // 'p inccnf' files with duplicated literals), thus compare modulo those.
  const vector<int> sorted = sorted_without_duplicates (clause);
  for (auto i = assumption_clauses.begin (); i != assumption_clauses.end ();
       i++) {
    if (!includes (sorted.begin (), sorted.end (), i->first.begin (),
                   i->first.end ()))
      continue;
    const int64_t assumption_id = i->second;
    LOG (clause,
         "LRAT TRACER tracing original clause[%" PRId64
         "] as derived from assumption clause[%" PRId64 "]",
         id, assumption_id);
    lrat_add_clause (id, sorted, {assumption_id});
    lrat_delete_clause (assumption_id);
#ifndef QUIET
    added++, deleted++;
#endif
    assumption_clauses.erase (i);
    return;
  }
}

void LratTracer::begin_proof (int64_t id) {
  if (file->closed ())
    return;
  LOG ("LRAT TRACER tracing begin of proof");
  latest_id = id;
}

/*------------------------------------------------------------------------*/

bool LratTracer::closed () { return file->closed (); }

#ifndef QUIET

void LratTracer::print_statistics () {
  uint64_t bytes = file->bytes ();
  uint64_t total = added + deleted;
  MSG ("LRAT %" PRId64 " added clauses %.2f%%", added,
       percent (added, total));
  MSG ("LRAT %" PRId64 " deleted clauses %.2f%%", deleted,
       percent (deleted, total));
  MSG ("LRAT %" PRId64 " bytes (%.2f MB)", bytes,
       bytes / (double) (1 << 20));
}

#endif

void LratTracer::close (bool print) {
  assert (!closed ());
  file->close ();
#ifndef QUIET
  if (print) {
    MSG ("LRAT proof file '%s' closed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

void LratTracer::flush (bool print) {
  assert (!closed ());
  file->flush ();
#ifndef QUIET
  if (print) {
    MSG ("LRAT proof file '%s' flushed", file->name ());
    print_statistics ();
  }
#else
  (void) print;
#endif
}

} // namespace CaDiCaL
