#!/usr/bin/env bash

# Validate commit hygiene for all non-merge commits after the vanity hash
# enforcement point.  Checks:
#   1. Subject line format (capitalized, length, no trailing period, imperative)
#   2. Vanity hash prefix enforcement (commits after VANITY_BASE must start with 0000)
#   3. AI-generated trailer detection
#   4. Subject/body separation
#
# Usage:
#   scripts/check-commitlog.sh [--quiet|-q] [--range REV_RANGE]
#
# Exit 0 on success, 1 on validation failure.

set -e

# --- Terminal colors ---
set_colors()
{
    if [ -t 1 ]; then
        RED='\033[1;31m'
        GREEN='\033[1;32m'
        YELLOW='\033[1;33m'
        NC='\033[0m'
    else
        RED=''
        GREEN=''
        YELLOW=''
        NC=''
    fi
}
set_colors

QUIET=false
REV_RANGE=""
RANGE_START=""
RANGE_END=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --quiet | -q)
            QUIET=true
            shift
            ;;
        --range)
            [[ $# -ge 2 ]] || {
                echo "Missing value for --range" >&2
                exit 1
            }
            REV_RANGE="$2"
            shift 2
            ;;
        --range=*)
            REV_RANGE="${1#*=}"
            shift
            ;;
        --help | -h)
            echo "Usage: $0 [--quiet|-q] [--range REV_RANGE] [--help|-h]"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Vanity hash enforcement: commits after this point must start with "0000".
VANITY_BASE="0000e59602509f70319e2e4b915fcf1b9a1e2476"
VANITY_PREFIX="0000"

# AI tool identities (mirrors commit-msg.hook)
AI_ID='(Claude Code|Claude Sonnet|Claude Opus|Claude Haiku|Claude AI'
AI_ID+='|GitHub Copilot|Copilot Chat'
AI_ID+='|ChatGPT|GPT-[0-9][0-9.]*[[:alnum:].-]*'
AI_ID+='|Gemini [0-9]|Gemini Pro|Gemini Ultra'
AI_ID+='|Cursor AI|Cursor Tab'
AI_ID+='|Windsurf|Codeium|[Cc]odewhisperer'
AI_ID+='|claude-flow|Devin AI|Devin Bot'
AI_ID+='|Amazon Q Developer'
AI_ID+=')'
TRAILER_AI="(Co-Authored-By|Signed-off-by|Suggested-by|Made-with):[[:space:]]*.*${AI_ID}"
MARKER_PATTERN='(🤖|Generated with \[)'

# Imperative mood blacklist (mirrors commit-msg.hook)
IMPERATIVE_BLACKLIST=(
    added adds adding
    adjusted adjusts adjusting
    amended amends amending
    avoided avoids avoiding
    bumped bumps bumping
    changed changes changing
    checked checks checking
    committed commits committing
    copied copies copying
    corrected corrects correcting
    created creates creating
    decreased decreases decreasing
    deleted deletes deleting
    disabled disables disabling
    dropped drops dropping
    duplicated duplicates duplicating
    enabled enables enabling
    excluded excludes excluding
    fixed fixes fixing
    handled handles handling
    implemented implements implementing
    improved improves improving
    included includes including
    increased increases increasing
    installed installs installing
    introduced introduces introducing
    merged merges merging
    moved moves moving
    pruned prunes pruning
    refactored refactors refactoring
    released releases releasing
    removed removes removing
    renamed renames renaming
    replaced replaces replacing
    resolved resolves resolving
    reverted reverts reverting
    showed shows showing
    tested tests testing
    tidied tidies tidying
    updated updates updating
    used uses using
)

if ! git cat-file -e "${VANITY_BASE}^{commit}" 2> /dev/null; then
    echo -e "${RED}[!] Base commit ${VANITY_BASE} not found. Run 'git fetch --unshallow' or 'git fetch'.${NC}" >&2
    exit 1
fi

if [[ -n "$REV_RANGE" ]]; then
    if [[ "$REV_RANGE" == *..* ]]; then
        RANGE_START="${REV_RANGE%%..*}"
        RANGE_END="${REV_RANGE##*..}"
        if ! git rev-parse --verify "${RANGE_START}^{commit}" > /dev/null 2>&1; then
            echo -e "${RED}[!] Range start ${RANGE_START} not found.${NC}" >&2
            exit 1
        fi
    else
        RANGE_END="$REV_RANGE"
    fi
    if ! git rev-parse --verify "${RANGE_END}^{commit}" > /dev/null 2>&1; then
        echo -e "${RED}[!] Range end ${RANGE_END} not found.${NC}" >&2
        exit 1
    fi
else
    RANGE_START="$VANITY_BASE"
    RANGE_END="HEAD"
    REV_RANGE="${RANGE_START}..${RANGE_END}"
fi

rev_list_args=("$RANGE_END" "^$VANITY_BASE")
if [[ -n "$RANGE_START" ]]; then
    rev_list_args+=("^$RANGE_START")
fi

REPAIR_BASE="${RANGE_START:-$VANITY_BASE}"

commits=$(git rev-list --no-merges "${rev_list_args[@]}")
if [ -z "$commits" ]; then
    $QUIET || echo -e "${GREEN}No commits to check.${NC}"
    exit 0
fi

# --- validate each commit ---
failed=0
warnings=0
suspicious=()

while IFS= read -r commit; do
    [ -z "$commit" ] && continue

    sh=$(git show -s --format=%h "$commit")
    subj=$(git show -s --format=%s "$commit")
    msg=$(git show -s --format=%B "$commit")

    issues=""
    warns=""
    has_issue=0
    has_warn=0

    # 1. Subject line format
    subj_len=${#subj}
    first="${subj:0:1}"
    last="${subj: -1}"

    if [[ $subj_len -le 5 ]]; then
        has_warn=1
        warns+="Subject very short ($subj_len chars)|"
        warnings=$((warnings + 1))
    elif [[ $subj_len -gt 60 ]]; then
        has_issue=1
        issues+="Subject too long ($subj_len chars, max 60)|"
        failed=$((failed + 1))
    fi

    case "$first" in
        [a-z])
            has_issue=1
            issues+="Subject not capitalized|"
            failed=$((failed + 1))
            ;;
    esac

    if [[ "$last" == "." ]]; then
        has_issue=1
        issues+="Subject ends with period|"
        failed=$((failed + 1))
    fi

    # Imperative mood
    shopt -s nocasematch
    read -r first_word _ <<< "$subj"
    for word in "${IMPERATIVE_BLACKLIST[@]}"; do
        if [[ "$first_word" == "$word" ]]; then
            has_issue=1
            issues+="Use imperative mood (\"$first_word\" -> use base form)|"
            failed=$((failed + 1))
            break
        fi
    done
    shopt -u nocasematch

    # Subject/body separation
    second_line=$(echo "$msg" | sed -n '2p')
    if [[ -n "$second_line" ]] && [[ "$second_line" =~ [^[:space:]] ]]; then
        has_issue=1
        issues+="Missing blank line between subject and body|"
        failed=$((failed + 1))
    fi

    # 2. Vanity hash prefix
    if [[ "$commit" != ${VANITY_PREFIX}* ]]; then
        has_issue=1
        issues+="Hash ${sh} does not start with \"${VANITY_PREFIX}\" (run python3 scripts/vanity-hash.py)|"
        failed=$((failed + 1))
    fi

    # 3. AI-generated trailer detection
    if echo "$msg" | grep -Eiq "$TRAILER_AI"; then
        has_issue=1
        issues+="AI-generated commit trailer detected|"
        failed=$((failed + 1))
    fi
    if echo "$msg" | grep -Eiq "$MARKER_PATTERN"; then
        has_issue=1
        issues+="AI-generated marker detected|"
        failed=$((failed + 1))
    fi

    # --- report ---
    if [[ $has_issue -eq 1 || $has_warn -eq 1 ]]; then
        printf '%b%s%b %s\n' "${YELLOW}Commit " "$sh:" "${NC}" "$subj"

        if [[ $has_issue -eq 1 ]]; then
            IFS='|' read -ra arr <<< "${issues%|}"
            for i in "${arr[@]}"; do
                [ -n "$i" ] && printf '  [ %bFAIL%b ] %s\n' "${RED}" "${NC}" "$i"
            done
            suspicious+=("$sh: $subj")
        fi

        if [[ $has_warn -eq 1 ]]; then
            IFS='|' read -ra arr <<< "${warns%|}"
            for w in "${arr[@]}"; do
                [ -n "$w" ] && printf '  %b!%b %s\n' "${YELLOW}" "${NC}" "$w"
            done
        fi
    fi
done <<< "$commits"

if [[ $failed -gt 0 ]]; then
    echo -e "\n${RED}Problematic commits:${NC}"
    for c in "${suspicious[@]}"; do
        echo -e "  ${RED}-${NC} $c"
    done
    echo -e "\n${RED}Recommended actions:${NC}"
    echo -e "1. Install hooks: ${YELLOW}scripts/install-git-hooks${NC}"
    echo -e "2. Never use ${YELLOW}--no-verify${NC}"
    echo -e "3. Amend if needed: ${YELLOW}git rebase -i ${REPAIR_BASE}${NC}"
    echo ""
    echo -e "${RED}[!] Commit-log validation failed.${NC}" >&2
    exit 1
fi

if [[ $warnings -gt 0 ]]; then
    $QUIET || echo -e "\n${YELLOW}Some commits have quality warnings but passed validation.${NC}"
fi

$QUIET || echo -e "${GREEN}All commits OK.${NC}"
exit 0
