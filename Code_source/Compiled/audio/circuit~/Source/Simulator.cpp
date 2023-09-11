/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Made by Timothy Schoen
 */

#define _USE_MATH_DEFINES
#include <cmath>

#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include <memory>

#include "Simulator.h"
#include "MNA.h"
#include "Models.h"
#include "Components.h"
#include "Netlist.h"

// Read pin index from string, error checked
std::pair<std::vector<std::string>, std::vector<int>> getPinsAndArguments(std::vector<std::string> args, int numPins)
{
    std::vector<int> pins;

    for (int i = static_cast<int>(args.size()) - numPins; i < args.size(); i++) {
        int pin = 0;
        try {
            pin = std::stoi(args[i]);
        } catch (...) {
            pd_error(NULL, "circuit~: invalid number of pins for %s", args[0].c_str());
        }

        pins.push_back(pin);
    }

    args.erase(args.end() - numPins, args.end());
    args.erase(args.begin());

    return { args, pins };
}

// Deallocate netlist
void simulator_free(void* netlist)
{
    auto* net = static_cast<NetList*>(netlist);
    delete net;
}

// Create a new netlist with the same components as the old one, for a full state reset
void* simulator_reset(void* netlist, int blockSize, double sampleRate)
{
    auto* net = static_cast<NetList*>(netlist);

    auto lastNetlist = net->getLastNetlist();
    auto nNets = net->getLastNumNets();

    delete net;

    return new NetList(lastNetlist, nNets, blockSize, sampleRate);
}

// Construct a netlist from pure-data arguments
void* simulator_create(int argc, t_atom* argv, int blockSize, double sampleRate)
{
    // Parse input text from pure-data arguments
    std::stringstream netlistStream;
    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL)
            netlistStream << argv[i].a_w.w_symbol->s_name;
        else if (argv[i].a_type == A_FLOAT)
            netlistStream << argv[i].a_w.w_float;

        netlistStream << ' ';
    }

    NetlistDescription netlistDescription;

    // first we split by ;
    std::string obj;
    while (std::getline(netlistStream, obj, ';')) {
        std::vector<std::string> arguments;
        std::vector<std::string> optargs;
        std::string segment;
        std::stringstream inputcode(obj);

        // then we split by space
        while (std::getline(inputcode, segment, ' ')) {
            // trim whitespace and add to segments array
            auto const strBegin = segment.find_first_not_of(" ");
            if (strBegin != std::string::npos) {

                auto const strEnd = segment.find_last_not_of(" ");
                auto const strRange = strEnd - strBegin + 1;

                arguments.push_back(segment.substr(strBegin, strRange));
            }
        }
        if (!arguments.size())
            continue;

        // See if there is a model flag set
        std::string model = "";
        for (int i = 0; i < arguments.size() - 1; i++) {
            if (!arguments[i].compare("-model")) {
                model = arguments[i + 1];
                arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            }
        }

        if (!arguments[0].compare("resistor") && arguments.size() > 3) {
            auto [args, pins] = getPinsAndArguments(arguments, 2);
            netlistDescription.emplace_back(tResistor, args, pins, "");
        } else if (!arguments[0].compare("capacitor") && arguments.size() > 3) {
            auto [args, pins] = getPinsAndArguments(arguments, 2);
            netlistDescription.emplace_back(tCapacitor, args, pins, "");
        } else if (!arguments[0].compare("voltage") && arguments.size() > 3) {
            auto [args, pins] = getPinsAndArguments(arguments, 2);
            netlistDescription.emplace_back(tVoltage, args, pins, "");
        } else if (!arguments[0].compare("diode") && arguments.size() > 2) {
            auto [args, pins] = getPinsAndArguments(arguments, 2);
            netlistDescription.emplace_back(tDiode, args, pins, model);
        } else if (!arguments[0].compare("bjt") && arguments.size() > 4) {
            auto [args, pins] = getPinsAndArguments(arguments, 3);
            netlistDescription.emplace_back(tBJT, args, pins, model);
        } else if (!arguments[0].compare("mosfet") && arguments.size() > 4) {
            auto [args, pins] = getPinsAndArguments(arguments, 3);
            netlistDescription.emplace_back(tMOSFET, args, pins, model);
        } else if (!arguments[0].compare("jfet") && arguments.size() > 4) {
            auto [args, pins] = getPinsAndArguments(arguments, 3);
            netlistDescription.emplace_back(tJFET, args, pins, model);
        } else if (!arguments[0].compare("opamp") && arguments.size() > 3) {
            auto [args, pins] = getPinsAndArguments(arguments, 3);
            netlistDescription.emplace_back(tOpAmp, args, pins, "");
        } else if (!arguments[0].compare("transformer") && arguments.size() > 5) {
            auto [args, pins] = getPinsAndArguments(arguments, 4);
            netlistDescription.emplace_back(tTransformer, args, pins, "");
        } else if (!arguments[0].compare("gyrator") && arguments.size() > 5) {
            auto [args, pins] = getPinsAndArguments(arguments, 4);
            netlistDescription.emplace_back(tGyrator, args, pins, "");
        } else if (!arguments[0].compare("inductor") && arguments.size() > 3) {
            auto [args, pins] = getPinsAndArguments(arguments, 2);
            netlistDescription.emplace_back(tInductor, args, pins, "");
        } else if (!arguments[0].compare("potmeter") && arguments.size() > 5) {
            auto [args, pins] = getPinsAndArguments(arguments, 3);
            netlistDescription.emplace_back(tPotmeter, args, pins, "");
        } else if (!arguments[0].compare("current") && arguments.size() > 3) {
            auto [args, pins] = getPinsAndArguments(arguments, 2);
            netlistDescription.emplace_back(tCurrent, args, pins, "");
        } else if (!arguments[0].compare("probe") && arguments.size() > 2) {
            auto [args, pins] = getPinsAndArguments(arguments, 2);
            netlistDescription.emplace_back(tProbe, args, pins, "");
        } else if (!arguments[0].compare("triode") && arguments.size() > 3) {
            auto [args, pins] = getPinsAndArguments(arguments, 3);
            netlistDescription.emplace_back(tTriode, args, pins, model);
        } else if (!arguments[0].compare("-iter") && arguments.size() > 1) {
            auto [args, pins] = getPinsAndArguments(arguments, 0);
            netlistDescription.emplace_back(tIter, args, pins, "");
        } else {
            pd_error(NULL, "circuit~: unknown combination of identifier \"%s\" and %lu arguments", arguments[0].c_str(), arguments.size() - 1);
        }
    }

    // A valid netlist contains no unused numbers
    // To make designing netlists easier, we do this programmatically so the user doesn't need to worry about it
    std::vector<int> usedPins = { 0 };
    for (auto const& [name, args, pins, model] : netlistDescription) {
        for (auto& pin : pins) {
            usedPins.push_back(pin);
        }
    }

    // Sort and remove duplicates
    std::sort(usedPins.begin(), usedPins.end());
    usedPins.erase(std::unique(usedPins.begin(), usedPins.end()), usedPins.end());

    for (auto& [name, args, pins, model] : netlistDescription) {
        for (int i = 0; i < usedPins.size(); i++) {

            for (auto& pin : pins) {
                if (pin == usedPins[i]) {
                    // Replace each pin with the index of the equivalent pin to remove holes from the netlist
                    pin = i;
                }
            }
        }
    }

    return new NetList(netlistDescription, static_cast<int>(usedPins.size()), blockSize, sampleRate);
}

void simulator_set_input(void* netlist, int idx, double input)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setInput(idx, input);
}

double simulator_get_output(void* netlist, int idx)
{
    auto* net = static_cast<NetList*>(netlist);
    return net->getOutput(idx);
}

void simulator_tick(void* netlist)
{
    auto* net = static_cast<NetList*>(netlist);
    net->simulateTick();
}

int simulator_num_inlets(void* netlist)
{
    return static_cast<NetList*>(netlist)->getMaxDynamicArgument();
}

int simulator_num_outlets(void* netlist)
{
    auto num_out = static_cast<NetList*>(netlist)->getMNA().output.size();
    return static_cast<int>(num_out);
}

void simulator_set_dc_block(void* netlist, int block)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setBlockDC(block);
}

void simulator_set_iter(void* netlist, int iter)
{
    auto* net = static_cast<NetList*>(netlist);
    net->setMaxIter(iter);
}
