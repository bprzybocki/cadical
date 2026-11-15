#!/bin/sh

#--------------------------------------------------------------------------#

die () {
  cecho "${HIDE}test/icnf/run.sh:${NORMAL} ${BAD}error:${NORMAL} $*"
  exit 1
}

msg () {
  cecho "${HIDE}test/icnf/run.sh:${NORMAL} $*"
}

for dir in . .. ../..
do
  [ -f $dir/scripts/colors.sh ] || continue
  . $dir/scripts/colors.sh || exit 1
  break
done

#--------------------------------------------------------------------------#

[ -d ../test -a -d ../test/icnf ] || \
die "needs to be called from a top-level sub-directory of CaDiCaL"

[ x"$CADICALBUILD" = x ] && CADICALBUILD="../build"

[ -x "$CADICALBUILD/cadical" ] || \
  die "can not find '$CADICALBUILD/cadical' (run 'make' first)"

cecho -n "$HILITE"
cecho "---------------------------------------------------------"
cecho "ICNF testing in '$CADICALBUILD'" 
cecho "---------------------------------------------------------"
cecho -n "$NORMAL"

make -C $CADICALBUILD
res=$?
[ $res = 0 ] || exit $res

#--------------------------------------------------------------------------#

solver="$CADICALBUILD/cadical"

#--------------------------------------------------------------------------#

ok=0
failed=0

run () {
  msg "running ICNF tests ${HILITE}'$1'${NORMAL}"
  prefix=$CADICALBUILD/test-icnf
  icnf=../test/icnf/$1.icnf
  log=$prefix-$1.log
  err=$prefix-$1.err
  opts="$icnf --check"
  opts="$icnf"
  cecho "$solver \\"
  cecho "$opts"
  cecho -n "# $2 ..."
  "$solver" $opts 1>$log 2>$err
  res=$?
  if [ ! $res = $2 ] 
  then
    cecho " ${BAD}FAILED${NORMAL} (actual exit code $res)"
    failed=`expr $failed + 1`
  else
    cecho " ${GOOD}ok${NORMAL} (exit code '$res' as expected)"
    ok=`expr $ok + 1`
  fi
}

run empty 10
run false 20
run unit1 20
run unit2 10
run two1 20
run two2 10

#--------------------------------------------------------------------------#

# Checking DRAT and LRAT proofs of 'p inccnf' files where all cubes are
# unsatisfiable and the last cube is empty, so the proof has to derive the
# empty clause for the clauses part of the file, which is extracted as CNF.

dratchecker=$CADICALBUILD/drat-trim
lratchecker=$CADICALBUILD/lrat-trim

for checker in drat lrat
do
  binary=$CADICALBUILD/$checker-trim
  source=../test/cnf/$checker-trim.c
  if [ ! -f $binary -o $source -nt $binary ]
  then
    cmd="cc -O -o $binary $source"
    cecho "$cmd"
    $cmd 2>/dev/null || die "compiling '$source' failed"
  fi
done

proof () {
  name=$1
  shift
  msg "running ICNF proof tests ${HILITE}'$name'${NORMAL}"
  prefix=$CADICALBUILD/test-icnf-proof
  icnf=../test/icnf/$name.icnf
  cnf=$prefix-$name.cnf
  # Extract the clauses (all lines except header, comments and cubes) as
  # CNF in DIMACS format, with the maximum variable and number of clauses
  # (terminating zeros) in the header, as needed by the proof checkers.
  awk '
    /^[pac]/ { next }
    {
      for (i = 1; i <= NF; i++) {
        if ($i == 0) { clauses++; continue }
        var = $i < 0 ? -$i : $i
        if (var > maxvar) maxvar = var
      }
      body = body $0 "\n"
    }
    END { printf "p cnf %d %d\n%s", maxvar, clauses, body }
  ' $icnf > $cnf
  for format in drat lrat
  do
    prf=$prefix-$name.$format
    log=$prefix-$name-$format.log
    err=$prefix-$name-$format.err
    chk=$prefix-$name-$format.chk
    case $format in
      drat) checker=$dratchecker; formatopts=""; expected=0;;
      lrat) checker=$lratchecker; formatopts=" --lrat"; expected=20;;
    esac
    opts="$icnf $prf$formatopts $*"
    cecho "$solver \\"
    cecho "$opts"
    cecho -n "# 20 ..."
    "$solver" $opts 1>$log 2>$err
    res=$?
    if [ ! $res = 20 ]
    then
      cecho " ${BAD}FAILED${NORMAL} (actual exit code $res)"
      failed=`expr $failed + 1`
      continue
    fi
    cecho -n " ${GOOD}ok${NORMAL} ... checking $format proof"
    $checker $cnf $prf 1>$chk 2>&1
    res=$?
    if [ ! $res = $expected ]
    then
      cecho " ${BAD}FAILED${NORMAL} (see '$chk')"
      failed=`expr $failed + 1`
    else
      cecho " ${GOOD}ok${NORMAL}"
      ok=`expr $ok + 1`
    fi
  done
}

proof proofrestore -P1 # restored clauses after elimination
proof proofcore        # negated cores added by the stand-alone solver
proof proofids         # clause identifiers reserved for 'p inccnf'
proof proofcompact --compactint=1 --compactmin=1 --compactlim=0 # cores

#--------------------------------------------------------------------------#

[ $ok -gt 0 ] && OK="$GOOD"
[ $failed -gt 0 ] && FAILED="$BAD"

msg "${HILITE}ICNF testing results:${NORMAL} ${OK}$ok ok${NORMAL}, ${FAILED}$failed failed${NORMAL}"

exit $failed
