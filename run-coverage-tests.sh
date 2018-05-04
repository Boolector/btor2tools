#!/bin/sh
grep BTOR2_FORMAT_TAG_ btor2parser/btor2parser.h | \
sed -e 's,.*TAG_,,' -e 's/,.*$//g' | \
sort > /tmp/btor2parser-run-coverage-tags-in-header
grep 'PARSE (' btor2parser/btor2parser.c | \
sed -e 's,.*PARSE (,,' -e 's/,.*//g' | \
sort > /tmp/btor2parser-run-coverage-tags-in-parsed
diff \
  /tmp/btor2parser-run-coverage-tags-in-header \
  /tmp/btor2parser-run-coverage-tags-in-parsed | sed -e '/^[0-9]/d'
make clean && ./configure.sh -g -c && make && ./runtests.sh && gcov -o build btor2parser/btor2parser.c
echo "vi btor2parser.c.gcov"
