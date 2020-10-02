/***********************************************************************
 basic-server.cpp - Implements a fairly basic single-threaded Winsock 
    server program that waits for a connection, accepts it, echoes back
    any data it receives, and then goes back to listening when the
    client drops the connection.

 Compiling:
    VC++: cl -GX basic-server.cpp main.cpp ws-util.cpp wsock32.lib
    BC++: bcc32 basic-server.cpp main.cpp ws-util.cpp
    
 This program is hereby released into the public domain.  There is
 ABSOLUTELY NO WARRANTY WHATSOEVER for this product.  Caveat hacker.

 (Downloaded from http://tangentsoft.net/wskfaq/examples/basics/basic-server.cpp)
***********************************************************************/

#if !defined(WS_UTIL_H)
#define WS_UTIL_H

// Uncomment one.
#include <winsock.h>
//#include <winsock2.h>

extern const char* WSAGetLastErrorMessage(const char* pcMessagePrefix,
        int nErrorID = 0);
extern bool ShutdownConnection(SOCKET sd);

#endif // !defined (WS_UTIL_H)


/***********************************************************************
 ws-util.cpp - Some basic Winsock utility functions.

 This program is hereby released into the public domain.  There is
 ABSOLUTELY NO WARRANTY WHATSOEVER for this product.  Caveat hacker.
***********************************************************************/

#include <iostream>
#include <algorithm>
#include <strstream>


using namespace std;

#if !defined(_WINSOCK2API_) 
// Winsock 2 header defines this, but Winsock 1.1 header doesn't.  In
// the interest of not requiring the Winsock 2 SDK which we don't really
// need, we'll just define this one constant ourselves.
#define SD_SEND 1
#endif


//// Constants /////////////////////////////////////////////////////////

const int kBufferSize = 65536;
        

//// Statics ///////////////////////////////////////////////////////////

// List of Winsock error constants mapped to an interpretation string.
// Note that this list must remain sorted by the error constants'
// values, because we do a binary search on the list when looking up
// items.
static struct ErrorEntry {
    int nID;
    const char* pcMessage;

    ErrorEntry(int id, const char* pc = 0) : 
    nID(id), 
    pcMessage(pc) 
    { 
    }

    bool operator<(const ErrorEntry& rhs) 
    {
        return nID < rhs.nID;
    }
} gaErrorList[] = {
    ErrorEntry(0,                  "No error"),
    ErrorEntry(WSAEINTR,           "Interrupted system call"),
    ErrorEntry(WSAEBADF,           "Bad file number"),
    ErrorEntry(WSAEACCES,          "Permission denied"),
    ErrorEntry(WSAEFAULT,          "Bad address"),
    ErrorEntry(WSAEINVAL,          "Invalid argument"),
    ErrorEntry(WSAEMFILE,          "Too many open sockets"),
    ErrorEntry(WSAEWOULDBLOCK,     "Operation would block"),
    ErrorEntry(WSAEINPROGRESS,     "Operation now in progress"),
    ErrorEntry(WSAEALREADY,        "Operation already in progress"),
    ErrorEntry(WSAENOTSOCK,        "Socket operation on non-socket"),
    ErrorEntry(WSAEDESTADDRREQ,    "Destination address required"),
    ErrorEntry(WSAEMSGSIZE,        "Message too long"),
    ErrorEntry(WSAEPROTOTYPE,      "Protocol wrong type for socket"),
    ErrorEntry(WSAENOPROTOOPT,     "Bad protocol option"),
    ErrorEntry(WSAEPROTONOSUPPORT, "Protocol not supported"),
    ErrorEntry(WSAESOCKTNOSUPPORT, "Socket type not supported"),
    ErrorEntry(WSAEOPNOTSUPP,      "Operation not supported on socket"),
    ErrorEntry(WSAEPFNOSUPPORT,    "Protocol family not supported"),
    ErrorEntry(WSAEAFNOSUPPORT,    "Address family not supported"),
    ErrorEntry(WSAEADDRINUSE,      "Address already in use"),
    ErrorEntry(WSAEADDRNOTAVAIL,   "Can't assign requested address"),
    ErrorEntry(WSAENETDOWN,        "Network is down"),
    ErrorEntry(WSAENETUNREACH,     "Network is unreachable"),
    ErrorEntry(WSAENETRESET,       "Net connection reset"),
    ErrorEntry(WSAECONNABORTED,    "Software caused connection abort"),
    ErrorEntry(WSAECONNRESET,      "Connection reset by peer"),
    ErrorEntry(WSAENOBUFS,         "No buffer space available"),
    ErrorEntry(WSAEISCONN,         "Socket is already connected"),
    ErrorEntry(WSAENOTCONN,        "Socket is not connected"),
    ErrorEntry(WSAESHUTDOWN,       "Can't send after socket shutdown"),
    ErrorEntry(WSAETOOMANYREFS,    "Too many references, can't splice"),
    ErrorEntry(WSAETIMEDOUT,       "Connection timed out"),
    ErrorEntry(WSAECONNREFUSED,    "Connection refused"),
    ErrorEntry(WSAELOOP,           "Too many levels of symbolic links"),
    ErrorEntry(WSAENAMETOOLONG,    "File name too long"),
    ErrorEntry(WSAEHOSTDOWN,       "Host is down"),
    ErrorEntry(WSAEHOSTUNREACH,    "No route to host"),
    ErrorEntry(WSAENOTEMPTY,       "Directory not empty"),
    ErrorEntry(WSAEPROCLIM,        "Too many processes"),
    ErrorEntry(WSAEUSERS,          "Too many users"),
    ErrorEntry(WSAEDQUOT,          "Disc quota exceeded"),
    ErrorEntry(WSAESTALE,          "Stale NFS file handle"),
    ErrorEntry(WSAEREMOTE,         "Too many levels of remote in path"),
    ErrorEntry(WSASYSNOTREADY,     "Network system is unavailable"),
    ErrorEntry(WSAVERNOTSUPPORTED, "Winsock version out of range"),
    ErrorEntry(WSANOTINITIALISED,  "WSAStartup not yet called"),
    ErrorEntry(WSAEDISCON,         "Graceful shutdown in progress"),
    ErrorEntry(WSAHOST_NOT_FOUND,  "Host not found"),
    ErrorEntry(WSANO_DATA,         "No host data of that type was found")
};
const int kNumMessages = sizeof(gaErrorList) / sizeof(ErrorEntry);


//// WSAGetLastErrorMessage ////////////////////////////////////////////
// A function similar in spirit to Unix's perror() that tacks a canned 
// interpretation of the value of WSAGetLastError() onto the end of a
// passed string, separated by a ": ".  Generally, you should implement
// smarter error handling than this, but for default cases and simple
// programs, this function is sufficient.
//
// This function returns a pointer to an internal static buffer, so you
// must copy the data from this function before you call it again.  It
// follows that this function is also not thread-safe.

const char* WSAGetLastErrorMessage(const char* pcMessagePrefix, 
    int nErrorID /* = 0 */)
{
    // Build basic error string
    static char acErrorBuffer[256];
    ostrstream outs(acErrorBuffer, sizeof(acErrorBuffer));
    outs << pcMessagePrefix << ": ";

    // Tack appropriate canned message onto end of supplied message 
    // prefix. Note that we do a binary search here: gaErrorList must be
	// sorted by the error constant's value.
	ErrorEntry* pEnd = gaErrorList + kNumMessages;
    ErrorEntry Target(nErrorID ? nErrorID : WSAGetLastError());
    ErrorEntry* it = lower_bound(gaErrorList, pEnd, Target);
    if ((it != pEnd) && (it->nID == Target.nID)) {
        outs << it->pcMessage;
    }
    else {
        // Didn't find error in list, so make up a generic one
        outs << "unknown error";
    }
    outs << " (" << Target.nID << ")";

    // Finish error message off and return it.
    outs << ends;
    acErrorBuffer[sizeof(acErrorBuffer) - 1] = '\0';
    return acErrorBuffer;
}


//// ShutdownConnection ////////////////////////////////////////////////
// Gracefully shuts the connection sd down.  Returns true if we're
// successful, false otherwise.

bool ShutdownConnection(SOCKET sd)
{
    // Disallow any further data sends.  This will tell the other side
    // that we want to go away now.  If we skip this step, we don't
    // shut the connection down nicely.
    if (shutdown(sd, SD_SEND) == SOCKET_ERROR) {
        return false;
    }

    // Receive any extra data still sitting on the socket.  After all
    // data is received, this call will block until the remote host
    // acknowledges the TCP control packet sent by the shutdown above.
    // Then we'll get a 0 back from recv, signalling that the remote
    // host has closed its side of the connection.
    char acReadBuffer[kBufferSize];
    while (1) {
        int nNewBytes = recv(sd, acReadBuffer, kBufferSize, 0);
        if (nNewBytes == SOCKET_ERROR) {
            return false;
        }
        else if (nNewBytes != 0) {
            cerr << endl << "FYI, received " << nNewBytes <<
                    " unexpected bytes during shutdown." << endl;
        }
        else {
            // Okay, we're done!
            break;
        }
    }

    // Close the socket.
    if (closesocket(sd) == SOCKET_ERROR) {
        return false;
    }

    return true;
}

#include <winsock.h>

#include <iostream>

using namespace std;


////////////////////////////////////////////////////////////////////////
// Constants


////////////////////////////////////////////////////////////////////////
// Prototypes

SOCKET SetUpListener(const char* pcAddress, int nPort);
SOCKET AcceptConnection(SOCKET ListeningSocket, sockaddr_in& sinRemote);
bool EchoIncomingPackets(SOCKET sd);


//// DoWinsock /////////////////////////////////////////////////////////
// The module's driver function -- we just call other functions and
// interpret their results.

int DoWinsock(const char* pcAddress, int nPort)
{
    // Begin listening for connections
    cout << "Establishing the listener..." << endl;
    SOCKET ListeningSocket = SetUpListener(pcAddress, htons(nPort));
    if (ListeningSocket == INVALID_SOCKET) {
        cout << endl << WSAGetLastErrorMessage("establish listener") << 
                endl;
        return 3;
    }

    // Spin forever handling clients
    while (1) {
        // Wait for a connection, and accepting it when one arrives.
        cout << "Waiting for a connection, ^c to abort..." << flush;
        sockaddr_in sinRemote;
        SOCKET sd = AcceptConnection(ListeningSocket, sinRemote);
        if (sd != INVALID_SOCKET) {
            cout << "Accepted connection from " <<
                    inet_ntoa(sinRemote.sin_addr) << ":" <<
                    ntohs(sinRemote.sin_port) << "." << endl;
        }
        else {
            cout << endl << WSAGetLastErrorMessage(
                    "accept connection") << endl;
            return 3;
        }
        
        // Bounce packets from the client back to it.
        if (EchoIncomingPackets(sd)) {
            // Successfully bounced all connections back to client, so
            // close the connection down gracefully.
            cout << "Shutting connection down..." << flush;
            if (ShutdownConnection(sd)) {
                cout << "Connection is down." << endl;
            }
            else {
                cout << endl << WSAGetLastErrorMessage(
                        "shutdown connection") << endl;
                return 3;
            }
        }
        else {
            cout << endl << WSAGetLastErrorMessage(
                    "echo incoming packets") << endl;
            return 3;
        }
    }

#if defined(_MSC_VER)
    return 0;       // warning eater
#endif
}


//// SetUpListener /////////////////////////////////////////////////////
// Sets up a listener on the given interface and port, returning the
// listening socket if successful; if not, returns INVALID_SOCKET.

SOCKET SetUpListener(const char* pcAddress, int nPort)
{
    u_long nInterfaceAddr = inet_addr(pcAddress);
    if (nInterfaceAddr != INADDR_NONE) {
        SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sd != INVALID_SOCKET) {
            sockaddr_in sinInterface;
            sinInterface.sin_family = AF_INET;
            sinInterface.sin_addr.s_addr = nInterfaceAddr;
            sinInterface.sin_port = nPort;
            if (bind(sd, (sockaddr*)&sinInterface, 
                    sizeof(sockaddr_in)) != SOCKET_ERROR) {
                listen(sd, 1);
                return sd;
            }
        }
    }

    return INVALID_SOCKET;
}


//// AcceptConnection //////////////////////////////////////////////////
// Waits for a connection on the given socket.  When one comes in, we
// return a socket for it.  If an error occurs, we return 
// INVALID_SOCKET.

SOCKET AcceptConnection(SOCKET ListeningSocket, sockaddr_in& sinRemote)
{
    int nAddrSize = sizeof(sinRemote);
    return accept(ListeningSocket, (sockaddr*)&sinRemote, &nAddrSize);
}


//// EchoIncomingPackets ///////////////////////////////////////////////
// Bounces any incoming packets back to the client.  We return false
// on errors, or true if the client closed the socket normally.

bool EchoIncomingPackets(SOCKET sd)
{
    // Read data from client
    char acReadBuffer[kBufferSize];
    int nReadBytes;
    do {
        nReadBytes = recv(sd, acReadBuffer, kBufferSize, 0);
        if (nReadBytes > 0) {
            cout << "Received " << nReadBytes << 
                    " bytes from client." << endl;
        
            int nSentBytes = 0;
            while (nSentBytes < nReadBytes) {
                int nTemp = send(sd, acReadBuffer + nSentBytes,
                        nReadBytes - nSentBytes, 0);
                if (nTemp > 0) {
                    cout << "Sent " << nTemp << 
                            " bytes back to client." << endl;
                    nSentBytes += nTemp;
                }
                else if (nTemp == SOCKET_ERROR) {
                    return false;
                }
                else {
                    // Client closed connection before we could reply to
                    // all the data it sent, so bomb out early.
                    cout << "Peer unexpectedly dropped connection!" << 
                            endl;
                    return true;
                }
            }
        }
        else if (nReadBytes == SOCKET_ERROR) {
            return false;
        }
    } while (nReadBytes != 0);

    cout << "Connection closed by peer." << endl;
    return true;
}

/***********************************************************************
 main.cpp - The main() routine for all the "Basic Winsock" suite of
    programs from the Winsock Programmer's FAQ.  This function parses
    the command line, starts up Winsock, and calls an external function
    called DoWinsock to do the actual work.

 This program is hereby released into the public domain.  There is
 ABSOLUTELY NO WARRANTY WHATSOEVER for this product.  Caveat hacker.
***********************************************************************/

#include <winsock.h>

#include <stdlib.h>
#include <iostream>

using namespace std;


//// Prototypes ////////////////////////////////////////////////////////

extern int DoWinsock(const char* pcHost, int nPort);


//// Constants /////////////////////////////////////////////////////////

// Default port to connect to on the server
const int kDefaultServerPort = 4242;


//// main //////////////////////////////////////////////////////////////

int main(void)
{
   setbuf(stdout,NULL);

   const char* pcHost = "127.0.0.1";
   int nPort = kDefaultServerPort;

   // Start Winsock up
   WSAData wsaData;
  int nCode;
   if ((nCode = WSAStartup(MAKEWORD(1, 1), &wsaData)) != 0) {
  	cerr << "WSAStartup() returned error code " << nCode << "." <<
  			endl;
       return 255;
   }

   // Call the main example routine.
   int retval = DoWinsock(pcHost, nPort);

   // Shut Winsock back down and take off.
   WSACleanup();
   return retval;
}
