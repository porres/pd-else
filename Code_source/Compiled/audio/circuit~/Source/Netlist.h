#pragma once
#include <KLU/klu.h>

struct NetList
{
    typedef std::vector<IComponent*> ComponentList;
    
    NetList(int nodes) : nets(nodes), states(0)
    {
    }

    void addComponent(IComponent * c)
    {
        c->setupNets(nets, states, c->getPinLocs());
        components.push_back(c);
    }

    void buildSystem()
    {
        system.setSize(nets);
        for(int i = 0; i < components.size(); ++i)
        {
            components[i]->stamp(system);
        }
                
        setStepScale(0);
        system.tStep = 0;
        
        updatePre();
        updateMatrix();
        
        initKLU();
    }

    void setTimeStep(double tStepSize)
    {
        for(int i = 0; i < components.size(); ++i)
        {
            components[i]->scaleTime(system.tStep / tStepSize);
        }

        system.tStep = tStepSize;
        double stepScale = 1. / system.tStep;
        setStepScale(stepScale);
        
        refactorKLU();
    }
    
    void setStepScale(double stepScale)
    {
        // initialize matrix for LU and save it to cache
        for(int i = 0; i < nets; ++i)
        {
            system.b[i].initLU(stepScale);
            for(int j = 0; j < nets; ++j)
            {
                system.A[i][j].initLU(stepScale);
            }
        }
    }
    
    void simulateTick()
    {
        solve();
        system.time += system.tStep;
        update();
    }

    const MNASystem & getMNA() { return system; }

    void clearOutput() { std::fill(system.output.begin(), system.output.end(), 0.0f); }
    double getOutput(int idx) { return system.output[idx]; }
    void setBlockDC(bool block_dc) { system.block_dc = block_dc;}
    
    double& addDynamicArgument(std::string arg)
    {
        size_t delimiterPos = arg.find("$s");

        int idx = 0;
        // Check if the delimiter was found
        if (delimiterPos != std::string::npos) {
            idx = std::stoi(arg.substr(delimiterPos + 2));
        }
        
        variableArgs[idx] = 0.0f;
        return variableArgs[idx];
    }
    
    int getNumVariableArgs()
    {
        return variableArgs.size();
    }
    
    void setVariableArg(int idx, double value)
    {
        variableArgs[idx] = value;
    }
    
    void setMaxIter(int iter)
    {
        maxiter = iter;
    }

        
protected:
        
    std::map<int, double> variableArgs;
    
    int nets, states;
    ComponentList components;

    MNASystem system;

    klu_symbolic* Symbolic;
    klu_numeric* Numeric;
    klu_common Common;
    
    // parameters
    int maxiter         = 20;
    double solvertol    = 1e-6;
    bool nochecking     = false;
    
    std::vector<double> b;
    std::vector<double> AVal;
    std::vector<int> AI;
    std::vector<int> AJ;
    std::vector<MNACell*> nzpointers;
    
    void update()
    {
        for(int i = 0; i < components.size(); ++i)
        {
            components[i]->update(system);
        }
    }

    // return true if we're done
    bool newton()
    {
        bool done = 1;
        for(int i = 0; i < components.size(); ++i)
        {
            done = done && components[i]->newton(system);
        }
        return done;
    }


    void updatePre()
    {
        for(int i = 0; i < nets; ++i)
        {
            system.b[i].updatePre();
        }
        
        for(int j = 0; j < nzpointers.size(); ++j)
        {
            nzpointers[j]->updatePre();
        }
    }
    
    void initKLU()
    {
        klu_defaults (&Common);
        Common.tol = solvertol;
        Common.scale = nochecking ? -1 : 2;
        
        // Symbolic analysis
        Symbolic = klu_analyze ((int)nets - 1, &AI[0], &AJ[0], &Common);
        // Full numeric factorization: Only needed once!
        Numeric = klu_factor (&AI[0], &AJ[0], getAValues(), Symbolic, &Common);
    }

    void refactorKLU()
    {        
        klu_free_symbolic(&Symbolic, &Common);
        klu_free_numeric(&Numeric, &Common);
        
        // Symbolic analysis
        Symbolic = klu_analyze (nets - 1, &AI[0], &AJ[0], &Common);
        // Full numeric factorization: Only needed once!
        Numeric = klu_factor (&AI[0], &AJ[0], getAValues(), Symbolic, &Common);
    }
    
    void solve()
    {
        // Iterate junctions
        for(int iter = 0; iter < std::max(maxiter, 1); iter++)
        {
            // restore matrix state and add dynamic values
            updatePre();
            
            solveKLU();
            
            if(newton()) break;
        }
    }
    
    void solveKLU()
    {
        // Update our factorization or refactor if the last factorization failed
        klu_refactor (&AI[0], &AJ[0], getAValues(), Symbolic, Numeric, &Common);

        // Solve the system!
        klu_solve (Symbolic, Numeric, nets - 1, 1, getBValues(), &Common);

        for (size_t i = 1; i < nets; i++)
        {
            //system.xPlot[i] = b[i - 1];
            system.b[i].lu = b[i - 1];
        }
    }
    
    // Creates a sparse representation of the matrix, this is what KLU takes as input. We also swap rows with columns here.
    void updateMatrix()
    {
        b.clear();
        AI.clear();
        AJ.clear();
        AVal.clear();
        nzpointers.clear();
        int nonzero = 0;

        for (size_t i = 1; i < nets; i++)
        {
            for (int j = 1; j < nets; j++)
            {
                if(system.A[j][i].g != 0 || system.A[j][i].prelu != 0 || system.A[j][i].gdyn.size() || system.A[j][i].gtimed != 0)
                    nonzero++;
            }
        }

        // Allocate memory for CSR format
        b.resize(nets - 1);
        AI.resize(nets);
        AJ.resize(nonzero);
        AVal.resize(nonzero);
        nzpointers.resize(nonzero);
        // Reset nonzeros
        nonzero = 0;
        AI[0] = 0;

        // Create our CSR format
        for (size_t i = 1; i < nets; i++)
        {
            b[i - 1] = system.b[i].lu;

            for (size_t j = 1; j < nets; j++)
            {
                system.A[j][i].nonzero = false;
                bool notzero = system.A[j][i].prelu != 0 || system.A[j][i].gdyn.size() || system.A[j][i].gtimed != 0;

                if (notzero)
                {
                    system.A[j][i].nonzero = true;
                    AVal[nonzero] = system.A[j][i].lu;
                    AJ[nonzero] = j - 1;
                    nzpointers[nonzero] = &(system.A[j][i]);
                    nonzero++;
                }
            }

            AI[i] = nonzero;
        }
    }
    
    // List of all non-zero values in the A matrix
    double* getAValues()
    {
        for (size_t i = 0; i < nzpointers.size(); i++)
        {
            AVal[i] = (*nzpointers[i]).lu;
        }

        return &AVal[0];
    }

    // List of all values in the B matrix
    double* getBValues()
    {
        for (size_t i = 1; i < nets; i++)
        {
            b[i - 1] = system.b[i].lu;
        }

        return &b[0];
    }
};
