/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Written by Timothy Schoen, adopted from LiveSPICE
 */

#pragma once

using Model = std::map<std::string, double>;
using ModelList = std::map<std::string, std::map<std::string, double>>;
class Models {
    
    static inline const ModelList Triodes = {
        { "12AT7", {
            { "Ex", 1.234 },
            { "Mu", 67.49 },
            { "K", 1.73E-06 },
            { "Kg", 419.1 },
            { "Kp", 213.96 },
            { "Kvb", 300 },
            { "Rgk", 1000000.0 },
            { "Vg", 0.33 }
        } },
        { "12AU7", {
            { "Ex", 1.3 },
            { "Mu", 21.5 },
            { "K", 1.73E-06 },
            { "Kg", 1180 },
            { "Kp", 84 },
            { "Kvb", 300 },
            { "Rgk", 1000000.0 },
            { "Vg", 0.33 }
        } },
        { "12AX7", {
            { "Ex", 1.4 },
            { "Mu", 83.5 },
            { "K", 1.73E-06 },
            { "Kg", 1060 },
            { "Kp", 600 },
            { "Kvb", 300 },
            { "Rgk", 1000000.0 },
            { "Vg", 0.33 },
            { "Gamma", 1.26 },
            { "G", 0.002242 },
            { "Gg", 0.0006177 },
            { "C", 3.4 },
            { "Cg", 9.901 },
            { "Xi", 1.314 },
            { "Ig0", 8.025e-8 }
        } },
        { "12AY7", {
            { "Ex", 1.113 },
            { "Mu", 44.16 },
            { "K", 1.73E-06 },
            { "Kg", 1192.4 },
            { "Kp", 409.96 },
            { "Kvb", 300 },
            { "Rgk", 1000000.0 },
            { "Vg", 0.33 }
        } },
        { "12AZ7", {
            { "Ex", 1.371 },
            { "Mu", 74.08 },
            { "K", 1.73E-06 },
            { "Kg", 382 },
            { "Kp", 190.11 },
            { "Kvb", 300 },
            { "Rgk", 1000000.0 },
            { "Vg", 0.33 }
        } },
        { "6SL7GT", {
            { "Ex", 1.233 },
            { "Mu", 75.89 },
            { "Rgk", 2000.0 },
            { "Kvb", 7.0 }
        } }
    };
    
    static inline const ModelList Pentodes = {
        {"6L6GC-JJ", {
            {"Mu", 13.94},
            {"Ex", 1.063},
            {"Kg1", 320.2},
            {"Kg2", 4500},
            {"Kp", 33.2},
            {"Kvb", 29.6},
            {"Rgi", 1000}
        }},
        {"6V6GTA", {
            {"Mu", 10.7},
            {"Ex", 1.31},
            {"Kg1", 1672.0},
            {"Kg2", 4500},
            {"Kp", 41.16},
            {"Kvb", 12.7},
            {"Rgi", 2000}
        }},
        {"EL34-JJ", {
            {"Mu", 11.55},
            {"Ex", 1.35},
            {"Kg1", 650.2},
            {"Kg2", 6000},
            {"Kp", 51.85},
            {"Kvb", 25.4},
            {"Rgi", 1000}
        }},
        {"EL84-JJ", {
            {"Mu", 19.56},
            {"Ex", 1.35},
            {"Kg1", 593.0},
            {"Kg2", 4500},
            {"Kp", 131.34},
            {"Kvb", 17.5},
            {"Rgi", 1000}
        }},
        {"6550", {
            {"Mu", 7.9},
            {"Ex", 1.35},
            {"Kg1", 890},
            {"Kg2", 4200},
            {"Kp", 60},
            {"Kvb", 24},
            {"Rgi", 1000}
        }},
        {"KT88-JJ", {
            {"Mu", 12.43},
            {"Ex", 1.218},
            {"Kg1", 315.7},
            {"Kg2", 4500},
            {"Kp", 26.72},
            {"Kvb", 36.1},
            {"Rgi", 1000}
        }}
    };
    
    static inline const ModelList BJTs = {
        { "2N2222", {
            { "IS", 1e-14 },
            { "BF", 200 },
            { "BR", 3 },
            { "PNP", 0 },
        } },
        { "2N2222A", {
            { "IS", 14.34e-15 },
            { "BF", 255.9 },
            { "BR", 6.092 },
            { "PNP", 0 },
        } },
        { "2N3904", {
            { "IS", 1e-14 },
            { "BF", 300 },
            { "BR", 4 },
            { "PNP", 0 },
        } },
        { "2N5088", {
            { "IS", 5.911e-15 },
            { "BF", 1122 },
            { "BR", 1.271 },
            { "PNP", 0 },
        } },
        { "2N5089", {
            { "IS", 5.911e-15 },
            { "BF", 1434 },
            { "BR", 1.262 },
            { "PNP", 0 },
        } },
        { "2N5210", {
            { "IS", 5.911e-15 },
            { "BF", 809.9 },
            { "BR", 1.287 },
            { "PNP", 0 },
        } },
        { "2N4401", {
            { "IS", 26.03e-15 },
            { "BF", 4292 },
            { "BR", 1.01 },
            { "PNP", 0 },
        } },
        { "MPSA13", {
            { "IS", 360e-15 },
            { "BF", 337 },
            { "BR", 4 },
            { "PNP", 0 },
        } },
        { "MPSA18", {
            { "IS", 33.58e-15 },
            { "BF", 236 },
            { "BR", 5.774 },
            { "PNP", 0 },
        } },
        { "2SC1815", {
            { "IS", 2.04e-15 },
            { "BF", 400 },
            { "BR", 3.377 },
            { "PNP", 0 },
        } },
        { "2SC2240", {
            { "IS", 1.32e-13 },
            { "BF", 454 },
            { "BR", 7.41092 },
            { "PNP", 0 },
        } },
        { "BC107", {
            { "IS", 40.72e-15 },
            { "BF", 407 },
            { "BR", 7.41092 },
            { "PNP", 0 },
        } },
        { "BC107A", {
            { "IS", 7.049e-15 },
            { "BF", 375.5 },
            { "BR", 2.611 },
            { "PNP", 0 },
        } },
        { "BC107B", {
            { "IS", 7.049e-15 },
            { "BF", 381.7 },
            { "BR", 2.359 },
            { "PNP", 0 },
        } },
        { "BC108A", {
            { "IS", 7.049e-15 },
            { "BF", 375.5 },
            { "BR", 2.611 },
            { "PNP", 0 },
        } },
        { "BC108B", {
            { "IS", 7.049e-15 },
            { "BF", 381.7 },
            { "BR", 2.359 },
            { "PNP", 0 },
        } },
        { "BC108C", {
            { "IS", 7.049e-15 },
            { "BF", 677 },
            { "BR", 2.209 },
            { "PNP", 0 },
        } },
        { "BC109B", {
            { "IS", 7.049e-15 },
            { "BF", 381.7 },
            { "BR", 2.359 },
            { "PNP", 0 },
        } },
        { "BC109C", {
            { "IS", 7.049e-15 },
            { "BF", 677 },
            { "BR", 2.209 },
            { "PNP", 0 },
        } },
        { "BC549B", {
            { "IS", 4.679e-14 },
            { "BF", 458.7 },
            { "BR", 11.57 },
            { "PNP", 0 },
        } },
        { "BC549C", {
            { "IS", 7.049e-15 },
            { "BF", 493.2 },
            { "BR", 2.886 },
            { "PNP", 0 },
        } },
        { "AC127", {
            { "IS", 100e-12 },
            { "BF", 90 },
            { "BR", 20 },
            { "PNP", 0 },
        } },
        { "AC128", {
            { "IS", 100e-12 },
            { "BF", 90 },
            { "BR", 20 },
            { "PNP", 1 },
        } },
        { "2N2907", {
            { "IS", 1e-14 },
            { "BF", 250 },
            { "BR", 3 },
            { "PNP", 1 },
        } },
        { "2N3906", {
            { "IS", 1e-14 },
            { "BF", 200 },
            { "BR", 4 },
            { "PNP", 1 },
        } },
        { "2N4125", {
            { "IS", 1.41e-15 },
            { "BF", 85.04 },
            { "BR", 5.576 },
            { "PNP", 1 },
        } },
        { "2N5087", {
            { "IS", 6.734e-15 },
            { "BF", 254.1 },
            { "BR", 2.683 },
            { "PNP", 1 },
        } },
        { "2SA1015", {
            { "IS", 10.0004e-15 },
            { "BF", 239.283 },
            { "BR", 5 },
            { "PNP", 1 },
        } },
        { "MPSA43", {
            { "IS", 159e-15 },
            { "BF", 70.4 },
            { "BR", 17.6 },
            { "PNP", 1 },
        } },
        { "MPSA56", {
            { "IS", 9.62586e-12 },
            { "BF", 246.647 },
            { "BR", 0.1 },
            { "PNP", 1 },
        } },
        { "MPSA92", {
            { "IS", 95.3e-15 },
            { "BF", 98 },
            { "BR", 4.78 },
            { "PNP", 1 },
        } },
        { "MPS8599", {
            { "IS", 120.32e-15 },
            { "BF", 188.84 },
            { "BR", 24.295 },
            { "PNP", 1 },
        } },
        { "BC239A", {
            { "IS", 10e-15 },
            { "BF", 170 },
            { "BR", 1 },
            { "PNP", 0 },
        } },
        { "BC239B", {
            { "IS", 10e-15 },
            { "BF", 320 },
            { "BR", 1 },
            { "PNP", 0 },
        } },
        { "BC239C", {
            { "IS", 10e-15 },
            { "BF", 590 },
            { "BR", 1 },
            { "PNP", 0 },
        } }
    };
    
    static inline const ModelList JFETs = {
        { "2N5952", {
            { "IS", 1e-14 },
            { "Beta", 1.3143e-3 },
            { "Vt0", -2.021 },
            { "Lambda", 4e-3 },
            { "PNP", 0 },
        } },
        { "2N5457", {
            { "IS", 1e-14 },
            { "Beta", 1.125e-3 },
            { "Vt0", -1.372 },
            { "Lambda", 2.3e-3 },
            { "PNP", 0 },
        } },
        { "2N5458", {
            { "IS", 1e-14 },
            { "Beta", 488.9e-6 },
            { "Vt0", -2.882 },
            { "Lambda", 3.167e-3 },
            { "PNP", 0 },
        } },
        { "2N4392", {
            { "IS", 4.908e-16 },
            { "Beta", 4.225e-3 },
            { "Vt0", -2.208 },
            { "Lambda", 6.403e-2 },
            { "PNP", 0 },
        } },
        { "J112", {
            { "IS", 8.879e-16 },
            { "Beta", 5.972e-3 },
            { "Vt0", -2.33 },
            { "Lambda", 23.198e-3 },
            { "PNP", 0 },
        } },
        { "J174", {
            { "IS", 9.72750e-16 },
            { "Beta", 1.24461e-3 },
            { "Vt0", -7.1507 },
            { "Lambda", 2.01968e-2 },
            { "PNP", 1 },
        } }
    };
    
    static inline const ModelList MOSFETs = {
        { "IRF510", {
            { "Vt0", 3.82703 },
            { "Kp", 2.48457 },
            { "Lambda", 0 },
            { "PNP", 0 },
        } },
        { "IRF540", {
            { "Vt0", 3.58173 },
            { "Kp", 112.25 },
            { "Lambda", 0.00806114 },
            { "PNP", 0 },
        } },
        { "IRL540", {
            { "Vt0", 2.32261 },
            { "Kp", 193.939 },
            { "Lambda", 0 },
            { "PNP", 0 },
        } },
        { "IRL3705", {
            { "Vt0", 2.12306 },
            { "Kp", 47.9529 },
            { "Lambda", 0.01442 },
            { "PNP", 0 },
        } },
        { "BUZ901", {
            { "Vt0", 0.635 },
            { "Kp", 2.376 },
            { "Lambda", 0.11 },
            { "PNP", 1 },
        } },
    };
    
    static inline const ModelList Diodes = {
        { "Si", {
            { "Type", 0 }, { "IS", 1e-12 },
            //{"Description",  "Generic Silicon diode"},
        } },
        { "Ge", {
            { "Type", 0 }, { "IS", 1e-9 },
            //{"Description", "Generic Germanium diode"},
        } },
        { "Zener", {
            { "Type", 1 }, { "IS", 1e-15 },
            //{"Description", "Generic Zener diode"},
        } },
        { "Red", {
            { "Type", 2 }, { "IS", 93e-12 }, { "n", 3.73 },
            //{"Description", "Generic red LED"},
        } },
        { "Green", {
            { "Type", 2 }, { "IS", 93e-12 }, { "n", 4.61 },
            //{"Description", "Generic green LED"},
        } },
        { "Blue", {
            { "Type", 2 }, { "IS", 93e-12 }, { "n", 7.61 },
            //{"Description", "Generic blue LED"},
        } },
        { "1N4001", {
            { "Type", 0 },
            { "IS", 65.4e-12 },
            { "n", 1.36 },
        } },
        { "1N914", {
            { "Type", 0 },
            { "IS", 2.52e-9 },
            { "n", 1.752 },
        } },
        { "1N4148", {
            { "Type", 0 },
            { "IS", 2.52e-9 },
            { "n", 1.752 },
        } },
        { "MA150", {
            { "Type", 0 },
            { "IS", 2.52e-9 },
            { "n", 1.752 },
        } },
        { "1N4448", {
            { "Type", 0 },
            { "IS", 0.1e-12 },
            { "n", 2.07 },
        } },
        { "1N4739A", {
            { "Type", 1 },
            { "IS", 9.05e-15 },
            { "n", 1.1 },
        } },
        { "1N4742A", {
            { "Type", 1 },
            { "IS", 6.87e-15 },
            { "n", 1.49 },
        } },
        { "1N916", {
            { "Type", 0 },
            { "IS", 0.1e-12 },
            { "n", 2.07 },
        } },
        { "1N4733", {
            { "Type", 1 },
            { "IS", 1.214e-15 },
        } },
        { "1N34A", {
            { "Type", 0 },
            { "IS", 2e-7 },
            { "n", 1.3 },
        } }
    };
    
    static inline const ModelList OpAmps =  {
        {"LM308", {
            {"Rin", 40e6},
            {"Rout", 50},
            {"Aol", 300e3},
            {"Gbp", 1e6},
        }},
        {"3404A", {
            {"Rin", 1e12},
            {"Rout", 50},
            {"Aol", 100e3},
            {"Gbp", 1.2e6},
        }},
        {"4558", {
            {"Rin", 5e6},
            {"Rout", 100},
            {"Aol", 300e3},
            {"Gbp", 3e6},
        }},
        {"UA741", {
            {"Rin", 2e6},
            {"Rout", 75},
            {"Aol", 200e3},
            {"Gbp", 1e6},
        }},
        {"OP27", {
            {"Rin", 4e6},
            {"Rout", 70},
            {"Aol", 1.78e6},
            {"Gbp", 8e6},
        }},
        {"OP42", {
            {"Rin", 1e12},
            {"Rout", 50},
            {"Aol", 1e6},
            {"Gbp", 10e6},
        }},
        {"OP134", {
            {"Rin", 10e12},
            {"Rout", 10},
            {"Aol", 1e6},
            {"Gbp", 8e6},
        }},
        {"AD746", {
            {"Rin", 200e9},
            {"Rout", 10},
            {"Aol", 282e3},
            {"Gbp", 13e6},
        }},
        {"AD826", {
            {"Rin", 300e3},
            {"Rout", 8},
            {"Aol", 5.6e3},
            {"Gbp", 35e6},
        }},
        {"TL06x", {
            {"Rin", 1e12},
            {"Rout", 50},
            {"Aol", 315e3},
            {"Gbp", 1e6},
        }},
        {"TL07x", {
            {"Rin", 1e12},
            {"Rout", 50},
            {"Aol", 200e3},
            {"Gbp", 5.25e6},
        }},
        {"TL08x", {
            {"Rin", 1e12},
            {"Rout", 50},
            {"Aol", 100e3},
            {"Gbp", 3e6},
        }}
    };
    
public:
    static const ModelList& getModelsForComponent(const std::string& componentName)
    {
        if(componentName == "triode")
        {
            return Triodes;
        }
        if(componentName == "pentode")
        {
            return Pentodes;
        }
        if(componentName == "diode")
        {
            return Diodes;
        }
        if(componentName == "jfet")
        {
            return JFETs;
        }
        if(componentName == "mosfet")
        {
            return MOSFETs;
        }
        if(componentName == "bjt")
        {
            return BJTs;
        }
        if(componentName == "opamp")
        {
            return OpAmps;
        }

        assert(false);
        return BJTs; // just return something to suppress the warning...
    }
};
