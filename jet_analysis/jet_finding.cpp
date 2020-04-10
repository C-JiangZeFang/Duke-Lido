#include <fstream>
#include <string>
#include <math.h>
#include <vector>
#include "jet_finding.h"
#include "workflow.h"
#include "lorentz.h"
#include "simpleLogger.h"
#include "integrator.h"
#include <sstream>
#include <gsl/gsl_sf_gamma.h>
//gsl_sf_gamma_inc_Q(s, x)
inline int corp_index(double x, double xL, double xH, double dx, int Nx){
    if (x<xL || x>xH) return -1;
    else if (x==xH) return Nx-1;
    else return int((x-xL)/dx);
}

double inline F(double gamma_perp, double vperp, double dphi, double deta){
    return gamma_perp*(std::cosh(deta)-vperp*std::cos(dphi));
}

void redistribute(
          std::vector<current> & SourceList, 
          std::vector<HadronizeCurrent> & HadronizeList, 
          std::vector<std::vector<fourvec> > & DeltaPmu,
          std::vector<std::vector<double> > & DeltaPT,
          const int _Ny, const int _Nphi, 
          const double ymin, const double ymax,
          const double phimin, const double phimax,
          int coarse_level, double pTmin=0.){
    // use coarse grid
    int Ny = int(_Ny/coarse_level);
    int Nphi = int(_Nphi/coarse_level);
    const double dy = (ymax-ymin)/(Ny-1); 
    const double dphi = (phimax-phimin)/Nphi;
    const double vradial = 0.6;
    const double gamma_radial = 1./std::sqrt(1-std::pow(vradial,2));

    std::vector<std::vector<fourvec> > CoarsedPmu;
    std::vector<std::vector<double> > CoarsedPT;
    CoarsedPmu.resize(Ny);
    for (auto & it : CoarsedPmu) {
        it.resize(Nphi);
        for (auto & iit: it) iit = fourvec{0.,0.,0.,0.};
    }
    CoarsedPT.resize(Ny);
    for (auto & it : CoarsedPT) {
        it.resize(Nphi);
        for (auto & iit: it) iit = 0.;
    }


    LOG_INFO << SourceList.size() << " sources";
    //organizes souce by rapidity
    std::vector<current> clist;
    clist.resize(_Ny);
    for (int iy=0; iy<_Ny; iy++){
        clist[iy].chetas = std::cosh(ymin+iy*dy/coarse_level);
        clist[iy].shetas = std::sinh(ymin+iy*dy/coarse_level);
        clist[iy].cs = std::sqrt(.333);
        clist[iy].p.a[0] = 0.;
	clist[iy].p.a[1] = 0.;
	clist[iy].p.a[2] = 0.;
	clist[iy].p.a[3] = 0.;
    }
    for (auto & s: SourceList){
        int i = corp_index(std::atanh(s.shetas/s.chetas), ymin, ymax, dy/coarse_level, _Ny);
        if (i<0) continue;
        clist[i].p = clist[i].p + s.p;
    }
    for (int iy=0; iy<Ny; iy++){
        double y = ymin+iy*dy;    
        double chy = std::cosh(y), 
               shy = std::sinh(y);
        for (int iphi=0; iphi<Nphi; iphi++){
            double phi = phimin+(iphi+.5)*dphi;
            double cphi = std::cos(phi), sphi = std::sin(phi);
            fourvec Nmu0{chy,cphi,sphi,shy};
            auto code = [clist,
                         cphi, sphi, 
                         chy, shy, 
                         vradial, gamma_radial, pTmin]
                         (double costhetak){
                double one_and_cs = 4./3.;
                double res=0.;
                double sinthetak = std::sqrt(1.-std::pow(costhetak,2)); 
                double Tri_sinthetak= sinthetak*3.;
                double Tri_costhetak = costhetak*3.;
                for (auto & source : clist){
                    double cs = source.cs;
                    double chetas = source.chetas,
                           shetas = source.shetas;
                    double tauk = std::sqrt(1./cs/cs-costhetak*costhetak);
                    double coshyk = 1./cs/tauk,
                           sinhyk = costhetak/tauk;
                    double cosh_y_etas = chy*chetas-shy*shetas,
                           sinh_y_etas = shy*chetas-chy*shetas;
                    double cosh_y_etas_yk = cosh_y_etas*coshyk
                                          - sinh_y_etas*sinhyk;
                    double D = gamma_radial * cosh_y_etas_yk;
		    double D0 = gamma_radial * (cosh_y_etas_yk-vradial);
                    double alpha = vradial/cosh_y_etas_yk;
                    double alpha2 = std::pow(alpha,2);
                    double one_minus_alpha2 = 1. - alpha2;
                    double one_minus_alpha2_pow_n3d5 =
                                 std::pow(one_minus_alpha2, -3.5);
                    double one_minus_alpha2_pow_n2d5 = 
                               one_minus_alpha2_pow_n3d5 * one_minus_alpha2;
                          
                    double E03 = cs*source.p.t()+costhetak*source.p.z();
                    double E12 = cphi*source.p.x()+sphi*source.p.y();
                    double UdotG_phikint = 
                      gamma_radial*one_minus_alpha2_pow_n2d5* (
                       ( (2.+alpha2)*E03 + alpha*E12*Tri_sinthetak)*(
                              coshyk/cs
                            - sinhyk*Tri_costhetak
                            - Tri_sinthetak*vradial
                         )
                      );
                    double NdotG_phikint = 
                      one_minus_alpha2_pow_n3d5 * (
                        ((2.+3.*alpha2)*E03
                         +alpha*(4.+alpha2)*E12*sinthetak)*(
                              cosh_y_etas/cs
                            - sinh_y_etas*Tri_costhetak
                         )
                       - (alpha*(4.+alpha2)*E03
                         + (1.+4.*alpha2)*sinthetak*E12)*Tri_sinthetak
                      );
                    res += gsl_sf_gamma_inc_Q(5., pTmin/0.16*D0)*(one_and_cs*UdotG_phikint*D
                               - NdotG_phikint)/std::pow(D,4);    
                                
                }
                return res;
            };
            double error;
            double res = 3*M_PI/std::pow(4.*M_PI,2)*
                         quad_1d(code, {-.99,.99}, error, 0, 0.1, 20);
            CoarsedPmu[iy][iphi] = CoarsedPmu[iy][iphi] + Nmu0*res;
            CoarsedPT[iy][iphi] += res;
            /// add hadronization contribution
            for (auto & p : HadronizeList){
                double sigma = p.gamma_perp*(std::cosh(y-p.etas)
                               - p.v_perp*std::cos(phi-p.phiu) );
                double dpT = 3/(4.*M_PI)
                            *(4./3.*sigma*p.UdotG - dot(Nmu0, p.G))
                            /std::pow(sigma, 4);
                CoarsedPT[iy][iphi] += dpT;
                CoarsedPmu[iy][iphi] = CoarsedPmu[iy][iphi] + Nmu0*dpT;
            }
        }
    }
    // Interpolate the coarse grid to finer grid
    const double fine_dy = (ymax-ymin)/(_Ny-1); 
    const double fine_dphi = (phimax-phimin)/_Nphi;
    for (int i=0; i<_Ny; i++){
        double y = ymin + fine_dy*i;
        int ii = corp_index(y, ymin, ymax, dy, Ny);
        if (ii<0) continue;
        if (ii==Ny-1) ii--;
        double residue1 = (y-(ymin+dy*ii))/dy;
        double u[2] = {1.-residue1, residue1};
        for (int j=0; j<_Nphi; j++){ 
            double phi = phimin + fine_dphi*(j);
            int jj = corp_index(phi, phimin, phimax, dphi, Nphi);
            if (jj==Nphi-1) jj--;
            double residue2 = (phi-(phimin+dphi*(jj)))/dphi;
            double v[2] = {1.-residue2, residue2};
            for (int k1=0; k1<2; k1++){
                for (int k2=0; k2<2; k2++){
                    DeltaPmu[i][j] = DeltaPmu[i][j] + 
                        CoarsedPmu[ii+k1][jj+k2]*fine_dy*fine_dphi*u[k1]*v[k2];
                    DeltaPT[i][j] += 
                        u[k1]*v[k2]*CoarsedPT[ii+k1][jj+k2]*fine_dy*fine_dphi;
                }
            }
        }
    }
}

void FindJetTower(std::vector<particle> plist, 
             std::vector<current> SourceList,
             std::vector<HadronizeCurrent> HadronizeList,
             std::vector<double> Rs,
             double jetpTMin, 
             double jetyMin, 
             double jetyMax,
             std::string fheader, 
             double sigma_gen) {
    // contruct four momentum tower in the eta-phi plane
    int Neta = 300, Nphi = 300;
    double etamin = -3., etamax = 3.;
    double deta = (etamax-etamin)/(Neta-1); 
    // Phi range has to be the same as the range of std::atan2(*,*);
    double phimin = -M_PI, phimax = M_PI; 
    double dphi = (phimax-phimin)/Nphi;
    std::vector<std::vector<fourvec> > Pmutowers, dummyPmu;
    std::vector<std::vector<double> > PTtowers, dummyPT;
    Pmutowers.resize(Neta); dummyPmu.resize(Neta);
    for (auto & it : Pmutowers) {
        it.resize(Nphi);
        for (auto & iit: it) iit = fourvec{0.,0.,0.,0.};
    }
    for (auto & it : dummyPmu) {
        it.resize(Nphi);
        for (auto & iit: it) iit = fourvec{0.,0.,0.,0.};
    }

    PTtowers.resize(Neta); dummyPT.resize(Neta);
    for (auto & it : PTtowers) {
        it.resize(Nphi);
        for (auto & iit: it) iit = 0.;
    }
    for (auto & it : dummyPT) {
        it.resize(Nphi);
        for (auto & iit: it) iit = 0.;
    }



    // put hard particles into the towers
    for (auto & p : plist){
        double eta = p.p.pseudorap();
        double phi = p.p.phi();
        int ieta = corp_index(eta, etamin, etamax, deta, Neta);
        if (ieta<0) continue;
        int iphi = corp_index(phi, phimin, phimax, dphi, Nphi);
        Pmutowers[ieta][iphi] = Pmutowers[ieta][iphi] + p.p;
        PTtowers[ieta][iphi] += p.p.xT();
        if (p.p.xT()>0.7){
 	    dummyPmu[ieta][iphi] = dummyPmu[ieta][iphi] + p.p;
            dummyPT[ieta][iphi] += p.p.xT();
	}
    }
    // put soft energy-momentum deposition into the towers
    if (SourceList.size()>0){
	
        redistribute(
          SourceList, 
          HadronizeList,
          Pmutowers,
          PTtowers,
          Neta, Nphi,
          etamin, etamax,
          phimin, phimax,
          5, 0.);
        redistribute(
          SourceList,
          HadronizeList,
          dummyPmu,
          dummyPT,
          Neta, Nphi,
          etamin, etamax,
          phimin, phimax,
          5, .7);

    }
    // Use the towers to do jet finding: anti-kT
    int power = -1; 
    // loop over jetradius
    for (int iR=0; iR<Rs.size(); iR++){
        double jetRadius = Rs[iR];
        fastjet::JetDefinition jetDef(
                    fastjet::genkt_algorithm, jetRadius, power);
        std::vector<fastjet::PseudoJet> fjInputs;
        fastjet::Selector select_rapidity = 
              fastjet::SelectorRapRange(jetyMin, jetyMax);
        for (int ieta=0; ieta<Neta; ieta++) {
            double eta = etamin+ieta*deta;
            double chy = std::cosh(eta), shy = std::sinh(eta);
            for (int iphi=0; iphi<Nphi; iphi++) {
                auto ipmu = Pmutowers[ieta][iphi];
                double phi = phimin+iphi*dphi;
                double cphi = std::cos(phi), sphi = std::sin(phi);
                // when we do the first jet finding,
                // only use towers with positive contribution
                if (ipmu.t()>0){
                    fastjet::PseudoJet fp(ipmu.x(), ipmu.y(), 
                                          ipmu.z(), ipmu.t());
                    fp.set_user_info(new MyInfo(0, 0, 0));
                    fjInputs.push_back(fp);
                }
            }
        }
        
        fastjet::ClusterSequence clustSeq(fjInputs, jetDef);
        std::vector<fastjet::PseudoJet> AllJets = 
                                clustSeq.inclusive_jets(jetpTMin);
        std::vector<fastjet::PseudoJet> jets =
                        fastjet::sorted_by_pt(select_rapidity(AllJets));

        // once the jet is find, 
        // recombine all the bins with negative contribution
        std::stringstream filename_jets;
        filename_jets << fheader << "-jets.dat";
        std::ofstream f1(filename_jets.str(), std::ios_base::app);
        std::vector<fourvec> results;
        for (auto & j : jets){
            fourvec jetP{j.e(), j.px(), j.py(), j.pz()};
            fourvec newP{0., 0., 0., 0.};
            double Jphi = jetP.phi();
            double Jeta = jetP.pseudorap();
            for (double eta=Jeta-jetRadius; eta<=Jeta+jetRadius; eta+=deta){
                int ieta = corp_index(eta, etamin, etamax, deta, Neta);
                if (ieta<0) continue;
                double Rp = std::sqrt(1e-9+std::pow(jetRadius,2)
                                          -std::pow(Jeta-eta,2) ); 
                double chy = std::cosh(eta), shy = std::sinh(eta);
                for (double phi=Jphi-Rp; phi<=Jphi+Rp; phi+=dphi){
                    double newphi = phi;
                    if (phi<-M_PI) newphi+=2*M_PI;
                    if (phi>M_PI) newphi-=2*M_PI;
                    double cphi = std::cos(newphi), sphi = std::sin(newphi);
                    int iphi = corp_index(newphi, phimin, phimax, dphi, Nphi);
                    newP = newP + Pmutowers[ieta][iphi];
                }  
            }
            results.push_back(newP);    
            // label the jet flavor:
            // if heavy, label it by heavy quark id
            // else, label it by the hardest (pT) parton inside
            int flavor=0;
            fourvec pf{0,0,0,0};
            for (auto & p : plist){
                int absid = std::abs(p.pid);
                bool trigger = absid == 4 || 
                             absid == 411 || absid == 421 ||
                             absid == 413 || absid == 423 ||
                             absid == 5 || 
                             absid == 511 || absid == 521 ||
                             absid == 513 || absid == 523;
                if ( trigger){
                    double eta = p.p.pseudorap();
                    double phi = p.p.phi();
                    if (std::sqrt(std::pow(eta-Jeta,2)
                                 +std::pow(phi-Jphi,2)) < jetRadius) {
                        flavor = std::abs(p.pid);
                        pf = p.p;
                        break;
                    }
                }
            }      
            if (flavor==0)  {
                for (auto & p : plist){
                    double eta = p.p.pseudorap();
                    double phi = p.p.phi();
                    if (std::sqrt(std::pow(eta-Jeta,2)
                                 +std::pow(phi-Jphi,2)) < jetRadius) {
                        if ( p.p.xT()>pf.xT() ){
                            flavor = std::abs(p.pid);
                            pf = p.p;
                        }
                    }
                }
            }
            f1 << iR << " " 
              << newP.xT() << " " << newP.phi() << " " 
              << newP.pseudorap() << " " << newP.m2() << " " 
              << sigma_gen << " " 
              << flavor << " " 
              << pf.xT() << " " << pf.phi() << " " 
              << pf.pseudorap() << " "  << std::endl;
        }

        // also compute jetshape, with the following bins
        double bins[19] = {0, .05, .1, .15,  .2, .25, .3,
                          .35, .4, .45, .5,  .6, .7,  .8, 
                           1., 1.5, 2.0, 2.5, 3.0};
        std::stringstream filename_jetshape;
        filename_jetshape << fheader << "-jetshape.dat";
        std::ofstream f2(filename_jetshape.str(), std::ios_base::app);
        for (auto newP : results){
            double Jphi = newP.phi();
            double Jeta = newP.pseudorap();
            std::vector<double> hist;
            hist.resize(18);
            for (auto & it : hist) it = 0.;
            for (double eta=Jeta-3.; eta<=Jeta+3.; eta+=deta){

                int ieta = corp_index(eta, etamin, etamax, deta, Neta);
                if (ieta<0) continue;
                double Rp = std::sqrt(1e-9+std::pow(3.,2)
                                          -std::pow(Jeta-eta,2) ); 
                for (double phi=Jphi-Rp; phi<=Jphi+Rp; phi+=dphi){
                    double dist = std::sqrt(std::pow(Jeta-eta,2)
                                          + std::pow(Jphi-phi,2));
                    int index = 0;
                    for (index=0; index<18; index++){
                        if (dist>=bins[index] && dist<bins[index+1]) break;
                    }
                    double newphi = phi;
                    if (phi<-M_PI) newphi+=2*M_PI;
                    if (phi>M_PI) newphi-=2*M_PI;
                    int iphi = corp_index(newphi, phimin, phimax, dphi, Nphi);
                    hist[index] += dummyPT[ieta][iphi];
                }  
            }
            f2 << iR << " " << newP.xT() << " " << newP.phi() << " " 
               << newP.pseudorap() << " " << newP.m2() << " " 
               << sigma_gen << " ";
            for (int i=0; i<hist.size(); i++) 
                f2 << hist[i]/(bins[i+1]-bins[i]) << " "; 
            f2 << std::endl;
        }
    }   
}

void TestSource(
             std::vector<current> SourceList,
             std::vector<HadronizeCurrent> HadronizeList,
             std::string fname) {
    // contruct four momentum tower in the eta-phi plane
    int Neta = 43, Nphi = 48;
    double etamin = -3.15, etamax = 3.15;
    double deta = (etamax-etamin)/(Neta-1); 
    // Phi range has to be the same as the range of std::atan2(*,*);
    double phimin = -M_PI, phimax = M_PI; 
    double dphi = (phimax-phimin)/Nphi;
    std::vector<std::vector<fourvec> > towers;
    towers.resize(Neta);
    std::vector<std::vector<double> > dpTarray;
    towers.resize(Neta);
    dpTarray.resize(Neta);
    for (auto & it : towers) {
        it.resize(Nphi);
        for (auto & iit: it) iit = fourvec{0.,0.,0.,0.};
    }
    for (auto & it : dpTarray) {
        it.resize(Nphi);
        for (auto & iit: it) iit = 0;
    }

    fourvec T0{0,0,0,0};
    for (auto& r : towers){
        for (auto& it: r){
            T0 = T0 + it;
        }
    }
    if (SourceList.size() >0){
    redistribute(
          SourceList, 
          HadronizeList,
          towers,
          dpTarray,
          Neta, Nphi,
          etamin, etamax,
          phimin, phimax, 1);
    }

    fourvec T1{0,0,0,0};
    for (auto& r : towers){
        for (auto& it: r){
            T1 = T1 + it;
        }
    }
    LOG_INFO << "total" << T1-T0;

    std::ofstream f(fname.c_str());
    for (int ieta=0; ieta<Neta; ieta++) {
        double eta = etamin+ieta*deta;
        for (int iphi=0; iphi<Nphi; iphi++){
            double phi = phimin+iphi*dphi;
            double cphi = std::cos(phi), sphi = std::sin(phi);
            auto it = dpTarray[ieta][iphi];
            f << it << " ";
        }
        f << std::endl;
    }   
}


LeadingParton::LeadingParton(std::vector<double> _pTbins){
    pTbins = _pTbins;
    NpT = pTbins.size()-1;
    
    nchg.resize(NpT); for (auto & it:nchg) it=0.;
    npi.resize(NpT); for (auto & it:npi) it=0.;
    nD.resize(NpT); for (auto & it:nD) it=0.;
    nB.resize(NpT); for (auto & it:nB) it=0.;
    binwidth.resize(NpT); 
    for (int i=0; i<NpT; i++) 
        binwidth[i]=pTbins[i+1]-pTbins[i];
}
void LeadingParton::add_event(std::vector<particle> plist, double sigma_gen){
    for (auto & p : plist){
        int pid = std::abs(p.pid);
        bool ispi = pid==111 || pid == 211;
        bool ischg = pid==211 || pid==321 || pid==2212;
        bool isD = pid==411 || pid == 421 || pid == 413 || pid == 423;
        bool isB = pid==511 || pid == 521 || pid == 513 || pid == 523;
        if (std::abs(p.p.rap()) < 1.) {
            double pT = p.p.xT();
            int ii=0;
            for (int i=0; i<NpT; i++){
                if ( (pTbins[i]<pT) && (pT<pTbins[i+1]) ) {
                    ii = i;
                    break;
                }
            }
            if (ii==NpT) ii=NpT-1;
            if (ispi) npi[ii] += sigma_gen;
            if (ischg) nchg[ii] += sigma_gen;
            if (isD) nD[ii] += sigma_gen;
            if (isB) nB[ii] += sigma_gen;
        }
    }
}
void LeadingParton::write(std::string fheader){
    std::stringstream filename;
    filename << fheader << "-LeadingHadron.dat";
    std::ofstream f(filename.str());

    for (int i=0; i<NpT; i++) {
        f    << (pTbins[i]+pTbins[i+1])/2. << " "
             << pTbins[i] << " "
             << pTbins[i+1] << " "
             << nchg[i]/binwidth[i] << " "
             << npi[i]/binwidth[i] << " " 
             << nD[i]/binwidth[i] << " "
             << nB[i]/binwidth[i] << std::endl;
    }
    f.close();
}
