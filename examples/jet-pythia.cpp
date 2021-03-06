#include <string>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <exception>
#include <random>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <sstream>

#include "simpleLogger.h"
#include "Medium_Reader.h"
#include "workflow.h"
#include "pythia_jet_gen.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

void output_jet(std::string fname, std::vector<particle> plist){
    std::ofstream f(fname);
    for (auto & p : plist) f << p.pid << " " << p.p << " " << p.weight << std::endl;
    f.close();
}

int main(int argc, char* argv[]){
    using OptDesc = po::options_description;
    OptDesc options{};
    options.add_options()
          ("help", "show this help message and exit")
          ("pythia-setting,y",
           po::value<fs::path>()->value_name("PATH")->required(),
           "Pythia setting file")
          ("pythia-events,n",
            po::value<int>()->value_name("INT")->default_value(100,"100"),
           "number of Pythia events")
          ("pthat-low,l",
            po::value<int>()->value_name("INT")->default_value(100,"100"),
           "pThatMin")
          ("pthat-high,h",
            po::value<int>()->value_name("INT")->default_value(120,"120"),
           "pThatMax")
          ("lido-setting,s", 
            po::value<fs::path>()->value_name("PATH")->required(),
           "Lido table setting file")
          ("lido-table,t", 
            po::value<fs::path>()->value_name("PATH")->required(),
           "Lido table path to file")
          ("mu,m", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "medium scale paramtmer")
          ("afix,f", 
            po::value<double>()->value_name("FLOAT")->default_value(-1.0,"-1.0"),
            "fixed alphas value, -1 is running")
          ("k-factor,k", 
            po::value<double>()->value_name("FLOAT")->default_value(0.0,"0.0"),
            "K-factor for the delta-qhat")
          ("t-scale,a", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "rescale the T-dependence")
          ("e-scale,b", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "rescale the p-dependence")
          ("t-power,p", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "T-dependence power")
          ("e-power,q", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "p-dependence power")
          ("gamma,g", 
            po::value<double>()->value_name("FLOAT")->default_value(0.0,"0.0"),
            "kpara / kperp anisotropy parameter")
          ("qcut,c",
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"), 
            "separation scale Q2 = qcut * mD2")
          ("rvac,r", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"), 
            "vacuum-like radiation remove factor")    
          ("tf",
           po::value<double>()->value_name("FLOAT")->default_value(5.0,"5.0"),
           "medium stopping time [fm/c]")
          ("dt",
           po::value<double>()->value_name("FLOAT")->default_value(0.02,"0.02"),
           "hydro time step dtau [fm/c]")
          ("temp",
           po::value<double>()->value_name("FLOAT")->default_value(0.3,"0.3"),
           "medium temperature [GeV]")
    ;
    po::variables_map args{};
    try{
        po::store(po::command_line_parser(argc, argv).options(options).run(), args);
        if (args.count("help")){
                std::cout << "usage: " << argv[0] << " [options]\n"
                          << options;
                return 0;
        }    
        // check lido setting
        if (!args.count("lido-setting")){
            throw po::required_option{"<lido-setting>"};
            return 1;
        }
        else{
            if (!fs::exists(args["lido-setting"].as<fs::path>())){
                throw po::error{"<lido-setting> path does not exist"};
                return 1;
            }
        }
        // check lido table
        std::string table_mode;
        if (!args.count("lido-table")){
            throw po::required_option{"<lido-table>"};
            return 1;
        }
        else{
            table_mode = (fs::exists(args["lido-table"].as<fs::path>())) ? 
                         "old" : "new";
        }
        // check pythia setting
        if (!args.count("pythia-setting")){
            throw po::required_option{"<pythia-setting>"};
            return 1;
        }
        else{
            if (!fs::exists(args["pythia-setting"].as<fs::path>())) {
                throw po::error{"<pythia-setting> path does not exist"};
                return 1;
            }
        }

        // start
        std::vector<particle> plist, new_plist, pOut_list;

        /// HardGen
        PythiaGen pythiagen(
                args["pythia-setting"].as<fs::path>().string(),
                " ",
                args["pthat-low"].as<int>(),
                args["pthat-high"].as<int>(),
                0
        );
    
        /// Lido init
        initialize(table_mode,
                args["lido-setting"].as<fs::path>().string(),
                args["lido-table"].as<fs::path>().string(),
                args["mu"].as<double>(),
                args["afix"].as<double>(),
                args["k-factor"].as<double>(),
                args["t-scale"].as<double>(),
                args["e-scale"].as<double>(),
                args["t-power"].as<double>(),
                args["e-power"].as<double>(),
                args["gamma"].as<double>(),
                args["qcut"].as<double>(),
                args["rvac"].as<double>()
                );

        // proper times
        double tau0 = 0.6 * 5.026;
        double dtau = args["dt"].as<double>() * 5.026; // convert to GeV^-1
        double tauf = args["tf"].as<double>() * 5.026; // convert to GeV^-1

        double T0 = args["temp"].as<double>(); 
        int Nsteps = int(tauf/dtau);

        for (int ie=0; ie<args["pythia-events"].as<int>(); ie++){
            pythiagen.Generate(plist);
            // freestream form t=0 to tau=tau0
            for (auto & p : plist) {
              double dt = calcualte_dt_from_dtau(p.x, p.p, 0, tau0);
              p.freestream(dt); 
            }
            for (int i=0; i<Nsteps; i++){
                new_plist.clear();
			          double tau =tau0+ i*dtau;
                double T = T0*std::pow(tau0/tau, 0.4);
			          if (i%10==0) LOG_INFO << tau/5.026 << " [fm/c]\t" 
                                    << T << " [GeV]" << " #=" << plist.size();
                
		            for (auto & p : plist){
                   
                    double dt = calcualte_dt_from_dtau(p.x, p.p, tau, dtau);
                    double vz = p.x.z()/( p.x.t() + 1e-5 );
                    double eta = 0.5*std::log((1.+vz)/(1.-vz));
                    if ( std::fabs(eta)>3) continue;
		                int n = update_particle_momentum_Lido(
		                           dt, T, {0., 0., vz}, p, pOut_list);
					          for (auto & k : pOut_list) {
		                new_plist.push_back(k);
		            }
		        }
		        plist = new_plist;
		    }
            std::ostringstream s;
            s << "results/" << args["pthat-low"].as<int>() << "-"
              << args["pthat-high"].as<int>() << "-" << ie;
            output_jet(s.str(), plist);
        }
    }
    catch (const po::required_option& e){
        std::cout << e.what() << "\n";
        std::cout << "usage: " << argv[0] << " [options]\n"
                  << options;
        return 1;
    }
    catch (const std::exception& e) {
       // For all other exceptions just output the error message.
       std::cerr << e.what() << '\n';
       return 1;
    }    
    return 0;
}



