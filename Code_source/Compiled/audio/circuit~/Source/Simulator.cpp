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




int getPinValue(std::string arg) {
    int result = 0;
    try {
        result = std::stoi(arg);
    } catch (...) {
        pd_error(NULL, "Invalid pin index");
    }
    
    return result;
}

std::string replaceString(std::string subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}


double getArgumentValue(std::string arg) {
    double result = 0.0f;
    
    arg = replaceString(arg, "p", "e-12");
    arg = replaceString(arg, "n", "e-9");
    arg = replaceString(arg, "u", "e-6");
    arg = replaceString(arg, "m", "e-3");
    arg = replaceString(arg, "k", "e3");
    
    try {
        result = std::stod(arg);
    } catch (...) {
        pd_error(NULL, "Invalid circuit description argument");
    }
    
    return result;
}

void* create(int argc, t_atom* argv, double sampleRate)
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

    std::vector<std::tuple<std::string, std::vector<std::string>, std::vector<int>>> netlistDescription;

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

    auto* net = new NetList(allPins.size());

    std::map<int, int> pinAssignment;
    for(int i = 0; i < allPins.size(); i++)
    {
        pinAssignment[allPins[i]] = i;
    }


    int numOut = 0;
    for(const auto& [name, args, pins] : netlistDescription)
    {
        if(name == "resistor")
        {
            if (args[0].rfind("$s", 0) == 0) {
                net->addComponent(new VariableResistor(net->addDynamicArgument(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
            }
            else {
                net->addComponent(new Resistor(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
            }
        }
        else if(name == "capacitor")
        {
            net->addComponent(new Capacitor(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
        }
        else if(name == "voltage")
        {
            if (args[0].rfind("$s", 0) == 0) {
                net->addComponent(new VariableVoltage(net->addDynamicArgument(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
            }
            else {
                net->addComponent(new Voltage(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
            }
        }
        else if(name == "diode")
        {
            net->addComponent(new Diode(pinAssignment[pins[0]], pinAssignment[pins[1]]));
        }
        else if(name == "bjt")
        {
            net->addComponent(new BJT(pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]], getArgumentValue(args[0])));
        }
        else if(name == "transformer")
        {
            net->addComponent(new Transformer(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]], pinAssignment[pins[3]]));
        }
        else if(name == "inductor")
        {
            net->addComponent(new Inductor(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
        }
        else if(name == "opamp")
        {
            net->addComponent(new OpAmp(getArgumentValue(args[0]), getArgumentValue(args[1]), pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]]));
        }
        else if(name == "potmeter")
        {
            net->addComponent(new Potentiometer(net->addDynamicArgument(args[0]), getArgumentValue(args[1]), pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]]));
        }
        else if(name == "probe")
        {
            net->addComponent(new Probe(pinAssignment[pins[0]], pinAssignment[pins[1]], numOut++));
        }
    }
    
    
    net->buildSystem();

    net->setStepScale(0);
    
    // get DC solution
    net->simulateTick();
    net->setTimeStep(1.0 / sampleRate);
    
    return net;
}

void set_input(void* netlist, int idx, double input)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setVariableArg(idx, input);
}

void process(void* netlist)
{
    auto* net = static_cast<NetList*>(netlist);
    net->clearOutput();
    net->simulateTick();
}

int num_inlets(void* netlist)
{
    return static_cast<NetList*>(netlist)->getNumVariableArgs();
}

int num_outlets(void* netlist)
{
    return static_cast<NetList*>(netlist)->getMNA().output.size();
}

void set_dc_block(void* netlist, int block)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setBlockDC(block);
}

double get_output(void* netlist, int idx)
{
    auto* net = static_cast<NetList*>(netlist);
    return net->getMNA().output[idx];
}

void set_iter(void* netlist, int iter)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setMaxIter(iter);
}
