#include "DistributionFunctions.h"

namespace PLMD {

//+PLUMEDOC MODIFIER SUM
/**

Calculate the sum of all the colvars in the distribution.  Once calculated the final value is referenced
using lable.sum.

*/
//+ENDPLUMEDOC


sum::sum( const std::vector<std::string>& parameters ) :
DistributionFunction(parameters)
{
  plumed_massert(parameters.size()==0,"parameters should have zero size");
}

std::string sum::message(){
  std::ostringstream ostr;
  ostr<<"the sum of all the values"; 
  return ostr.str();
}

double sum::calculate( Value* value_in, std::vector<Value>& aux, Value* value_out ){
  copyDerivatives( value_in, value_out ); value_out->set( value_in->get() );
  return value_in->get();
}

void sum::finish( const double& p, Value* value_out ){
  value_out->set(p);
}

}
