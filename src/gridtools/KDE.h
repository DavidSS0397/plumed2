/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2017 The plumed team
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
#ifndef __PLUMED_gridtools_KDE_h
#define __PLUMED_gridtools_KDE_h

#include "HistogramBase.h"
#include "tools/KernelFunctions.h"
#include "tools/HistogramBead.h"

namespace PLMD {
namespace gridtools {

class KDE : public HistogramBase {
private:
  KernelFunctions* kernel;
  std::string kerneltype;
  bool firststep;
  bool ignore_out_of_bounds;
  double cheight;
  std::vector<double> cval;
  std::vector<unsigned> nbin, nneigh;
  std::vector<std::string> gmin, gmax;
  std::vector<double> min, max, gspacing, bandwidth;
  void setupHistogramBeads( std::vector<HistogramBead>& bead ) const ;
  double evaluateBeadValue( std::vector<HistogramBead>& bead, const std::vector<double>& gpoint, const std::vector<double>& args,
                            const double& height, std::vector<double>& der ) const ;
  KernelFunctions* setupValuesAndKernel( const std::vector<double>& args, const double& height, std::vector<Value*>& vv ) const ;
public:
  static void registerKeywords( Keywords& keys );
  explicit KDE(const ActionOptions&ao);
  ~KDE();
  void setupNeighborsVector();
  void getInfoForGridHeader( std::string& gtype, std::vector<std::string>& argn, std::vector<std::string>& min,
                             std::vector<std::string>& max, std::vector<unsigned>& out_nbin,
                             std::vector<double>& spacing, std::vector<bool>& pbc, const bool& dumpcube ) const ;
  void completeGridObjectSetup();
  void buildSingleKernel( std::vector<unsigned>& tflags, const double& height, std::vector<double>& args );
  double calculateValueOfSingleKernel( const std::vector<double>& args, std::vector<double>& der ) const ;
  void addKernelToGrid( const double& height, const std::vector<double>& args, const unsigned& bufstart, std::vector<double>& buffer ) const ;
  void addKernelForces( const unsigned& heights_index, const unsigned& itask, const std::vector<double>& args, const double& height, std::vector<double>& forces ) const ;
};

}
}
#endif
