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
#include <unistd.h>

#include "simpleLogger.h"
#include "Medium_Reader.h"
#include "workflow.h"
#include "pythia_jet_gen.h"
#include "Hadronize.h"
#include "jet_finding.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

void output_jet(std::string fname, std::vector<particle> plist){
    std::ofstream f(fname);
    for (auto & p : plist) f << p.pid << " " << p.p << " " << p.x0 << " " << p.x << std::endl;
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
          ("ic,i",
            po::value<fs::path>()->value_name("PATH")->required(),
           "trento initial condition file")
          ("eid,j",
           po::value<int>()->value_name("INT")->default_value(0,"0"),
           "trento event id") 
          ("heavy", 
            po::value<int>()->value_name("INT")->default_value(0,"0"),
           "require pythia event contains charm(4) or bottom(5) quark")
           ("output,o",
           po::value<fs::path>()->value_name("PATH")->default_value("./"),
           "output file prefix or folder")
          ("Q0,q",
           po::value<double>()->value_name("DOUBLE")->default_value(.4,".4"),
           "Scale [GeV] to insert in-medium transport")
    ;

    po::variables_map args{};
    try{
        po::store(po::command_line_parser(argc, argv).options(options).run(), args);
        if (args.count("help")){
                std::cout << "usage: " << argv[0] << " [options]\n"
                          << options;
                return 0;
        }    
        // check trento event
        if (!args.count("ic")){
            throw po::required_option{"<ic>"};
            return 1;
        }
        else{
            if (!fs::exists(args["ic"].as<fs::path>())) {
                throw po::error{"<ic> path does not exist"};
                return 1;
            }
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

        if (args["heavy"].as<int>() >= 4){
            if (args["heavy"].as<int>() == 4)
                LOG_INFO << "Requires Pythia parton level has charm quark(s)";
            else if (args["heavy"].as<int>() == 5)
                LOG_INFO << "Requires Pythia parton level has bottom quark(s)";
            else {
                throw po::error{"Heavy trigger invalided"};
                return 1;
            }
        }

        // Scale to insert In medium transport
        double Q0 = args["Q0"].as<double>();
        /// use process id to define filename
        int processid = getpid();

        std::vector<double> TriggerBin({10,20,40,60,80,120,160,200,300,
                                        400,500,700,1000,1500,2000});
        for (int iBin = 0; iBin < TriggerBin.size()-1; iBin++){
            /// Initialize a pythia generator for each pT trigger bin
            PythiaGen pythiagen(
                args["pythia-setting"].as<fs::path>().string(),
                args["ic"].as<fs::path>().string(),
                TriggerBin[iBin],
                TriggerBin[iBin+1],
                args["eid"].as<int>(),
                Q0
            );
            LOG_INFO << pythiagen.sigma_gen() << " "
                     << TriggerBin[iBin] << " "
                     << TriggerBin[iBin];
            for (int ie=0; ie<args["pythia-events"].as<int>(); ie++){
                std::vector<particle> plist, hlist, thermal_list,
                                      new_plist, pOut_list;
                std::vector<current> clist;
                // Initialize parton list from python
                pythiagen.Generate(plist, args["heavy"].as<int>());
                double sigma_gen = pythiagen.sigma_gen()
                                  / args["pythia-events"].as<int>();
                
                std::stringstream fheader;
                fheader << args["output"].as<fs::path>().string() 
                        << processid;
                std::vector<double> Rs({.2,.4,.6,.8,1.});
                FindJetTower(
                    plist, clist,
                    Rs, 10,
                    -3, 3,
                    fheader.str(), 
                    sigma_gen
                );
            }
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


