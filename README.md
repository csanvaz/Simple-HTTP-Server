# HW 4 Addendum

Some notes about facilities contained in this repository (such as [check_CRLF.sh](./check_CRLF.sh)).

## Autograder & Testcases
- you can run one test case a time. See the top of [runtests.sh](./runtests.sh) for usage.
- One of the test cases ([08-statistics/01](tests/08-statistics/01.sh)) use [Makefile](./Makefile) and [runtests.sh](./runtests.sh) as input,
so changing these files may mess up at least that test case. Do not commit these changes if you modified them. Since you can't change [Makefile](./Makefile), all your code will probably have to be written in [hw4.c](./hw4.c) (or in anyway that can be compiled with [Makefile](./Makefile) and the bare command `make`).
- port.txt contains the port number used by the server and the test cases. You are free to choose another port. Do not hardcode port number in your code, as the github autograder might use a secret port number.

## Debugging

A ``.vscode`` folder is provided, which contains the usual debugging settings. You can use the VSCode debugging interface or gdb as usual. To debug a single test case, start a debugging instance first (i.e. let the server start listening to connections), and then in another shell, make sure you are in the *root directory* of the project folder; then run, say, `./tests/02-ping/01.sh $(cat port.txt)` to start the client. The rest are usual; aside from outputing in different shells, they are just normal programs.

## Check line endings
There are two kinds of line endings in an http response: LF and CRLF. Some test cases may have both in the expected output. When you fail a test case by outputing the wrong kind of line endings, it might be hard to tell from the DIFF, so here are two ways to inspect line endings:

- [check_CRLF.sh](./check_CRLF.sh) is a handy script that prints out line endings of a file.
See comments at the top of this script for usage.
Feel free to tweak this script; the autograder does not care.

- You can also print a file character by character with the command
`od -c filename`.


## Misc

### Is the connection established?

The autograder will try to connect to the port for a maximum of 11 times; if it still fails to connect, you will see output like below, and the test case will not run (so it will not tell you "Failed" or "Passed").

```
➜  fa23-361-hw4-start git:(main) ✗ ./runtests.sh 01 01
rm -rf *.o hw4 d
gcc -fsanitize=address -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Werror -std=c17 -Wpedantic -O0 -g hw4.c -o hw4
Running positive test 01-connect/01     Using port 11337
PID: 5516
0
1
2
3
4
5
6
7
8
9
10
```

### Tests taking a long time?

Tests 9 & 10 may take 5-20 seconds if you implement things correctly. Other tests should take at most seconds. Github autograder has 100 seconds timeout, which shoud be plenty. If you wait any longer, it might be that something gets stuck.
