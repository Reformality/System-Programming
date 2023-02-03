#!/bin/bash

#DO NOT REMOVE THE FOLLOWING TWO LINES
git add $0 >> .local.git.out
git commit -a -m "Lab 2 commit" >> .local.git.out
git push >> .local.git.out || echo


#Your code here
for STRING in $(cat $1); do
    VALID=0
    LENGTH=${#STRING}
    echo $STRING > str_temp
    #check length
    if [ $LENGTH -gt 5 ]; then
        if [ $LENGTH -lt 33 ]; then
            let VALID=1
        fi
    fi

    if [ $VALID -eq 0 ]; then
        echo "Error: Password length invalid."
    else
        #+1 points for each char
        SCORE=$LENGTH
        #contains one of the following special characters (#$+%@)
        if grep -q [\#$\+%@] str_temp ; then
            let SCORE=SCORE+5
        fi
        #If the password contains at least one number (0-9)
        if grep -q [0-9] str_temp ; then
            let SCORE=SCORE+5
        fi
        #If the password contains at least one alpha character (A-Za-z)
        if grep -q [A-Za-z] str_temp ; then
            let SCORE=SCORE+5
        fi
        #If the password contains a repeated alphanumeric character (i.e. aa, bbb, 55555)
        if grep -qE "([0-9A-Za-z])\1+" str_temp ; then
            let SCORE=SCORE-10
        fi
        #If the password contains 3 or more consecutive lowercase characters (i.e. bbb, abe, this)
        if grep -q [a-z][a-z][a-z] str_temp ; then
            let SCORE=SCORE-3
        fi
        #If the password contains 3 or more consecutive uppercase characters (i.e. BBB, XQR, APPLE)
        if grep -q [A-Z][A-Z][A-Z] str_temp ; then
            let SCORE=SCORE-3
        fi
        #If the password contains 3 or more consecutive numbers (i.e. 55555, 1747, 123, 321)
        if grep -q [0-9][0-9][0-9] str_temp ; then
            let SCORE=SCORE-3
        fi

        echo "Password Score: "$SCORE
    fi



    
    #echo $STRING $LENGTH valid:$VALID   #testing
done

rm str_temp