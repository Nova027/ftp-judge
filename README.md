## Online Judge on top of Multi-Client FTP Server

- Typical FTP functionalities implemented using TCP Sockets : Download/Upload/Delete File, List Directory.
- Supports uploading C/C++ programs via FTP for auto-evaluated response : Compile Error / Runtime Error / TLE / Testcase Failed / Success.

______________________________________________________________________________________________________

Compilation, Execution examples :==

Compile server: g++ server.cpp -o server.out ; 
Compile client: g++ client.cpp -o client.out

Run server: ./server.out 8080 ; 
Run client: ./client.out localhost 8080

________________________________________________________________________________________________________

PLEASE NOTE!! 
The CODEJUD command works perfectly if the evaluated code uses only cin/scanf to take inputs. 
However, if it uses getline, fgets, etc, it might encounter Unexpected problems.... 
ONLY IF the files were manually generated in a NON-UNIX SYSTEM. Special care hs been taken to strip CARRIAGE RETURN from strings read.
But Unexpected behavior might be possible. Hence, it would be ideal to generate the files in a Linux OS.
________________________________________________________________________________________________________

CODEJUD has been implemented to return a Single Message containing complete status of Compilation, Execution, and Acceptance.
Server always deletes temporary files and Code files associated with CODEJUD
________________________________________________________________________________________________________

Commands implemented & Their Syntaxes: (By default, <filename> INCLUDES extension, unless stated otherwise)
1. LIST (List contents of Directory) -> LIST
2. DELE (Delete File) -> DELE <filename>
3. RETR (Download File) -> RETR <filename>
4. STOR (Upload File) -> STOR <filename>
5. CODEJUD (Upload C/C++ File for Evaluation) -> CODEJUD <(.c/.cpp)filename>
6. QUIT -> QUIT			..... Pressing CTRL+C also Quits Client

Examples for RETR, STOR, DELE:
"RETR testcase.txt" : Transfer "testcase.txt" file from Server to Client side
"STOR video.mp4" : Transfer "video.mp4" file from Client to Server side
"DELE video.mp4" : Delete "video.mp4" file at Server side

Example for CODEJUD:
"CODEJUD add.cpp" : Server will Receive & Evaluate "add.cpp" C++ code
"CODEJUD div.c" : Server will Receive & Evaluate "div.c" C code

________________________________________________________________________________________________________

Warning: CODEJUD command will delete files with the same filename at Server-side.

string status (returned by ftp_action function (and others) at Server-side) :
> -  "-1" => Fatal Error at Client Side (Server kills Connection). <br>
> -  "f_error" => Ignorable Error at Client Side (Server discards current operation). <br>
> - None of the above => Successful operation.

Under almost no circumstance, does the Running Server exit, unless explicitly forced to do so.
Other minor bugs may cause Server exit in very rare unforeseen cases. In that case, Client also exits normally.

However, even in case of minor issues, the client tends to exit, unless the error is ignorable. 
Even for a possible Server-side problem, the Client is forced to exit most times.
