/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2020 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License version 2 as
*   published by the Free Software Foundation;
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*/

#include "beamforming-vector.h"
#include <ns3/log.h>
#include <ns3/uinteger.h>
#include <ns3/angles.h>
#include <ns3/double.h>


namespace ns3{


complexVector_t CreateQuasiOmniBfv (uint32_t antennaRows, uint32_t antennaColumns)
{
  complexVector_t omni;
  uint32_t size = antennaRows * antennaColumns;
  double power = 1 / sqrt (size);
  for (uint32_t ind = 0; ind < antennaRows; ind++)
    {
      std::complex<double> c = 0.0;
      if (antennaRows % 2 == 0)
        {
          c = exp(std::complex<double> (0, M_PI * ind * ind / antennaRows));
        }
      else
        {
          c = exp(std::complex<double> (0, M_PI * ind * (ind+1) / antennaRows));
        }

      for (uint32_t ind2 = 0; ind2 < antennaColumns; ind2++)
        {
          std::complex<double> d = 0.0;
          if (antennaColumns % 2 == 0)
            {
              d = exp(std::complex<double> (0, M_PI * ind2 * ind2 / antennaColumns));
            }
          else
            {
              d = exp(std::complex<double> (0, M_PI * ind2 * (ind2+1) / antennaColumns));
            }

          omni.push_back (c * d * power);
        }
    }
  return omni;
}

complexVector_t CreateDirectionalBfv (const Ptr<const ThreeGppAntennaArrayModel>& antenna,
                                      uint16_t sector, double elevation)
{
  complexVector_t tempVector;

  DoubleValue horizontalBeamStep;
  antenna->GetAttribute ("HorizontalBeamStep", horizontalBeamStep);
  double hAngle_radian;

  //double hAngle_radian = 2 * M_PI * (static_cast<double> (sector) / static_cast<double> (360.0 / horizontalBeamStep.Get ()));

  /*if (horizontalBeamStep.Get () == 9.0)
  {
    hAngle_radian = M_PI * (static_cast<double> (sector) / static_cast<double> (180.0 / horizontalBeamStep.Get ()));
  }
  else if (horizontalBeamStep.Get () == 30.0 || 
           horizontalBeamStep.Get () == 24.0 || 
           horizontalBeamStep.Get () == 18.0)
  {*/
    hAngle_radian = M_PI * (static_cast<double> (sector) / static_cast<double> (180.0 / horizontalBeamStep.Get ())) - M_PI / 2.0;
  
  
  double vAngle_radian = elevation * M_PI / 180.0;
  uint16_t size = antenna->GetNumberOfElements ();
  double power = 1 / sqrt (size);
  for (auto ind = 0; ind < size; ind++)
    {
    
    Vector loc = antenna->GetElementLocation (ind);
    double phase = -2.0 * M_PI * (sin (vAngle_radian) * cos (hAngle_radian) * loc.x
                                + sin (vAngle_radian) * sin (hAngle_radian) * loc.y
                                + cos (vAngle_radian) * loc.z);
    tempVector.push_back (exp (std::complex<double> (0, phase)) * power);
    }
  return tempVector;
}

complexVector_t CreateDirectPathBfv (const Ptr<MobilityModel>& a,
                                     const Ptr<MobilityModel>& b,
                                     const Ptr<const ThreeGppAntennaArrayModel>& antenna)
{
  complexVector_t antennaWeights;

  // retrieve the position of the two devices
  Vector aPos = a->GetPosition ();
  Vector bPos = b->GetPosition ();

  // compute the azimuth and the elevation angles
  Angles completeAngle (bPos, aPos);

  double hAngleRadian = fmod (completeAngle.phi, 2.0 * M_PI); // the azimuth angle
  if (hAngleRadian < 0)
    {
      hAngleRadian += 2.0 * M_PI;
    }
  double vAngleRadian = completeAngle.theta; // the elevation angle

  // retrieve the number of antenna elements
  int totNoArrayElements = antenna->GetNumberOfElements ();

  // the total power is divided equally among the antenna elements
  double power = 1 / sqrt (totNoArrayElements);

  // compute the antenna weights
  for (int ind = 0; ind < totNoArrayElements; ind++)
    {
      Vector loc = antenna->GetElementLocation (ind);
      double phase = -2 * M_PI * (sin (vAngleRadian) * cos (hAngleRadian) * loc.x
                                  + sin (vAngleRadian) * sin (hAngleRadian) * loc.y
                                  + cos (vAngleRadian) * loc.z);
      antennaWeights.push_back (exp (std::complex<double> (0, phase)) * power);
    }

  return antennaWeights;
}

}
