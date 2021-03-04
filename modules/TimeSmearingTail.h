/*
 *  Delphes: a framework for fast simulation of a generic collider experiment
 *  Copyright (C) 2012-2014  Universite catholique de Louvain (UCL), Belgium
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TimeSmearingTail_h
#define TimeSmearingTail_h

/** \class TimeSmearingTail
 *
 *  Performs transverse time smearing.
 *
 *  \author Michele Selvaggi - UCL, Louvain-la-Neuve
 *
 *  With addition of non-gaussian tails (R+Preghenella, preghenella@bo.infn.it)
 *  non-gaussian tails can be added on both the left/right side of the peak
 *  according to the "TailLeft" and "TailRight" parameters
 *  following a q-gaussian PDF, where Tail is the value of q (=1 for gaussian)
 *
 */

#include "classes/DelphesModule.h"
#include "TMath.h"

class TIterator;
class TObjArray;
class TF1;

class TimeSmearingTail: public DelphesModule
{
public:
  TimeSmearingTail();
  ~TimeSmearingTail();

  void Init();
  void Process();
  void Finish();

private:

  static Double_t qExp(Double_t x, Double_t q) {
    if (TMath::Abs(q - 1.) < 0.001)
      return TMath::Exp(x);
    if ((1. + (1. - q) * x) > 0.)
      return TMath::Power(1. + (1. - q) * x, 1. / (1. - q));
    return TMath::Power(0., 1. / (1. - q));
  };

  static Double_t qTOF(Double_t *_x, Double_t *_par) {
    Double_t x = _x[0];
    Double_t sigma = _par[0];
    Double_t qR = _par[1];
    Double_t qL = _par[2];
    Double_t beta = 1. / (2. * sigma * sigma);
    //
    if (x <= 0.) return qExp(-beta * x * x, qL);
    return qExp(-beta * x * x, qR);
  };

  TF1 *fSignal;
  
  Double_t fTimeResolution;
  Double_t fTailRight;
  Double_t fTailLeft;

  TIterator *fItInputArray; //!

  const TObjArray *fInputArray; //!

  TObjArray *fOutputArray; //!

  ClassDef(TimeSmearingTail, 1)
};

#endif
