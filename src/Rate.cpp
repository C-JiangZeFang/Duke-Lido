#include "Rate.h"
#include "integrator.h"
#include "sampler.h"
#include "minimizer.h"
#include "random.h"
#include "approx_functions.h"
#include "matrix_elements.h"
#include "Langevin.h"
#include <iostream>

template <>
Rate<LO, 2, 2, double(*)(const double, void *)>::
	Rate(std::string Name, std::string configfile, double(*f)(const double, void *)):
StochasticBase<2>(Name+"/rate", configfile),
X(std::make_shared<Xsection<LO, 2, double(*)(const double, void *)>>(Name, configfile, f) )
{
	// read configfile
	boost::property_tree::ptree config;
	std::ifstream input(configfile);
	read_xml(input, config);

	std::vector<std::string> strs;
	boost::split(strs, Name, boost::is_any_of("/"));
	std::string model_name = strs[0];
	std::string process_name = strs[1];
	auto tree = config.get_child(model_name+"."+process_name);
	_mass = tree.get<double>("mass");
	_degen = tree.get<double>("degeneracy");
	_active = (tree.get<std::string>("<xmlattr>.status")=="active")?true:false;

	// Set Approximate function for X and dX_max
	//StochasticBase<2>::_ZeroMoment->SetApproximateFunction(approx_R22);
	//StochasticBase<2>::_FunctionMax->SetApproximateFunction(approx_dR22_max);
}

template <>
Rate<GB, 2, 2, double(*)(const double*, void *)>::
	Rate(std::string Name, std::string configfile, double(*f)(const double*, void *)):
StochasticBase<2>(Name+"/rate", configfile),
X(std::make_shared<Xsection<GB, 2, double(*)(const double*, void *)>>(Name, configfile, f) )
{
	// read configfile
	boost::property_tree::ptree config;
	std::ifstream input(configfile);
	read_xml(input, config);

	std::vector<std::string> strs;
	boost::split(strs, Name, boost::is_any_of("/"));
	std::string model_name = strs[0];
	std::string process_name = strs[1];
	auto tree = config.get_child(model_name+"."+process_name);
	_mass = tree.get<double>("mass");
	_degen = tree.get<double>("degeneracy");
	_active = (tree.get<std::string>("<xmlattr>.status")=="active")?true:false;
}

template <>
Rate<GB, 2, 4, double(*)(const double*, void *)>::
	Rate(std::string Name, std::string configfile, double(*f)(const double*, void *)):
StochasticBase<2>(Name+"/rate", configfile),
X(std::make_shared<Xsection<GB, 4, double(*)(const double*, void *)>>(Name, configfile, f) )
{
	// read configfile
	boost::property_tree::ptree config;
	std::ifstream input(configfile);
	read_xml(input, config);

	std::vector<std::string> strs;
	boost::split(strs, Name, boost::is_any_of("/"));
	std::string model_name = strs[0];
	std::string process_name = strs[1];
	auto tree = config.get_child(model_name+"."+process_name);
	_mass = tree.get<double>("mass");
	_degen = tree.get<double>("degeneracy");
	_active = (tree.get<std::string>("<xmlattr>.status")=="active")?true:false;
}

/*****************************************************************/
/*************************Sample dR ******************************/
/*****************************************************************/
/*------------------Implementation for 2 -> 2--------------------*/
template <>
void Rate<LO, 2, 2, double(*)(const double, void *)>::
		sample(std::vector<double> parameters,
			std::vector< fourvec > & final_states){
	double E = parameters[0];
	double T = parameters[1];
	double v1 = std::sqrt(1. - std::pow(_mass/E,2));
	auto dR_dxdy = [E, T, v1, this](const double * x){
		double M = this->_mass;
		double E2 = T*(std::exp(x[0])-1.), costheta = x[1];
		if (costheta > 1. || costheta < -1.) return 0.;
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sqrts = std::sqrt(s);
		double Xtot = this->X->GetZeroM({sqrts,T}).s;
		double Jacobian = E2 + T;
    	return 1./E*E2*std::exp(-E2/T)*(s-M*M)*2*Xtot/16./M_PI/M_PI*Jacobian;
	};
	bool status = true;
	auto res = sample_nd(dR_dxdy, 2, {{0., 3.}, {-1., 1.}},
						std::exp(StochasticBase<2>::GetFmax(parameters).s), status);
	if (status == false){
		final_states.resize(1);
		final_states[0] = fourvec{E, 0, 0, std::sqrt(E*E-_mass*_mass)};
		return;
	}
	double E2 = T*(std::exp(res[0])-1.),
		   costheta = res[1];
	double sintheta = std::sqrt(1. - costheta*costheta);
	double s = 2.*E2*E*(1. - v1*costheta) + _mass*_mass;
	double sqrts = std::sqrt(s);
	X->sample({sqrts, T}, final_states);

    // give incoming partilce a random phi angle
	double phi = Srandom::dist_phi(Srandom::gen);
    // com velocity
    double vcom[3] = { E2*sintheta/(E2+E)*cos(phi),
						E2*sintheta/(E2+E)*sin(phi),
						 (E2*costheta+v1*E)/(E2+E)	};
	/*
	FS now is in Z-oriented CoM frame
	1) FS.rotate_back
	2) FS.boost_back
	*/
	fourvec p1{E, 0, 0, v1*E};
	auto p1com = p1.boost_to(vcom[0], vcom[1], vcom[2]);
	for(auto & p: final_states){
		p = p.rotate_back(p1com);
		p = p.boost_back(vcom[0], vcom[1], vcom[2]);
	}
}
/*------------------Implementation for 2 -> 3--------------------*/
template <>
void Rate<GB, 2, 2, double(*)(const double*, void *)>::
		sample(std::vector<double> parameters,
			std::vector< fourvec > & final_states){
	double E = parameters[0];
	double T = parameters[1];
	double v1 = std::sqrt(1. - std::pow(_mass/E,2));
	auto dR_dxdy = [E, T, v1, this](const double * x){
		double M = this->_mass;
		double E2 = T*(std::exp(x[0])-1.), costheta = x[1];
		if (costheta > 1. || costheta < -1.) return 0.;
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sqrts = std::sqrt(s);
    	// interp cross-section
		double Xtot = this->X->GetZeroM({sqrts, T}).s;
		double Jacobian = E2 + T;
    	return 1./E*E2*std::exp(-E2/T)*(s-M*M)*2*Xtot/16./M_PI/M_PI*Jacobian;
	};
	bool status = true;
	auto res = sample_nd(dR_dxdy, 2, {{0., 3.}, {-1., 1.}}, std::exp(StochasticBase<2>::GetFmax(parameters).s), status);
	double E2 = T*(std::exp(res[0])-1.), costheta = res[1];
	double sintheta = std::sqrt(1. - costheta*costheta);
	double phi = Srandom::dist_phi(Srandom::gen);
	double cosphi = std::cos(phi), sinphi = std::sin(phi);
	double vcom[3] = { E2*sintheta/(E2+E)*cosphi,
						E2*sintheta/(E2+E)*sinphi,
						(E2*costheta+v1*E)/(E2+E) };
	double s = 2.*E2*E*(1. - v1*costheta) + _mass*_mass;
	double sqrts = std::sqrt(s);
	X->sample({sqrts, T}, final_states);

	fourvec p1{E, 0, 0, v1*E};
	auto p1com = p1.boost_to(vcom[0], vcom[1], vcom[2]);
	for(auto & p: final_states){
		p = p.rotate_back(p1com);
		p = p.boost_back(vcom[0], vcom[1], vcom[2]);
	}
}

/*------------------Implementation for 3 -> 2--------------------*/
template <>
void Rate<GB, 2, 4, double(*)(const double*, void *)>::
		sample(std::vector<double> parameters,
			std::vector< fourvec > & final_states){
	double E = parameters[0];
	double T = parameters[1];
	double M = _mass;
	double M2 = M*M;
	// sample dR
	// x are: k, E2, cosk, cos2, phi2
	auto code = [E, T, this](const double * x){
		double M = this->_mass;
		double M2 = M*M;
		double k = x[0], E2 = x[1], cosk = x[2], cos2 = x[3], phi2 = x[4];
		double sink = std::sqrt(1.-cosk*cosk), sin2 = std::sqrt(1.-cos2*cos2);
		double cosphi2 = std::cos(phi2), sinphi2 = std::sin(phi2);
		double v1 = std::sqrt(1. - M*M/E/E);
		fourvec p1mu{E, 0, 0, v1*E};
		fourvec p2mu{E2, E2*sin2*cosphi2, E2*sin2*sinphi2, E2*cos2};
		fourvec kmu{k, k*sink, 0., k*cosk};
		fourvec Ptot = p1mu+p2mu+kmu, P12 = p1mu+p2mu, P1k = p1mu+kmu;
		double s = dot(Ptot, Ptot), s12 = dot(P12, P12), s1k = dot(P1k, P1k);
		double sqrts = std::sqrt(s), sqrts12 = std::sqrt(s12), sqrts1k = std::sqrt(s1k);
		double v12[3] = { P12.x()/P12.t(), P12.y()/P12.t(), P12.z()/P12.t() };
		double xinel = (s12-M2)/(s-M2), yinel = (s1k/s-M2/s12)/(1.-s12/s)/(1.-M2/s12);
		// interp Xsection
		double Xtot = std::exp(this->X->GetZeroM({sqrts, T, xinel, yinel}).s);
		return std::exp(-(k+E2)/T)*k*E2*Xtot/E/8./std::pow(2.*M_PI, 5);
	};
	bool status = true;
	auto res = sample_nd(code, 5, {{0.0*T, 10.0*T}, {0.0*T, 10.0*T}, {-1., 1.}, {-1., 1.}, {0., 2.*M_PI}}, std::exp(StochasticBase<2>::GetFmax(parameters).s), status);
	// sample Xsection
	double k = res[0];
	double E2 = res[1];
	double cosk = res[2];
	double cos2 = res[3];
	double sink = std::sqrt(1.-cosk*cosk), sin2 = std::sqrt(1.-cos2*cos2);
	double phik = Srandom::dist_phi(Srandom::gen);// randomed phi_k
	double phi2 = res[4]+phik; // phi2 is relative to phi_k
	double cosphi2 = std::cos(phi2), sinphi2 = std::sin(phi2);
	double v1 = std::sqrt(1. - M*M/E/E);
	fourvec p1mu{E, 0, 0, v1*E};
	fourvec p2mu{E2, E2*sin2*cosphi2, E2*sin2*sinphi2, E2*cos2};
	fourvec kmu{k, k*sink*std::cos(phik), k*sink*std::sin(phik), k*cosk};
	fourvec Ptot = p1mu+p2mu+kmu, P12 = p1mu+p2mu, P1k = p1mu+kmu;
	double s = dot(Ptot, Ptot), s12 = dot(P12, P12), s1k = dot(P1k, P1k);
	double sqrts = std::sqrt(s), sqrts12 = std::sqrt(s12), sqrts1k = std::sqrt(s1k);
	double v12[3] = { P12.x()/P12.t(), P12.y()/P12.t(), P12.z()/P12.t() };
	double xinel = (s12-M2)/(s-M2), yinel = (s1k/s-M2/s12)/(1.-s12/s)/(1.-M2/s12);
    X->sample({sqrts,T,xinel,yinel}, final_states);

	// p1 and k in 12-com frame
	auto p1_in12 = p1mu.boost_to(v12[0], v12[1], v12[2]);
	auto k_in12 = kmu.boost_to(v12[0], v12[1], v12[2]);
	// |p1|
	double p1abs = std::sqrt(p1_in12.x()*p1_in12.x() + p1_in12.y()*p1_in12.y() + p1_in12.z()*p1_in12.z());
    // X-frame z-direction
	double zdir[3] = {p1_in12.x()/p1abs, p1_in12.y()/p1abs, p1_in12.z()/p1abs};
	// project k onto z-dir
	double kdotz = zdir[0]*k_in12.x() + zdir[1]*k_in12.y() + zdir[2]*k_in12.z();
	// get kperp_to-zdir
	double kperp[3] = {k_in12.x() - kdotz*zdir[0], k_in12.y() - kdotz*zdir[1], k_in12.z() - kdotz*zdir[2]};
	double kperpabs = std::sqrt(kperp[0]*kperp[0]+kperp[1]*kperp[1]+kperp[2]*kperp[2]);
	// define x-dir
	double xdir[3] = {kperp[0]/kperpabs, kperp[1]/kperpabs, kperp[2]/kperpabs};
	// y-dir = z-dir \cross x-dir
	double ydir[3] = {zdir[1]*xdir[2]-zdir[2]*xdir[1], xdir[0]*zdir[2]-xdir[2]*zdir[0], zdir[0]*xdir[1]-xdir[0]*zdir[1]};
	for (auto & p : final_states){
		// rotate it back
		double px = p.x()*xdir[0] + p.y()*ydir[0] + p.z()*zdir[0];
		double py = p.x()*xdir[1] + p.y()*ydir[1] + p.z()*zdir[1];
		double pz = p.x()*xdir[2] + p.y()*ydir[2] + p.z()*zdir[2];
		p.a[1] = px;
		p.a[2] = py;
		p.a[3] = pz;
		p = p.boost_back(v12[0], v12[1], v12[2]);
	}
	final_states.push_back(kmu);
}

/*****************************************************************/
/*************************Find dR_max ****************************/
/*****************************************************************/
/*------------------Implementation for 2 -> 2--------------------*/
template <>
scalar Rate<LO, 2, 2, double(*)(const double, void*)>::
		find_max(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	auto dR_dxdy = [E, T, this](const double * x){
		double M = this->_mass;
		double v1 = std::sqrt(1. - M*M/E/E);
		double E2 = T*(std::exp(x[0])-1.), costheta = x[1];
		if (E2 <= 0. || costheta >= 1. || costheta <= -1.) return 0.;
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sqrts = std::sqrt(s);
		double Xtot = this->X->GetZeroM({sqrts,T}).s;
		double Jacobian = E2 + T;
    	return -1./E*E2*std::exp(-E2/T)*(s-M*M)*2*Xtot/16./M_PI/M_PI*Jacobian;
	};
	// use f(E(x), y)*dE/dx, x = log(1+E/T), y = costheta
    // x start from 1, y start from 0
    // x step 0.3, cosphi step 0.3
    // save a slightly larger fmax
	auto val = -minimize_nd(dR_dxdy, 2, {1., 0.}, {0.2, 0.2}, 1000, 1e-8)*1.5;
    return scalar{std::log(val)};
}
/*------------------Implementation for 2 -> 3--------------------*/
template <>
scalar Rate<GB, 2, 2, double(*)(const double*, void*)>::
		find_max(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	auto dR_dxdy = [E, T, this](const double * x){
		double M = this->_mass;
		double v1 = std::sqrt(1. - M*M/E/E);
		double E2 = T*(std::exp(x[0])-1.), costheta = x[1];
		if (E2 <= 0. || costheta >= 1. || costheta <= -1.) return 0.;
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sqrts = std::sqrt(s);
    	// interp Xsection
		double Xtot = this->X->GetZeroM({sqrts, T}).s;
		double Jacobian = E2 + T;
    	return -1./E*E2*std::exp(-E2/T)*(s-M*M)*2*Xtot/16./M_PI/M_PI*Jacobian;
	};
	// use f(E(x), y)*dE/dx, x = log(1+E/T), y = costheta
    // x start from 1, y start from 0
    // x step 0.3, cosphi step 0.3
    // save a slightly larger fmax
	auto val = -minimize_nd(dR_dxdy, 2, {1., 0.}, {0.2, 0.2}, 1000, 1e-8)*3;
    return scalar{std::log(val)};
}
/*------------------Implementation for 3 -> 2--------------------*/
template <>
scalar Rate<GB, 2, 4, double(*)(const double*, void*)>::
		find_max(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	// x are: k, E2, cosk, cos2, phi2
	auto code = [E, T, this](const double * x){
		double M = this->_mass;
		double M2 = M*M;
		double k = x[0], E2 = x[1], cosk = x[2], cos2 = x[3], phi2 = x[4];
		if (k<0.01*T || E2<0.01*T || k>8*T || E2>8*T || std::abs(cosk)>.999|| std::abs(cos2)>.999|| phi2<0. || phi2 > 2.*M_PI) return 0.;
		double sink = std::sqrt(1.-cosk*cosk), sin2 = std::sqrt(1.-cos2*cos2);
		double cosphi2 = std::cos(phi2), sinphi2 = std::sin(phi2);
		double v1 = std::sqrt(1. - M*M/E/E);
		fourvec p1mu{E, 0, 0, v1*E};
		fourvec p2mu{E2, E2*sin2*cosphi2, E2*sin2*sinphi2, E2*cos2};
		fourvec kmu{k, k*sink, 0., k*cosk};
		fourvec Ptot = p1mu+p2mu+kmu, P12 = p1mu+p2mu, P1k = p1mu+kmu;
		double s = dot(Ptot, Ptot), s12 = dot(P12, P12), s1k = dot(P1k, P1k);
		double sqrts = std::sqrt(s), sqrts12 = std::sqrt(s12), sqrts1k = std::sqrt(s1k);
    	double v12[3] = { P12.x()/P12.t(), P12.y()/P12.t(), P12.z()/P12.t() };
		double xinel = (s12-M2)/(s-M2), yinel = (s1k/s-M2/s12)/(1.-s12/s)/(1.-M2/s12);
    	// interp Xsection
		double Xtot = std::exp(this->X->GetZeroM({sqrts, T, xinel, yinel}).s);
		return std::exp(-(k+E2)/T)*k*E2*Xtot/E/8./std::pow(2.*M_PI, 5);
	};
	auto loc = MC_maximize(code, 5,
				{{T, T*2}, {T, T*2},{-.5, .5}, {-0.5, 0.5}, {1.5,2.}}, 500);
	double xloc[5] = {loc[0],loc[1],loc[2],loc[3],loc[4]};
	auto val = code(xloc)*2.;
	return scalar{std::log(val)};
}


/*****************************************************************/
/*************************Integrate dR ***************************/
/*****************************************************************/
/*------------------Implementation for 2 -> 2--------------------*/
template <>
scalar Rate<LO, 2, 2, double(*)(const double, void*)>::
		calculate_scalar(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	auto code = [E, T, this](const double * x){
		double M = this->_mass;
		double v1 = std::sqrt(1. - M*M/E/E);
		double E2 = x[0], costheta = x[1];
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sqrts = std::sqrt(s);
		double Xtot = this->X->GetZeroM({sqrts,T}).s;
    	std::vector<double> res{1./E*E2*std::exp(-E2/T)*(s-M*M)*2*Xtot/16./M_PI/M_PI};
		return res;
	};
	double xmin[2] = {0., -1.};
	double xmax[2] = {10.*T,1.};
	double err;
	auto val = quad_nd(code, 2, 1, xmin, xmax, err);
	return scalar{_degen*val[0]};
}
/*------------------Implementation for 2 -> 3--------------------*/
// Pure Gunion bertsch
template <>
scalar Rate<GB, 2, 2, double(*)(const double*, void*)>::
		calculate_scalar(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	auto code = [E, T, this](const double * x){
		double M = this->_mass;
		double v1 = std::sqrt(1. - M*M/E/E);
		double E2 = x[0], costheta = x[1];
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sqrts = std::sqrt(s);
    	// interp Xsection
		double Xtot = this->X->GetZeroM({sqrts, T}).s;
    	std::vector<double> res{1./E*E2*std::exp(-E2/T)*(s-M*M)*2*Xtot/16./M_PI/M_PI};
		return res;
	};
	double xmin[2] = {0., -1.};
	double xmax[2] = {10.*T,1.};
	double err;
	auto val = quad_nd(code, 2, 1, xmin, xmax, err);
	return scalar{_degen*val[0]};
}
/*------------------Implementation for 3 -> 2--------------------*/

template <>
scalar Rate<GB, 2, 4, double(*)(const double*, void*)>::
		calculate_scalar(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	// x are: k, E2, cosk, cos2, phi2
	auto code = [E, T, this](const double * x){
		double M = this->_mass;
		double M2 = M*M;
		double k = x[0], E2 = x[1], cosk = x[2], cos2 = x[3], phi2 = x[4];
		double sink = std::sqrt(1.-cosk*cosk), sin2 = std::sqrt(1.-cos2*cos2);
		double cosphi2 = std::cos(phi2), sinphi2 = std::sin(phi2);
		double v1 = std::sqrt(1. - M*M/E/E);
		fourvec p1mu{E, 0, 0, v1*E};
		fourvec p2mu{E2, E2*sin2*cosphi2, E2*sin2*sinphi2, E2*cos2};
		fourvec kmu{k, k*sink, 0., k*cosk};
		fourvec Ptot = p1mu+p2mu+kmu, P12 = p1mu+p2mu, P1k = p1mu+kmu;
		double s = dot(Ptot, Ptot), s12 = dot(P12, P12), s1k = dot(P1k, P1k);
		double sqrts = std::sqrt(s), sqrts12 = std::sqrt(s12), sqrts1k = std::sqrt(s1k);
    	double v12[3] = { P12.x()/P12.t(), P12.y()/P12.t(), P12.z()/P12.t() };
		double xinel = (s12-M2)/(s-M2), yinel = (s1k/s-M2/s12)/(1.-s12/s)/(1.-M2/s12);
    	// interp Xsection
		double Xtot = std::exp(this->X->GetZeroM({sqrts, T, xinel, yinel}).s);
		return std::exp(-(k+E2)/T)*k*E2*Xtot/E/8./std::pow(2.*M_PI, 5);
	};
	double xmin[5] = {0.,   0.,  -1., -1, 0.};
	double xmax[5] = {10*T, 10*T, 1., 1., 2.*M_PI};
	double error;
	double res = vegas(code, 5, xmin, xmax, error);
	return scalar{_degen*res};
}

/*****************************************************************/
/*******************Integrate Delta_p^mu*dR **********************/
/*****************************************************************/
/*------------------No need to do this generally-----------------*/
template <char const *str, size_t N1, size_t N2, typename F>
fourvec Rate<str, N1, N2, F>::calculate_fourvec(std::vector<double> parameters){
	return fourvec::unity();
}
/*------------------Implementation for 2 -> 2--------------------*/
template <>
fourvec Rate<LO, 2, 2, double(*)(const double, void*)>::
		calculate_fourvec(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	auto code = [E, T, this](const double * x){
		double M = this->_mass;
		double v1 = std::sqrt(1. - M*M/E/E);
		double E2 = x[0], costheta = x[1];
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sintheta = std::sqrt(1. - costheta*costheta);
		double sqrts = std::sqrt(s);
		double vcom[3] = {E2*sintheta/(E2+E), 0., (E2*costheta+v1*E)/(E2+E)};
		fourvec p1{E, 0, 0, v1*E};
		// A vector in p1z(com)-oriented com frame
		auto fmu0 = this->X->GetFirstM({sqrts,T});
		// rotate it back from p1z(com)-oriented com frame
		auto fmu1 = fmu0.rotate_back(p1.boost_to(vcom[0], vcom[1], vcom[2]));
		// boost back to the matter frame
		auto fmu2 = fmu1.boost_back(vcom[0], vcom[1], vcom[2]);
		double common = 1./E*E2*std::exp(-E2/T)*(s-M*M)*2./16./M_PI/M_PI;
		fmu2 = fmu2 * common;
		// Set tranverse to zero due to azimuthal symmetry;
		std::vector<double> res{fmu2.t(), fmu2.z()};
		return res;
	};
	double xmin[2] = {0., -1.};
	double xmax[2] = {10.*T, 1.};
	double err;
	auto val = quad_nd(code, 2, 2, xmin, xmax, err);
	return fourvec{_degen*val[0], 0.0, 0.0, _degen*val[1]};
}

/*****************************************************************/
/***********Integrate Delta_p^mu*Delta_p^mu*dR *******************/
/*****************************************************************/
/*------------------No need to do this generally-----------------*/
template <char const *str, size_t N1, size_t N2, typename F>
tensor Rate<str, N1, N2, F>::calculate_tensor(std::vector<double> parameters){
	return tensor::unity();
}
/*------------------Implementation for 2 -> 2--------------------*/
template <>
tensor Rate<LO, 2, 2, double(*)(const double, void*)>::
		calculate_tensor(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
	auto code = [E, T, this](const double * x){
		double M = this->_mass;
		double v1 = std::sqrt(1. - M*M/E/E);
		double E2 = x[0], costheta = x[1], phi = x[2];
		double s = 2.*E2*E*(1. - v1*costheta) + M*M;
		double sintheta = std::sqrt(1. - costheta*costheta);
		double sqrts = std::sqrt(s);
		double vcom[3] = {E2*sintheta/(E2+E)*cos(phi), E2*sintheta/(E2+E)*sin(phi),
						 (E2*costheta+v1*E)/(E2+E)};
		fourvec p1{E, 0, 0, v1*E};
		// A vector in p1z(com)-oriented com frame
		auto fmunu0 = this->X->GetSecondM({sqrts,T});
		// rotate it back from p1z(com)-oriented com frame
		auto fmunu1 = fmunu0.rotate_back(p1.boost_to(vcom[0], vcom[1], vcom[2]));
		// boost back to the matter frame
		auto fmunu2 = fmunu1.boost_back(vcom[0], vcom[1], vcom[2]);
		double common = 1./E*E2*std::exp(-E2/T)*(s-M*M)*2./32./std::pow(M_PI, 3);
		fmunu2 = fmunu2 * common;
		// Set tranverse to zero due to azimuthal symmetry;
		std::vector<double> res{fmunu2.T[0][0], fmunu2.T[1][1],
								fmunu2.T[2][2], fmunu2.T[3][3]};
		return res;
	};
	double xmin[3] = {0., -1., -M_PI};
	double xmax[3] = {10.*T, 1., M_PI};
	double err;
	auto val = quad_nd(code, 3, 4, xmin, xmax, err);
	return tensor{_degen*val[0], 0., 0., 0.,
				  0., _degen*val[1], 0., 0.,
				  0., 0., _degen*val[2], 0.,
				  0., 0., 0., _degen*val[3]};
}


//Diff-1-to-2 Rate
template<>
EffRate12<2, double(*)(const double*, void *)>::
    EffRate12(std::string Name, std::string configfile, double(*f)(const double *, void *)):
StochasticBase<2>(Name+"/rate", configfile),
_f(f)
{
   // read configfile
   boost::property_tree::ptree config;
   std::ifstream input(configfile);
   read_xml(input, config);

   std::vector<std::string> strs;
   boost::split(strs, Name, boost::is_any_of("/"));
   std::string model_name = strs[0];
   std::string process_name = strs[1];
   auto tree = config.get_child(model_name + "." + process_name);
   _mass = tree.get<double>("mass");
   _active = (tree.get<std::string>("<xmlattr>.status")=="active")?true:false;
}

// Sample Final states
template<>
void EffRate12<2, double(*)(const double*, void *)>::
    sample(std::vector<double> parameters, std::vector< fourvec > & final_states){
    double E = parameters[0];
    double T = parameters[1];
    double pabs = std::sqrt(E*E - _mass*_mass);

    auto dR_dxdy = [E, T, this](const double *x){
        double params[3] = {E, T, this->_mass};
        double result = this->_f(x, params);
        return result;
    };

    bool status=true;
    auto res = sample_nd(dR_dxdy, 2, {{0.,1.},{0.,1.}}, std::exp(StochasticBase<2>::GetFmax(parameters).s), status);

	
	double x = res[0], y = res[1];
	double k0 = x*E;
	double kT = x*y*E;
	double phi = Srandom::dist_phi(Srandom::gen);
	double kx = kT*std::cos(phi), ky = kT*std::sin(phi);
	double kz = std::sqrt(k0*k0-kT*kT);
	double pznew = pabs-kz;
	double Enew = std::sqrt(pznew*pznew + kT*kT + _mass*_mass);

	final_states.resize(2);
    final_states[0] = fourvec{Enew, -kx, -ky, pznew};
    final_states[1] = fourvec{k0, kx, ky, kz};

    return;
}

// Find the max of dR/dx/dy
template <>
scalar EffRate12<2, double(*)(const double*, void*)>::
		find_max(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
    // First, use Monte Carlo to find local maxima region (kind of crucial here)
    auto dR_dxdy = [E, T, this](const double * x){
        // if out of physics range, please return 0.
        if (x[0] <= 0 || x[0] >= 1  || x[1] <=0 || x[1] >= 1)
            return 0.;
        double params[3] = {E, T, this->_mass};
        double result = this->_f(x, params);
        return result;
    };
    auto loc = MC_maximize(dR_dxdy, 2, {{0,1}, {0,1}}, 2000);
    double xloc[2] = {loc[0],loc[1]};
   
    return scalar{std::log(dR_dxdy(xloc)*2)};
}


// Calculate Rate Here! qhat not included yet
template <>
scalar EffRate12<2, double(*)(const double*, void*)>::
		calculate_scalar(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];

	// dR/dx/dy to be intergated
	auto dR_dxdy = [E, T, this](const double * x){
        double params[3] = {E, T, this->_mass};
        double result = this->_f(x, params);
        std::vector<double> res{result};
        return result;
	};
	double xmin[2] = {0., 0.};
	double xmax[2] = {1., 1.};
	double err;
    auto val = vegas(dR_dxdy, 2, xmin, xmax, err, 5000); 
	return scalar{val};
}


template <size_t N, typename F>
fourvec EffRate12<N, F>::calculate_fourvec(std::vector<double> parameters){
	return fourvec::unity();
}
template <size_t N, typename F>
tensor EffRate12<N, F>::calculate_tensor(std::vector<double> parameters){
	return tensor::unity();
}


//EffRate21 constructor
template<>
EffRate21<2, double(*)(const double*, void *)>::
    EffRate21(std::string Name, std::string configfile, double(*f)(const double *, void *)):
StochasticBase<2>(Name+"/rate", configfile),
_f(f)
{
    // read configfile
    boost::property_tree::ptree config;
    std::ifstream input(configfile);
    read_xml(input, config);

    std::vector<std::string> strs;
    boost::split(strs, Name, boost::is_any_of("/"));
    std::string model_name = strs[0];
    std::string process_name = strs[1];
    auto tree = config.get_child(model_name + "." + process_name);
    _mass = tree.get<double>("mass");
    _active = (tree.get<std::string>("<xmlattr>.status") == "active") ? true: false;
}

// Sample Final status
template<>
void EffRate21<2, double(*)(const double*, void *)>::
    sample(std::vector<double> parameters, std::vector< fourvec > & final_states){
    double E = parameters[0];
    double T = parameters[1];
    double pabs = std::sqrt(E*E - _mass*_mass);

    auto dR_dxdy = [E, T, this](const double *x){
        double params[3] = {E, T, this->_mass};
        double result = this->_f(x, params);
        return result;
    };

    bool status=true;
    auto res = sample_nd(dR_dxdy, 2, {{-.99999, .99999}, {0., 1.}}, StochasticBase<2>::GetFmax(parameters).s, status);

	double xp = res[0], y = res[1];
	double x = std::atanh(xp);
	double kz = x*pabs;
	double k0 = std::abs(kz)/std::sqrt(1.01-y*y);
	double kT = y*k0;
	//LOG_INFO << xp << " " << y << " " << x << " " << kz << " " << kT; 
	double phi = Srandom::dist_phi(Srandom::gen);
	double kx = kT*std::cos(phi), ky = kT*std::sin(phi);
	double pznew = pabs+kz;
	double Enew = std::sqrt(pznew*pznew + kT*kT + _mass*_mass);

	final_states.resize(2);
    final_states[0] = fourvec{Enew, kx, ky, pznew};
    final_states[1] = fourvec{k0, kx, ky, kz};

    return ;
}


// Find the max of dR/dx/dy
template <>
scalar EffRate21<2, double(*)(const double*, void*)>::
		find_max(std::vector<double> parameters){
	double E = parameters[0];
	double T = parameters[1];
    auto dR_dxdy = [E, T, this](const double * x){
        // if out of physics range, please return 0.
        if (x[0] >= .99999 || x[0] <= 0. || x[1] <=0 || x[1] >= 1)
            return 0.;
        double params[4] = {E, T, this->_mass};
        double result = this->_f(x, params);
        return result;
    };
    auto loc = MC_maximize(dR_dxdy, 2, {{0,1}, {0,1}}, 1000);
    double xloc[2] = {loc[0],loc[1]};
    
    return scalar{dR_dxdy(xloc)*2};
}




// Calculate the Rate here! qhat not included yet
template<>
scalar EffRate21<2, double(*)(const double*, void*)>::
        calculate_scalar(std::vector<double> parameters){
    double E = parameters[0];
    double T = parameters[1];

    auto dR_dxdy = [E, T, this](const double *x){
        double params[3] = {E, T, this->_mass};
        double result = this->_f(x, params);
        std::vector<double> res{result};
        return result;
    };

    double xmin[2] = {-.99999, 0.};
    double xmax[2] = {.99999, 1.};
    double err;

    auto val = vegas(dR_dxdy, 2, xmin, xmax, err, 10000);
    return scalar{val};
}

template<size_t N, typename F>
fourvec EffRate21<N, F>::calculate_fourvec(std::vector<double> parameters){
    return fourvec::unity();
}
template<size_t N, typename F>
tensor EffRate21<N, F>::calculate_tensor(std::vector<double> parameters){
    return tensor::unity();    
}


// For 2->2 with leading order pQCD
template class Rate<LO,2,2,double(*)(const double, void*)>; 
// For 2->3 with Gunion-Bertsch w/o LPM
template class Rate<GB,2,2,double(*)(const double*, void*)>; 
// For 3->2 with Gunion-Bertsch w/o LPM
template class Rate<GB,2,4,double(*)(const double*, void*)>;
// For 1->2 and 2->1 induced by diffusion
template class EffRate12<2,double(*)(const double*, void*)>; // 1->2 
template class EffRate21<2,double(*)(const double*, void*)>; // 2->1 
