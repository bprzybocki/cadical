#ifndef _lrattracer_h_INCLUDED
#define _lrattracer_h_INCLUDED

#include "file.hpp"
#include "tracer.hpp"

#include <unordered_set>

namespace CaDiCaL {

class LratTracer : public FileTracer {

  Internal *internal;
  File *file;
  bool binary;

#ifndef QUIET
  int64_t added, deleted;
#endif
  int64_t latest_id;
  std::vector<int64_t> delete_ids;

  // Identifiers of weakened clauses, i.e., original or derived clauses
  // moved to the extension stack (for instance by variable elimination or
  // equivalence substitution), which might be restored with the same
  // identifier.  Their deletion is not traced, since after restoring them
  // they would be used with an identifier the checker considers deleted.
  // Keeping them is sound, since the checker has already accepted them.
  //
  std::unordered_set<int64_t> weakened;

  // Assumption clauses (negated cores) traced since the last query (sorted
  // and without duplicated literals) with their identifiers.  They justify
  // original clauses added by the user afterwards which contain one of them
  // (as the stand-alone solver does on 'p inccnf' files, see 'cpp' file).
  //
  std::vector<std::pair<std::vector<int>, int64_t>> assumption_clauses;

  void put_binary_zero ();
  void put_binary_lit (int external_lit);
  void put_binary_id (int64_t id);

  // support LRAT
  void lrat_add_clause (int64_t, const std::vector<int> &,
                        const std::vector<int64_t> &);
  void lrat_delete_clause (int64_t);

public:
  // own and delete 'file'
  LratTracer (Internal *, File *file, bool binary);
  ~LratTracer ();

  void connect_internal (Internal *i) override;
  void begin_proof (int64_t) override;

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
