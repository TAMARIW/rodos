echo "::: Running $1 against $2"
# Kill test executable (with SIGKILL) after 6 seconds.  Necessary
# because sometimes tests might deadlock.
timeout -s 9 6 $1 > "$1.output"
# The tests are only used to generate coverage information, thus they
# are all treated as succeeding.
linecount=(`wc -l "$2"`)
pattern="-----------------------------------------------------"
grep -h -B 0 -A $linecount -e $pattern $1.output > $1.output_trimmed
grep -h -B 0 -A $linecount -e $pattern $2 > $1.expected_trimmed
diff -ruh $1.output_trimmed $1.expected_trimmed

