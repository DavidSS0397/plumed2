/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2014 The plumed team
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
#include "FlexibleBin.h"
#include "ActionWithArguments.h"
#include <cmath>
#include <iostream>
#include <vector>
#include "tools/Matrix.h"

using namespace std;
namespace PLMD{


FlexibleBin::FlexibleBin(int type, ActionWithArguments *paction,  double const &d , vector<double> &smin, vector<double> &smax):type(type),paction(paction),sigma(d),sigmamin(smin),sigmamax(smax){
	// initialize the averages and the variance matrices
	if(type==diffusion){
		unsigned ncv=paction->getNumberOfArguments();	
		vector<double> average(ncv*(ncv+1)/2);
		vector<double> variance(ncv*(ncv+1)/2);
	}
	paction->log<<"  Limits for sigmas using adaptive hills:  \n"; 
	for(unsigned i=0;i<paction->getNumberOfArguments();++i){
		paction->log<<"   CV  "<<paction->getPntrToArgument(i)->getName()<<":\n";
		if(sigmamin[i]>0.){
				limitmin.push_back(true);
				paction->log<<"       Min "<<sigmamin[i];	
				sigmamin[i]*=sigmamin[i];	// this is because the matrix which is calculated is the sigmasquared	
			}else{
				limitmin.push_back(false);
				paction->log<<"       Min No ";	
		}
		if(sigmamax[i]>0.){
				limitmax.push_back(true);
				paction->log<<"       Max "<<sigmamax[i];	
				sigmamax[i]*=sigmamax[i];		
			}else{
				limitmax.push_back(false);
				paction->log<<"       Max No ";	
		}
		paction->log<<" \n";	
	}

}
/// Update the flexible bin 
/// in case of diffusion based: update at every step
/// in case of gradient based: update only when you add the hill 
void FlexibleBin::update(bool nowAddAHill){
	unsigned ncv=paction->getNumberOfArguments();
	unsigned dimension=ncv*(ncv+1)/2;	
	// this is done all the times from scratch. It is not an accumulator 
	unsigned  k=0;
	unsigned i,j;
	vector<double> cv;
	vector<double> delta;
	// if you use this below then the decay is in time units
	//double decay=paction->getTimeStep()/sigma;
	// to be consistent with the rest of the program: everything is better to be in timesteps
	double decay=1./sigma;
	// here update the flexible bin according to the needs	
	switch (type){
	// This should be called every time
	case diffusion:
		//
		// THE AVERAGE VALUE
		//
		// beware: the pbc
		delta.resize(ncv);
		for(i=0;i<ncv;i++)cv.push_back(paction->getArgument(i));
		if(average.size()==0){ // initial time: just set the initial vector
			average.resize(ncv);
			for(i=0;i<ncv;i++)average[i]=cv[i];
		}else{ // accumulate
			for(i=0;i<ncv;i++){
				delta[i]=paction->difference(i,average[i],cv[i]);
				average[i]+=decay*delta[i];
				average[i]=paction->bringBackInPbc(i,average[i]); // equation 8 of "Metadynamics with adaptive Gaussians"
			}

		}
		//
		// THE VARIANCE
		//
		if(variance.size()==0){
			variance.resize(dimension,0.); // nonredundant members dimension=ncv*(ncv+1)/2;
		}else{
			k=0;
			for(i=0;i<ncv;i++){
				for(j=i;j<ncv;j++){ // upper diagonal loop
					variance[k]+=decay*(delta[i]*delta[j]-variance[k]);
					k++;
				}
			}
		}
		break;
	case geometry:
		//
		//this calculates in variance the \nabla CV_i \dot \nabla CV_j
		//
		variance.resize(dimension);
		//cerr<< "Doing geometry "<<endl;
		// now the signal for retrieving the gradients should be already given by checkNeedsGradients.
		// here just do the projections
		// note that the call  checkNeedsGradients() in BiasMetaD takes care of switching on the call to gradients
		if (nowAddAHill){// geometry is sync with hill deposition
			//cerr<< "add a hill "<<endl;
			k=0;
			for(unsigned i=0;i<ncv;i++){
				for(unsigned j=i;j<ncv;j++){
					// eq 12 of "Metadynamics with adaptive Gaussians"
					variance[k]=sigma*sigma*(paction->getProjection(i,j));
					k++;
				}
			};
		};
		break;
	default:
		cerr<< "This flexible bin is not recognized  "<<endl;
		exit(1)	;
	}

}

vector<double> FlexibleBin::getMatrix() const{
	return variance;
}

///
/// Calculate the matrix of  (dcv_i/dx)*(dcv_j/dx)^-1 
/// that is needed for the metrics in metadynamics
///
///
vector<double> FlexibleBin::getInverseMatrix() const{
	unsigned ncv=paction->getNumberOfArguments();
	Matrix<double> matrix(ncv,ncv); 
	unsigned i,j,k;	
	k=0;
	//paction->log<<"------------ GET INVERSE MATRIX ---------------\n";
        // place the matrix in a complete matrix for compatibility 
	for (i=0;i<ncv;i++){
		for (j=i;j<ncv;j++){
			matrix(j,i)=matrix(i,j)=variance[k];
			k++;	
		}
	}
//	paction->log<<"MATRIX\n";
//	matrixOut(paction->log,matrix);	
#define NEWFLEX
#ifdef NEWFLEX
       	// diagonalize to impose boundaries (only if boundaries are set)
        Matrix<double> eigenvecs(ncv,ncv); 	
 	vector<double> eigenvals(ncv);		

	//eigenvecs: first is eigenvec number, second is eigenvec component	
        if(diagMat( matrix , eigenvals , eigenvecs )!=0){plumed_merror("diagonalization in FlexibleBin failed! This matrix is weird\n");};	
	
//	paction->log<<"EIGENVECS \n";
//	matrixOut(paction->log,eigenvecs);
      	
        //for (i=0;i<ncv;i++){//loop on the dimension 	
        //	for (j=0;j<ncv;j++){//loop on the dimension 	
        //	eigenvecs[i][j]=0.;
        //	if(i==j)eigenvecs[i][j]=1.;
        //	}	
        //}
	
	for (i=0;i<ncv;i++){//loop on the dimension 
          if( limitmax[i] ){
		  //limit every  component that is larger
	       	  for (j=0;j<ncv;j++){//loop on components	
	          	if(pow(eigenvals[j]*eigenvecs[j][i],2)>pow(sigmamax[i],2) ){
                  	     eigenvals[j]=sqrt(pow(sigmamax[i]/(eigenvecs[j][i]),2))*copysign(1.,eigenvals[j]);
		  	}			
	          }		
	  }
	}
	for (i=0;i<ncv;i++){//loop on the dimension 
	  // find the largest one:  if it is smaller than min  then rescale
	  if( limitmin[i] ){
	           unsigned imax=0;		
                   double fmax=-1.e10;
	       	   for (j=0;j<ncv;j++){//loop on components	
                        double fact=pow(eigenvals[j]*eigenvecs[j][i],2);
		        if(fact>fmax){
		         fmax=fact;imax=j;
		        } 	
        	   }
                   if(fmax<pow(sigmamin[i],2) ){
                             eigenvals[imax]=sqrt(pow(sigmamin[i]/(eigenvecs[imax][i]),2))*copysign(1.,eigenvals[imax]);
                   }

	  }

	}  

	// normalize eigenvecs
        Matrix<double> newinvmatrix(ncv,ncv); 	
	for (i=0;i<ncv;i++){
		for (j=0;j<ncv;j++){
			newinvmatrix[j][i]=eigenvecs[j][i]/eigenvals[j];
		}
	}

        vector<double> uppervec(ncv*(ncv+1)/2);
	k=0;
	for (i=0;i<ncv;i++){
		for (j=i;j<ncv;j++){
			double scal=0;
			for(unsigned l=0;l<ncv;++l){
				scal+=eigenvecs[l][i]*newinvmatrix[l][j];
			}	
			uppervec[k]=scal; k++;			
		}
	}
#else 
	// get the inverted matrix
	Matrix<double> invmatrix(ncv,ncv);	
	Invert(matrix,invmatrix);
        vector<double> uppervec(ncv*(ncv+1)/2); 
        // upper diagonal of the inverted matrix (that is symmetric) 
	k=0;
	for (i=0;i<ncv;i++){
		for (j=i;j<ncv;j++){
			uppervec[k]=invmatrix(i,j);
			//paction->log<<"VV "<<i<<" "<<j<<" "<<uppervec[k]<<"\n";	
			k++;
		}
	}
	//paction->log<<"------------ END GET INVERSE MATRIX ---------------\n";
#endif

	return uppervec;  
}

}
