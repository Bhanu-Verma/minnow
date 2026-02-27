#include "debug.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

namespace {
void get_URL( const string& host, const string& path )
{
  debug( "Function called: get_URL( \"{}\", \"{}\" )", host, path );

  // Create a new TCP socket and connect it to the server
  TCPSocket sock {};
  sock.connect( Address( host, "80" ) );

  const string request
    = "GET " + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n" + "\r\n";
  sock.write( request );

  string response {};
  while ( !sock.eof() ) {
    string line {};
    sock.read( line );
    response += line;
  }

  cout << response;
}
} // namespace

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
