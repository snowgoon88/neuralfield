import numpy as np
import tools
import copy
import itertools


''' Script for defining a 1D neural field 
    The equation reads:

    U(x, t+1) = U(x,t) + dt_tau * (-U(x,t) + [w o f(V)](x,t) + I(x,t) + h)

    where [f o g](x) denotes the circular convolution of f and g taken in x

    f is a heaviside : f(x) = 1_[x>=0]
    There are 2+nw free parameters : [dt_tau, h]
    with nw parameters for the weights; these weights being:

      * a DOG : w(x) = Ae exp(-x**2 / (2 * (ks*si)**2)) - ka Ae exp(-x**2 / (2 si**2))
         4 params : Ae >= 0 ; ks in [0, 1], ka in [0, 1], si > 0

      * a DOE : w(x) = Ae exp(-4 |x| / (ks*si)**2) - ka Ae exp(-4 |x| / si**2)
         4 params : Ae >= 0 ; ks in [0, 1], ka in [0, 1], si > 0

      * a DOL : w(x) = Ae [1 - |x|/(2 ks si)]^+ - ka Ae [1 - |x| / (2 si)]^+
         4 params : Ae >= 0 ; ks in [0, 1] ; ka in [0, 1] ; si > 0

      * a stepwise : w(x) = Ae 1_(|x| < ks si) - ka Ae 1_(|x| < si)   
         4 params : Ae >= 0 ; ks in [0, 1], ka in [0,1] , si > 0

    The equation is simulated synchronously, with Euler and a time step dt

    The neuralfield has a toric topology
'''

def heaviside_tf(x):
    return 0.5 * (np.sign(x) + 1)

def sigmoid_tf(x,a=1,b=-1,x0=0):
    return a / (1.0 + np.exp(b*(x-x0)))

def rectified_tf(x):
    y = x.copy()
    y[y < 0] = 0.0
    return y

def dog(dx, params):
    Ae, ke, ki, si = params
    return Ae * np.exp(-dx**2/(2.0 * (ke*si)**2)) - ki * Ae * np.exp(-dx**2/(2.0 * si**2))

def doe(dx, params): 
    Ae, ke, ki, si = params
    return Ae * np.exp(-4.0 * np.fabs(dx)/((ke*si)**2)) - ki * Ae * np.exp(-4.0 * np.fabs(dx)/(si**2))

def pos_part_s(x):
    if(x >= 0):
        return x
    else:
        return 0
pos_part = np.vectorize(pos_part_s, otypes=[np.float])

def dol(dx, params):
    Ae, ke, ki, si = params
    return Ae * pos_part(1.0 - np.fabs(dx)/(2.0 * ke*si)) - ki * Ae * pos_part(1.0 - np.fabs(dx)/(2.0 * si))

def step(dx, params):
    Ae, ke, ki, si = params
    return Ae * np.piecewise(dx, [np.fabs(dx) < ke*si], [1]) - ki * Ae * np.piecewise(dx, [np.fabs(dx) < si], [1]) #- k *ki * Ae * np.piecewise(dx, [np.fabs(dx) >= si], [1])

class DNF:
    
    def __init__(self, size, weights_name, tf):
        if(not weights_name in ["dog", "doe", "dol", "step", "optim_step"]):
            raise '''Unrecognized weight kernel, options are
            - dog : difference of gaussians
            - doe : difference of exponentiels
            - dol : difference of linear functions
            - step : step wise function
            - optim_step : step wise function with a linear complexity
            '''
        self.weights_name = weights_name
        self.dict_weights = {'dog' : dog, 'dol': dol, 'doe': doe, 'step': step, 'optim_step': step}
        if(not self.weights_name in self.dict_weights.keys()):
            raise "Unknown weight function '%s'" % self.weights_name

        self.transfer_function = tf

        self.size = size
        self.u = np.zeros(size)
        self.fu = self.transfer_function(self.u)

        self.dt_tau = 0
        self.h = 0
        self.w = np.zeros(self.size)
        self.w_params = None

    def __build_weights(self, wparams):
        self.w = np.arange(self.size[0])
        for i in range(self.size[0]):
            self.w[i] = np.min([self.w[i], self.size[0] - self.w[i]])
        self.w = self.dict_weights[self.weights_name](self.w, wparams)

    def reset(self):
        self.u = np.zeros(self.size)
        self.fu = self.transfer_function(self.u)        

    def set_params(self, params):
        self.dt_tau = params[0]
        self.h = params[1]
        self.w_params = copy.copy(params[2:])
        if self.weights_name in ["dog", "doe", "dol", "step"]:
            self.__build_weights(params[2:])
        else:
            # If weights_name == 'optim_step', we do not have to build anything
            pass

    def get_lateral_contributions(self):
        if self.weights_name in ["dog", "doe", "dol", "step"]:
            # We use the generic computation with a FFT based convolution
            return tools.cconv(self.fu, self.w) 
        elif self.weights_name == 'optim_step':
            return self.lateral_optim_step()

    def lateral_optim_step(self):
        # Reminder : the Step function is here defined as :
        #a stepwise : w(x) = Ae 1_(|x| < ke si) - ki Ae 1_(|x| < si)
        # 1 params : Ae >= 0 ; ke in [0, 1], ki in [0,1] ,  si > 0
        
        Ae, ke, ki, si = self.w_params
        se = ke * si
        Ai = ki * Ae

        lat = np.zeros(self.size)
        # We need to compute the lateral contribution at the first position 
        # with an order of N sums and 2 products            
        N = self.size[0]
        # We identify the corner points of the steps
        # which are the ones for which the lateral contribution will change as we move  
        # along a sliding window
        #          -------
        #          |     |
        # ---------=     =---------
        # On the above step function, the corner points are denoted by "="
        # In corners[0] is the leftmost, corners[1] is the rightmost
        # Mind that we use a toric topology, so that for the first unit 
        # the leftmost corner is actually on the right because of the wrap around

        # we must handle the specific cases where the excitatory or inhibitory
        # radii cover the whole field
        
        all_excitatories = se >= N-1
        if not all_excitatories:
            corners_exc = [int(np.ceil(N-se)), int(np.floor(se))]
            lat_e = Ae * (sum(itertools.islice(self.fu, 0, corners_exc[1]+1)) + sum(itertools.islice(self.fu,corners_exc[0],None)))
        else:
            lat_e = Ae * sum(self.fu)

        all_inhibitories = si >= N-1
        if not all_inhibitories:
            corners_inh = [int(np.ceil(N-si)), int(np.floor(si))]
            lat_i = Ai * (sum(itertools.islice(self.fu,0, corners_inh[1]+1)) + sum(itertools.islice(self.fu,corners_inh[0], None)))
        else:
            lat_i = Ai * sum(self.fu)

        lat[0] = lat_e - lat_i

        for i in xrange(1, N):
            # We can then compute the lateral contributions of the other locations
            # with a sliding window by just updating the contributions
            # at the "corners" of the weight function
            if not all_excitatories:
                lat_e -= Ae * self.fu[corners_exc[0]]
                corners_exc[0] += 1
                if(corners_exc[0] >= N):
                    corners_exc[0] = 0
            
                corners_exc[1] += 1
                if(corners_exc[1] >= N):
                    corners_exc[1] = 0
                lat_e += Ae * self.fu[corners_exc[1]]

            if not all_inhibitories:
                lat_i -= Ai * self.fu[corners_inh[0]]
                corners_inh[0] += 1
                if(corners_inh[0] >= N):
                    corners_inh[0] = 0
            
                corners_inh[1] += 1
                if(corners_inh[1] >= N):
                    corners_inh[1] = 0
                lat_i += Ai * self.fu[corners_inh[1]]

            lat[i] = lat_e - lat_i
        return lat

    def step(self, I):
        self.u = self.u + self.dt_tau * (-self.u + self.get_lateral_contributions()  + I + self.h)
        self.fu = self.transfer_function(self.u)

    def get_output(self):
        return self.fu.copy()

if(__name__ == '__main__'):
    params = [0.2552566456392502, -0.5031245130938667, 0.14836041727719693, 0.07709071682932468, 0.7505707917741667, 100.0]
    N = 100
    I = np.random.random((N,))

    field = DNF((N,), 'step', heaviside_tf)
    field.set_params(params)
    for i in range(100):
        field.step(I)
    output_step = field.get_output()

    field = DNF((N,), 'optim_step', heaviside_tf)
    field.set_params(params)
    for i in range(100):
        field.step(I)
    output_optim_step = field.get_output()

    import matplotlib.pyplot as plt
    
    f, axarr = plt.subplots(2, sharex=True)
    axarr[0].plot(output_step,'b')
    axarr[1].plot(output_optim_step,'r')
    plt.show()
