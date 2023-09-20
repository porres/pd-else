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
constexpr double vTolerance = 1e-4;

// thermal voltage for diodes/transistors
constexpr double vThermal = 0.026;
}

struct IComponent {
    virtual ~IComponent() = default;

    // setup pins and calculate the size of the full netlist
    // the Component<> will handle this automatically
    //
    //  - netSize is the current size of the netlist
    //  - pins is an array of circuits nodes
    //
    virtual void setupNets(int& netSize) = 0;

    // stamp constants into the matrix
    virtual void stamp(MNAMatrix& A, MNAVector& b) = 0;

    // update state variables, only tagged nodes
    // this is intended for fixed-time compatible
    // testing to make sure we can code-gen stuff
    virtual void update(MNAResultVector& x) { }

    // return true if we're done - will keep iterating
    // until all the components are happy
    virtual bool newton(MNAResultVector& x) { return true; }

    // called when switching from ininite timestep to a finite one
    virtual void initTransientAnalysis(double tStepSize) { }
    
    // for probe to enable DC blocking
    virtual void setDCBlock(bool blockDC) {}
};

template<int nPins = 0, int nInternalNets = 0>
struct Component : IComponent {
    static constexpr int nNets = nPins + nInternalNets;

    int pinLoc[nPins];
    int nets[nNets];

    void setupNets(int& netSize) override
    {
        for (int i = 0; i < nPins; i++) {
            nets[i] = pinLoc[i];
        }

        for (int i = 0; i < nInternalNets; i++) {
            nets[nPins + i] = netSize++;
        }
    }
    
    void stampStatic(MNAMatrix& A, double value, int r, int c)
    {
        A[r][c].g += value;
    }
    
    void stampTimed(MNAMatrix& A, double value, int r, int c)
    {
        A[r][c].gtimed += value;
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

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        double g = 1. / r;
        stampStatic(A, +g, nets[0], nets[0]);
        stampStatic(A, -g, nets[0], nets[1]);
        stampStatic(A, -g, nets[1], nets[0]);
        stampStatic(A, +g, nets[1], nets[1]);
    }
};

struct VariableResistor : Component<2> {
    double& r;
    double gPos, gNeg;

    VariableResistor(double& r, int l0, int l1)
        : r(r)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        gPos = 1. / std::max(r, 1.0);
        gNeg = -gPos;

        A[nets[0]][nets[0]].gdyn.push_back(&gPos);
        A[nets[0]][nets[1]].gdyn.push_back(&gNeg);
        A[nets[1]][nets[0]].gdyn.push_back(&gNeg);
        A[nets[1]][nets[1]].gdyn.push_back(&gPos);
    }

    void update(MNAResultVector& x) final
    {
        gPos = 1. / std::max(r, 1.0);
        gNeg = -gPos;
    }
};

struct Capacitor : Component<2, 1> {
    const double c;
    double state;
    double voltage;

    Capacitor(double c, int l0, int l1)
        : c(c)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        state = 0;
        voltage = 0;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // we can use a trick here, to get the capacitor to
        // work on its own line with direct trapezoidal:
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
        // is 2*c*v - i/t,  but we fix this for display in update
        // and correct the current-part on time-step changes

        // trapezoidal needs another factor of two for the g
        // since c*(v1 - v0) = (i1 + i0)/(2*t), where t = 1/T
        double g = 2.0 * c;

        stampTimed(A, +1.0, nets[0], nets[2]);
        stampTimed(A, -1.0, nets[1], nets[2]);

        stampTimed(A, -g, nets[0], nets[0]);
        stampTimed(A, +g, nets[0], nets[1]);
        stampTimed(A, +g, nets[1], nets[0]);
        stampTimed(A, -g, nets[1], nets[1]);

        stampStatic(A, +2.0 * g, nets[2], nets[0]);
        stampStatic(A, -2.0 * g, nets[2], nets[1]);

        stampStatic(A, -1.0, nets[2], nets[2]);

        // see the comment about v:C[%d] below
        b[nets[2]].gdyn.push_back(&state);
    }

    void update(MNAResultVector& x) final
    {
        state = x[nets[2]];

        // solve legit voltage from the pins
        voltage = x[nets[0]] - x[nets[1]];
    }

    void initTransientAnalysis(double timeStep) final
    {
        state = 2.0 * c * voltage;
    }
};

struct Inductor : Component<2, 1> {
    const double l;
    double g;
    double state;
    double voltage;
    double tStep = 0.0;

    Inductor(double l, int l0, int l1)
        : l(l)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        state = 0;
        voltage = 0;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        stampStatic(A, +1, nets[0], nets[2]);
        stampStatic(A, -1, nets[1], nets[2]);
        stampStatic(A, -1, nets[2], nets[0]);
        stampStatic(A, +1, nets[2], nets[1]);

        A[nets[2]][nets[2]].gdyn.push_back(&g);
        b[nets[2]].gdyn.push_back(&state);
    }

    void update(MNAResultVector& x) final
    {
        // solve legit voltage from the pins
        voltage = (x[nets[0]] - x[nets[1]]);

        double tStepSize = tStep != 0 ? tStep : 1.0;
        g = 1. / ((2. * l) / (1. / tStepSize));
        
        state = voltage + g * x[nets[2]];
    }
    
    void initTransientAnalysis(double tStepSize) final
    {
        tStep = tStepSize;
        state = voltage / l;
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

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        stampStatic(A, -1, nets[0], nets[2]);
        stampStatic(A, +1, nets[1], nets[2]);

        stampStatic(A, +1, nets[2], nets[0]);
        stampStatic(A, -1, nets[2], nets[1]);

        b[nets[2]].g = v;
    }
};

// probe a differential voltage
// also forces this voltage to actually get solved :)
struct Probe : Component<2, 1> {

    class {
    public:
        // Process a new input sample and return the filtered sample
        double filter(double inputSample)
        {
            // Latch on to the first non-zero value as the centre point
            // Ideally we could do this in initTransientAnalysis, but this is much more reliable against clicks
            if(!initialised && inputSample != 0.0) {
                initialised = true;
                prevInSample = inputSample;
                return inputSample;
            }
            else if(!initialised)
            {
                return inputSample;
            }
            
            double outputSample = inputSample - prevInSample + alpha * prevOutSample;
            prevInSample = inputSample;
            prevOutSample = outputSample;
            return outputSample;
        }

    private:
        double alpha = 0.9997;      // Alpha coefficient for the filter
        double prevInSample = 0.0;  // Previous sample (state)
        double prevOutSample = 0.0; // Previous sample (state)
        bool initialised = false;
    } dcBlocker;

    double& output;
    bool blockDC = true;

    Probe(int l0, int l1, double& output)
        : output(output)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // vp + vn - vd = 0
        stampStatic(A, +1, nets[2], nets[0]);
        stampStatic(A, -1, nets[2], nets[1]);
        stampStatic(A, -1, nets[2], nets[2]);
    }
    
    void setDCBlock(bool shouldBlockDC) final
    {
        blockDC = shouldBlockDC;
    }

    void update(MNAResultVector& x) final
    {
        if (blockDC) {
            output += dcBlocker.filter(x[nets[2]]);
        } else {
            output += x[nets[2]];
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

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // this is identical to voltage source
        // except voltage is dynanic
        stampStatic(A, -1, nets[0], nets[2]);
        stampStatic(A, +1, nets[1], nets[2]);

        stampStatic(A, +1, nets[2], nets[0]);
        stampStatic(A, -1, nets[2], nets[1]);

        b[nets[2]].gdyn.push_back(&v);
    }
};

// POD-struct for PN-junction data, for diodes and BJTs
//
struct JunctionPN {
    // variables
    double geq = 0.0, ieq = 0.0, veq = 0.0;

    // parameters
    double is, nvt, rnvt, vcrit;

    void initJunctionPN(double newIs, double n)
    {
        is = newIs;
        nvt = n * vThermal;
        rnvt = 1 / nvt;
        vcrit = nvt * std::log(nvt / (is * std::sqrt(2.)));
    }

    // linearize junction at the specified voltage
    //
    // ideally we could handle series resistance here as well
    // to avoid putting it on a separate node, but not sure how
    // to make that work as it looks like we'd need Lambert-W then
    void linearizeJunctionPN(double v)
    {
        double e = is * std::exp(v * rnvt);
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
        
        if (std::abs(dv) < vTolerance)
            return true;

        // check critical voltage and adjust voltage if over
        if (v > vcrit) {
            // this formula comes from Qucs documentation
            v = veq + nvt * std::log((std::max)(is, 1 + dv * rnvt));
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
    Diode(int l0, int l1, const Model& model) : rs(10.0)
    {
        pinLoc[0] = l0;
        pinLoc[1] = l1;

        double is = 35e-12;
        double n = 1.0;
        
        if (model.count("IS"))
            is = model.at("IS");
        if (model.count("n"))
            n = model.at("n");

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

    bool newton(MNAResultVector& x) final
    {
        return pn.newtonJunctionPN(x[nets[2]]);
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
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
        stampStatic(A, -1, nets[3], nets[0]);
        stampStatic(A, +1, nets[3], nets[1]);
        stampStatic(A, +1, nets[3], nets[2]);

        stampStatic(A, +1, nets[0], nets[3]);
        stampStatic(A, -1, nets[1], nets[3]);
        stampStatic(A, -1, nets[2], nets[3]);

        stampStatic(A, rs, nets[3], nets[3]);

        A[nets[2]][nets[2]].gdyn.push_back(&pn.geq);
        b[nets[2]].gdyn.push_back(&pn.ieq);
    }
};

struct BJT : Component<3, 4> {
    // emitter and collector junctions
    JunctionPN pnC, pnE;

    // forward and reverse alpha
    double af, ar, rsbc, rsbe;

    const bool pnp;

    BJT(int b, int c, int e, const Model& model, bool pnp)
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

        if (model.count("IS"))
            is = model.at("IS");
        if (model.count("BF"))
            bf = model.at("BF");
        if (model.count("BR"))
            br = model.at("BR");
        if (model.count("PNP")) {
            if (model.at("PNP") != pnp) {
                pd_error(nullptr, "circuit~: BJT model does not match with set PNP value. Proceeding with custom PNP value");
            }
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

    bool newton(MNAResultVector& x) final
    {
        return pnC.newtonJunctionPN(x[nets[3]]) & pnE.newtonJunctionPN(x[nets[4]]);
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
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
        stampStatic(A, 1 - ar, nets[0], nets[5]);
        stampStatic(A, 1 - af, nets[0], nets[6]);

        // diode currents to external collector and emitter
        stampStatic(A, -1, nets[1], nets[5]);
        stampStatic(A, -1, nets[2], nets[6]);

        // series resistances
        stampStatic(A, rsbc, nets[5], nets[5]);
        stampStatic(A, rsbe, nets[6], nets[6]);

        // current - junction connections
        // for the PNP case we flip the signs of these
        // to flip the diode junctions wrt. the above
        if (pnp) {
            stampStatic(A, -1, nets[5], nets[3]);
            stampStatic(A, +1, nets[3], nets[5]);

            stampStatic(A, -1, nets[6], nets[4]);
            stampStatic(A, +1, nets[4], nets[6]);
        } else {
            stampStatic(A, +1, nets[5], nets[3]);
            stampStatic(A, -1, nets[3], nets[5]);

            stampStatic(A, +1, nets[6], nets[4]);
            stampStatic(A, -1, nets[4], nets[6]);
        }

        // external voltages to collector current
        stampStatic(A, -1, nets[5], nets[0]);
        stampStatic(A, +1, nets[5], nets[1]);

        // external voltages to emitter current
        stampStatic(A, -1, nets[6], nets[0]);
        stampStatic(A, +1, nets[6], nets[2]);

        // source transfer currents to external pins
        stampStatic(A, +ar, nets[2], nets[5]);
        stampStatic(A, +af, nets[1], nets[6]);

        // dynamic variables
        A[nets[3]][nets[3]].gdyn.push_back(&pnC.geq);
        b[nets[3]].gdyn.push_back(&pnC.ieq);

        A[nets[4]][nets[4]].gdyn.push_back(&pnE.geq);
        b[nets[4]].gdyn.push_back(&pnE.ieq);
    }
};

struct Transformer final : Component<4, 2> {

    const double turnsRatio;

    Transformer(double scale, int primary1, int primary2, int secondary1, int secondary2)
        : turnsRatio(scale)
    {
        pinLoc[0] = primary1;
        pinLoc[1] = primary2;
        pinLoc[2] = secondary1;
        pinLoc[3] = secondary2;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        stampStatic(A, +1, nets[5], nets[0]);
        stampStatic(A, -1, nets[5], nets[4]);
        stampStatic(A, +1, nets[4], nets[4]);
        stampStatic(A, +1, nets[3], nets[4]);
        stampStatic(A, -1, nets[2], nets[4]);
        stampStatic(A, +1, nets[1], nets[5]);
        stampStatic(A, -1, nets[0], nets[5]);
        stampStatic(A, -turnsRatio, nets[5], nets[3]);
        stampStatic(A, turnsRatio, nets[5], nets[2]);
        stampStatic(A, -turnsRatio, nets[4], nets[5]);
    }
};

struct TappedTransformer final : Component<5, 2> {
    const double turnsRatio;

    TappedTransformer(double scale, int primary1, int primary2, int secondary1, int secondary2, int centerTap)
        : turnsRatio(scale)
    {
        pinLoc[0] = primary1;
        pinLoc[1] = primary2;
        pinLoc[2] = secondary1;
        pinLoc[3] = secondary2;
        pinLoc[4] = centerTap;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        stampStatic(A, +1, nets[6], nets[0]);
        stampStatic(A, -1, nets[6], nets[5]);
        stampStatic(A, +1, nets[5], nets[5]);
        stampStatic(A, +1, nets[3], nets[5]);
        stampStatic(A, -1, nets[2], nets[5]);
        stampStatic(A, +1, nets[1], nets[6]);
        stampStatic(A, -1, nets[0], nets[6]);
        stampStatic(A, -turnsRatio, nets[6], nets[3]);
        stampStatic(A, turnsRatio, nets[6], nets[2]);
        stampStatic(A, -turnsRatio, nets[5], nets[6]);
        
        // Stamp the center tap connection
        stampStatic(A, -1, nets[4], nets[4]);
        stampStatic(A, +1, nets[3], nets[4]);
        stampStatic(A, -turnsRatio, nets[2], nets[4]);
    }
};

struct OpAmp final : Component<3, 1> {

    const double gain, vMax;
    double v = 0.0, gvPos = gMin, gvNeg = gMin;

    OpAmp(double G, double UMax, int invertingInput, int nonInvertingInput, int output)
        : gain(G)
        , vMax(UMax)
    {
        pinLoc[0] = invertingInput;
        pinLoc[1] = nonInvertingInput;
        pinLoc[2] = output;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // http://qucs.sourceforge.net/tech/node67.html explains all this
        stampStatic(A, -1, nets[3], nets[2]);
        stampStatic(A, +1, nets[2], nets[3]);
        A[nets[3]][nets[0]].gdyn.push_back(&gvPos);
        A[nets[3]][nets[1]].gdyn.push_back(&gvNeg);
        b[nets[3]].gdyn.push_back(&v);
    }

    void update(MNAResultVector& x) final
    {
        // Calculate the voltage difference between non-inverting and inverting inputs
        double vIn = x[nets[0]] - x[nets[1]];

        // Calculate the dynamic conductance gv and its negative ngv
        gvPos = gain / (1 + std::pow(M_PI_2 / vMax * gain * vIn, 2)) + 1e-12;
        gvNeg = -gvPos;

        // Calculate the OpAmp's output voltage
        double vOut = vMax * (M_2_PI)* std::atan(vIn * gain * M_PI_2 / vMax);

        // Calculate the voltage v at the output node
        v = vIn * gvPos - vOut;
    }
};

struct OpAmp2 : Component<5, 6>
{
    // diode clamps
    JunctionPN  pnPP, pnNN;
    
    double gain, ro, ri;
    
    OpAmp2(int vInP, int vInN, int vOut, int vPP, int vNN, Model model)
    {
        pinLoc[0] = vOut;
        pinLoc[1] = vInP;
        pinLoc[2] = vInN;
        pinLoc[3] = vPP;
        pinLoc[4] = vNN;
        
        // the DC voltage gain
        gain = 10e3;
        ri = 1.0 / 50e6;
        ro = 10;
        
        if (model.count("Rin"))
            ri = 1.0 / model.at("Rin");
        if (model.count("Rout"))
            ro = model.at("Rout");
        if (model.count("Aol"))
            gain = model.at("Aol");
       // if (model.count("Gbp"))
       //     kp = model.at("Gbp");

        double is = 8e-16;
        double n = 1.0;
        pnPP.initJunctionPN(is, n);
        pnNN.initJunctionPN(is, n);
        
        pnPP.linearizeJunctionPN(0);
        pnNN.linearizeJunctionPN(0);
    }
    
    bool newton(MNAResultVector& x) final
    {
        return pnPP.newtonJunctionPN(x[nets[5]])
        & pnNN.newtonJunctionPN(x[nets[6]]);
    }
    
    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // What we want here is a high-gain VCVS where
        // we then bypass to rails if we get too close.
        //
        // Here it's important not to have series resistance
        // for three diodes, otherwise we can still exceed rails.
        //
        // NOTE: the following ignores supply currents
        //
        //      0   1   2   3  4   5   6   7   8    9
        //    vout in+ in- V+ V- vd+ vd- id+ id- iout
        // 0 |  .   .   .   .  .   .   .  +1  -1   +1 | vout
        // 1 |  .   .   .   .  .   .   .   .   .    . | in+
        // 2 |  .   .   .   .  .   .   .   .   .    . | in-
        // 3 |  .   .   .   .  .   .   .   .   .    . | v+
        // 4 |  .   .   .   .  .   .   .   .   .    . | v-
        // 5 |  .   .   .   .  . gpp   .  -1   .    . | vd+  = i0pp
        // 6 |  .   .   .   .  .   . gnn   .  -1    . | vd-  = i0nn
        // 7 | -1   .   .  +1  .  +1   .  ~0   .    . | id+  = +1.5
        // 8 | +1   .   .   . -1   .  +1   .  ~0    . | id-  = +1.5
        // 9 | -1  +g  -g   .  .   .   .   .   .   ro | iout
        //
        // We then add one useless extra row just to add
        // the currents together, so one can plot the real
        // current that actually get pushed into vOut
        
        // output currents
        stampStatic(A, +1, nets[0], nets[7]);
        stampStatic(A, -1, nets[0], nets[8]);
        stampStatic(A, +1, nets[0], nets[9]);
        
        // output feedback
        stampStatic(A, -1, nets[7], nets[0]);
        stampStatic(A, +1, nets[8], nets[0]);
        stampStatic(A, -1, nets[9], nets[0]);
        
        // voltage input
        stampStatic(A, +gain, nets[9], nets[1]);
        stampStatic(A, -gain, nets[9], nets[2]);
        
        // supply voltages
        stampStatic(A, +1, nets[7], nets[3]);
        stampStatic(A, -1, nets[8], nets[4]);
        
        // voltage drops from the supply, should be slightly
        // more than the drop voltage drop across the diodes
        b[nets[7]].g += 1.5;
        b[nets[8]].g += 1.5;
        
        // diode voltages to currents
        stampStatic(A, +1, nets[7], nets[5]);
        stampStatic(A, -1, nets[5], nets[7]);
        
        stampStatic(A, +1, nets[8], nets[6]);
        stampStatic(A, -1, nets[6], nets[8]);
        
        // the series resistance for the diode clamps
        // needs to be small to handle the high gain,
        // but still use something just slightly non-zero
        double rs = gMin;
        
        // series resistances for diodes
        // allow "gmin" just for pivoting?
        stampStatic(A, rs, nets[7], nets[7]);
        stampStatic(A, rs, nets[8], nets[8]);
        
        // series output resistance
        stampStatic(A, ro, nets[9], nets[9]);

        stampStatic(A, +ri, nets[1], nets[1]);
        stampStatic(A, -ri, nets[1], nets[2]);
        stampStatic(A, -ri, nets[2], nets[1]);
        stampStatic(A, +ri, nets[2], nets[2]);
        
        // junctions
        A[nets[5]][nets[5]].gdyn.push_back(&pnPP.geq);
        A[nets[6]][nets[6]].gdyn.push_back(&pnNN.geq);
        
        b[nets[5]].gdyn.push_back(&pnPP.ieq);
        b[nets[6]].gdyn.push_back(&pnNN.ieq);
        
        
        // this is useless as far as simulation goes
        // it's just for getting a nice current value
        stampStatic(A, +1, nets[10], nets[7]);
        stampStatic(A, -1, nets[10], nets[8]);
        stampStatic(A, +1, nets[10], nets[9]);
        stampStatic(A, +1, nets[10], nets[10]);
    }
};


template<int N>
struct Switch : Component<N+1> {
  
    bool isFixed;
    double fixedState = 0.0f;
    double& state; // State represents the position of the switch, 0 for not connected), 1 for connected

    double geq[N+1][N+1] = {0};
    
    Switch(double* state, std::vector<int> pins) : state(*state), isFixed(false)
    {
        for(int i = 0; i < N; i++)
        {
            Component<N+1>::pinLoc[i] = pins[i];
        }
    }
    
    Switch(double s, std::vector<int> pins) : fixedState(s), state(fixedState), isFixed(true)
    {
        for(int i = 0; i < N; i++)
        {
            Component<N+1>::pinLoc[i] = pins[i];
        }
    }
    
    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        auto& nets = Component<N+1, 0>::nets;
        
        if(isFixed)
        {
            int initState = std::clamp(static_cast<int>(state), 1, N);
            Component<N+1>::stampStatic(A, 1.0, nets[0], nets[0]);
            Component<N+1>::stampStatic(A, 1.0, nets[0], nets[initState]);
            Component<N+1>::stampStatic(A, 1.0, nets[initState], nets[0]);
            Component<N+1>::stampStatic(A, 1.0, nets[initState], nets[initState]);
            
            for(int i = 1; i < N+1; i++)
            {
                if(i == initState) continue;
                
                Component<N+1>::stampStatic(A, gMin, nets[0], nets[0]);
                Component<N+1>::stampStatic(A, -gMin, nets[0], nets[i]);
                Component<N+1>::stampStatic(A, -gMin, nets[i], nets[0]);
                Component<N+1>::stampStatic(A, gMin, nets[i], nets[i]);
            }
        }
        else {
            for(int i = 1; i < N+1; i++)
            {
                A[nets[0]][nets[0]].gdyn.push_back(&geq[0][0]);
                A[nets[0]][nets[i]].gdyn.push_back(&geq[0][i]);
                A[nets[i]][nets[0]].gdyn.push_back(&geq[i][0]);
                A[nets[i]][nets[i]].gdyn.push_back(&geq[i][i]);
            }
        }
    }

    void update(MNAResultVector& x) final
    {
        if(isFixed) return;
        
        for(int i = 1; i < N+1; i++)
        {
            geq[0][0] = gMin;
            geq[0][i] = -gMin;
            geq[i][0] = -gMin;
            geq[i][i] = gMin;
        }
        
        int currentState = std::clamp(static_cast<int>(state), 1, N);
        
        geq[0][0] = gMin;
        geq[0][currentState] = -gMin;
        geq[currentState][0] = -gMin;
        geq[currentState][currentState] = gMin;
    }
};

struct Potentiometer final : Component<3> {
    const double r;
    double gPosInv;
    double& pos;
    double gPos;
    double gNegInv;
    double ing;

    Potentiometer(double& position, double resistance, int vIn, int vWiper, int vOut)
        : pos(position)
        , r(resistance)
    {
        pinLoc[0] = vWiper;
        pinLoc[1] = vIn;
        pinLoc[2] = vOut;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // Calculate conductance
        gPos = r * 0.5;
        gPosInv = r * 0.5;
        gNegInv = -gPos;
        ing = -gPosInv;

        // Stamp it dynamically, so it can be adjusted when the pot is moved
        A[nets[0]][nets[0]].gdyn.push_back(&gPos);
        A[nets[0]][nets[1]].gdyn.push_back(&gNegInv);
        A[nets[1]][nets[0]].gdyn.push_back(&gNegInv);
        A[nets[1]][nets[1]].gdyn.push_back(&gPos);
        A[nets[0]][nets[0]].gdyn.push_back(&gPosInv);
        A[nets[0]][nets[2]].gdyn.push_back(&ing);
        A[nets[2]][nets[0]].gdyn.push_back(&ing);
        A[nets[2]][nets[2]].gdyn.push_back(&gPosInv);
    }

    void update(MNAResultVector& x) final
    {
        const double input = std::clamp(pos, 1e-3, 1.0 - 1e-3); // take out the extremes and prevent 0 divides

        // Update conductance variables
        gPos = 1.0 / (r * input);
        gPosInv = 1.0 / (r - (r * input));
        gNegInv = -gPos;
        ing = -gPosInv;
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

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        double g = 1.0 / r;
        double invG = 1.0 / (r - (r * pos));
        
        g = std::clamp(g, 1e-4, 1.0 - 1e-4);
        invG = std::clamp(invG, 1e-4, 1.0 - 1e-4);
        
        stampStatic(A, +g, nets[0], nets[0]);
        stampStatic(A, -g, nets[0], nets[1]);
        stampStatic(A, -g, nets[1], nets[0]);
        stampStatic(A, +g, nets[1], nets[1]);
        
        stampStatic(A, +invG, nets[0], nets[0]);
        stampStatic(A, -invG, nets[0], nets[2]);
        stampStatic(A, -invG, nets[2], nets[0]);
        stampStatic(A, +invG, nets[2], nets[2]);
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

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // see https://github.com/Qucs/qucsator/blob/d48b91e28f7c7b7718dabbdfd6cc9dfa0616d841/src/components/gyrator.cpp
        
        double g = 1.0 / r;
        stampStatic(A, g, nets[0], nets[1]);
        stampStatic(A, -g, nets[0], nets[2]);

        stampStatic(A, g, nets[1], nets[3]);
        stampStatic(A, -g, nets[1], nets[0]);

        stampStatic(A, g, nets[2], nets[0]);
        stampStatic(A, -g, nets[2], nets[3]);

        stampStatic(A, g, nets[3], nets[2]);
        stampStatic(A, -g, nets[3], nets[1]);
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

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // Write current to RHS
        b[nets[0]].g -= a;
        b[nets[1]].g += a;
    }
};

struct VariableCurrent final : Component<2> {
    double& aPos;
    double aNeg;

    VariableCurrent(double& ampere, int pin1, int pin2)
        : aPos(ampere)
    {
        pinLoc[0] = pin1;
        pinLoc[1] = pin2;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        b[nets[0]].gdyn.push_back(&aNeg);
        b[nets[1]].gdyn.push_back(&aPos);
    }

    void update(MNAResultVector& x) final
    {
        aNeg = -aPos;
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
    double ip, ids, gm, gds, e1;
    
    // Variables to store voltages from the previous iteration
    double lastV[3] = {0};
    
    // Dynamic matrices for nodal analysis
    double ieq[3] = {0};
    double geq[3][3] = {0};

    Triode(int plate, int grid, int cathode, const Model& model)
    {
        pinLoc[0] = plate;
        pinLoc[1] = grid;
        pinLoc[2] = cathode;

        if (model.count("Ex"))
            ex = model.at("Ex");
        if (model.count("Mu"))
            mu = model.at("Mu");
        if (model.count("Kg"))
            kg1 = model.at("Kg");
        if (model.count("Kp"))
            kp = model.at("Kp");
        if (model.count("Kvb"))
            kvb = model.at("Kvb");
        if (model.count("Rgk"))
            rgk = model.at("Rgk");
        if (model.count("Vg"))
            vg = model.at("Vg");
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        double g = 2.0 * cgp;
        stampTimed(A, +1, nets[1], nets[3]);
        stampTimed(A, -1, nets[0], nets[3]);

        stampTimed(A, -g, nets[1], nets[1]);
        stampTimed(A, +g, nets[1], nets[0]);
        stampTimed(A, +g, nets[0], nets[1]);
        stampTimed(A, -g, nets[0], nets[0]);
        stampStatic(A, +2.0 * g, nets[3], nets[1]);
        stampStatic(A, -2.0 * g, nets[3], nets[0]);
        stampStatic(A, -1.0, nets[3], nets[3]);

        b[nets[3]].gdyn.push_back(&vcgp);

        g = 2.0 * cgk;
        stampTimed(A, +1.0, nets[1], nets[4]);
        stampTimed(A, -1.0, nets[2], nets[4]);

        stampTimed(A, -g, nets[1], nets[1]);
        stampTimed(A, +g, nets[1], nets[2]);
        stampTimed(A, +g, nets[2], nets[1]);
        stampTimed(A, -g, nets[2], nets[2]);
        stampStatic(A, +2.0 * g, nets[4], nets[1]);
        stampStatic(A, -2.0 * g, nets[4], nets[2]);
        stampStatic(A, -1.0, nets[4], nets[4]);

        b[nets[4]].gdyn.push_back(&vcgk);

        g = 2 * cpk;
        stampTimed(A, +1.0, nets[0], nets[5]);
        stampTimed(A, -1.0, nets[2], nets[5]);

        stampTimed(A, -g, nets[0], nets[0]);
        stampTimed(A, +g, nets[0], nets[2]);
        stampTimed(A, +g, nets[2], nets[0]);
        stampTimed(A, -g, nets[2], nets[2]);
        stampStatic(A, +2.0 * g, nets[5], nets[0]);
        stampStatic(A, -2.0 * g, nets[5], nets[2]);
        stampStatic(A, -1.0, nets[5], nets[5]);

        b[nets[5]].gdyn.push_back(&vcpk);

        A[nets[0]][nets[0]].gdyn.push_back(&geq[0][0]);
        A[nets[0]][nets[1]].gdyn.push_back(&geq[0][1]);
        A[nets[0]][nets[2]].gdyn.push_back(&geq[0][2]);

        A[nets[1]][nets[1]].gdyn.push_back(&geq[1][1]);
        A[nets[1]][nets[2]].gdyn.push_back(&geq[1][2]);

        A[nets[2]][nets[0]].gdyn.push_back(&geq[2][0]);
        A[nets[2]][nets[1]].gdyn.push_back(&geq[2][1]);
        A[nets[2]][nets[2]].gdyn.push_back(&geq[2][2]);

        b[nets[0]].gdyn.push_back(&ieq[0]);
        b[nets[1]].gdyn.push_back(&ieq[1]);
        b[nets[2]].gdyn.push_back(&ieq[2]);
    }

    void update(MNAResultVector& x) final
    {
        // Update internal voltage variables based on node voltages
        vcgp = x[nets[3]];
        vcgk = x[nets[4]];
        vcpk = x[nets[5]];

        vgk = x[nets[1]] - x[nets[2]];
        vpk = x[nets[0]] - x[nets[2]];
        vgp = x[nets[1]] - x[nets[0]];
    }

    void calcKoren(MNAResultVector& x)
    {
        // Calculate intermediate voltage differences
        const double cvgk = x[nets[1]] - x[nets[2]];
        const double cvpk = x[nets[0]] - x[nets[2]];

        // Calculate e1 using Koren's model
        e1 = (cvpk / kp) * std::log(1.0 + std::exp(kp * (1.0 / mu + cvgk / std::sqrt(kvb + std::pow(cvpk, 2)))));

        gds = e1 > 0 ? ex * std::sqrt(e1) / kg1 : 1e-8;
        // Calculate ids, gds, and gm based on e1
        ids = e1 > 0 ? (std::pow(e1, ex) / kg1) * (1.0 + (e1 >= 0) - (e1 < 0.0)) : cvpk * gds;

        gm = gds / mu;

        // Calculate rs and gcr
        const double rs = -ids + gds * cvpk + gm * cvgk;
        const double gcr = cvgk > vg ? rgk : 0.0;
        
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
        lastV[0] = x[nets[0]];
        lastV[1] = x[nets[1]];
        lastV[2] = x[nets[2]];
    }

    void initTransientAnalysis(double tStepSize) final
    {
        vcgp = 2.0 * cgp * vgp;
        vcgk = 2.0 * cgk * vgk;
        vcpk = 2.0 * cpk * vpk;
    }
    
    bool checkConvergence(double v, double dv)
    {
        dv /= v > 0 ? v : 1;
        return std::abs(dv) < vTolerance;
    }

    bool newton(MNAResultVector& x) final
    {
        // Apply the Newton-Raphson method for convergence
        double newV[3] = {x[nets[0]], x[nets[1]], x[nets[2]]};
        
        // Limit voltage changes to 0.5V
        if (newV[1] > lastV[1] + 0.5)
            newV[1] = lastV[1] + 0.5;
        if (newV[1] < lastV[1] - 0.5)
            newV[1] = lastV[1] - 0.5;
        if (newV[2] > lastV[2] + 0.5)
            newV[2] = lastV[2] + 0.5;
        if (newV[2] < lastV[2] - 0.5)
            newV[2] = lastV[2] - 0.5;
        
        // Check for convergence based on voltage changes
        bool done = checkConvergence(newV[0], lastV[0] - newV[0]) && checkConvergence(newV[1], lastV[1] - newV[1]) && checkConvergence(newV[2], lastV[2] - newV[2]);

        // Calculate triode parameters using Koren's model
        calcKoren(x);

        return done;
    }
};
 
struct MOSFET : public Component<3> {
    const double pnp; // Positive or negative MOSFET indicator (-1 or 1)

    double vt = 1.5;     // Threshold voltage
    double beta = 0.02;  // Beta parameter (transconductance)
    double lambda = 0.0; // Lambda parameter (channel-length modulation)

    double lastV[3] = {0}; // Last voltage values for convergence check
    double ids;                    // Drain current (output current)

    double geq[3][3] = { { 0.0 } }; // Matrix to store conductance values
    double ieq[3] = { 0.0 };        // Vector to store current values

    // Simple MOSFET implementation, inspired by circuitjs
    MOSFET(bool isPNP, int gate, int source, int drain, const Model& model)
        : pnp(isPNP ? -1.0 : 1.0)
    {
        pinLoc[0] = gate;
        pinLoc[1] = source;
        pinLoc[2] = drain;

        if (model.count("Kp"))
            beta = model.at("Kp");
        if (model.count("Vt0"))
            vt = model.at("Vt0");
        if (model.count("Lambda"))
            vt = model.at("Lambda");

    }

    void stamp(MNAMatrix& A, MNAVector& b) override
    {
        A[nets[2]][nets[2]].gdyn.push_back(&geq[2][2]);
        A[nets[2]][nets[1]].gdyn.push_back(&geq[2][1]);
        A[nets[2]][nets[0]].gdyn.push_back(&geq[2][0]);

        A[nets[1]][nets[2]].gdyn.push_back(&geq[1][2]);
        A[nets[1]][nets[1]].gdyn.push_back(&geq[1][1]);
        A[nets[1]][nets[0]].gdyn.push_back(&geq[1][0]);

        b[nets[2]].gdyn.push_back(&ieq[2]);
        b[nets[1]].gdyn.push_back(&ieq[1]);
    }

    bool converged(double last, double now) const
    {
        double dv = std::abs(last - now);

        // high beta MOSFETs are more sensitive to small differences, so we are stricter about convergence testing
        dv = beta > 1 ? dv * 100 : dv;
        return std::abs(dv) < vTolerance;
    }

    bool newton(MNAResultVector& x) override
    {
        bool allConverged = false;
        double vs[3] = { x[nets[0]], x[nets[1]], x[nets[2]] }; // copy since we don't want to change the actual values

        if (vs[1] > lastV[1] + 0.5)
            vs[1] = lastV[1] + 0.5;
        if (vs[1] < lastV[1] - 0.5)
            vs[1] = lastV[1] - 0.5;
        if (vs[2] > lastV[2] + 0.5)
            vs[2] = lastV[2] + 0.5;
        if (vs[2] < lastV[2] - 0.5)
            vs[2] = lastV[2] - 0.5;

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
        if (converged(lastV[0], vs[0]) && converged(lastV[1], vs[1]) && converged(lastV[2], vs[2]))
            allConverged = true;

        lastV[0] = vs[0];
        lastV[1] = vs[1];
        lastV[2] = vs[2];
        
        double realVgs = vgs;
        double realVds = vds;
        vgs *= pnp;
        vds *= pnp;
        const double be = beta * (1.0 + lambda * vds);
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
            ids = be * ((vgs - vt) * vds - vds * vds * 0.5);
            gm = be * vds;
            gds = be * (vgs - vds - vt);
        } else {
            // saturation; gds = 0
            gm = be * (vgs - vt);
            // use very small gds to avoid non-convergence
            gds = 1e-8;
            ids = 0.5 * be * (vgs - vt) * (vgs - vt) + (vds - (vgs - vt)) * gds;
        }

        const double rs = (-pnp) * ids + gds * realVds + gm * realVgs;

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
    JFET(bool isPNP, int gate, int source, int drain, const Model& model)
    : MOSFET(isPNP, gate, source, drain, {})
    {
        // Like a 2N5458 JFET
        vt = -4.5;
        beta = 0.00125;
        double is = 35e-12;
        
        if (model.count("IS"))
            is = model.at("IS");
        if (model.count("Kp"))
            beta = model.at("Kp");
        if (model.count("Lambda"))
            lambda = model.at("Lambda");
        if (model.count("Vt0"))
            vt = model.at("Vt0");

        if (model.count("PNP")) {
            if (model.at("PNP") != isPNP) {
                pd_error(nullptr, "circuit~: jfet model does not match with set PNP value. Proceeding with custom PNP value");
            }
        }

        diode = std::make_unique<Diode>(isPNP ? source : gate, isPNP ? gate : source, 10, is, 1.0);
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        MOSFET::stamp(A, b);
        diode->stamp(A, b);
    }

    void setupNets(int& netSize) final
    {
        diode->setupNets(netSize);

        for (int i = 0; i < 3; i++) {
            nets[i] = pinLoc[i];
        }
    }

    bool newton(MNAResultVector& x) final
    {
        return diode->newton(x) & MOSFET::newton(x);
    }
};

/* Experimental components, still disabled by default until they are tested more
// Voltage Follower (buffer)
struct VoltageFollower : public Component<2, 1> {
    double vOut = 0.0; // Output voltage

    VoltageFollower(int inputNode, int outputNode)
    {
        pinLoc[0] = inputNode;
        pinLoc[1] = outputNode;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // Stamp the connection from input to output
        stampStatic(A, +1, nets[0], nets[2]);
        stampStatic(A, -1, nets[1], nets[2]);

        stampStatic(A, +1, nets[2], nets[0]);
        stampStatic(A, -1, nets[2], nets[1]);
        
        b[nets[2]].gdyn.push_back(&vOut);
    }

    void update(MNAResultVector& x) final
    {
        // Set the output voltage to be the same as the input voltage (voltage follower behavior)
        vOut = x[nets[0]];
    }
};

struct DelayBuffer : public Component<2, 1> {
    double vOut = 0.0; // Output voltage
    double vOutDelayed = 0.0;

    DelayBuffer(int inputNode, int outputNode)
    {
        pinLoc[0] = inputNode;
        pinLoc[1] = outputNode;
    }

    void stamp(MNAMatrix& A, MNAVector& b) final
    {
        // Stamp the connection from input to output
        stampStatic(A, +1, nets[0], nets[2]);
        stampStatic(A, -1, nets[1], nets[2]);

        stampStatic(A, +1, nets[2], nets[0]);
        stampStatic(A, -1, nets[2], nets[1]);
        
        b[nets[2]].gdyn.push_back(&vOutDelayed);
    }

    void update(MNAResultVector& x) final
    {
        // Set the output voltage to be the same as the input voltage (voltage follower behavior)
        vOutDelayed = vOut;
        vOut = x[nets[0]];
    }
};
*/
// Experimental pentode implementation
struct Pentode : public Component<4, 3> {
            
    double mu = 10.7;    // Amplification factor (μ)
    double ex = 1.5;     // Exponential factor for Koren's equation
    double kg1 = 1672; // kg1 coefficient for Koren's equation
    double kg2 = 4500; // kg2 coefficient for Koren's equation
    double kp = 41.16;   // kp coefficient for Koren's equation
    double kvb = 12.7;    // kvb coefficient for Koren's equation
    double rgk = 2000;    // Grid resistor (Rgk)
    double vg = 13.0;    // Grid voltage offset

    // Capacitance values
    const double cgp = 2.4e-12; // Grid to plate capacitor (Cgp)
    const double cgk = 2.3e-12; // Grid to cathode capacitor (Cgk)
    const double cpk = 0.9e-12; // Plate to cathode capacitor (Cpk)

    // Voltages and currents
    double vgp = 0.0, vgk = 0.0, vpk = 0.0;
    double vcgp = 0.0f, vcgk = 0.0, vcpk = 0.0;
    double ip, ids, gm, gds, e1;
    
    // Variables to store voltages from the previous iteration
    double lastV[4] = {0};
    
    // Dynamic matrices for nodal analysis
    std::vector<double> ieq = std::vector<double>(4, 0);
    std::vector<std::vector<double>> geq = std::vector<std::vector<double>>(4, std::vector<double>(4, 0));

    Pentode(int plate, int grid, int cathode, int screen, const Model& model)
    {
        pinLoc[0] = plate;
        pinLoc[1] = grid;
        pinLoc[2] = cathode;
        pinLoc[3] = screen;
        
        if (model.count("Ex"))
            ex = model.at("Ex");
        if (model.count("Mu"))
            mu = model.at("Mu");
        if (model.count("Kg1"))
            kg1 = model.at("Kg1");
        if (model.count("Kg2"))
            kg1 = model.at("Kg2");
        if (model.count("Kp"))
            kp = model.at("Kp");
        if (model.count("Kvb"))
            kvb = model.at("Kvb");
    }

    void stamp(MNAMatrix& A, MNAVector& x) final
    {
        double g = 2.0 * cgp;
        stampTimed(A, +1, nets[1], nets[4]);
        stampTimed(A, -1, nets[0], nets[4]);

        stampTimed(A, -g, nets[1], nets[1]);
        stampTimed(A, +g, nets[1], nets[0]);
        stampTimed(A, +g, nets[0], nets[1]);
        stampTimed(A, -g, nets[0], nets[0]);
        stampStatic(A, +2.0 * g, nets[4], nets[1]);
        stampStatic(A, -2.0 * g, nets[4], nets[0]);
        stampStatic(A, -1.0, nets[4], nets[4]);

        x[nets[4]].gdyn.push_back(&vcgp);

        g = 2.0 * cgk;
        stampTimed(A, +1.0, nets[1], nets[5]);
        stampTimed(A, -1.0, nets[2], nets[5]);

        stampTimed(A, -g, nets[1], nets[1]);
        stampTimed(A, +g, nets[1], nets[2]);
        stampTimed(A, +g, nets[2], nets[1]);
        stampTimed(A, -g, nets[2], nets[2]);
        stampStatic(A, +2.0 * g, nets[5], nets[1]);
        stampStatic(A, -2.0 * g, nets[5], nets[2]);
        stampStatic(A, -1.0, nets[5], nets[5]);

        x[nets[5]].gdyn.push_back(&vcgk);

        g = 2 * cpk;
        stampTimed(A, +1.0, nets[0], nets[6]);
        stampTimed(A, -1.0, nets[2], nets[6]);

        stampTimed(A, -g, nets[0], nets[0]);
        stampTimed(A, +g, nets[0], nets[2]);
        stampTimed(A, +g, nets[2], nets[0]);
        stampTimed(A, -g, nets[2], nets[2]);
        stampStatic(A, +2.0 * g, nets[6], nets[0]);
        stampStatic(A, -2.0 * g, nets[6], nets[2]);
        stampStatic(A, -1.0, nets[6], nets[6]);

        x[nets[6]].gdyn.push_back(&vcpk);

        A[nets[0]][nets[0]].gdyn.push_back(&geq[0][0]);
        A[nets[0]][nets[1]].gdyn.push_back(&geq[0][1]);
        A[nets[0]][nets[2]].gdyn.push_back(&geq[0][2]);

        A[nets[1]][nets[1]].gdyn.push_back(&geq[1][1]);
        A[nets[1]][nets[2]].gdyn.push_back(&geq[1][2]);

        A[nets[2]][nets[0]].gdyn.push_back(&geq[2][0]);
        A[nets[2]][nets[1]].gdyn.push_back(&geq[2][1]);
        A[nets[2]][nets[2]].gdyn.push_back(&geq[2][2]);
        A[nets[2]][nets[3]].gdyn.push_back(&geq[2][3]);
        
        A[nets[3]][nets[0]].gdyn.push_back(&geq[3][0]);
        A[nets[3]][nets[1]].gdyn.push_back(&geq[3][1]);
        A[nets[3]][nets[2]].gdyn.push_back(&geq[3][2]);
        A[nets[3]][nets[2]].gdyn.push_back(&geq[3][3]);

        x[nets[0]].gdyn.push_back(&ieq[0]);
        x[nets[1]].gdyn.push_back(&ieq[1]);
        x[nets[2]].gdyn.push_back(&ieq[2]);
        x[nets[3]].gdyn.push_back(&ieq[3]);
    }

    void update(MNAResultVector& b) final
    {
        // Update internal voltage variables based on node voltages
        vcgp = b[nets[4]];
        vcgk = b[nets[5]];
        vcpk = b[nets[6]];

        vgk = b[nets[1]] - b[nets[2]];
        vpk = b[nets[0]] - b[nets[2]];
        vgp = b[nets[1]] - b[nets[0]];
    }

    void calcKoren(MNAResultVector& b)
    {
        // Calculate intermediate voltage differences
        const double cvgk = b[nets[1]] - b[nets[2]];
        const double cvpk = b[nets[0]] - b[nets[2]];
        const double cvg2k = b[nets[3]] - b[nets[2]];
                
        auto ln1exp = [](double x) {
            return (x > 50.0) ? x : std::log(1.0 + std::exp(x));
        };
        
        e1 = (cvg2k / kp) * ln1exp(kp * (1.0 / mu + cvgk / std::sqrt(kvb + std::pow(cvg2k, 2))));
 
        gds = e1 > 0.0 ? ex * std::sqrt(e1) / kg1 : 1e-8;
        // Calculate ids, gds, and gm based on e1
        ids = e1 > 0.0 ? (std::pow(e1, ex) / kg1) * atan(cvpk / kvb) : cvpk * gds;
        
        gm = gds / mu;
        
        double vgg2 = cvgk + (cvg2k / mu);
        double id2 = vgg2 > 0.0 ? pow(vgg2, 1.5) / kg2 : 1e-8;
        double gg2 =  e1 > 0.0 ? ex * std::sqrt(e1) / kg2 : 1e-8;

        // Calculate rs and gcr
        const double rs = -ids + gds * cvpk + gm * cvgk;
        const double rs2 = -id2 + (gg2 * cvg2k);
        const double gcr = cvgk > vg ? rgk : gMin;

        // Populate the geq and ieq matrices
        geq[0][0] = gds;
        geq[0][1] = gm;
        geq[0][2] = -gds - gm;

        geq[1][1] = gcr;
        geq[1][2] = -gcr;

        geq[2][0] = -gds;
        geq[2][1] = -gm - gcr;
        geq[2][2] = gds + gm + gcr + gg2;

        geq[3][2] = -gg2;
        geq[2][3] = -gg2;
        geq[3][3] = gg2;


        ieq[0] = rs;
        ieq[1] = 0;
        ieq[2] = -rs - rs2;
        ieq[3] = rs2;

        // Store current voltages for comparison in the next iteration
        lastV[0] = b[nets[0]];
        lastV[1] = b[nets[1]];
        lastV[2] = b[nets[2]];
        lastV[3] = b[nets[3]];
    }

    void initTransientAnalysis(double tStepSize) final
    {
        vcgp = 2.0 * cgp * vgp;
        vcgk = 2.0 * cgk * vgk;
        vcpk = 2.0 * cpk * vpk;
    }
    
    bool checkConvergence(double v, double dv)
    {
        dv /= v > 0 ? v : 1;
        return std::abs(dv) < vTolerance;
    }
    
    bool newton(MNAResultVector& b) final
    {
        // Apply the Newton-Raphson method for convergence
        double newV[4] = {b[nets[0]], b[nets[1]], b[nets[2]], b[nets[3]]};
    
        
        // Check for convergence based on voltage changes
        bool done = checkConvergence(newV[0], lastV[0] - newV[0]) && checkConvergence(newV[1], lastV[1] - newV[1]) && checkConvergence(newV[2], lastV[2] - newV[2]) && checkConvergence(newV[3], lastV[3] - newV[3]);
        
        // Calculate triode parameters using Koren's model
        calcKoren(b);
        
        return done;
    }
};
