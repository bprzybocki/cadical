#ifndef _drattracer_h_INCLUDED
#define _drattracer_h_INCLUDED

#include "file.hpp"
#include "tracer.hpp"

#include <unordered_set>

namespace CaDiCaL {

class DratTracer : public FileTracer {

  Internal *internal;
  File *file;
  bool binary;
#ifndef QUIET
  int64_t added, deleted;
#endif

  // Identifiers of weakened clauses, i.e., original or derived clauses
  // moved to the extension stack (for instance by variable elimination or
  // equivalence substitution), which might be restored.  Their deletion is
  // not traced, since DRAT has no way to justify adding them again after
  // restoring them (in general they are neither RUP nor RAT).  Keeping them
  // is sound, since the checker has already accepted them.
  //
  std::unordered_set<int64_t> weakened;

  // Assumption clauses (negated cores) traced since the last query (sorted
  // and without duplicated literals).  Original clauses added by the user
  // afterwards containing one of them are traced too (see 'cpp' file).
  //
  std::vector<std::vector<int>> assumption_clauses;

  void put_binary_zero ();
  void put_binary_lit (int external_lit);

  // support DRAT
  void drat_add_clause (const std::vector<int> &);
  void drat_delete_clause (const std::vector<int> &);

public:
  // own and delete 'file'
  DratTracer (Internal *, File *file, bool binary);
  ~DratTracer ();

  void connect_internal (Internal *i) override;
  void begin_proof (int64_t) override {} // skip

  void add_original_clause (int64_t, bool, const std::vector<int> &,
                            bool = false) override;

  void add_derived_clause (int64_t, bool, int, const std::vector<int> &,
                           const std::vector<int64_t> &) override;

  void delete_clause (int64_t, bool, const std::vector<int> &) override;

  void weaken_minus (int64_t, const std::vector<int> &) override;

  void solve_query () override;

  void add_assumption_clause (int64_t, const std::vector<int> &,
                              const std::vector<int64_t> &) override;

  void finalize_clause (int64_t, const std::vector<int> &) override {
  } // skip

  void report_status (int, int64_t) override {} // skip

#ifndef QUIET
  void print_statistics ();
#endif
  bool closed () override;
  void close (bool) override;
  void flush (bool) override;
};

} // namespace CaDiCaL

#endif
