#!/bin/bash
# usage: ./check_CRLF.sh [file] 0/1
# example: ./check_CRLF.sh 01-heap_size/01.expected
# example: ./check_CRLF.sh 01-heap_size/01.expected 0 // just LF endings
# example: ./check_CRLF.sh 01-heap_size/01.expected 1 // just CRLF endings

RED='\033[41;37m'
RESET='\033[0m'
GREEN='\033[42m'
YELLOW='\033[43m'

# if $1 has a line that ends with LF, report the line number and emit warning
function check_CRLF
{
    LINE_NUM=1
    while read -r line
    do
        if [[ $line =~ [^[:space:]]$ ]]
        then
            # if $2 is not set or is 0
            if [[ -z $2 ]] || [[ $2 -eq 0 ]]
            then
                echo -e "${LINE_NUM}: ${GREEN}\\\n${RESET}"
            fi
        else
            # if $2 is not set or is 1
            if [[ -z $2 ]] || [[ $2 -eq 1 ]]
            then 
                echo -e "${LINE_NUM}: ${YELLOW}\\\r\\\n${RESET}"
            fi
        fi
        ((LINE_NUM++))
    done < $1
}

check_CRLF $1 $2