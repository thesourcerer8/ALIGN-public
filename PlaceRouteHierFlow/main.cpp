#include "toplevel.h"

int main(int argc, char** argv ){
  std::vector<string> args;
  for (unsigned int i=0; i<=argc; ++argc) {
    args.push_back( argv[i]);
  }
  return toplevel( args);
}
