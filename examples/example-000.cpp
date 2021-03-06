#include <neuralfield.hpp>

using Input = double;

void fillInput(neuralfield::values_iterator begin,
	       neuralfield::values_iterator end,
	       const Input& x) {
  int i = 0;
  int N = std::distance(begin, end);
  for(; begin != end; ++begin, ++i) {
    double d2 = (i - x)*(i-x);
    *begin = exp(-d2 / (2.0 * (N/4.0)*(N/4.0)));
  }
}

int main(int argc, char* argv[]) {

  {
    // DNF 1D : testing exceptions if the graph is badly defined
    std::cout << std::string(10, '*') << std::endl;

    int N = 10;
    bool toric = true;
    bool scaling = !toric;
    
    auto net = neuralfield::network();
    auto input = neuralfield::input::input<Input>(N, fillInput, "input");
    auto g_exc = neuralfield::link::gaussian(1.5, 2., toric, scaling, N, "gexc");
    auto g_inh =  neuralfield::link::gaussian(1.3, 10., toric, scaling, N, "ginh");
    auto fu = neuralfield::function::function("sigmoid", N,"fu");
    
    g_exc->connect(fu);
    //fu->connect(g_exc);
    g_inh->connect(input);

    // Trying to init the network while 
    try {
      net->init();
    }
    catch(std::exception& e) {
      std::cout << "An exception was thrown : " << e.what() << std::endl;
    }

    fu->connect(g_exc);
    try {
      net->init();
    }
    catch(std::exception& e) {
      std::cout << "An exception was thrown : " << e.what() << std::endl;
    }
  }

  {
    std::cout << std::string(10, '*') << std::endl;

    int N = 10;
    
    auto net = neuralfield::network();
    
    neuralfield::input::input<Input>(N, fillInput, "input");
    auto input = net->get_input<Input>("input");

    auto f1 = neuralfield::function::function("sigmoid", N, "f1");
    auto f2 = neuralfield::function::function("relu", N, "f2");
    auto f12 = f1 + f2;

    f1->connect(input);
    f2->connect(input);
    
    net->init();
    //input->fill(N/2);
    net->step();
    
  }
  

  {
    // 1D network , should work fine
    std::cout << std::string(10, '*') << " 1D " << std::endl;
    
    int N = 10;
    
    auto net = neuralfield::network();

    auto input = neuralfield::input::input<Input>(N, fillInput, "input");

    bool toric = false;
    bool scaling = !toric;
    auto g_exc = neuralfield::link::gaussian(1.5, 2., toric, scaling, N, "gexc");

    auto fu = neuralfield::function::function("sigmoid", N, "fu");

    g_exc->connect(input);
    fu->connect(g_exc);

    net->print();
    
    net->init();

    net->set_input<Input>("input", N/2);
    std::cout << *(net->get("input")) << std::endl;
    for(unsigned int i = 0 ; i < 1000 ; ++i)
      net->step();
  }

  {
    // 2D network , should work fine
    std::cout << std::string(10, '*') << " 2D " << std::endl;
    
    int N = 30;
    bool toric = false;
    bool scaling = !toric;
    
    auto net = neuralfield::network();

    auto input = neuralfield::input::input<Input>(N, N, fillInput, "input");
    auto g_exc = neuralfield::link::gaussian(1.5, 2., toric, scaling, N, N, "gexc");
    auto g_inh = neuralfield::link::gaussian(1.4, 20., toric, scaling, N, N, "ginh");
    auto fu = neuralfield::function::function("sigmoid", N, N, "fu");
    auto u = neuralfield::buffered::leaky_integrator(0.01, N, N, "u");
    
    g_exc->connect(fu);
    g_inh->connect(fu);
    fu->connect(u);
    u->connect(input + g_exc + g_inh);

    net->print();
    net->init();
    
    for(unsigned int i = 0 ; i < 10 ; ++i)
      net->step();
  }
  
  
  // Easy way to define a DNF
  {
    std::cout << std::string(10, '*') << std::endl;
    
    int N = 10;
    
    auto net = neuralfield::network();

    auto input = neuralfield::input::input<Input>(N, fillInput, "input");

    bool toric = false;
    bool scaling = !toric;
    auto g_exc = neuralfield::link::gaussian(1.5, 2., toric, scaling, N);
    
    auto g_inh =  neuralfield::link::gaussian(1.3, 10., toric, scaling, N);
    
    auto fu = neuralfield::function::function("sigmoid", N);

    auto u = neuralfield::buffered::leaky_integrator(0.01, N);
    
    g_exc->connect(fu);
    g_inh->connect(fu);
    fu->connect(u);
    u->connect(input);

    net->init();
    
    for(unsigned int i = 0 ; i < 1000 ; ++i)
      net->step();
  }


  {
    int N = 10;
    // A Network is a container of all the layers
    // which will rule the evaluation of the layers
    auto net = neuralfield::network();

	
    neuralfield::input::input<Input>(N, fillInput, "input");

    // To call the fill method, you need to cast the pointer
    auto input = std::static_pointer_cast<neuralfield::input::Layer<Input>>(net->get("input"));
    input->fill(N/2);
    std::cout << "Input : " << *input << std::endl;

    bool toric = false;
    bool scaling = !toric;
	
    // We can instantiate a parametric functional layer
    // providing the parameters directly
    auto g_exc = neuralfield::link::gaussian(1.5, 2., toric, scaling, N, "gexc");
	
    auto g_inh =  neuralfield::link::gaussian(1.3, 10., toric, scaling, N, "ginh");
	
    auto fu = neuralfield::function::function("sigmoid", N, "fu");
	
    auto u = neuralfield::buffered::leaky_integrator(0.01, N);

    // We connect all the layers together
    g_exc->connect(net->get("fu"));
    g_inh->connect(fu);
    fu->connect(u);
    u->connect(input);


    net->init();
	
    for(unsigned int i = 0 ; i < 1000 ; ++i) {
      net->step();
    }
	
    std::cout << "Simulation ended" << std::endl;
    std::cout << "u : " << *u << std::endl;
  }

}
