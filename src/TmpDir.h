#include <boost/filesystem.hpp>
#include <iostream>

class TmpDir {       
  public:             
    void static createTempFile(std::string* tcf, char* tcd);
};
