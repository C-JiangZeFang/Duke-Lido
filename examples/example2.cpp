#include <string>
#include <iostream>
#include <exception>

#include "Pythia8/Pythia.h"
#include "simpleLogger.h"
#include "workflow.h"

using namespace Pythia8;

// This sample program evolve heavy quark (E0=30, M=1.3) in a static medium for t=3.0fm/c and calculate the average energy loss (<E-E0>)
int main(int argc, char* argv[]){
	Pythia pythia;
	// Declare Pythia object
  	pythia.readString("HardQCD:all = off"); // will repeat this line in the xml for demonstration  
	pythia.readString("HardQCD:hardccbar = on");
  	//pythia.readString("SoftQCD:all = on");
  	pythia.readString("HadronLevel:Decay = off");
  	pythia.readString("HadronLevel:all = off");
    pythia.readString("PartonLevel:all = on");
  	pythia.readString("PartonLevel:ISR = on");
  	pythia.readString("PartonLevel:MPI = off");
  	pythia.readString("PartonLevel:FSR = on");
	pythia.readString("PartonLevel:remnants = off");
	//pythia.readString("PromptPhoton:all=on");
  	pythia.readString("WeakSingleBoson:all=off");
  	pythia.readString("WeakDoubleBoson:all=off");
  	pythia.readString("Beams:idA = 2212");
  	pythia.readString("Beams:idB = 2212");
  	pythia.readString("Beams:eCM = 5020");
  	//pythia.readString("HardQCD:hardbbbar = on");
  	//pythia.readString("HardQCD:hardccbar = on");
  	pythia.readString("PhaseSpace:pTHatMin = 200");
    pythia.readString("PhaseSpace:pTHatMin = 210");
	pythia.readString("PhaseSpace:bias2Selection = off");
	pythia.readString("PhaseSpace:bias2SelectionPow = 6.");
	pythia.readString("Random:setSeed = on");
	pythia.readString("Random:seed = 120");
    pythia.init();

	std::vector<particle> plist;
    ////////////////////////////////////////////
	double mu = 2.0; // don't change...
	double const_alphas = 0.3; // don't change...
	double A = 1.0; // don't change...
	double B = 0.0; // don't change...
	////////////////////////////////////////////
	double M = 1.5; // GeV
	double T = 0.3; // GeV
	double L = 3; // fm
	double dt = 0.02; // fm/c
	double fmc_to_GeV_m1 = 5.026;

	for (int iEvent = 0; iEvent < 1000000; ++iEvent) 
    {

	    pythia.next();
        std::map<int,std::vector<std::vector<double>>> gluon_list;
		for (size_t i = 0; i < pythia.event.size(); ++i) 
		{
     		if(pythia.event[i].id() ==21 && pythia.event[i].status() == -51)
            {
            	int mother1=pythia.event[i].mother1();
           		int mother2=pythia.event[i].mother2();
            	if(mother2 == 0 && (abs(pythia.event[mother1].id())==4))
            	{
                	int motherid=pythia.event[mother1].iTopCopyId();
            		gluon_list[motherid].push_back({pythia.event[i].e(),pythia.event[i].px(),pythia.event[i].py(),pythia.event[i].pz(),pythia.event[mother1].e(),pythia.event[mother1].px(),pythia.event[mother1].py(),pythia.event[mother1].pz()});
            	}
            }
    	}
    for(auto& kv : gluon_list)
    {
	  particle p;
	  int fi=kv.first;
	  double e0=sqrt(pythia.event[fi].px()*pythia.event[fi].px()+pythia.event[fi].py()*pythia.event[fi].py()+pythia.event[fi].pz()*pythia.event[fi].pz()+M*M);
	  fourvec p0{e0, pythia.event[fi].px(), pythia.event[fi].py(), pythia.event[fi].pz()};
	  //std::cout<<pythia.event[fi].id()<<" "<<dot(p0,p0)<<std::endl;
	  p.mass = M; // mass
	  p.pid = 4; // charm quark
	  p.x = fourvec{0,0,0,0}; // initial position
	  p.p = p0; // initial momenta
      for (auto& p4 : kv.second)
      {
        pregluon G1;
		double qe0=sqrt(p4[5]*p4[5]+p4[6]*p4[6]+p4[7]*p4[7]+M*M);
		G1.p0 = fourvec{qe0,p4[5],p4[6],p4[7]}; // the mother parton energy
		G1.k1 = fourvec{p4[0],p4[1],p4[2],p4[3]}; // the initial four-momenta of the preformed gluon
	    G1.kn = G1.k1; // the current four-momenta of the preformed gluon
		G1.t0 = p.x.t(); // creation time, in units of [GeV^{-1}]
		G1.T0 = T; // Temperature at the creation time;
		//G1.local_mfp = ...; // for medium-incuded gluon, this shoul be ~ qhat/mD^2
					// for vacuum radiated gluon, this quantity should not be used
		//Add it to the preformed gluon list
		G1.is_vac=true;
		p.radlist.push_back(G1);
      }
	  plist.push_back(p);
    }
  }
	
	int Nsteps = int(L/dt);
	int Nparticles=plist.size();
	double Ei= 0.;
	for (auto & p : plist) Ei += p.p.t();
	Ei /= Nparticles;

	initialize("new", "./settings.xml", mu, const_alphas, A, B);

	double time = 0.;
    double sum = 0.;
	
	for (int it=0; it<Nsteps; ++it){
		if (it%10 ==0) LOG_INFO << it << " steps, " << "time = " << time << " [fm/c]";
		time += dt;
		for (auto & p : plist){
			// x-update
			p.freestream(dt*fmc_to_GeV_m1);
			// p-update
			int channel = update_particle_momentum_Lido(dt*fmc_to_GeV_m1, T, {0.0, 0.0, 0.0}, p);
		}
	}
	
	double Ef = 0.;
	for (auto & p : plist) Ef += p.p.t();
	Ef /= Nparticles;

	LOG_INFO << "Initial energy: " << Ei << " GeV";
	LOG_INFO << "Final average energy: " << Ef << " GeV";

	/*
	std::fstream fs;
    fs.open ("hq_gluon_modified.dat", std::fstream::out | std::fstream::app);
    for (auto & p : plist)
	{
		for (auto& g : p.radlist)
		{
			if(g.is_vac == true)
			{
				fs<<g.kn.x()<<" "<<g.kn.y()<<" "<<g.kn.z()<<" "<<g.kn.t()<<" "<<g.p0.x()<<" "<<g.p0.y()<<" "<<g.p0.z()<<" "<<g.p0.t()<<endl;
			}
		}
	}
	*/
	return 0;
}


