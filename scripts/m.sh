#!/bin/dash
#set -x # for debugging
set -e # exit on errors

# commands are defined as "cmd_X () ...", descriptions are in variable "$cmd_X"
cmd_check='    check installation requirements'
cmd_g='        pass source code through the code generator'
cmd_go='       pass source code through the code generator (old version)'
cmd_tc='       run native C++ tests as a continuous TDD loop'
cmd_tp='       run native Python++ tests as a continuous TDD loop'
cmd_TT='       test command with "getopts abc: f"'

cmd_check () {
    tool_version uname   '-mrs'                   &&
    tool_version python3 '-V'                     &&
    tool_version pip3    '-V 2>/dev/null'         &&
   #tool_version inv     '--version'              &&
    tool_version pio     '--version'              &&
    tool_version git     '--version'              &&
    tool_version make    '-v | head -1'           &&
    tool_version g++     '-v  2>&1 | grep " ver"' &&
    tool_version fswatch '--version | head -1'    &&
    true
}

cmd_g () {
    scripts/gen.py "$@" common/monty/
}

cmd_go () {
    scripts/codegen.py "$@" qstr.h common/monty/ dash3.cpp qstr.cpp
}

cmd_tc () { cd apps/nat-cpp && make tdd; }

cmd_tp () { cd apps/nat-py && make tdd; }

cmd_TT () {
    while getopts abc: f; do
        case $f in
            a|b) flag=$f;;
            c)   carg=$OPTARG;;
            \?)  printf '%s' $USAGE; exit 1;;
        esac
    done
    shift $(($OPTIND-1)) 
    echo test command $#: "$@" . "<<$carg>>" $0
}

tool_version () {
    if type $1 >/dev/null; then
        eval "$*"
        printf "\t"; which $1
    else
        error_msg $1 is missing
    fi
}

# print to stderr (in bold if tty) and return a non-zero status
error_msg () {
    if [ -t 2 ]; then
        echo "[1m<<< $* >>>[0m"
    else
        echo "<<< $* >>>"
    fi >&2
    false
}

#--- the rest is boilerplate ... ----------------------------------------------

os=$(uname -s) # Darwin, Linux, etc

# hidden "T" command to test proper argument quoting
#  run as: m T a b '   c d   ' e
#  output: T 4: a b    c d    e .
cmd_T () echo T $#: "$@" .

# extract all the "cmd_*" variables and return their suffix
known_commands () { set | sed -n 's/cmd_\(.*\)=.*/\1/p'; }

print_usage () {
    local d cmd
    echo 'Usage: m <cmd> ...'
    echo
    for d in $(known_commands | sort -f); do
        eval cmd=\${cmd_$d}
        case "$cmd" in
          ?*) echo "    m $d$cmd" ;;
        esac
    done
    echo
}

# run first arg if it is a known command, else print usage info
cmd=cmd_$1
if type $cmd >/dev/null; then shift; else cmd=print_usage; fi
$cmd "$@"

# vim: fdm=indent :
