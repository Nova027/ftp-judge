/*
 Author - Swarnava Das (20CS60R07)

 C++ program for FTP Server with Codejudge Functionality
 
*/

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <dirent.h>

#include <errno.h>

#include <iostream>
#include <fstream>
#include <string>

#include <cstring>
#include <cmath>
#include <cstdlib>

using namespace std;

#define SA struct sockaddr
#define max_clients 30





/*............................................................... Helper Functions (To check File statistics) .........................................................*/

// Check if given name/path exists
bool file_exists(string fname)
{
    struct stat fstats;

    if(stat(fname.c_str(), &fstats) == 0)
        return true;

    return false; 
}


// Check if given name/path refers to a directory
bool isdir(string fname)
{
    struct stat fstats;
    stat(fname.c_str(), &fstats);

    if(fstats.st_mode & S_IFDIR)
        return true;

    return false;
}


// Check if given name/path refers to a regular file
bool isregular(string fname)
{
    struct stat fstats;
    stat(fname.c_str(), &fstats);

    if(fstats.st_mode & S_IFREG)
        return true;

    return false;
}


// Check if the (valid) file is accessible
bool file_readable(string fname)
{
    ifstream fin(fname);

    if(fin.good())
    {
        fin.close();
        return true;
    }

    fin.close();
    return false;
}


// Return file size of a readable file
unsigned long long int filesize(string fname)
{
    struct stat fstats;
    stat(fname.c_str(), &fstats);
    
    return fstats.st_size;
}




/*.................................................................... Data Transfer Functions ...........................................................................*/

// Send file of name 'fname' to Socket FD 'sockfd'
string send_file(int sockfd, unsigned long int iters, int lastchunk, string fname)
{
    // File empty
    if(lastchunk == 0)
        return "Transfer Complete";

    char readbuf[1024];

    // Open file in Read Mode; Binary Mode
    ifstream fin(fname, ios::binary);
        
    unsigned long int i;
    for(i=0; i<iters; i++)
    {
        if(!fin.good())
        {
            // To check for Unexpected Read errors
            
            for(unsigned long int j=i; j<iters+1; j++)
                send(sockfd, "garbage", 1024, 0);           // Send garbage just to avoid Blocking Client
            
            fin.close();

            break;
        }

        bzero(readbuf, 1024);

        // Read & Send 1024 Bytes at a time
        fin.read(readbuf, 1024);
        send(sockfd, readbuf, 1024, 0);
    }

    bzero(readbuf, 1024);

    if(i<iters)
    {
        // Complete File couldn't be Read

        sprintf(readbuf, "%d", -1);
        send(sockfd, readbuf, 1024, 0);           // Indicate Unsuccessful Send

        fin.close();
        
        return "-1";
    }

    if(!fin.good())
    {
        // To check for Unexpected Read errors

        send(sockfd, "garbage", 1024, 0);
        fin.close();

        sprintf(readbuf, "%d", -1);
        send(sockfd, readbuf, 1024, 0);           // Indicate Unsuccessful Send
        
        return "-1";        
    }

    // Read & Send Last Chunk
    fin.read(readbuf, lastchunk);
    send(sockfd, readbuf, lastchunk, 0);
    
    bzero(readbuf, 1024);    
    
    sprintf(readbuf, "%d", 1);
    send(sockfd, readbuf, 1024, 0);             // Indicate Successful Send

    fin.close();
    return "Transfer Complete";
}



// Receive file of name 'fname' from Socket FD 'sockfd'
string recv_file(int sockfd, unsigned long int iters, int lastchunk, string fname)
{
    char writebuf[1024];

    // Open file in Write Mode; Binary Mode         .........(Overwrites in case of CODEJUD)
    ofstream fout(fname, ios::binary);

    // If file empty (done)
    if(lastchunk == 0)
    {
        fout.close();
        return "File Received Successfully";
    }

    for (unsigned long int i=0; i<iters; ++i)
    {
        // Receive & Write 1024 bytes into file at a time
        bzero(writebuf, 1024);

        int valread = read(sockfd, writebuf, sizeof(writebuf));
        if(valread<=0)
        {
            fout.close();
            printf("Error Receiving File!\n");
        
            return "-1";
        }

        if(fout)
            fout.write(writebuf, 1024);

        else
        {
            fout.close();

            printf("Error Writing to File!\n");
            return "-1";
        }
    }

    // Receive & Write Last chunk
    bzero(writebuf, 1024);

    int valread = read(sockfd, writebuf, lastchunk);
    if(valread<=0)
    {
        fout.close();
        printf("Error Receiving File!\n");
        
        return "-1";
    }

    if(fout)
        fout.write(writebuf, lastchunk);

    // Check if File was received successfully
    bzero(writebuf, 1024);

    valread = read(sockfd, writebuf, sizeof(writebuf));
    if(valread<=0)
    {
        fout.close();
        printf("\n Error Receiving File!\n");
        
        return "-1";
    }

    if(strcmp(writebuf, "-1") == 0)
    {
        fout.close();
        printf("File Error at Client-side!\n");

        return "-1";
    }

    fout.close();

    return "File Received Successfully";
}



// Sends List of Files at Current Directory to Client
string listfiles(int sockfd)
{
    struct dirent *entry;
    DIR *dir = opendir("./");   // "./" denotes PWD

    char buff[1024];
    bzero(buff, 1024);

    if(dir == NULL)
    {
        sprintf(buff, "%s", ".");
        send(sockfd, buff, 1024, 0);

        return "Directory Error!";
    }

    while((entry = readdir(dir)) != NULL)
    {
        bzero(buff, 1024);

        if(strcmp(entry->d_name, ".")!=0 && strcmp(entry->d_name, "..")!=0)
        {
            // ".." & "." represent parent & current directories
            sprintf(buff, "%s", entry->d_name);

            send(sockfd, buff, 1024, 0);
        }
    }

    // "." used as Termination Signal
    // Client won't confuse it with other entries as we don't send "." or ".."
    
    bzero(buff, 1024);

    sprintf(buff, "%s", ".");
    send(sockfd, buff, 1024, 0);

    closedir(dir);
	
    return "Successfully Listed";
}




/*................................................... Request Processing Functions ............................................*/

// Perform action specified by reqtype : Retr / Stor / Dele

string ftp_action(int reqcode, int sockfd, string fname="")
{
    char buff[1024];
    bzero(buff, 1024);

    if(reqcode == 3)
    {
        /*..... RETR command .....*/

        // Check if the given fname is valid, readable, etc.
        if(!file_exists(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "File not found!";
        }

        if(isdir(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "Specified path is a Directory";
        }

        if(!isregular(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "Specified name is not a Valid File!";
        }

        if(!file_readable(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "Specified File does not have Read Access";
        }

        // Calculate File Size
        unsigned long int iters = filesize(fname)/1024;
        int lastchunk = filesize(fname)%1024;
        
        if(lastchunk == 0 && iters != 0)
        {
            iters--;
            lastchunk = 1024;
        }

        sprintf(buff, "%lu", iters);
        send(sockfd, buff, 1024, 0);        // Send iteration count (Req.d to set File Receive loop)

        bzero(buff, 1024);
        sprintf(buff, "%d", lastchunk);
        send(sockfd, buff, 1024, 0);        // Send size of last chunk        

        // Check if Client is Ready to Receive
        int valread = read(sockfd, buff, sizeof(buff));

        if(valread<=0 || (strcmp(buff, "1")!=0 && strcmp(buff, "-1")!=0))
        {
            printf("\n Unexpected Client behavior\n");
            return "-1";
        }

        if(strcmp(buff, "1") == 0)
            return send_file(sockfd, iters, lastchunk, fname);      // Send file if it's ready

        return "File already exists at Client-side";
    }


    if(reqcode >= 4)
    {
        /*..... STOR (also works with CODEJUD) command ....*/
        unsigned long int iters = 0;
        int lastchunk;

        // Wait for iteration count
        int valread = read(sockfd, buff, sizeof(buff));
        if(valread<=0)
            return "-1";

        if(strcmp(buff, "-1") == 0)
            return "f_error";           // Some issue with file @ Client-side

        sscanf(buff, "%lu", &iters);

        // Wait for last chunk size
        valread = read(sockfd, buff, sizeof(buff));
        if(valread<=0)
            return "-1";

        sscanf(buff, "%d", &lastchunk);

        // Send Status Ready / Not-Ready to Server
        bzero(buff, 1024);

        if(reqcode == 4 && file_exists(fname))
        {
            // Need not Check for CODEJUD
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);           // Indicate Termination Request

            return "File Already exists!";
        }

        sprintf(buff, "%d", 1);
        send(sockfd, buff, 1024, 0);           // Indicate Recv_ready status

        return recv_file(sockfd, iters, lastchunk, fname);      // Start Receiving if Ready
    }


    if(reqcode == 2)
    {
        /*..... DELE .....*/
	
        // Check if the given fname is valid, readable, etc.
        if(!file_exists(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "File not found!";
        }

        if(isdir(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "Specified path is a Directory";
        }

        if(!isregular(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "Specified name is not a Valid File!";
        }

        if(!file_readable(fname))
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);
            return "Specified File does not have Read Access";
        }

        if(remove(fname.c_str()) != 0)
        {
            sprintf(buff, "%d", -1);
            send(sockfd, buff, 1024, 0);

            return "File Deletion Failed!";
        }

        // Deletion is Successful only if all the above if statements don't hold
        sprintf(buff, "%d", 1);
        send(sockfd, buff, 1024, 0);

        return "File Deleted Successfully!";

    }


    return "empty";        // Default (won't arise)
}



// Identify the type of Request Received from Client
int reqtype(string req)
{
    if(req=="LIST")
	return 1;

    if(req[0]=='D' && req[1]=='E' && req[2]=='L' && req[3]=='E' && req[4]==' ')
        return 2;

    if(req[0]=='R' && req[1]=='E' && req[2]=='T' && req[3]=='R' && req[4]==' ')
	return 3;

    if(req[0]=='S' && req[1]=='T' && req[2]=='O' && req[3]=='R' && req[4]==' ')
	return 4;

    if(req[0]=='C' && req[1]=='O' && req[2]=='D' && req[3]=='E' && req[4]=='J' && req[5]=='U' && req[6]=='D' && req[7]==' ')
        return 5;

    return 0;
}



// Extract Filename from entered FTP Command
string extract_fname(string req)
{
	int len = req.length();
	
	string fname = "";
	for(int i = 5; i < len; ++i)
		fname += req[i];

	return fname;
}



// Judge a Given C/C++ code, and check if it: 
/*
    > Compiles Successfully
    > Runs within 1 second (for every testcase)
    > Runs without errors
    > Runs as Expected (matches Expected outputs)
*/

string codejudge(string fname, string ftype)
{
    string report = "Evaluation Status: ";
    int cmdstatus;

    struct timespec start, end;

    string cfname = fname + "." + ftype;            // Code Filename
    string efname = ftype + ".out";                 // Executable Filename
    string ifname = "input_" + fname + ".txt";      // Input Filename
    string ofname = "output_" + fname + ".txt";     // Output Filename
    string tfname = "testcase_" + fname + ".txt";   // Input Filename

    for(int i=0; (i < fname.length() && isalpha(fname[i])) ; i++)
        efname = fname[i] + efname;

    remove(ofname.c_str());
    remove(efname.c_str());


    /*.......................... Compilation Phase .....................*/

    string compil = cfname + " -o " + efname + " >/dev/null 2>&1";
    
    if(ftype == "c")
        compil = "gcc " + compil;

    else
        compil = "g++ " + compil;

    cmdstatus = system(compil.c_str());

    if(cmdstatus != 0)
    {
        report += "COMPILE_ERROR";
        return report;
    }

    report += "COMPILE_SUCCESS";


    /*....................... Execution Phase ..........................*/

    string ex_cmd = "timeout 1 ./" + efname + " >> " + ofname + " 2> /dev/null";

    int ex_stat = 0;    // { 0:Success; 1:TLE ; 2:Runtime_Error }

    if(!file_exists(ifname) || isdir(ifname) || !isregular(ifname))
    {
        // Doesn't take inputs
        clock_gettime(CLOCK_MONOTONIC, &start);
        cmdstatus = system(ex_cmd.c_str());         // Run Executable File with timeout
        clock_gettime(CLOCK_MONOTONIC, &end);

        long double extime = (end.tv_sec - start.tv_sec)*1e9;
        extime += (end.tv_nsec - start.tv_nsec);                // time (in nanosecs)

        if(extime >= 1e9)
            ex_stat = 1;    // TLE

        else if(cmdstatus != 0)
            ex_stat = 2;    // RTE
    }
    
    else
    {
        // Takes input from ifname file
        string testcase;
        ifstream fin(ifname);

        if(!fin.good())
            ex_stat = 2;    // Considered as RTE because it happened in Exec. Phase

        while(getline(fin, testcase))
        {
            // Pass testcase to executable file

            int tlen = testcase.length();

            if(tlen!=0 && testcase[tlen-1] == '\r')     // Removing CR if present
            {
                string tmp = "";

                for(int i = 0; i < tlen-1; ++i)
                    tmp += testcase[i];

                testcase = tmp;
            }

            clock_gettime(CLOCK_MONOTONIC, &start);
            cmdstatus = system(("echo \"" + testcase + "\" | " + ex_cmd).c_str());               // Run Executable File with timeout
            clock_gettime(CLOCK_MONOTONIC, &end);

            long double extime = (end.tv_sec - start.tv_sec)*1e9;
            extime += (end.tv_nsec - start.tv_nsec);                // time (in nanosecs)

            if(extime >= 1e9)
            {
                ex_stat = 1;    // TLE
                break;
            }

            else if(cmdstatus != 0)
            {
                ex_stat = 2;    // RTE
                break;
            }
        }

        fin.close();
    }


    // Check execution status
    if(ex_stat == 1 || ex_stat == 2)
    {
        if(ex_stat == 1)
            report += "\tTIME_LIMIT_EXCEEDED";

        else
            report += "\tRUN_ERROR";

        remove(ofname.c_str());
        remove(efname.c_str());

        return report;
    }

    report += "\tRUN_SUCCESS";


    /*........................... Result Matching Phase .........................*/

    if(!file_exists(tfname) || isdir(tfname) || !isregular(tfname) || !file_exists(ofname) || isdir(ofname) || !isregular(ofname))
    {
        // If Testcase or Output File missing or Inaccessible (Corner case)
        report += "\tCANNOT_TEST";

        remove(ofname.c_str());
        remove(efname.c_str());

        return report;
    }

    ifstream outf(ofname);
    ifstream tst(tfname);

    int match_stat = 0;                 // {0:Success; 1:Mismatch; 2:Unexpected_Matching_Error}
    string printed, expected;

    while(true)
    {
        if(!getline(outf, printed))
        {
            if(getline(tst, expected))
                match_stat = 2;         // outf reached EOF but tst did not

            break;
        }

        if(!getline(tst, expected))
        {
            match_stat = 2;             // tst reached EOF but outf did not
            break;
        }
        
        int elen = expected.length();
        if(elen!=0 && expected[elen-1] == '\r')     // Removing CR if present
        {
            string tmp = "";
            for(int i = 0; i < elen-1; ++i)
                tmp += expected[i];

            expected = tmp;
        }

        if(printed != expected)
        {
            match_stat = 1;              // Mismatch
            break;
        }
    }

    // Check Match status
    if(match_stat == 0)
        report += "\tACCEPTED";

    else if(match_stat == 1)
        report += "\tWRONG_ANSWER";

    else
        report += "\tUNEXPECTED_MATCHING_ERROR";

    remove(ofname.c_str());
    remove(efname.c_str());

    return report;
}





/*................................................... Socket Programming & Communication Driver Functions ............................................*/


// Function designed for ONE-TIME Message Exchange between Single Client and Server
// Returns True iff the ONE-TIME exchange was succesful
bool scComm(int sd, struct sockaddr_in client)
{
    /*........................ Read Client's Message .........................*/
    char buff[1024];
    bzero(buff, 1024);

    // Read the message from client and copy it in buffer
    int valread = read(sd, buff, 1024);
    
    // Check if client exit
    if(valread == 0 || strcmp(buff, ":exit") == 0 || strcmp(buff, "QUIT") == 0) 
    { 
        // Client disconnected , get the details and print 
        printf("Client %s:%d Disconnected\n\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        
	//Close the socket (mark as 0 in list) for reuse 
        close(sd);
        return false;
    } 
    
    string req(buff);
    printf("Action Requested by Client: \"%s\"\n", buff);


    /*............................ Process Request ..........................*/
    char reply[1024];
    bzero(reply, 1024);

    // Check request type
    int reqcode = reqtype(req);

    string status;
    string fname;

    bool existed = false;       // Flag to check if file existed before Execution of Command

    if(reqcode == 0)
        status = "Invalid Request";

    else if(reqcode == 1)
        status = listfiles(sd);    // LIST
    	
    else if(reqcode <= 4)
    {
        // < DELE / RETR / STOR >
    	fname = extract_fname(req);

        if(file_exists(fname))
            existed = true;

    	status = ftp_action(reqcode, sd, fname);
    }

    else
    {
        // CODEJUD
        // Expected Format: CODEJUD <c_code_name>.c    ;  OR ;    CODEJUD <cplusplus_code_name>.cpp //

        // Extract fname, type
        string ftype = "";
        fname = "";

        int i;
        for(i = req.length() - 1; req[i]!='.' && i>=0; --i)
            ftype = req[i] + ftype;

        for(int j = 8; j < i; ++j)
            fname += req[j];

        if(ftype!="c" && ftype!="cpp")
            status = "Not a Valid Code";

        else
        {
            status = ftp_action(reqcode, sd, (fname+"."+ftype));    // STOR
            if(status != "-1" && status != "f_error")
                status = codejudge(fname, ftype);

            // Delete File once Judgement is done           ......(Regardless of Successful Evaluation or Evaluation Status)
            remove(fname.c_str());
        }
    }

    
    // Check Communication Status & Do the needful
    if(status == "-1")
    {
        if(reqcode == 2 && !existed)
        {
            // Del file which Client tried to Store Unsuccessfully
            // (only if it didn't exist prior to request)
            remove(fname.c_str());
        }

        // Client disconnected , get the details and print 
        printf("Client %s:%d Disconnected\n\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        //Close the socket (mark as 0 in list) for reuse 
        close(sd);

        return false;
    }

    if(status == "f_error")
    {
        cout<<"Issue with file @ Client-side\n"<<endl;
        return true;
    }

    sprintf(reply, "%s", status.c_str());
    
    
    /*..................... Send final ACK reply ...................*/
    bzero(buff, 1024);

    // Keep Server Message in buff
    sprintf(buff, "%s", reply);

    // Send the contents of buff to Client
    send(sd, buff, strlen(buff), 0);

    printf("Final Reply Sent: \"%s\"\n\n", buff);

    return true;        // If one communication round was successful

}





// Driver Function to Set up Server-side Sockets, and handle Concurrency via "Select"

int main(int argc, char const *argv[])
{
    int PORT = atoi(argv[1]);
    int opt = 1;
    int master_socket, new_conn;
    int activity, sd; 
    int max_sd;
	
    int client_socket[max_clients];
    struct sockaddr_in servaddr, cli; 
    
    fd_set readfds;     // Set of Socket Descriptors
    
    // Initialise all client_sockets to 0 (closed) 
    for(int i = 0; i < max_clients; i++) 
        client_socket[i] = 0;
        
    // Create a TCP Socket as Master socket
    master_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(master_socket == -1) 
    { 
        perror("Socket Creation failed...\n"); 
        exit(0); 
    } 
    
    printf("Socket Created..\n");

    // Set master socket to allow multiple connections
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) 
    { 
        perror("setsockopt failed...\n"); 
        exit(0);
    }

    printf("Multi-Client support...\n");
    
    // Assign IP; PORT
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT); 
    
    // Binding newly created socket to given IP and Port
    if (bind(master_socket, (struct sockaddr *)&servaddr, sizeof(servaddr))<0) 
    { 
        perror("Socket Bind failed...\n"); 
        exit(0);
    } 
    
    printf("Socket Bind complete..\n"); 

    // Listening for Connections (max 3)
    if (listen(master_socket, 3) < 0) 
    {
        perror("Listen failed...\n"); 
        exit(0); 
    }

    printf("\nMaster Server Listening..\n");

 
    int addrlen = sizeof(cli); 

    // Master Server keeps running        
    while(true) 
    { 
        // Clear Read FDs 
        FD_ZERO(&readfds); 
    
        // Add Master socket to SD set 
        FD_SET(master_socket, &readfds); 
        max_sd = master_socket; 
        
        // Add Child sockets to SD set
        for(int i = 0; i < max_clients; i++) 
        {
            sd = client_socket[i]; 
                
            // If valid socket descriptor, then mark corresponding Read FD 
            if(sd > 0) 
                FD_SET(sd, &readfds); 
            
            // Update upper limit of SDs if necessary
            if(sd > max_sd) 
                max_sd = sd; 
        }
    
        // Wait for an event (interrupt) on one of the sockets indefinitely 
        activity = select(max_sd+1, &readfds, NULL, NULL, NULL); 
    
        if((activity < 0) && (errno!=EINTR))
        {
            perror("Select error...\n");
            exit(0);
        }
        
        // Check Master socket for incoming connections
        if(FD_ISSET(master_socket, &readfds)) 
        {
            new_conn = accept(master_socket, (struct sockaddr *)&cli, (socklen_t*)&addrlen);

            if(new_conn < 0) 
            { 
                perror("Accept error...\n"); 
                exit(0); 
            }

            printf("New Connection Established... Client ID = %s : %d\n", inet_ntoa(cli.sin_addr) , ntohs(cli.sin_port));

            // add new socket to array of sockets 
            for(int i = 0; i < max_clients; i++) 
            { 
                // if Empty SD found 
                if(client_socket[i] == 0 ) 
                {
                    client_socket[i] = new_conn;
                    break; 
                }
            }
        }
        
        // Perform communication between waiting clients and active child sockets
        for(int i = 0; i < max_clients; i++) 
        {
            sd = client_socket[i];          // Service the client @ Socket descriptor sd
                
            if(FD_ISSET(sd, &readfds)) 
            {
                bool success = scComm(sd, cli); // ? bool or something else?

                if(!success)
                    client_socket[i] = 0;   // Client exit, close the SD for other clients to connect
            }
        }
    }

    // Close the master socket
    close(master_socket);

    return 0;
}
