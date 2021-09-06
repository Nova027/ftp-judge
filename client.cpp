/*
 Author - Swarnava Das (20CS60R07)

 C++ program for FTP Client with Codejudge Functionality
 
*/

#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h>

#include <sys/stat.h>

#include <cstring>
#include <string>
#include <iostream>
#include <fstream>

using namespace std;






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




/*.................................................................... File Transfer Functions ...........................................................................*/


// Send file of name 'fname' to Socket FD 'sockfd'
void send_file(int sockfd, unsigned long int iters, int lastchunk, string fname)
{
    // File empty
    if(lastchunk == 0)
        return;
    
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
                send(sockfd, "garbage", 1024, 0);			// Send garbage just to avoid Blocking Server
            
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
        
        perror("\nUnexpected File Error\n");
        exit(0);
    }

    if(!fin.good())
    {
    	// To check for Unexpected Read errors
        send(sockfd, "garbage", 1024, 0);
        fin.close();

        sprintf(readbuf, "%d", -1);
        send(sockfd, readbuf, 1024, 0);           // Indicate Unsuccessful Send
        
        perror("\nUnexpected File Error\n");
        exit(0);
    }

    // Read & Send last chunk
    fin.read(readbuf, lastchunk);
    send(sockfd, readbuf, lastchunk, 0);
    
    bzero(readbuf, 1024);    
    
    sprintf(readbuf, "%d", 1);
    send(sockfd, readbuf, 1024, 0);			    // Indicate Successful Send

    fin.close();
}




// Receive file of name 'fname' from Socket FD 'sockfd'
void recv_file(int sockfd, unsigned long int iters, int lastchunk, string fname)
{
    char writebuf[1024];

    // Open file in Write Mode; Binary Mode
    ofstream fout(fname, ios::binary);

    // If file empty (done)
    if(lastchunk == 0)
    {
        fout.close();
        return;
    }

    for (unsigned long int i=0; i<iters; ++i)
    {
    	// Receive & Write 1024 bytes into file at a time
    	bzero(writebuf, 1024);

    	int valread = read(sockfd, writebuf, sizeof(writebuf));
        if(valread<=0)
        {
        	fout.close();
        	close(sockfd);

        	perror("\n Error Receiving File!\n");
        	exit(0);
        }

        if(fout)
        	fout.write(writebuf, 1024);

        else
        {
        	fout.close();
        	close(sockfd);

        	perror("\n Error Writing to File!\n");
        	exit(0);
        }

    }

    // Receive & Write Last chunk
    bzero(writebuf, 1024);

    int valread = read(sockfd, writebuf, lastchunk);
    if(valread<=0)
    {
        fout.close();
        close(sockfd);

        perror("\n Error Receiving File!\n");
        exit(0);
    }

    if(fout)
        fout.write(writebuf, lastchunk);

    // Check if File was received successfully
    bzero(writebuf, 1024);

    valread = read(sockfd, writebuf, sizeof(writebuf));
    if(valread<=0)
    {
        fout.close();
        close(sockfd);

        perror("\n Error Receiving File!\n");
        exit(0);
    }

    if(strcmp(writebuf, "-1") == 0)
    {
        fout.close();
        close(sockfd);

        perror("\n File Error at Server-side!\n");
        exit(0);
    }

    fout.close();
}





/*................................................... Request Processing Functions ............................................*/


// Wait for Server to perform action specified by reqtype
// Waits for: Retr / Stor / List / Dele
string ftp_service(int reqcode, int sockfd, string fname="")
{
	char buff[1024];
	bzero(buff, 1024);

	if(reqcode == 3)
	{
		/*...... RETR command .....*/

		unsigned long int iters = 0;
		int lastchunk;


		// Wait to receive iteration count
		int valread = read(sockfd, buff, sizeof(buff));
		if(valread<=0)
		{
			close(sockfd);
			perror("\n Error Receiving File Size!\n");
			exit(0);
		}

		if(strcmp(buff, "-1") == 0)
			return "Issue with file @ Server-side";

		sscanf(buff, "%lu", &iters);

		// Wait to receive last chunk size
		valread = read(sockfd, buff, sizeof(buff));
		if(valread<=0)
		{
			close(sockfd);
			perror("\n Error Receiving File Size!\n");
			exit(0);
		}

		sscanf(buff, "%d", &lastchunk);

		// Send Status Ready / Not-Ready to Server
		bzero(buff, 1024);

		if(file_exists(fname))
		{
			sprintf(buff, "%d", -1);
			send(sockfd, buff, 1024, 0);           // Indicate Termination Request

			return "File Already exists!";
		}

		sprintf(buff, "%d", 1);
		send(sockfd, buff, 1024, 0);           // Indicate Recv_ready status

		recv_file(sockfd, iters, lastchunk, fname);      // Start Receiving if Ready
		return "File Received Successfully";
	}

	
	if(reqcode == 4)
	{
		/*.... STOR command ....*/
		
		// Check if the given fname is valid, readable, etc.
		if(!file_exists(fname))
		{
			sprintf(buff, "%d", -1);
			send(sockfd, buff, 1024, 0);
			return " File not found!";
		}

		if(isdir(fname))
		{
			sprintf(buff, "%d", -1);
			send(sockfd, buff, 1024, 0);
			return " Specified path is a Directory";
		}

		if(!isregular(fname))
		{
			sprintf(buff, "%d", -1);
			send(sockfd, buff, 1024, 0);
			return " Specified name is not a Valid File!";
		}

		if(!file_readable(fname))
		{
			sprintf(buff, "%d", -1);
			send(sockfd, buff, 1024, 0);
			return " Specified File does not have Read Access";
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

		// Check if Server is Ready to Receive
		int valread = read(sockfd, buff, sizeof(buff));
		if(valread<=0 || (strcmp(buff, "1")!=0 && strcmp(buff, "-1")!=0))
		{
			// Indicate Unexpected Server behavior
			close(sockfd);
			perror("\n Error Receiving File Size!\n");
			exit(0);
		}

		if(strcmp(buff, "1") == 0)
		{
			// Send file if it's ready (Will Always happen for CODEJUD)
			send_file(sockfd, iters, lastchunk, fname);
			return "Transfer Complete";
		}

		return "File already exists at Server-side";
	}


	if(reqcode == 2)
	{
		/*..... DELE .....*/

		// Wait for File status: "-1" => Failure
		int valread = read(sockfd, buff, sizeof(buff));
		
		if(valread<=0)
		{
			close(sockfd);
			perror("\n Error Receiving File Status!\n");
			exit(0);
		}

		if(strcmp(buff, "-1") == 0)
			return "Issue with file @ Server-side";

		return "No Issues reported by Server";   
	}


	if(reqcode == 1)
	{
		/* LIST - Receive & Print List of files @ Server-side */
		cout<<"Printing List of Names:"<<endl;

		while(true)
		{
			// Wait for Filenames
			int valread = read(sockfd, buff, sizeof(buff));
			if(valread<=0)
			{
				close(sockfd);
				perror("\n Error Receiving Names!\n");
				exit(0);
			}

			if(strcmp(buff, ".") == 0)
				return "Server sent LIST_END signal";

			cout<<"> "<<buff<<endl;		// Print File / Folder name received

			bzero(buff, 1024);
		}
	}


	return "empty";		// Default (won't arise)
}



// Identify the type of Request sent to Server
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







/*................................................... Socket Programming & Communication Driver Functions ............................................*/

// Function designed for Chat between Client and Server
// Valid Requests: EXIT, DELE, RETR, STOR, QUIT
void cscomm(int sockfd) 
{
	char buff[1024];

	// Loop indefinitely until Client Exits
	while(true)
	{
		/*....................... Send Command to Server ................*/
		bzero(buff, sizeof(buff));

		printf("\n Enter Request to send: ");
		cin.getline(buff, 1024);

		// Send Request
		write(sockfd, buff, sizeof(buff));

		// Check if Client entered "QUIT" Command
		if(strcmp(buff, ":exit")==0 || strcmp(buff, "QUIT")==0)
				break;

		string req(buff);

		printf("\n Request sent to Server: \"%s\"\n", buff);


		/*....................... Get Request Served ....................*/      
		// Check request type
		int reqcode = reqtype(req);

		string status;
		string fname;

		if(reqcode == 0)
		    status = "Invalid Request";

		else if(reqcode == 1)
		    status = ftp_service(reqcode, sockfd);    // LIST

		else if(reqcode <= 4)
		{
		    // < DELE / RETR / STOR >
		    fname = extract_fname(req);
		    status = ftp_service(reqcode, sockfd, fname);
		}

		else
		{
		    // CODEJUD
		    status = "Server senpai is judging me";

		    // Extract fname, type
		    string ftype = "";
		    fname = "";

		    for(int i = 8; i < req.length(); ++i)
			fname += req[i];

		    for(int i = req.length() - 1; req[i]!='.' && i>=0; --i)
			ftype = req[i] + ftype;

		    if(ftype!="c" && ftype!="cpp")
			status = "Unexpected File Type";

		    else
			status = ftp_service(4, sockfd, fname);       // STOR (4 is reqcode)

		    // Leave & Wait for Server's Code Judgement
		}

		cout<<" Status: "<<status<<endl;

		if(status[0] == ' ')
			continue;				// Equivalent to f_error @ Server-side     ......(Will also work if CODEJUD fails at Client-side)


		/*............. Read Final ACK Reply Message from Server ...........*/
		bzero(buff, sizeof(buff));

		// Read the Reply from Server and copy it in buffer
		int valread = read(sockfd, buff, sizeof(buff));

		// Check for Read errors
		if(valread<=0)
		{
			close(sockfd);
			perror("\n Reply Not Received!\n");
			exit(0);
		}

		printf(" Reply received from Server: \"%s\" \n", buff);

	}
}





// Driver Function to Set up Client-side Socket
int main(int argc, char const *argv[]) 
{
	// IPA and Server Port no. as Command Line Args
	char const *ip_temp = argv[1];
	char const *ipa;

	if(strcmp(ip_temp, "localhost") == 0)
		ipa = "127.0.0.1";

	else
		ipa = ip_temp;


	int PORT = atoi(argv[2]);

	//................... Connect with the Server ..................//

	int sock = 0; 
	struct sockaddr_in serv_addr;	

	// Create Socket
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("\n Socket creation error\n"); 
		exit(0);
	}

	printf("\n Socket Created...\n");
	

	// Assign Socket Port-8080, Type, IP
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(PORT); 

	// Convert Loopback IPv4 address from text to binary form 
	if(inet_pton(AF_INET, ipa, &serv_addr.sin_addr) <= 0) 
	{ 
		perror("\n Invalid address\n"); 
		exit(0);
	}


	// Connect to Server
	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{ 
		perror("\n Connection Failed\n"); 
		exit(0);
	}

	printf("\n Connection Established...\n");


	//.................... Communication .....................//
	
	cscomm(sock);
	close(sock);
	
	return 0; 
} 
