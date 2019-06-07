/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2019 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "WeightedAtomAverage.h"
#include "core/ActionWithArguments.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"
#include "core/Atoms.h"
#include <cmath>

using namespace std;

namespace PLMD {
namespace vatom {

void WeightedAtomAverage::registerKeywords(Keywords& keys) {
  ActionWithVirtualAtom::registerKeywords(keys);
  ActionWithValue::registerKeywords(keys); keys.remove("NUMERICAL_DERIVATIVES");
  keys.add("optional","WEIGHTS","what weights should be used when calculating the center.  If this keyword is not present the geometric center is computed. "
           "If WEIGHTS=@masses is used the center of mass is computed.  If WEIGHTS=@charges the center of charge is computed.  If "
           "the label of an action is provided PLUMED assumes that that action calculates a list of symmetry functions that can be used "
           "as weights. Lastly, an explicit list of numbers to use as weights can be provided");
  keys.addFlag("MASS",false,"calculate the center of mass");
}

WeightedAtomAverage::WeightedAtomAverage(const ActionOptions&ao):
  Action(ao),
  ActionWithVirtualAtom(ao),
  myx(0), myw(0), nspace(1), bufstart(0),
  weight_mass(false),
  weight_charge(false),
  first(true),
  val_weights(NULL)
{
  vector<AtomNumber> atoms;
  parseAtomList("ATOMS",atoms); bool usemass = false; parseFlag("MASS",usemass);
  if(atoms.size()==0) error("at least one atom should be specified");
  std::vector<std::string> str_weights; parseVector("WEIGHTS",str_weights);
  if( usemass ) {
      if( str_weights.size()>0 ) error("USEMASS is incompatible with WEIGHTS");
      str_weights.resize(1); str_weights[0]="@masses";
  }
  if( str_weights.size()==0 ) {
    log<<"  computing the geometric center of atoms:\n";
    weights.resize( atoms.size() );
    for(unsigned i=0; i<atoms.size(); i++) weights[i] = 1.;
  } else if( str_weights.size()==1 ) {
    if( str_weights[0]=="@masses" ) {
      weight_mass=true;
      log<<"  computing the center of mass of atoms:\n";
    } else if( str_weights[0]=="@charges" ) {
      weight_charge=true;
      log<<"  computing the center of charge of atoms:\n";
    } else {
      std::size_t dot=str_weights[0].find_first_of("."); unsigned nargs=0; std::vector<Value*> args;
      if( dot!=std::string::npos ) {
        ActionWithValue* action=plumed.getActionSet().selectWithLabel<ActionWithValue*>( str_weights[0].substr(0,dot) );
        if( !action ) {
          std::string str=" (hint! the actions in this ActionSet are: ";
          str+=plumed.getActionSet().getLabelList<ActionWithValue*>()+")";
          error("cannot find action named " + str_weights[0] +str);
        }
        action->interpretDataLabel( str_weights[0], this, nargs, args );
      } else {
        ActionWithValue* action=plumed.getActionSet().selectWithLabel<ActionWithValue*>( str_weights[0] );
        if( !action ) {
          std::string str=" (hint! the actions in this ActionSet are: ";
          str+=plumed.getActionSet().getLabelList<ActionWithValue*>()+")";
          error("cannot find action named " + str_weights[0] +str);
        }
        action->interpretDataLabel( str_weights[0], this, nargs, args );
      }
      if( args.size()!=1 ) error("should only have one value as input to WEIGHT");
      if( args[0]->getRank()!=1 || args[0]->getShape()[0]!=atoms.size() ) error("value input for WEIGHTS has wrong shape");
      val_weights = args[0]; std::vector<std::string> empty(1); empty[0] = (val_weights->getPntrToAction())->getLabel();
      if( (val_weights->getPntrToAction())->valuesComputedInChain() ) (val_weights->getPntrToAction())->addActionToChain( empty, this );
      log.printf("  atoms are weighted by values in vector labelled %s \n",val_weights->getName().c_str() );
    }
  } else {
    log<<" with weights:";
    if( str_weights.size()!=atoms.size() ) error("number of elements in weight vector does not match the number of atoms");
    weights.resize( atoms.size() );
    for(unsigned i=0; i<weights.size(); ++i) {
      if(i%25==0) log<<"\n";
      Tools::convert( str_weights[i], weights[i] ); log.printf(" %f",weights[i]);
    }
    log.printf("\n");
  }
  log.printf("  of atoms:");
  for(unsigned i=0; i<atoms.size(); ++i) {
    if(i>0 && i%25==0) log<<"\n";
    log.printf("  %d",atoms[i].serial());
  }
  log<<"\n";
  requestAtoms(atoms); if( val_weights ) addDependency( val_weights->getPntrToAction() ); 
}

unsigned WeightedAtomAverage::getNumberOfWeightDerivatives() const {
  if( val_weights ) return (val_weights->getPntrToAction())->getNumberOfDerivatives();
  return 0;
}

void WeightedAtomAverage::setStashIndices( unsigned& nquants ) {
  myx = nquants; myw = nquants + getNumberOfStoredQuantities(); 
  nquants += getNumberOfStoredQuantities() + 1;
} 

void WeightedAtomAverage::getSizeOfBuffer( const unsigned& nactive_tasks, unsigned& bufsize ) {
  bufstart = bufsize; if( !doNotCalculateDerivatives() ) nspace = 1 + getNumberOfDerivatives();
  unsigned ntmp_vals = getNumberOfStoredQuantities(); bufsize += (ntmp_vals + 1)*nspace;
  if( final_vals.size()!=ntmp_vals ) {
      final_vals.resize( ntmp_vals ); weight_deriv.resize( getNumberOfDerivatives() );
      final_deriv.resize( ntmp_vals ); for(unsigned i=0;i<ntmp_vals;++i) final_deriv[i].resize( getNumberOfDerivatives() );
      if( val_weights ) {
          val_deriv.resize( 3 ); val_forces.resize( (val_weights->getPntrToAction())->getNumberOfDerivatives() );
          for(unsigned i=0;i<3;++i) val_deriv[i].resize( (val_weights->getPntrToAction())->getNumberOfDerivatives() );
      }
  }
  ActionWithValue::getSizeOfBuffer( nactive_tasks, bufsize );
} 

void WeightedAtomAverage::prepareForTasks( const unsigned& nactive, const std::vector<unsigned>& pTaskList ) {
  // Check that we have the mass information we need on the first step
  if( first ) {
    // Check if we have masses if we need them
    if( weight_mass ) {
        for(unsigned i=0; i<getNumberOfAtoms(); i++) {
          if(std::isnan(getMass(i))) {
            error(
              "You are trying to compute a CENTER or COM but masses are not known.\n"
              "        If you are using plumed driver, please use the --mc option"
            );
          }
        }
    // Check we have charges if we need them
    } else if ( weight_charge && !plumed.getAtoms().chargesWereSet() ) {
        error(
            "You are trying to compute a center of charnge but chargest are not known.\n"
            "        If you are using plumed driver, please use the --mc option"
          );
    }
    first=false;
  }
  setupEntity();
}

void WeightedAtomAverage::calculate() {
  if( actionInChain() ) return;
  runAllTasks();
}

void WeightedAtomAverage::performTask( const unsigned& task_index, MultiValue& myvals ) const {
  Vector pos = getPosition( task_index ); double w;
  if( weight_mass ) {
    w = getMass(task_index);
  } else if( weight_charge ) {
    if( !plumed.getAtoms().chargesWereSet() ) plumed_merror("cannot calculate center of charge if chrages are unset");
    w = getCharge(task_index);
  } else if( val_weights && actionInChain() ) {
    w = myvals.get( val_weights->getPositionInStream() );
  } else if( val_weights ) {
    w = val_weights->get(task_index);
  } else {
    plumed_dbg_assert( task_index<weights.size() );
    w = weights[task_index];
  }
  unsigned ntmp_vals = getNumberOfStoredQuantities();
  myvals.addValue( myw, w ); compute( task_index, w, pos, myvals );
  if( !doNotCalculateDerivatives() && val_weights && fabs(w)>epsilon ) {
      double invw = 1 / w; unsigned base = 3*getNumberOfAtoms();
      unsigned istrn = val_weights->getPositionInStream();
      for(unsigned k=0; k<myvals.getNumberActive(istrn); ++k) {
          unsigned kindex = myvals.getActiveIndex(istrn,k);
          double der = myvals.getDerivative( istrn, kindex );
          for(unsigned j=0; j<ntmp_vals;++j) addDerivative( j, base+kindex, der*invw*myvals.get(myx+j), myvals );
          myvals.addDerivative( myw, base+kindex, der ); myvals.updateIndex( myw, base+kindex );
      }
  }
}

void WeightedAtomAverage::gatherForVirtualAtom( const MultiValue& myvals, std::vector<double>& buffer ) const { 
  unsigned ntmp_vals = getNumberOfStoredQuantities() + 1, bstart = bufstart;
  for(unsigned i=0;i<ntmp_vals;++i) { buffer[bstart] += myvals.get(myx+i); bstart += nspace; }

  if( !doNotCalculateDerivatives() ) {
      bstart = bufstart;
      for(unsigned i=0;i<ntmp_vals;++i) {
          for(unsigned k=0; k<myvals.getNumberActive(myx+i); ++k) {
              unsigned kindex = myvals.getActiveIndex(myx+i,k);
              plumed_dbg_assert( bstart + 1 + kindex<buffer.size() );
              buffer[bstart + 1 + kindex] += myvals.getDerivative( myx+i, kindex );
          }
          bstart += nspace;
      }
  }
}

void WeightedAtomAverage::transformFinalValueAndDerivatives( const std::vector<double>& buffer ) {
  unsigned ntmp_vals = getNumberOfStoredQuantities(); 
  double ww = buffer[bufstart + ntmp_vals*nspace];
  // This finalizes the value 
  for(unsigned i=0;i<ntmp_vals;++i) final_vals[i] = buffer[bufstart+i*nspace]/ww;
  finalizeValue( final_vals );
  if( !doNotCalculateDerivatives() ) {
      for(unsigned i=0; i<getNumberOfDerivatives(); ++ i ) {
          for(unsigned j=0; j<ntmp_vals; ++j) {
              final_deriv[j][i] = buffer[bufstart + j*nspace + 1 + i] / ww;
          }
          weight_deriv[i] = buffer[bufstart + ntmp_vals*nspace + 1 + i ] / ww;
      }
      finalizeDerivatives( final_vals, final_deriv, weight_deriv, val_deriv );
  }
}

void WeightedAtomAverage::applyForcesToValue( const std::vector<double>& fff ) {
  val_forces.assign( val_forces.size(), 0.0 );
  for(unsigned j=0;j<fff.size();++j) {
      for(unsigned k=0;k<val_deriv[j].size();++k ) val_forces[k] += fff[j]*val_deriv[j][k]; 
  }
  ActionWithArguments* aarg = dynamic_cast<ActionWithArguments*>( val_weights->getPntrToAction() );
  ActionAtomistic* aat = dynamic_cast<ActionAtomistic*>( val_weights->getPntrToAction() );
  unsigned start=0; 
  if( aarg ) aarg->setForcesOnArguments( 0, val_forces, start );
  if( aat ) aat->setForcesOnAtoms( val_forces, start ); 
}

}
}
