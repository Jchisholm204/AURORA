#include <fstream>
#include "YAML_Doc.hpp"
using namespace std;

//set the microapp_name and version which will become part of the YAML doc.
YAML_Doc::YAML_Doc(const std::string& miniApp_Name, const std::string& miniApp_Version, const std::string& destination_Directory, const std::string& destination_FileName){
  miniAppName = miniApp_Name;
  miniAppVersion = miniApp_Version;
  destinationDirectory = destination_Directory;
  destinationFileName = destination_FileName;
}

//inherits the destructor from YAML_Element
YAML_Doc::~YAML_Doc(void){
}

/*
* generates YAML from the elements of the document and, when an output
* file is supplied, writes it to that file
*/
string YAML_Doc::generateYAML(const std::string& output_file){
  string yaml;
  yaml =  yaml + "Mini-Application Name: " + miniAppName + "\n";
  yaml =  yaml + "Mini-Application Version: " + miniAppVersion + "\n";
  for(size_t i=0; i<children.size(); i++){
    yaml = yaml + children[i]->printYAML("");
  }

  if (!output_file.empty()) {
    ofstream myfile;
    myfile.open(output_file.c_str());
    myfile << yaml;
    myfile.close();
  }
  return yaml;
}


