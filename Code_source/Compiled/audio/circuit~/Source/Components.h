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

    // return a pointer to array of pin locations
    // NOTE: these will eventually be GUI locations to be unified
    virtual int const* getPinLocs() const = 0;

    // setup pins and calculate the size of the full netlist
    // the Component<> will handle this automatically
    //
    //  - netSize is the current size of the netlist
    //  - pins is an array of circuits nodes
    //
    virtual void setupNets(int& netSize, int const* pins) = 0;

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

    int const* getPinLocs() const final { return pinLoc; }

    void setupNets(int& netSize, int const* pins) final
    {
        for (int i = 0; i < nPins; ++i) {
            nets[i] = pins[i];
        }

        for (int i = 0; i < nInternalNets; ++i) {
            nets[nPins + i] = netSize++;
        }
    }
};

struct Resistor : Component<2> {
    double r;

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
    double c;
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

        // then we can store this for display here
        // since this value won't be used at this point
        m.b[nets[2]].lu = c * voltage;
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
    double l;
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
    double v;

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
    int outChannel;

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
        if (!initialised) {
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
    double rs;

    // l0 -->|-- l1 -- parameters default to approx 1N4148
    Diode(int l0, int l1,
        double rs = 10., double is = 35e-12, double n = 1.24)
        : rs(rs)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        pn.initJunctionPN(is, n);

        // initial condition v = 0
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

    bool pnp;

    BJT(int b, int c, int e, bool pnp = false)
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
        double is = 6.734e-15;
        double n = 1.24;
        pnE.initJunctionPN(is / af, n);
        pnC.initJunctionPN(is / ar, n);

        pnE.linearizeJunctionPN(0);
        pnC.linearizeJunctionPN(0);
    }

    bool newton(MNASystem& m) final
    {
        return pnC.newtonJunctionPN(m.b[nets[3]].lu) && pnE.newtonJunctionPN(m.b[nets[4]].lu);
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

struct Transformer final : Component<4, 2> {
    double b;

    Transformer(double scale, int a, int b, int c, int d)
        : b(scale)
    {
        pinLoc[0] = a;
        pinLoc[1] = b;
        pinLoc[2] = c;
        pinLoc[3] = d;
    }

    void stamp(MNASystem& m) final
    {
        m.stampStatic(1, nets[5], nets[3]);
        m.stampStatic(-1, nets[5], nets[4]);
        m.stampStatic(1, nets[4], nets[4]);
        m.stampStatic(1, nets[0], nets[4]);
        m.stampStatic(-1, nets[1], nets[4]);
        m.stampStatic(1, nets[2], nets[5]);
        m.stampStatic(-1, nets[3], nets[5]);
        m.stampStatic(-b, nets[5], nets[0]);
        m.stampStatic(b, nets[5], nets[1]);
        m.stampStatic(-b, nets[4], nets[5]);
    }
};

struct OpAmp final : Component<3, 1> {

    double amp;
    double g, gv, ngv;
    double Uout, Uin;
    double v, vmax;

    OpAmp(double G, double UMax, int a, int b, int c)
        : g(G)
        , vmax(UMax)
    {
        pinLoc[0] = a;
        pinLoc[1] = b;
        pinLoc[2] = c;
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
        Uin = m.b[nets[0]].lu - m.b[nets[1]].lu;
        gv = g / (1 + pow(M_PI_2 / vmax * g * Uin, 2)) + 1e-12;
        ngv = -gv;
        Uout = vmax * (M_2_PI)*atan(Uin * g * M_PI_2 / vmax);
        v = Uin * gv - Uout;
    }
};

struct Potentiometer final : Component<3, 0> {
    double r;
    double g;
    double ig;
    double input;
    double& pos;
    double ng;
    double ing;

    Potentiometer(double& position, double resistance, int vIn, int vOut, int vInvOut)
        : pos(position)
        , r(resistance)
    {
        pinLoc[0] = vIn;
        pinLoc[1] = vOut;
        pinLoc[2] = vInvOut;
    }

    void stamp(MNASystem& m) final
    {
        g = r * 0.5;
        ig = r * 0.5;
        ng = -g;
        ing = -ig;
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
        auto input = std::max(std::min(pos, 0.95), 0.05); // take out the extremes and prevent 0 divides
        g = 1. / (r * input);
        ig = 1. / (r - (r * input));
        ng = -g;
        ing = -ig;
    }
};

struct Gyrator final : Component<4> {
    double r;

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
        m.stampStatic(1. / r, nets[0], nets[1]);
        m.stampStatic(1. / r, nets[0], nets[2]);

        m.stampStatic(-1. / r, nets[1], nets[0]);
        m.stampStatic(1. / r, nets[1], nets[3]);

        m.stampStatic(1. / r, nets[2], nets[0]);
        m.stampStatic(-1. / r, nets[2], nets[3]);

        m.stampStatic(-1. / r, nets[3], nets[1]);
        m.stampStatic(1. / r, nets[3], nets[2]);
    }
};

struct Current final : Component<2> {
    double a;
    Current(double ampere, int pin1, int pin2)
        : a(ampere)
    {
        pinLoc[0] = pin1;
        pinLoc[1] = pin2;
    }

    void stamp(MNASystem& m) final
    {
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
