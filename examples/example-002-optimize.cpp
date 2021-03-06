#include "neuralfield.hpp"
#include "optimization-scenario.hpp"
#include <iostream>

#include "rng_generators.h"
typedef popot::rng::CRNG RNG_GENERATOR;

#include "popot.h"
typedef popot::algorithm::ParticleStochasticSPSO::VECTOR_TYPE TVector;



void fillInput(neuralfield::values_iterator begin,
	       neuralfield::values_iterator end,
	       const Input& x) {
  std::copy(x.begin(), x.end(), begin);
}

double evaluate(unsigned int nb_steps,
		double sigma,
		double dsigma,
		std::vector<int> shape,
		std::shared_ptr<neuralfield::Network> net,
		double * params) {

  // Set the parameters of the field
  // params = [dttau     h,  Ap,   sm, ka, ks]

  double dt_tau = params[0];
  double h = params[1];
  double Ap = params[2];
  double sm = params[3];
  double ka = params[4];
  double ks = params[5];
  
  double Am = ka * Ap;
  double sp = ks * sm;
  
  net->get("gexc")->set_parameters({Ap, sp});
  net->get("ginh")->set_parameters({Am, sm});
  net->get("h")->set_parameters({h});
  net->get("u")->set_parameters({dt_tau});

  bool toric_fitness = false;
  
  // Test the net on the different scenarii
  auto s1 = RandomCompetition(nb_steps, shape, sigma, dsigma, toric_fitness);
  double f1 = s1.evaluate(net);

  auto s2 = StructuredCompetition(nb_steps, shape, sigma, dsigma, toric_fitness, 5, 1./5.);
  double f2 = s2.evaluate(net);
  
  return f1 + f2;
}


void test(unsigned int nb_steps,
	  double sigma,
	  double dsigma,
	  std::vector<int> shape,
	  std::shared_ptr<neuralfield::Network> net,
	  double * params) {


  std::cout << "Testing" << std::endl;
  
  double  dt_tau = params[0];
  double baseline = params[1];
  double Ap = params[2];
  double sm = params[3];
  double ka = params[4];
  double ks = params[5];
  
  double Am = ka * Ap;
  double sp = ks * sm;
  
  net->get("gexc")->set_parameters({Ap, sp});
  net->get("ginh")->set_parameters({Am, sm});
  net->get("h")->set_parameters({baseline});
  net->get("u")->set_parameters({dt_tau});
  
  int size = 1;
  for(auto s: shape)
    size *= s;

  if(shape.size() == 1) {
    std::cout << "Scaling factors gexc" << std::endl;
    for(int i = 0; i < size; ++i)
      std::cout << std::static_pointer_cast<neuralfield::link::Gaussian>(net->get("gexc"))->_scaling_factors[i] << " ";
    std::cout << std::endl;
    
    std::cout << "Scaling factors ginh" << std::endl;
    for(int i = 0; i < size; ++i)
      std::cout << std::static_pointer_cast<neuralfield::link::Gaussian>(net->get("ginh"))->_scaling_factors[i] << " ";
    std::cout << std::endl;
  }
  else if(shape.size() == 2) {
    std::cout << "Scaling factors gexc" << std::endl;

    for(int i = 0; i < shape[1]; ++i) {
      for(int j = 0 ; j < shape[0]; ++j) {
	std::cout << std::static_pointer_cast<neuralfield::link::Gaussian>(net->get("gexc"))->_scaling_factors[i*shape[0] + j] << " ";
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Scaling factors ginh" << std::endl;

    for(int i = 0; i < shape[1]; ++i) {
      for(int j = 0 ; j < shape[0]; ++j) {
	std::cout << std::static_pointer_cast<neuralfield::link::Gaussian>(net->get("ginh"))->_scaling_factors[i*shape[0] + j] << " ";
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
    
  }
  
  
  bool toric_fitness = true;
  auto s1 = RandomCompetition(nb_steps, shape, sigma, dsigma, toric_fitness);

  std::cout << "Fitnesses " << std::endl;
  for(unsigned int i = 0 ; i < 10 ; ++i) {
    auto f1 = s1.evaluate(net);
    std::cout << f1 << " ";
  }
  std::cout << std::endl;

  // Export the input/output and templates of the last run
  auto input = net->get("input");
  auto fu = net->get("fu");
   
  std::ofstream out_input, out_fu;
  out_input.open("input.data");
  out_fu.open("fu.data");
  
  auto it_input = input->begin();
  auto it_fu = fu->begin();

  if(shape.size() == 1) {
    for(int i = 0 ; i < shape[0] ; ++i, ++it_input, ++it_fu) {
      out_input << *it_input << std::endl;
      out_fu << *it_fu << std::endl;
    }
  }
  else if(shape.size() == 2) {
    for(int i = 0 ; i < shape[0] ; ++i) {
      for(int j = 0 ; j < shape[1]; ++j, ++it_input, ++it_fu) {
	out_input << *it_input << std::endl;
	out_fu << *it_fu << std::endl;
      }
      out_input << std::endl;
      out_fu << std::endl;
    }
  }

  out_fu.close();
  out_input.close();
  
  s1.dump_bounds();
  std::cout << "The input, fu are dumped in input.data and fu.data"  << std::endl;
  std::cout << "You can use gnuplot e.g. to plot them  :" << std::endl;
  if(shape.size() == 1) {
  std::cout << "     plot \"input.data\" w l , \"fu.data\" w l, \"lb_bound.data\" w l, \"ub_bound.data\" w l " << std::endl;
  }
  else if(shape.size() == 2) {
  std::cout << "     splot \"input.data\" w l , \"fu.data\" w l, \"lb_bound.data\" w l, \"ub_bound.data\" w l " << std::endl;
  }
    
}

int main(int argc, char * argv[]) {

  if(argc != 6 and argc != 7) {
    std::cerr << "Script to optimize a 2D neural field for a competition scenario" << std::endl;
    std::cerr << "Usage : " << argv[0] << " sigma dsigma toric scale N <M>" << std::endl;
    std::exit(-1);
  }

  RNG_GENERATOR::rng_srand();
  RNG_GENERATOR::rng_warm_up();
  
  double dt_tau = 0.01;
  double baseline = 0.0;
  double Ap = 1.5;
  double sp = 2.;
  double Am = -1.3;
  double sm = 10.;
  bool toric = std::atoi(argv[3]);
  bool scale = std::atoi(argv[4]);
  unsigned int Nsteps = 100;

  double sigma = std::atof(argv[1]);
  double dsigma = std::atof(argv[2]);

  std::vector<int> shape;

  shape.push_back(std::atoi(argv[5]));
  if(argc == 7)
    shape.push_back(std::atoi(argv[6]));
  
  auto input = neuralfield::input::input<Input>(shape, fillInput, "input");

  auto h = neuralfield::function::constant(baseline, shape, "h");
  auto u = neuralfield::buffered::leaky_integrator(dt_tau, shape, "u");
  auto g_exc = neuralfield::link::gaussian(Ap, sp, toric, scale, shape,"gexc");
  auto g_inh =  neuralfield::link::gaussian(Am, sm, toric, scale, shape, "ginh");
  auto fu = neuralfield::function::function("sigmoid", shape, "fu");

  g_exc->connect(fu);
  g_inh->connect(fu);
  fu->connect(u);
  u->connect(g_exc + g_inh + input + h);
  
  auto net = neuralfield::get_current_network();
  net->print();

  net->init();

  // Parametrization of popot
  
  const unsigned int Nparams = 6;

  //                                  dttau     h,  Ap,   sm, ka, ks 
  std::array<double, Nparams> lbounds({0.01, -5.0, 0.01,  0.0001, -1., 0.001});
  std::array<double, Nparams> ubounds({1.00,  5.0, 10000.0,   3.0, -0.0001, 1.});
  auto lbound = [lbounds] (size_t index) -> double { return lbounds[index];};
  auto ubound = [ubounds] (size_t index) -> double { return ubounds[index];};
  
  auto stop =   [] (double fitness, int epoch) -> bool { return epoch >= 1000 || fitness <= 1e-5;};
  
  auto cost_function = [Nsteps, shape, net, sigma, dsigma] (TVector& pos) -> double { 
    return evaluate(Nsteps, sigma, dsigma, shape, net, pos.getValuesPtr());
  };
  
  auto algo = popot::algorithm::stochastic_montecarlo_spso2006(Nparams, 
							       lbound, 
							       ubound, 
							       stop, 
							       cost_function, 
							       1);
    
  // We run the algorithm
  algo->run(1);
  
  std::cout << "Best particle :" << algo->getBest() << std::endl;

  auto best_params = algo->getBest().getPosition().getValuesPtr();
  test(Nsteps, sigma, dsigma, shape, net, best_params);

  std::cout << "Parameters : " << std::endl;
  std::cout << "  dt_tau : " << best_params[0] << std::endl;
  std::cout << "  h      : " << best_params[1] << std::endl;
  std::cout << "  Ap     : " << best_params[2] << std::endl;
  std::cout << "  sp     : " << best_params[3]*best_params[5] << std::endl;
  std::cout << "  Am     : " << best_params[2]*best_params[4] << std::endl;
  std::cout << "  sm     : " << best_params[3] << std::endl;

  std::cout << std::endl;
  std::cout << " To test it : " << std::endl;
  std::cout << " ./examples/example-002-test "
	    <<  best_params[0] << " "
	    <<  best_params[1] << " "
	    <<  best_params[2] << " "
	    <<  best_params[3]*best_params[5] << " "
	    <<  best_params[2]*best_params[4] << " "
	    <<  best_params[3] << " "
	    <<  int(toric) << " "
	    <<  int(scale) << " ";
  for(auto& s: shape)
    std::cout << s << " ";
  std::cout << std::endl;
    
  return 0;
}
