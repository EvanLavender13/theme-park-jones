#!/bin/bash
# Refuses a staged test case, new or with its TEST_CASE line changed, that has more than two
# SECTIONs. A section per input, condition, or entry point enumerates cases instead of testing a
# property (.claude/agents/test-writer.md). The pre-commit hook runs it; it reads the staged
# versions, since they are what is committed, and prints each refused case as file:line.

MAX_SECTIONS=2
STATUS=0
while IFS= read -r -d '' FILE; do
    # The staged line numbers of the TEST_CASE lines the commit adds or changes.
    STARTS=$(git diff --cached -U0 -- "$FILE" | awk '
        /^@@/ { split($3, at, ","); line = substr(at[1], 2); next }
        /^\+\+\+/ { next }
        /^\+/ { if ($0 ~ /^\+TEST_CASE\(/) printf "%d ", line; line++ }')
    if [ -z "$STARTS" ]; then
        continue
    fi
    if ! git show ":$FILE" | awk -v starts="$STARTS" -v file="$FILE" -v most="$MAX_SECTIONS" '
            BEGIN { n = split(starts, s, " "); for (i = 1; i <= n; i++) checked[s[i]] = 1 }
            function close_case() {
                if (open && count > most) {
                    print file ":" open ": " count " SECTIONs in one TEST_CASE"
                    refused = 1
                }
            }
            /^TEST_CASE\(/ { close_case(); open = (NR in checked) ? NR : 0; count = 0 }
            open { count += gsub(/SECTION\(/, "&") }
            END { close_case(); exit refused }'; then
        STATUS=1
    fi
done < <(git diff --cached --name-only --diff-filter=AM -z -- ':(glob)tests/**/*.cpp')
if [ $STATUS -ne 0 ]; then
    echo "A test case tests one property, not one section per input; at most $MAX_SECTIONS SECTIONs."
fi
exit $STATUS
