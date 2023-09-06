#include <vector>
#include <string>
#include <cstdio>
#include <cmath>

#include <iostream>
#include <sstream>
#include <map>

#include "Simulator.h"
#include "MNA.h"
#include "Components.h"
#include "Netlist.h"


// Read pin index from string, error checked
int getPinValue(std::string arg) {
    int result = 0;
    try {
        result = std::stoi(arg);
    } catch (...) {
        pd_error(NULL, "Invalid pin index");
    }
    
    return result;
}

// Deallocate netlist
void netlist_free(void* netlist)
{
    auto* net = static_cast<NetList*>(netlist);
    delete net;
}

// Create a new netlist with the same components as the old one, for a full state reset
void* netlist_reset(void* netlist, double sampleRate)
{
    auto* net = static_cast<NetList*>(netlist);
    
    auto description = net->netlistDescription;
    auto pins = net->pinAssignment;
    
    delete net;
    
    net = new NetList(description, pins);
    
    net->buildSystem();
    
    net->setStepScale(0);
    
    // get DC solution
    net->simulateTick();
    net->setTimeStep(1.0 / sampleRate);
    
    return net;
}

// Construct a netlist from pure-data arguments
void* netlist_create(int argc, t_atom* argv, double sampleRate)
{
    // Parse input text from pure-data arguments
    std::ostringstream argStream;
    for (int i = 0; i < argc; i++)
    {
        if (argv[i].a_type == A_SYMBOL)
            argStream << argv[i].a_w.w_symbol->s_name;
        else if (argv[i].a_type == A_FLOAT)
            argStream << argv[i].a_w.w_float;
        
        argStream << ' ';
    }
    
    std::stringstream netlistStream(argStream.str());
    std::string obj;
    
    NetlistDescription netlistDescription;
    
    // first we split by ;
    while(std::getline(netlistStream,obj,';'))
    {
        std::vector<std::string> seglist;
        std::vector<std::string> optargs;
        std::string segment;
        std::stringstream inputcode(obj);
        
        // then we split by space
        while(std::getline(inputcode, segment, ' '))
        {
            // trim whitespace and add to segments array
            const auto strBegin = segment.find_first_not_of(" ");
            if (strBegin != std::string::npos) {
                
                const auto strEnd = segment.find_last_not_of(" ");
                const auto strRange = strEnd - strBegin + 1;
                
                seglist.push_back(segment.substr(strBegin, strRange));
            }
        }
        if(!seglist.size()) continue;
        
        // Skip over ground, it doesn't require implementation
        if(!seglist[0].compare("ground"))
        {
            continue;
        }
        else if(!seglist[0].compare("resistor") && seglist.size() > 3) {
            netlistDescription.emplace_back("resistor", std::vector<std::string>{seglist[1]}, std::vector<int>{getPinValue(seglist[2]), getPinValue(seglist[3])});
        }
        else if(!seglist[0].compare("capacitor") && seglist.size() > 3) {
            netlistDescription.emplace_back("capacitor", std::vector<std::string>{seglist[1]}, std::vector<int>{getPinValue(seglist[2]), getPinValue(seglist[3])});
        }
        else if(!seglist[0].compare("voltage")&& seglist.size() > 3) {
            netlistDescription.emplace_back("voltage", std::vector<std::string>{seglist[1]}, std::vector<int>{getPinValue(seglist[2]), getPinValue(seglist[3])});
        }
        else if(!seglist[0].compare("input") && seglist.size() > 2) {
            netlistDescription.emplace_back("input", std::vector<std::string>{}, std::vector<int>{getPinValue(seglist[1]), getPinValue(seglist[2])});
        }
        else if(!seglist[0].compare("diode") && seglist.size() > 2) {
            netlistDescription.emplace_back("diode", std::vector<std::string>{}, std::vector<int>{getPinValue(seglist[1]), getPinValue(seglist[2])});
        }
        else if(!seglist[0].compare("bjt") && seglist.size() > 3) {
            netlistDescription.emplace_back("bjt", std::vector<std::string>{seglist[4]}, std::vector<int>{getPinValue(seglist[1]), getPinValue(seglist[2]), getPinValue(seglist[3])});
        }
        else if(!seglist[0].compare("opamp") && seglist.size() > 3) {
            if(seglist.size() == 4)
            {
                netlistDescription.emplace_back("opamp", std::vector<std::string>{"10", "15"}, std::vector<int>{getPinValue(seglist[1]), getPinValue(seglist[2]), getPinValue(seglist[3])});
            }
            if(seglist.size() == 5)
            {
                netlistDescription.emplace_back("opamp", std::vector<std::string>{seglist[1], "15"}, std::vector<int>{getPinValue(seglist[2]), getPinValue(seglist[3]), getPinValue(seglist[4])});
            }
            if(seglist.size() == 6)
            {
                netlistDescription.emplace_back("opamp", std::vector<std::string>{seglist[1], seglist[2]}, std::vector<int>{getPinValue(seglist[3]), getPinValue(seglist[4]), getPinValue(seglist[5])});
            }
        }
        else if(!seglist[0].compare("transformer") && seglist.size() > 5) {
            netlistDescription.emplace_back("transformer", std::vector<std::string>{seglist[1]}, std::vector<int>{getPinValue(seglist[2]), getPinValue(seglist[3]), getPinValue(seglist[4]), getPinValue(seglist[5])});
        }
        else if(!seglist[0].compare("gyrator") && seglist.size() > 5) {
            netlistDescription.emplace_back("gyrator", std::vector<std::string>{seglist[1]}, std::vector<int>{getPinValue(seglist[2]), getPinValue(seglist[3]), getPinValue(seglist[4]), getPinValue(seglist[5])});
        }
        else if(!seglist[0].compare("inductor") && seglist.size() > 3) {
            netlistDescription.emplace_back("inductor", std::vector<std::string>{seglist[1]}, std::vector<int>{getPinValue(seglist[2]), getPinValue(seglist[3])});
        }
        else if(!seglist[0].compare("potmeter") && seglist.size() > 5) {
            netlistDescription.emplace_back("potmeter", std::vector<std::string>{seglist[1], seglist[2]}, std::vector<int>{getPinValue(seglist[3]), getPinValue(seglist[4]), getPinValue(seglist[5])});
        }
        else if(!seglist[0].compare("probe") && seglist.size() > 2)
        {
            netlistDescription.emplace_back("probe", std::vector<std::string>{}, std::vector<int>{getPinValue(seglist[1]), getPinValue(seglist[2])});
        }
    }
    
    // A valid netlist contains no unused numbers
    // To make designing netlists easier, we do this programmatically so the user doesn't need to worry about it
    std::vector<int> allPins = {0};
    for(const auto& [name, args, pins] : netlistDescription)
    {
        for(auto& pin : pins)
        {
            allPins.push_back(pin);
        }
    }
    
    std::sort(allPins.begin(), allPins.end());
    allPins.erase(std::unique(allPins.begin(), allPins.end()), allPins.end());

    std::map<int, int> pinAssignment;
    for(int i = 0; i < allPins.size(); i++)
    {
        pinAssignment[allPins[i]] = i;
    }
    
    auto* net = new NetList(netlistDescription, pinAssignment);
    
    net->buildSystem();
    net->setStepScale(0);
    
    // get DC solution
    net->simulateTick();
    net->setTimeStep(1.0 / sampleRate);
    
    return net;
}

void netlist_set_input(void* netlist, int idx, double input)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setDynamicArgument(idx, input);
}

double netlist_get_output(void* netlist, int idx)
{
    auto* net = static_cast<NetList*>(netlist);
    return net->getMNA().output[idx];
}

void netlist_tick(void* netlist)
{
    auto* net = static_cast<NetList*>(netlist);
    net->clearOutput();
    net->simulateTick();
}

int netlist_num_inlets(void* netlist)
{
    return static_cast<NetList*>(netlist)->getNumDynamicArguments();
}

int netlist_num_outlets(void* netlist)
{
    auto num_out = static_cast<NetList*>(netlist)->getMNA().output.size();
    return static_cast<int>(num_out);
}

void netlist_set_dc_block(void* netlist, int block)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setBlockDC(block);
}

void netlist_set_iter(void* netlist, int iter)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setMaxIter(iter);
}
