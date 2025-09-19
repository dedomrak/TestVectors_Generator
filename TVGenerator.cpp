
#include "./containers/Array.h"          // Make dynamic array class Array available
#include "./containers/Map.h"            // Make associated hash table class Map available
#include "./util/Message.h"        // Make message handlers available, not used in this example
#include "./verilog/veri_file.h"      // Make Verilog reader available
#include "./verilog/VeriModule.h"     // Definition of a VeriModule and VeriPrimitive
#include "./verilog/VeriId.h"         // Definitions of all identifier definition tree nodes
#include "./verilog/VeriExpression.h" // Definitions of all verilog expression tree nodes
#include "./verilog/VeriModuleItem.h" // Definitions of all verilog module item tree nodes
#include "./verilog/VeriStatement.h"  // Definitions of all verilog statement tree nodes
#include "./verilog/VeriMisc.h"       // Definitions of all extraneous verilog tree nodes (ie. range, path, strength, etc...)
#include "./verilog/VeriScope.h"      // Symbol table of locally declared identifiers
#include "./verilog/VeriLibrary.h"    // Definition of VeriLibrary
#include "./verilog/veri_yacc.h"

#include <string.h>
#include <vector>
#include <queue>
#include "support_funcs.h"

#ifdef VERIFIC_NAMESPACE
using namespace Verific ;
#endif

struct Port {
    std::string name;
    std::string direction;
    std::string type;
    std::string bus_size;
    std::queue<char> test_queue;
    bool isClock = false;
};

struct Clock {
    std::string name;
    int period;
};
struct BusRange {
    int high;
    int low;
};

extern std::string checkAndReturnBusDimension(char *busName);
BusRange getBusRangeVals(Port bus);
int genRandVectorValue();


int main(int argc, char **argv)
{

    std::string file_nm;
    std::string tv_file;

    //--------------------------------------------------------------
    // PARSE ARGUMENTS
    //--------------------------------------------------------------
    const char *file_name = 0 ;
    std::string clksString;
    int vectorNumber = 1;
    std::vector<Clock> allClocksList;

    for (int i = 1; i < argc; i++) {
        if (Strings::compare(argv[i], "-i")) {
            i++ ;
            file_nm = (i < argc) ? argv[i]: 0 ;
            continue ;
        } else if (Strings::compare(argv[i], "-testvec")) {
            i++ ;
            tv_file = (i < argc) ? argv[i]: 0 ;
            continue ;
        } else if (Strings::compare(argv[i], "-vecNumber")) {
            i++ ;
            vectorNumber = atoi((i < argc) ? argv[i]: "1") ;
            continue ;
        }
    }
    if(argc==1)
    {
        Message::PrintLine("Usage: Auto Testbench generator:\n");
        Message::PrintLine("         -i                     <input Verilog IP file>\n");
        Message::PrintLine("         -testvec           <Output testvectors file> \n") ;
        Message::PrintLine("         -vecNumber     <generated testvectors number>\n") ;
        return 1 ;
    }

    if(file_nm.empty()) {
        Message::PrintLine("Input file is missing!") ;
        return 1 ;
    }

    if (!veri_file::Analyze(file_nm.c_str(), veri_file::SYSTEM_VERILOG)) return 2 ;

    // Get the list of top modules
    Array *top_mod_array = veri_file::GetTopModules() ;
    if (!top_mod_array) {
        Message::Error(0,"Cannot find any top module. Check for recursive instantiation") ;
        return 4 ;
    }
    VeriModule *module  = (VeriModule *) top_mod_array->GetFirst() ; // Get the first top level module
    delete top_mod_array ; top_mod_array = 0 ; // Cleanup, it is not required anymore

    // Just to see that this is a module, and not a primitive
    VeriIdDef *module_id = (module) ? module->Id() : 0 ;
    if(!module_id) {
        Message::Error(0, "module should always have an 'identifier'.") ;
    }

    if (module_id->IsUdp()) {
        /* This is a Verilog UDP, a primitive */
    } else {
        /* This is a module */
    }

    // Iterate through the module's list of ports ('95 style port or ANSI port declarations)
    Array *ports = module->GetPortConnects() ;
    VeriExpression *port ;
    unsigned i ;
    std::string topModule;
    std::vector<Port> inputPortList;
    std::vector<Port> outputPortList;
    std::vector<Port> inoutPortList;
    std::vector<Port> allPortList;
    std::string tvFileString;

    FOREACH_ARRAY_ITEM(ports, i, port) {
        if (!port) continue ; // Check for NULL pointer

        switch(port->GetClassId()) {
        case ID_VERIANSIPORTDECL:
        {
            VeriAnsiPortDecl *ansi_port = static_cast<VeriAnsiPortDecl*>(port) ;
            if(!ansi_port)
                continue;
            // eg. :
            // input reg [5:0] a, b, c ...

            // Get data type for this declaration
            VeriDataType *data_type = ansi_port->GetDataType() ; // VeriDataType includes 'type' (VERI_REG, VERI_WIRE, VERI_STRUCT etc), array dimension(s), signing ...
            if(!data_type) continue;
            unsigned port_dir = ansi_port->GetDir() ; // a token : VERI_INPUT, VERI_OUTPUT or VERI_INOUT..
            unsigned j ;
            char *portDir;
            Port portId;
            if(port_dir==VERI_INPUT) {
                portDir="Input";
                portId.direction = "input";
            }
            else if(port_dir==VERI_OUTPUT) {
                portDir="Output";
                portId.direction = "output";
            }
            else if(port_dir==VERI_INOUT) {
                portDir="Inout";
                portId.direction = "inout";
            }
            else
                portDir="Unknown dir";
            std::string portDirString = portDir;

            VeriIdDef *port_id ;
            // Iterate through all ids declared in this Ansi port decl
            FOREACH_ARRAY_ITEM(ansi_port->GetIds(), j, port_id) {
                if (!port_id) continue ;
                char *port_name = const_cast<char *>(port_id->Name());
                for (std::vector<Clock>::iterator it = allClocksList.begin() ; it != allClocksList.end(); ++it) {
                    if((*it).name ==portId.name)
                        portId.isClock = true;
                }
                std::string busDim=checkAndReturnBusDimension(port_name);
                if(!busDim.empty()) {
                    portId.bus_size = busDim;
                }
                unsigned port_dir   = port_id->Dir() ;
                if(port_dir==VERI_INPUT) {
                    portDir="Input";
                    portId.direction = "input";
                }
                else if(port_dir==VERI_OUTPUT) {
                    portDir="Output";
                    portId.direction = "output";
                }
                else if(port_dir==VERI_INOUT) {
                    portDir="Inout";
                    portId.direction = "inout";
                }
                else
                    portDir="Unknown dir";

                port_name="";
                char *port_type="";
                if ( data_type->GetType() == VERI_REAL) //port is REAL
                    port_type="real";
                else if ( data_type->GetType() == VERI_WIRE) //port is WIRE
                    port_type="wire";
                else if ( data_type->GetType() == VERI_LOGIC) //port is WIRE
                    port_type="logic";
                else if ( data_type->GetType() == VERI_REG) //port is IXS_TYPE
                    port_type="reg";
                else if ( data_type->GetType() == VERI_TRI) //port is VERI_TRI
                    port_type="tri";
                else if ( data_type->GetType() == VERI_WAND) //port is VERI_WAND
                    port_type="wand";
                else if ( data_type->GetType() == VERI_TRIAND)
                    port_type="triand";
                else if ( data_type->GetType() == VERI_WOR)
                    port_type="wor";
                else if ( data_type->GetType() == VERI_TRIOR)
                    port_type="trior";
                else if ( data_type->GetType() == VERI_TRIREG)
                    port_type="trireg";
                else if ( data_type->GetType() == VERI_TRI0)
                    port_type="tri0";
                else if ( data_type->GetType() == VERI_TRI1)
                    port_type="tri1";
                else if ( data_type->GetType() == VERI_UWIRE)
                    port_type="uwire";
                else if ( data_type->GetType() == VERI_SUPPLY0)
                    port_type="supply0";
                else if ( data_type->GetType() == VERI_SUPPLY1)
                    port_type="supply1";
                else if ( data_type->GetType() == VERI_INTEGER)
                    port_type="integer";
                else if ( data_type->GetType() == VERI_INT)
                    port_type="int";
                else if ( data_type->GetType() == VERI_BYTE)
                    port_type="byte";
                else if ( data_type->GetType() == VERI_SHORTINT)
                    port_type="shortint";
                else if ( data_type->GetType() == VERI_LONGINT)
                    port_type="longint";
                else if ( data_type->GetType() == VERI_BIT)
                    port_type="bit";
                else if ( data_type->GetType() == VERI_SHORTREAL)
                    port_type="shortreal";

                else {
                    port_type=const_cast<char *> (data_type->GetName());
                    if(!port_type)
                        port_type="Unknown";
                }

                portId.type = port_type;
                allPortList.push_back(portId);

            }
            break ;
        }

        case ID_VERIIDREF:
        {
            VeriIdRef *id_ref = static_cast<VeriIdRef*>(port) ;
            if(!id_ref)
                continue;

            // Get the resolved identifier definition that is referred here
            VeriIdDef *id = id_ref->FullId() ;
            char *portDir;
            Port portId;
            char *port_name = const_cast<char *>(id->Name());
            portId.name = port_name;

            for (std::vector<Clock>::iterator it = allClocksList.begin() ; it != allClocksList.end(); ++it) {
                if((*it).name ==portId.name)
                    portId.isClock = true;
            }

            std::string busDim=checkAndReturnBusDimension(port_name);
            if(!busDim.empty()) {
                portId.bus_size = busDim;
            }


            port_name="";
            unsigned is_port    = id->IsPort() ;  // Should return true in this case
            unsigned port_dir   = id->Dir() ;     // returns VERI_INPUT, VERI_OUTPUT, VERI_INOUT
            if(port_dir==VERI_INPUT) {
                portDir="Input";
                portId.direction = "input";
            }
            else if(port_dir==VERI_OUTPUT) {
                portDir="Output";
                portId.direction = "output";
            }
            else if(port_dir==VERI_INOUT) {
                portDir="Inout";
                portId.direction = "inout";
            }
            else
                portDir="Unknown dir";
            std::string portDirString = portDir;
            unsigned port_type  = id->Type() ;    // returns VERI_WIRE, VERI_REG, etc ...
            char *port_typeName="";
            if ( port_type == VERI_REAL) //port is REAL
                port_typeName="real";
            else if ( port_type == VERI_WIRE) //port is WIRE
                port_typeName="wire";
            else if (port_type == VERI_LOGIC) //port is WIRE
                port_typeName="logic";
            else if ( port_type == VERI_REG) //port is IXS_TYPE
                port_typeName="reg";
            else if ( port_type == VERI_TRI) //port is VERI_TRI
                port_typeName="tri";
            else if ( port_type == VERI_WAND) //port is VERI_WAND
                port_typeName="wand";
            else if (port_type == VERI_TRIAND)
                port_typeName="triand";
            else if ( port_type == VERI_WOR)
                port_typeName="wor";
            else if ( port_type == VERI_TRIOR)
                port_typeName="trior";
            else if ( port_type == VERI_TRIREG)
                port_typeName="trireg";
            else if ( port_type == VERI_TRI0)
                port_typeName="tri0";
            else if ( port_type == VERI_TRI1)
                port_typeName="tri1";
            else if ( port_type == VERI_UWIRE)
                port_typeName="uwire";
            else if ( port_type == VERI_SUPPLY0)
                port_typeName="supply0";
            else if ( port_type == VERI_SUPPLY1)
                port_typeName="supply1";
            else if ( port_type == VERI_INTEGER)
                port_typeName="integer";
            else if ( port_type == VERI_INT)
                port_typeName="int";
            else if ( port_type == VERI_BYTE)
                port_typeName="byte";
            else if ( port_type == VERI_SHORTINT)
                port_typeName="shortint";
            else if ( port_type == VERI_LONGINT)
                port_typeName="longint";
            else if ( port_type == VERI_BIT)
                port_typeName="bit";
            else if ( port_type == VERI_SHORTREAL)
                port_typeName="shortreal";
            else {
                VeriAnsiPortDecl *ansi_port = static_cast<VeriAnsiPortDecl*>(port) ;
                if(!ansi_port)
                    continue;
                VeriDataType *data_type = ansi_port->GetDataType() ;
                if(!data_type)
                    continue;
                port_typeName=const_cast<char *> ( data_type->GetName());
                if(!port_typeName /*|| !strstr(typesList,port_type)*/)
                    port_typeName="Unknown";
            }

            portId.type = port_typeName;
            allPortList.push_back(portId);
            break ;
        }
        default:
            Message::Error(port->Linefile(),"unknown port found") ;
        }
    }

    FILE * pTVFile;
    if(!file_name)
        pTVFile = fopen ("test_vecs.tv","w");
    else {
        pTVFile = fopen (file_name,"w");
    }
    if(!pTVFile)
    {
        printf("Error in export file open\n") ;
        return 1;
    }

    tvFileString = "# Ports:\n";
    tvFileString =tvFileString + "#\t\tINPUT\t|\tOUTPUT \n";
    std::string outputPorts;
    std::string  firstTestVector;
    int outputPortsNumber = 0;
    int portNumber = 0;
    for (std::vector<Port>::iterator it = allPortList.begin() ; it != allPortList.end(); ++it) {
        if((*it).bus_size.empty()){
            if((*it).direction=="input") {
                tvFileString = tvFileString  + (*it).name +", ";
                firstTestVector = firstTestVector +"0";
            }
            else {
                outputPorts = outputPorts + (*it).name +", ";
                outputPortsNumber++;
            }
            portNumber++;
        }
        else {
            BusRange bRange = getBusRangeVals(*it);
            int max = bRange.high;
            int min = bRange.low;
            for(int i = 0; i<=(max-min); i++) {
                if((*it).direction=="input") {
                    tvFileString = tvFileString  + (*it).name+"["+std::to_string(max-i)+"]" +", ";
                    firstTestVector = firstTestVector +"0";
                }
                else {
                    outputPorts = outputPorts  + (*it).name+"["+std::to_string(max-i)+"]" +", ";
                    outputPortsNumber++;
                }
                portNumber++;
            }
        }
    }
    tvFileString = tvFileString + " | " + outputPorts ;
    tvFileString.pop_back();
    tvFileString = tvFileString + "\n\n";

    for(;outputPortsNumber>0; outputPortsNumber--) {
        firstTestVector = firstTestVector +"X";
    }
    firstTestVector = firstTestVector +"\n";
    tvFileString = tvFileString + firstTestVector;


    for(;vectorNumber>0; vectorNumber--) {
        std::string testVec;
        for(int i =portNumber ;i>0; i--) {
            testVec = testVec +  std::to_string(genRandVectorValue());
        }
        testVec = testVec + "\n";
        tvFileString = tvFileString + testVec;
    }

    tvFileString =tvFileString + "#End of testvector file! \n";
    fprintf (pTVFile,tvFileString.c_str());
    fclose(pTVFile);

    return 0 ; // status OK.
}


#include <cstdio>

BusRange getBusRangeVals(Port bus) {
    BusRange retVal;
    retVal.low=-1;
    retVal.high=-1;

    std::string busRange = bus.bus_size;
    std::string busRemovedBrackets = busRange.substr(1,busRange.size()-2);

    char *key = ":";
    std::vector<char *> splitVector;
    splitVector = splitString(busRemovedBrackets.c_str(),key);
    if(splitVector.size()!=2)
        return retVal;
    int leftRangeVal = std::stoi(splitVector[0]);
    int rightRangeVal = std::stoi(splitVector[1]);
    if(leftRangeVal>=rightRangeVal) {
        retVal.low = rightRangeVal;
        retVal.high = leftRangeVal;
    }
    else {
        retVal.low = leftRangeVal ;
        retVal.high = rightRangeVal;
    }
    return retVal;
}

#include <cstdlib>   // for rand() and srand()
#include <ctime>

int genRandVectorValue() {
    // Initialize the random seed once
    static bool seeded = false;
    if (!seeded) {
        srand(static_cast<unsigned int>(time(nullptr)));
        seeded = true;
    }
    // Generate and return 0 or 1
    return rand() % 2;
}


