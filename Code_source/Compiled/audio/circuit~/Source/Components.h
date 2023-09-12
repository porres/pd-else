/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Originally made by mystran, modified by Timothy Schoen
 */

#pragma once

namespace {
// gMin for diodes etc..
constexpr double gMin = 1e-12;
// voltage tolerance
constexpr double vTolerance = 5e-5;

// thermal voltage for diodes/transistors
constexpr double vThermal = 0.026;
}

struct IComponent {
    virtual ~IComponent() { }

    // setup pins and calculate the size of the full netlist
    // the Component<> will handle this automatically
    //
    //  - netSize is the current size of the netlist
    //  - pins is an array of circuits nodes
    //
    virtual void setupNets(int& netSize) = 0;

    // stamp constants into the matrix
    virtual void stamp(MNASystem& m) = 0;

    // update state variables, only tagged nodes
    // this is intended for fixed-time compatible
    // testing to make sure we can code-gen stuff
    virtual void update(MNASystem& m) { }

    // return true if we're done - will keep iterating
    // until all the components are happy
    virtual bool newton(MNASystem& m) { return true; }

    // time-step change, for caps to fix their state-variables
    virtual void scaleTime(double told_per_new) { }
};

template<int nPins = 0, int nInternalNets = 0>
struct Component : IComponent {
    static constexpr int nNets = nPins + nInternalNets;

    int pinLoc[nPins];
    int nets[nNets];

    virtual void setupNets(int& netSize)
    {
        for (int i = 0; i < nPins; ++i) {
            nets[i] = pinLoc[i];
        }

        for (int i = 0; i < nInternalNets; ++i) {
            nets[nPins + i] = netSize++;
        }
    }
};

struct Resistor : Component<2> {
    const double r;

    Resistor(double r, int l0, int l1)
        : r(r)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;
    }

    void stamp(MNASystem& m) final
    {
        double g = 1. / r;
        m.stampStatic(+g, nets[0], nets[0]);
        m.stampStatic(-g, nets[0], nets[1]);
        m.stampStatic(-g, nets[1], nets[0]);
        m.stampStatic(+g, nets[1], nets[1]);
    }
};

struct VariableResistor : Component<2> {
    double& r;
    double g, g_negative;

    VariableResistor(double& r, int l0, int l1)
        : r(r)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;
    }

    void stamp(MNASystem& m) final
    {
        g = 1. / std::max(r, 1.0);
        g_negative = -g;

        m.A[nets[0]][nets[0]].gdyn.push_back(&g);
        m.A[nets[0]][nets[1]].gdyn.push_back(&g_negative);
        m.A[nets[1]][nets[0]].gdyn.push_back(&g_negative);
        m.A[nets[1]][nets[1]].gdyn.push_back(&g);
    }

    void update(MNASystem& m) final
    {
        g = 1. / std::max(r, 1.0);
        g_negative = -g;
    }
};

struct Capacitor : Component<2, 1> {
    const double c;
    double stateVar;
    double voltage;

    Capacitor(double c, int l0, int l1)
        : c(c)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        stateVar = 0;
        voltage = 0;
    }

    void stamp(MNASystem& m) final
    {
        // we can use a trick here, to get the capacitor to
        // work on it's own line with direct trapezoidal:
        //
        // | -g*t  +g*t  +t | v+
        // | +g*t  -g*t  -t | v-
        // | +2*g  -2*g  -1 | state
        //
        // the logic with this is that for constant timestep:
        //
        //  i1 = g*v1 - s0   , s0 = g*v0 + i0
        //  s1 = 2*g*v1 - s0 <-> s0 = 2*g*v1 - s1
        //
        // then if we substitute back:
        //  i1 = g*v1 - (2*g*v1 - s1)
        //     = s1 - g*v1
        //
        // this way we just need to copy the new state to the
        // next timestep and there's no actual integration needed
        //
        // the "half time-step" error here means that our state
        // is 2*c*v - i/t but we fix this for display in update
        // and correct the current-part on time-step changes

        // trapezoidal needs another factor of two for the g
        // since c*(v1 - v0) = (i1 + i0)/(2*t), where t = 1/T
        double g = 2 * c;

        m.stampTimed(+1, nets[0], nets[2]);
        m.stampTimed(-1, nets[1], nets[2]);

        m.stampTimed(-g, nets[0], nets[0]);
        m.stampTimed(+g, nets[0], nets[1]);
        m.stampTimed(+g, nets[1], nets[0]);
        m.stampTimed(-g, nets[1], nets[1]);

        m.stampStatic(+2 * g, nets[2], nets[0]);
        m.stampStatic(-2 * g, nets[2], nets[1]);

        m.stampStatic(-1, nets[2], nets[2]);

        // see the comment about v:C[%d] below
        m.b[nets[2]].gdyn.push_back(&stateVar);
    }

    void update(MNASystem& m) final
    {
        stateVar = m.b[nets[2]].lu;

        // solve legit voltage from the pins
        voltage = m.b[nets[0]].lu - m.b[nets[1]].lu;
    }

    void scaleTime(double told_per_new) final
    {
        // the state is 2*c*voltage - i/t0
        // so we subtract out the voltage, scale current
        // and then add the voltage back to get new state
        //
        // note that this also works if the old rate is infinite
        // (ie. t0=0) when going from DC analysis to transient
        //
        double qq = 2 * c * voltage;
        stateVar = qq + (stateVar - qq) * told_per_new;
    }
};

struct Inductor : Component<2, 1> {
    const double l;
    double g;
    double stateVar;
    double voltage;

    Inductor(double l, int l0, int l1)
        : l(l)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        stateVar = 0;
        voltage = 0;
    }

    void stamp(MNASystem& m) final
    {
        m.stampStatic(+1, nets[0], nets[2]); // Naar A->B system
        m.stampStatic(-1, nets[1], nets[2]);
        m.stampStatic(-1, nets[2], nets[0]); // Naar A->C system
        m.stampStatic(+1, nets[2], nets[1]);

        // m.stampInvTimed(1. / (l * 2.), nets[2], nets[2]);
        m.A[nets[2]][nets[2]].gdyn.push_back(&g);
        m.b[nets[2]].gdyn.push_back(&stateVar);
    }

    void update(MNASystem& m) final
    {
        // solve legit voltage from the pins
        voltage = (m.b[nets[0]].lu - m.b[nets[1]].lu);

        // we shouldn't need to do this??
        if (m.tStep != 0)
            g = 1. / ((2. * l) / (1. / m.tStep));
        else
            g = 0;
        stateVar = voltage + g * m.b[nets[2]].lu;
    }

    void scaleTime(double told_per_new) final
    {
        // I still have to implement this. This function would allow the inductor to remain stable when changing time-steps
        // However, currently we never change time-step during runtime so this is fine for now
    }
};

struct Voltage : Component<2, 1> {
    const double v;

    Voltage(double v, int l0, int l1)
        : v(v)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;
    }

    void stamp(MNASystem& m) final
    {
        m.stampStatic(-1, nets[0], nets[2]);
        m.stampStatic(+1, nets[1], nets[2]);

        m.stampStatic(+1, nets[2], nets[0]);
        m.stampStatic(-1, nets[2], nets[1]);

        m.b[nets[2]].g = v;
    }
};

// probe a differential voltage
// also forces this voltage to actually get solved :)
struct Probe : Component<2, 1> {

    class {
    public:
        void setInitialState(double state)
        {
            prevInSample = state;
        }

        // Process a new input sample and return the filtered sample
        double filter(double inputSample)
        {
            double outputSample = inputSample - prevInSample + alpha * prevOutSample;
            prevInSample = inputSample;
            prevOutSample = outputSample;
            return outputSample;
        }

    private:
        double alpha = 0.9997;      // Alpha coefficient for the filter
        double prevInSample = 0.0;  // Previous sample (state)
        double prevOutSample = 0.0; // Previous sample (state)
    } dc_blocker;

    bool initialised = false;
    const int outChannel;

    Probe(int l0, int l1, int outChannel)
        : outChannel(outChannel)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;
    }

    void stamp(MNASystem& m) final
    {
        // vp + vn - vd = 0
        m.stampStatic(+1, nets[2], nets[0]);
        m.stampStatic(-1, nets[2], nets[1]);
        m.stampStatic(-1, nets[2], nets[2]);
    }

    void update(MNASystem& m) final
    {
        // As soon as a voltage gets written, treat that as the DC offset point
        if (!initialised && m.b[nets[2]].lu != 0.0) {
            dc_blocker.setInitialState(m.b[nets[2]].lu);
            initialised = true;
        }

        if (m.blockDC) {
            m.output[outChannel] += dc_blocker.filter(m.b[nets[2]].lu);
        } else {
            m.output[outChannel] += m.b[nets[2]].lu;
        }
    }
};

// function voltage generator
struct VariableVoltage : Component<2, 1> {
    double& v;

    VariableVoltage(double& v, int l0, int l1)
        : v(v)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;
    }

    void stamp(MNASystem& m) final
    {
        // this is identical to voltage source
        // except voltage is dynanic
        m.stampStatic(-1, nets[0], nets[2]);
        m.stampStatic(+1, nets[1], nets[2]);

        m.stampStatic(+1, nets[2], nets[0]);
        m.stampStatic(-1, nets[2], nets[1]);

        m.b[nets[2]].gdyn.push_back(&v);
    }
};

// POD-struct for PN-junction data, for diodes and BJTs
//
struct JunctionPN {
    // variables
    double geq, ieq, veq;

    // parameters
    double is, nvt, rnvt, vcrit;

    void initJunctionPN(double new_is, double n)
    {
        is = new_is;
        nvt = n * vThermal;
        rnvt = 1 / nvt;
        vcrit = nvt * log(nvt / (is * sqrt(2.)));
    }

    // linearize junction at the specified voltage
    //
    // ideally we could handle series resistance here as well
    // to avoid putting it on a separate node, but not sure how
    // to make that work as it looks like we'd need Lambert-W then
    void linearizeJunctionPN(double v)
    {
        double e = is * exp(v * rnvt);
        double i = e - is + gMin * v;
        double g = e * rnvt + gMin;

        geq = g;
        ieq = v * g - i;
        veq = v;
    }

    // returns true if junction is good enough
    bool newtonJunctionPN(double v)
    {
        double dv = v - veq;
        if (fabs(dv) < vTolerance)
            return true;

        // check critical voltage and adjust voltage if over
        if (v > vcrit) {
            // this formula comes from Qucs documentation
            v = veq + nvt * log((std::max)(is, 1 + dv * rnvt));
        }

        linearizeJunctionPN(v);

        return false;
    }
};

struct Diode : Component<2, 2> {
    JunctionPN pn;

    // should make these parameters
    const double rs;

    // l0 -->|-- l1 -- parameters default to approx 1N4148
    Diode(int l0, int l1, std::string model) : rs(10.0)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        double is = 35e-12;
        double n = 1.0;

        if (Models::Diodes.count(model)) {
            auto const& modelDescription = Models::Diodes.at(model);

            if (modelDescription.count("IS"))
                is = modelDescription.at("IS");
            if (modelDescription.count("n"))
                n = modelDescription.at("n");
        } else if (!model.empty()) {
            pd_error(NULL, "circuit~: unknown diode model");
        }

        pn.initJunctionPN(is, n);

        // initial condition v = 0
        pn.linearizeJunctionPN(0);
    }

    // l0 -->|-- l1 -- parameters default to approx 1N4148
    Diode(int l0, int l1, double rs = 10., double is = 35e-12, double n = 1.24)
        : rs(rs)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        pn.initJunctionPN(is, n);
        pn.linearizeJunctionPN(0);
    }

    bool newton(MNASystem& m) final
    {
        return pn.newtonJunctionPN(m.b[nets[2]].lu);
    }

    void stamp(MNASystem& m) final
    {
        // Diode could be built with 3 extra nodes:
        //
        // |  .  .    .       . +1 | V+
        // |  .  .    .       . -1 | V-
        // |  .  .  grs    -grs -1 | v:D
        // |  .  . -grs grs+geq  . | v:pn = ieq
        // | -1 +1   +1       .  . | i:pn
        //
        // Here grs is the 1/rs series conductance.
        //
        // This gives us the junction voltage (v:pn) and
        // current (i:pn) and the composite voltage (v:D).
        //
        // The i:pn row is an ideal transformer connecting
        // the floating diode to the ground-referenced v:D
        // where we connect the series resistance to v:pn
        // that solves the diode equation with Newton.
        //
        // We can then add the 3rd row to the bottom 2 with
        // multipliers 1 and -rs = -1/grs and drop it:
        //
        // |  .  .   . +1 | V+
        // |  .  .   . -1 | V-
        // |  .  . geq -1 | v:pn = ieq
        // | -1 +1  +1 rs | i:pn
        //
        // Note that only the v:pn row here is non-linear.
        //
        // We could even do away without the separate row for
        // the current, which would lead to the following:
        //
        // | +grs -grs     -grs |
        // | -grs +grs     +grs |
        // | -grs +grs +grs+geq | = ieq
        //
        // In practice we keep the current row since it's
        // nice to have it as an output anyway.
        //
        m.stampStatic(-1, nets[3], nets[0]);
        m.stampStatic(+1, nets[3], nets[1]);
        m.stampStatic(+1, nets[3], nets[2]);

        m.stampStatic(+1, nets[0], nets[3]);
        m.stampStatic(-1, nets[1], nets[3]);
        m.stampStatic(-1, nets[2], nets[3]);

        m.stampStatic(rs, nets[3], nets[3]);

        m.A[nets[2]][nets[2]].gdyn.push_back(&pn.geq);
        m.b[nets[2]].gdyn.push_back(&pn.ieq);
    }
};

struct BJT : Component<3, 4> {
    // emitter and collector junctions
    JunctionPN pnC, pnE;

    // forward and reverse alpha
    double af, ar, rsbc, rsbe;

    const bool pnp;

    BJT(int b, int c, int e, std::string model, bool pnp)
        : pnp(pnp)
    {
        pinLoc[0] = b;
        pinLoc[1] = e;
        pinLoc[2] = c;

        // this attempts a 2n3904-style small-signal
        // transistor, although the values are a bit
        // arbitrarily set to "something reasonable"

        // forward and reverse beta
        double bf = 220;
        double br = 20;

        double is = 6.734e-15;
        double n = 1.24;

        if (Models::BJTs.count(model)) {
            auto const& modelDescription = Models::BJTs.at(model);

            if (modelDescription.count("IS"))
                is = modelDescription.at("IS");
            if (modelDescription.count("BF"))
                bf = modelDescription.at("BF");
            if (modelDescription.count("BR"))
                br = modelDescription.at("BR");
            if (modelDescription.count("PNP")) {
                if (modelDescription.at("PNP") != pnp) {
                    pd_error(NULL, "circuit~: BJT model does not match with set PNP value. Proceeding with custom PNP value");
                }
            }
        } else if (!model.empty()) {
            pd_error(NULL, "circuit~: unknown bjt model");
        }

        // forward and reverse alpha
        af = bf / (1 + bf);
        ar = br / (1 + br);

        // these are just rb+re and rb+rc
        // this is not necessarily the best way to
        // do anything, but having junction series
        // resistances helps handle degenerate cases
        rsbc = 5.8371;
        rsbe = 8.49471;

        //
        // the basic rule is that:
        //  af * ise = ar * isc = is
        //
        // FIXME: with non-equal ideality factors
        // we can get non-sensical results, why?
        //
        pnE.initJunctionPN(is / af, n);
        pnC.initJunctionPN(is / ar, n);

        pnE.linearizeJunctionPN(0);
        pnC.linearizeJunctionPN(0);
    }

    bool newton(MNASystem& m) final
    {
        return pnC.newtonJunctionPN(m.b[nets[3]].lu) & pnE.newtonJunctionPN(m.b[nets[4]].lu);
    }

    void stamp(MNASystem& m) final
    {
        // The basic idea here is the same as with diodes
        // except we do it once for each junction.
        //
        // With the transfer currents sourced from the
        // diode currents, NPN then looks like this:
        //
        // 0 |  .  .  .  .  . 1-ar 1-af | vB
        // 1 |  .  .  .  .  .   -1  +af | vC
        // 2 |  .  .  .  .  .  +ar   -1 | vE
        // 3 |  .  .  . gc  .   -1    . | v:Qbc  = ic
        // 4 |  .  .  .  . ge    .   -1 | v:Qbe  = ie
        // 5 | -1 +1  . +1  . rsbc    . | i:Qbc
        // 6 | -1  . +1  . +1    . rsbe | i:Qbe
        //     ------------------------
        //      0  1  2  3  4    5    6
        //
        // For PNP version, we simply flip the junctions
        // by changing signs of (3,5),(5,3) and (4,6),(6,4).
        //
        // Also just like diodes, we have junction series
        // resistances, rather than terminal resistances.
        //
        // This works just as well, but should be kept
        // in mind when fitting particular transistors.
        //

        // diode currents to external base
        m.stampStatic(1 - ar, nets[0], nets[5]);
        m.stampStatic(1 - af, nets[0], nets[6]);

        // diode currents to external collector and emitter
        m.stampStatic(-1, nets[1], nets[5]);
        m.stampStatic(-1, nets[2], nets[6]);

        // series resistances
        m.stampStatic(rsbc, nets[5], nets[5]);
        m.stampStatic(rsbe, nets[6], nets[6]);

        // current - junction connections
        // for the PNP case we flip the signs of these
        // to flip the diode junctions wrt. the above
        if (pnp) {
            m.stampStatic(-1, nets[5], nets[3]);
            m.stampStatic(+1, nets[3], nets[5]);

            m.stampStatic(-1, nets[6], nets[4]);
            m.stampStatic(+1, nets[4], nets[6]);
        } else {
            m.stampStatic(+1, nets[5], nets[3]);
            m.stampStatic(-1, nets[3], nets[5]);

            m.stampStatic(+1, nets[6], nets[4]);
            m.stampStatic(-1, nets[4], nets[6]);
        }

        // external voltages to collector current
        m.stampStatic(-1, nets[5], nets[0]);
        m.stampStatic(+1, nets[5], nets[1]);

        // external voltages to emitter current
        m.stampStatic(-1, nets[6], nets[0]);
        m.stampStatic(+1, nets[6], nets[2]);

        // source transfer currents to external pins
        m.stampStatic(+ar, nets[2], nets[5]);
        m.stampStatic(+af, nets[1], nets[6]);

        // dynamic variables
        m.A[nets[3]][nets[3]].gdyn.push_back(&pnC.geq);
        m.b[nets[3]].gdyn.push_back(&pnC.ieq);

        m.A[nets[4]][nets[4]].gdyn.push_back(&pnE.geq);
        m.b[nets[4]].gdyn.push_back(&pnE.ieq);
    }
};

// Define a Transformer component
struct Transformer final : Component<4, 2> {

    const double turnsRatio;

    Transformer(double scale, int primary1, int primary2, int secondary1, int secondary2)
        : turnsRatio(scale)
    {
        // Assign pin locations with more descriptive names
        pinLoc[0] = primary1;
        pinLoc[1] = primary2;
        pinLoc[2] = secondary1;
        pinLoc[3] = secondary2;
    }

    // Function to stamp the Transformer component into the MNA (Modified Nodal Analysis) system
    void stamp(MNASystem& m) final
    {
        // Stamping equations representing the transformer in the MNA system
        // These equations model the electrical behavior of the transformer
        m.stampStatic(1, nets[5], nets[0]);
        m.stampStatic(-1, nets[5], nets[4]);
        m.stampStatic(1, nets[4], nets[4]);
        m.stampStatic(1, nets[3], nets[4]);
        m.stampStatic(-1, nets[2], nets[4]);
        m.stampStatic(1, nets[1], nets[5]);
        m.stampStatic(-1, nets[0], nets[5]);
        m.stampStatic(-turnsRatio, nets[5], nets[3]);
        m.stampStatic(turnsRatio, nets[5], nets[2]);
        m.stampStatic(-turnsRatio, nets[4], nets[5]);
    }
};

struct OpAmp final : Component<3, 1> {

    const double g, vmax;
    double v, gv, ngv, Uout, Uin;

    OpAmp(double G, double UMax, int invertingInput, int nonInvertingInput, int output)
        : g(G)
        , vmax(UMax)
    {
        pinLoc[0] = invertingInput;
        pinLoc[1] = nonInvertingInput;
        pinLoc[2] = output;
    }

    void stamp(MNASystem& m) final
    {
        // http://qucs.sourceforge.net/tech/node67.html explains all this
        m.stampStatic(-1, nets[3], nets[2]);
        m.stampStatic(1, nets[2], nets[3]);
        m.A[nets[3]][nets[0]].gdyn.push_back(&gv);
        m.A[nets[3]][nets[1]].gdyn.push_back(&ngv);
        m.b[nets[3]].gdyn.push_back(&v);
    }

    void update(MNASystem& m) final
    {
        // Calculate the voltage difference between non-inverting and inverting inputs
        Uin = m.b[nets[0]].lu - m.b[nets[1]].lu;

        // Calculate the dynamic conductance gv and its negative ngv
        gv = g / (1 + pow(M_PI_2 / vmax * g * Uin, 2)) + 1e-12;
        ngv = -gv;

        // Calculate the OpAmp's output voltage
        Uout = vmax * (M_2_PI)*atan(Uin * g * M_PI_2 / vmax);

        // Calculate the voltage v at the output node
        v = Uin * gv - Uout;
    }
};

struct Potentiometer final : Component<3> {
    const double r;
    double g;
    double ig;
    double& pos;
    double ng;
    double ing;

    Potentiometer(double& position, double resistance, int vIn, int vWiper, int vOut)
        : pos(position)
        , r(resistance)
    {
        pinLoc[0] = vWiper;
        pinLoc[1] = vIn;
        pinLoc[2] = vOut;
    }

    void stamp(MNASystem& m) final
    {
        // Calculate conductance
        g = r * 0.5;
        ig = r * 0.5;
        ng = -g;
        ing = -ig;

        // Stamp it dynamically so it can be adjusted when the pot is moved
        m.A[nets[0]][nets[0]].gdyn.push_back(&g);
        m.A[nets[0]][nets[1]].gdyn.push_back(&ng);
        m.A[nets[1]][nets[0]].gdyn.push_back(&ng);
        m.A[nets[1]][nets[1]].gdyn.push_back(&g);
        m.A[nets[0]][nets[0]].gdyn.push_back(&ig);
        m.A[nets[0]][nets[2]].gdyn.push_back(&ing);
        m.A[nets[2]][nets[0]].gdyn.push_back(&ing);
        m.A[nets[2]][nets[2]].gdyn.push_back(&ig);
    }

    void update(MNASystem& m) final
    {
        const double input = std::clamp(pos, 1e-3, 1.0 - 1e-3); // take out the extremes and prevent 0 divides

        // Update conductance variables
        g = 1. / (r * input);
        ig = 1. / (r - (r * input));
        ng = -g;
        ing = -ig;
    }
};

struct StaticPotentiometer final : Component<3> {
    const double r;
    double pos;

    StaticPotentiometer(double position, double resistance, int vIn, int vWiper, int vOut)
        : pos(position)
        , r(resistance)
    {
        pinLoc[0] = vWiper;
        pinLoc[1] = vIn;
        pinLoc[2] = vOut;
    }

    void stamp(MNASystem& m) final
    {
        double g = 1.0 / r;
        double invG = 1.0 / (r - (r * pos));
        
        g = std::clamp(g, 1e-4, 1.0 - 1e-4);
        invG = std::clamp(invG, 1e-4, 1.0 - 1e-4);
        
        m.stampStatic(g, nets[0], nets[0]);
        m.stampStatic(-g, nets[0], nets[1]);
        m.stampStatic(-g, nets[1], nets[0]);
        m.stampStatic(g, nets[1], nets[1]);
        
        m.stampStatic(invG, nets[0], nets[0]);
        m.stampStatic(-invG, nets[0], nets[2]);
        m.stampStatic(-invG, nets[2], nets[0]);
        m.stampStatic(invG, nets[2], nets[2]);
    }
};

struct Gyrator final : Component<4> {
    const double r;

    Gyrator(double r, int pin1, int pin2, int pin3, int pin4)
        : r(r)
    {
        pinLoc[0] = pin1;
        pinLoc[1] = pin2;
        pinLoc[2] = pin3;
        pinLoc[3] = pin4;
    }

    void stamp(MNASystem& m) final
    {
        // see https://github.com/Qucs/qucsator/blob/d48b91e28f7c7b7718dabbdfd6cc9dfa0616d841/src/components/gyrator.cpp
        m.stampStatic(1. / r, nets[0], nets[1]);
        m.stampStatic(-1. / r, nets[0], nets[2]);

        m.stampStatic(1. / r, nets[1], nets[3]);
        m.stampStatic(-1. / r, nets[1], nets[0]);

        m.stampStatic(1. / r, nets[2], nets[0]);
        m.stampStatic(-1. / r, nets[2], nets[3]);

        m.stampStatic(1. / r, nets[3], nets[2]);
        m.stampStatic(-1. / r, nets[3], nets[1]);
    }
};

struct Current final : Component<2> {
    const double a;
    Current(double ampere, int pin1, int pin2)
        : a(ampere)
    {
        pinLoc[0] = pin1;
        pinLoc[1] = pin2;
    }

    void stamp(MNASystem& m) final
    {
        // Write current to RHS
        m.b[nets[0]].g = -a;
        m.b[nets[1]].g = a;
    }
};

struct VariableCurrent final : Component<2> {
    double& a;
    double a_negative;

    VariableCurrent(double& ampere, int pin1, int pin2)
        : a(ampere)
    {
        pinLoc[0] = pin1;
        pinLoc[1] = pin2;
    }

    void stamp(MNASystem& m) final
    {
        m.b[nets[0]].gdyn.push_back(&a_negative);
        m.b[nets[1]].gdyn.push_back(&a);
    }

    void update(MNASystem& m) final
    {
        a_negative = -a;
    }
};

struct Triode : public Component<3, 3> {
    // Triode model parameters
    double mu = 85.0;    // Amplification factor (μ)
    double ex = 1.4;     // Exponential factor for Koren's equation
    double kg1 = 1060.0; // kg1 coefficient for Koren's equation
    double kp = 600.0;   // kp coefficient for Koren's equation
    double kvb = 300;    // kvb coefficient for Koren's equation
    double rgk = 1e6;    // Grid resistor (Rgk)
    double vg = 0.33;    // Grid voltage offset

    // Capacitance values
    const double cgp = 2.4e-12; // Grid to plate capacitor (Cgp)
    const double cgk = 2.3e-12; // Grid to cathode capacitor (Cgk)
    const double cpk = 0.9e-12; // Plate to cathode capacitor (Cpk)

    // Voltages and currents
    double vgp = 0.0, vgk = 0.0, vpk = 0.0;
    double vcgp = 0.0f, vcgk = 0.0, vcpk = 0.0;
    double ip, ig, ipg;
    double ids, gm, gds, e1;
    
    // Variables to store voltages from the previous iteration
    double v0, v1, v2;
    double lastv0 = 0;
    double lastv1 = 0;
    double lastv2 = 0;

    // Dynamic matrices for nodal analysis
    std::vector<double> ieq = std::vector<double>(3, 0);
    std::vector<std::vector<double>> geq = std::vector<std::vector<double>>(3, std::vector<double>(3, 0));

    Triode(int plate, int grid, int cathode, std::string model)
    {
        pinLoc[0] = plate;
        pinLoc[1] = grid;
        pinLoc[2] = cathode;

        if (Models::Triodes.count(model)) {
            auto const& modelDescription = Models::Triodes.at(model);

            if (modelDescription.count("Ex"))
                ex = modelDescription.at("Ex");
            if (modelDescription.count("Mu"))
                mu = modelDescription.at("Mu");
            if (modelDescription.count("Kg"))
                kg1 = modelDescription.at("Kg");
            if (modelDescription.count("Kp"))
                kp = modelDescription.at("Kp");
            if (modelDescription.count("Kvb"))
                kvb = modelDescription.at("Kvb");
            if (modelDescription.count("Rgk"))
                rgk = modelDescription.at("Rgk");
            if (modelDescription.count("Vg"))
                vg = modelDescription.at("Vg");
        } else if (!model.empty()) {
            pd_error(NULL, "circuit~: unknown triode model");
        }
    }

    void stamp(MNASystem& m)
    {
        double g = 2 * cgp;
        m.stampTimed(+1, nets[1], nets[3]);
        m.stampTimed(-1, nets[0], nets[3]);

        m.stampTimed(-g, nets[1], nets[1]);
        m.stampTimed(+g, nets[1], nets[0]);
        m.stampTimed(+g, nets[0], nets[1]);
        m.stampTimed(-g, nets[0], nets[0]);
        m.stampStatic(+2 * g, nets[3], nets[1]);
        m.stampStatic(-2 * g, nets[3], nets[0]);
        m.stampStatic(-1, nets[3], nets[3]);

        m.b[nets[3]].gdyn.push_back(&vcgp);

        g = 2 * cgk;
        m.stampTimed(+1, nets[1], nets[4]);
        m.stampTimed(-1, nets[2], nets[4]);

        m.stampTimed(-g, nets[1], nets[1]);
        m.stampTimed(+g, nets[1], nets[2]);
        m.stampTimed(+g, nets[2], nets[1]);
        m.stampTimed(-g, nets[2], nets[2]);
        m.stampStatic(+2 * g, nets[4], nets[1]);
        m.stampStatic(-2 * g, nets[4], nets[2]);
        m.stampStatic(-1, nets[4], nets[4]);

        m.b[nets[4]].gdyn.push_back(&vcgk);

        g = 2 * cpk;
        m.stampTimed(+1, nets[0], nets[5]);
        m.stampTimed(-1, nets[2], nets[5]);

        m.stampTimed(-g, nets[0], nets[0]);
        m.stampTimed(+g, nets[0], nets[2]);
        m.stampTimed(+g, nets[2], nets[0]);
        m.stampTimed(-g, nets[2], nets[2]);
        m.stampStatic(+2 * g, nets[5], nets[0]);
        m.stampStatic(-2 * g, nets[5], nets[2]);
        m.stampStatic(-1, nets[5], nets[5]);

        m.b[nets[5]].gdyn.push_back(&vcpk);

        m.A[nets[0]][nets[0]].gdyn.push_back(&geq[0][0]);
        m.A[nets[0]][nets[1]].gdyn.push_back(&geq[0][1]);
        m.A[nets[0]][nets[2]].gdyn.push_back(&geq[0][2]);

        m.A[nets[1]][nets[1]].gdyn.push_back(&geq[1][1]);
        m.A[nets[1]][nets[2]].gdyn.push_back(&geq[1][2]);

        m.A[nets[2]][nets[0]].gdyn.push_back(&geq[2][0]);
        m.A[nets[2]][nets[1]].gdyn.push_back(&geq[2][1]);
        m.A[nets[2]][nets[2]].gdyn.push_back(&geq[2][2]);

        m.b[nets[0]].gdyn.push_back(&ieq[0]);
        m.b[nets[1]].gdyn.push_back(&ieq[1]);
        m.b[nets[2]].gdyn.push_back(&ieq[2]);
    }

    void update(MNASystem& m)
    {
        // Update internal voltage variables based on node voltages
        vcgp = m.b[nets[3]].lu;
        vcgk = m.b[nets[4]].lu;
        vcpk = m.b[nets[5]].lu;

        vgk = m.b[nets[1]].lu - m.b[nets[2]].lu;
        vpk = m.b[nets[0]].lu - m.b[nets[2]].lu;
        vgp = m.b[nets[1]].lu - m.b[nets[0]].lu;
    }

    void calcKoren(MNASystem& m)
    {
        // Calculate intermediate voltage differences
        const double cvgk = m.b[nets[1]].lu - m.b[nets[2]].lu;
        const double cvpk = m.b[nets[0]].lu - m.b[nets[2]].lu;

        // Calculate e1 using Koren's model
        e1 = (cvpk / kp) * std::log(1 + std::exp(kp * (1.0 / mu + cvgk / sqrt(kvb + pow(cvpk, 2)))));

        // Calculate ids, gds, and gm based on e1
        ids = (std::pow(e1, ex) / kg1) * (1 + (e1 >= 0) - (e1 < 0));
        gds = ex * sqrt(e1) / kg1;
        gm = gds / mu;

        // Check if ids is zero or not finite
        if (ids == 0 || !std::isfinite(ids)) {
            gds = 1e-8;
            gm = gds / mu;
            ids = cvpk * gds;
        }

        // Calculate rs and gcr
        const double rs = -ids + gds * cvpk + gm * cvgk;
        const double gcr = cvgk > vg ? rgk : 0;

        // Populate the geq and ieq matrices
        geq[0][0] = gds;
        geq[0][1] = gm;
        geq[0][2] = -gds - gm;

        geq[1][1] = gcr;
        geq[1][2] = -gcr;

        geq[2][0] = -gds;
        geq[2][1] = -gm - gcr;
        geq[2][2] = gds + gm + gcr;

        ieq[0] = rs;
        ieq[1] = 0;
        ieq[2] = -rs;

        // Store current voltages for comparison in the next iteration
        lastv0 = m.b[nets[0]].lu;
        lastv1 = m.b[nets[1]].lu;
        lastv2 = m.b[nets[2]].lu;
    }

    void scaleTime(double told_per_new, double tStepSize)
    {
        const double qcgp = 2 * cgp * vgp;
        vcgp = qcgp + (vcgp - qcgp) * told_per_new;

        const double qcgk = 2 * cgk * vgk;
        vcgk = qcgk + (vcgk - qcgk) * told_per_new;

        const double qcpk = 2 * cpk * vpk;
        vcpk = qcgk + (vcpk - qcpk) * told_per_new;
    }

    bool newton(MNASystem& m)
    {
        // Apply the Newton-Raphson method for convergence
        double v0 = m.b[nets[0]].lu;
        double v1 = m.b[nets[1]].lu;
        double v2 = m.b[nets[2]].lu;

        // Limit voltage changes to 0.5V
        if (v1 > lastv1 + 0.5)
            v1 = lastv1 + 0.5;
        if (v1 < lastv1 - 0.5)
            v1 = lastv1 - 0.5;
        if (v2 > lastv2 + 0.5)
            v2 = lastv2 + 0.5;
        if (v2 < lastv2 - 0.5)
            v2 = lastv2 - 0.5;

        // Check for convergence based on voltage changes
        if (abs(lastv0 - v0) <= vTolerance && abs(lastv1 - v1) <= vTolerance && abs(lastv2 - v2) <= vTolerance)
            return true;

        // Calculate triode parameters using Koren's model
        calcKoren(m);

        return false;
    }
};

struct MOSFET : public Component<3> {
    const double pnp; // Positive or negative MOSFET indicator (-1 or 1)

    double vt = 1.5;     // Threshold voltage
    double beta = 0.02;  // Beta parameter (transconductance)
    double lambda = 0.0; // Lambda parameter (channel-length modulation)

    double lastv0, lastv1, lastv2; // Last voltage values for convergence check
    double ids;                    // Drain current (output current)

    double geq[3][3] = { { 0 } }; // Matrix to store conductance values
    double ieq[3] = { 0 };        // Vector to store current values

    // Simple MOSFET implementation, inspired by circuitjs
    MOSFET(bool isPNP, int gate, int source, int drain, std::string model)
        : pnp(isPNP ? -1.0 : 1.0)
    {
        pinLoc[0] = gate;
        pinLoc[1] = source;
        pinLoc[2] = drain;

        if (Models::MOSFETs.count(model)) {
            auto const& modelDescription = Models::MOSFETs.at(model);

            if (modelDescription.count("Kp"))
                beta = modelDescription.at("Kp"); // I'm not sure this is correct
            if (modelDescription.count("Vt0"))
                vt = modelDescription.at("Vt0");
            if (modelDescription.count("Lambda"))
                vt = modelDescription.at("Lambda");

            if (modelDescription.count("PNP")) {
                if (modelDescription.at("PNP") != isPNP) {
                    pd_error(NULL, "circuit~: mosfet model does not match with set PNP value. Proceeding with custom PNP value");
                }
            }
        } else if (!model.empty()) {
            pd_error(NULL, "circuit~: unknown mosfet model");
        }
    }

    void stamp(MNASystem& m) override
    {
        m.A[nets[2]][nets[2]].gdyn.push_back(&geq[2][2]);
        m.A[nets[2]][nets[1]].gdyn.push_back(&geq[2][1]);
        m.A[nets[2]][nets[0]].gdyn.push_back(&geq[2][0]);

        m.A[nets[1]][nets[2]].gdyn.push_back(&geq[1][2]);
        m.A[nets[1]][nets[1]].gdyn.push_back(&geq[1][1]);
        m.A[nets[1]][nets[0]].gdyn.push_back(&geq[1][0]);

        m.b[nets[2]].gdyn.push_back(&ieq[2]);
        m.b[nets[1]].gdyn.push_back(&ieq[1]);
    }

    bool converged(double last, double now)
    {
        double diff = std::abs(last - now);

        // high beta MOSFETs are more sensitive to small differences, so we are more strict about convergence testing
        diff = beta > 1 ? diff * 100 : diff;
        return diff < vTolerance;
    }

    bool newton(MNASystem& m) override
    {
        bool allConverged = false;
        double vs[3];

        // limit voltage changes to .5V
        vs[0] = m.b[nets[0]].lu;
        vs[1] = m.b[nets[1]].lu;
        vs[2] = m.b[nets[2]].lu;

        if (vs[1] > lastv1 + .5)
            vs[1] = lastv1 + .5;
        if (vs[1] < lastv1 - .5)
            vs[1] = lastv1 - .5;
        if (vs[2] > lastv2 + .5)
            vs[2] = lastv2 + .5;
        if (vs[2] < lastv2 - .5)
            vs[2] = lastv2 - .5;

        int gate = 0;
        int source = 1;
        int drain = 2;

        // if source voltage > drain (for NPN), swap source and drain
        // (opposite for PNP)
        if (pnp * vs[1] > pnp * vs[2]) {
            source = 2;
            drain = 1;
        }

        double vgs = vs[gate] - vs[source];
        double vds = vs[drain] - vs[source];
        if (converged(lastv1, vs[1]) && converged(lastv2, vs[2]) && converged(lastv0, vs[0]))
            allConverged = true;

        lastv0 = vs[0];
        lastv1 = vs[1];
        lastv2 = vs[2];
        double realvgs = vgs;
        double realvds = vds;
        vgs *= pnp;
        vds *= pnp;
        const double b = beta * (1 + lambda * vds);
        ids = 0;
        double gm = 0;
        double gds = 0;

        if (vgs < vt) {
            // should be all zero, but that causes a singular matrix,
            // so instead we treat it as a large resistor
            gds = 1e-8;
            ids = vds * gds;
        } else if (vds < vgs - vt) {
            // linear
            ids = b * ((vgs - vt) * vds - vds * vds * 0.5);
            gm = b * vds;
            gds = b * (vgs - vds - vt);
        } else {
            // saturation; gds = 0
            gm = b * (vgs - vt);
            // use very small gds to avoid nonconvergence
            gds = 1e-8;
            ids = 0.5 * b * (vgs - vt) * (vgs - vt) + (vds - (vgs - vt)) * gds;
        }

        const double rs = (-pnp) * ids + gds * realvds + gm * realvgs;

        // flip ids if we swapped source and drain above
        if ((source == 2 && pnp == 1) || (source == 1 && pnp == -1))
            ids = -ids;

        geq[drain][drain] = gds;
        geq[drain][source] = -gds - gm;
        geq[drain][gate] = gm;

        geq[source][drain] = -gds;
        geq[source][source] = gds + gm;
        geq[source][gate] = -gm;

        ieq[drain] = rs;
        ieq[source] = -rs;

        return allConverged;
    }
};

struct JFET : public MOSFET {
    std::unique_ptr<Diode> diode;

    // For a JFET, we just simulate a MOSFET with different params and an extra diode
    // This is a large simplification so that it can easily run in realtime, but might not be as accurate
    JFET(bool isPNP, int gate, int source, int drain, std::string model)
        : MOSFET(isPNP, gate, source, drain, "")
    {
        // Like a 2N5458 JFET
        vt = -4.5;
        beta = 0.00125;
        double is = 35e-12;

        if (Models::JFETs.count(model)) {
            auto const& modelDescription = Models::JFETs.at(model);

            if (modelDescription.count("IS"))
                is = modelDescription.at("IS");
            if (modelDescription.count("Kp"))
                beta = modelDescription.at("Kp");
            if (modelDescription.count("Lambda"))
                lambda = modelDescription.at("Lambda");
            if (modelDescription.count("Vt0"))
                vt = modelDescription.at("Vt0");

            if (modelDescription.count("PNP")) {
                if (modelDescription.at("PNP") != isPNP) {
                    pd_error(NULL, "circuit~: jfet model does not match with set PNP value. Proceeding with custom PNP value");
                }
            }
        } else if (!model.empty()) {
            pd_error(NULL, "circuit~: unknown jfet model");
        }

        diode = std::make_unique<Diode>(isPNP ? source : gate, isPNP ? gate : source, 10, is, 1.0);
    }

    void stamp(MNASystem& m) override
    {
        MOSFET::stamp(m);
        diode->stamp(m);
    }

    void setupNets(int& netSize) override
    {
        diode->setupNets(netSize);

        for (int i = 0; i < 3; ++i) {
            nets[i] = pinLoc[i];
        }
    }

    bool newton(MNASystem& m) override
    {
        return diode->newton(m) & MOSFET::newton(m);
    }
};
