/* Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; */
/*
 *   Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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

#include "node-distribution-scenario-interface.h"

namespace ns3 {

NodeDistributionScenarioInterface::~NodeDistributionScenarioInterface ()
{
}

const NodeContainer &
NodeDistributionScenarioInterface::GetBaseStations () const
{
  return m_bs;
}

const NodeContainer &
NodeDistributionScenarioInterface::GetUserTerminals () const
{
  return m_ut;
}

void
NodeDistributionScenarioInterface::SetBsHeight (double h)
{
  m_bsHeight = h;
}

void
NodeDistributionScenarioInterface::SetUtHeight (double h)
{
  m_utHeight = h;
}

void
NodeDistributionScenarioInterface::SetBsNumber (uint32_t n)
{
  m_bs.Create (n);
}

void
NodeDistributionScenarioInterface::SetUtNumber (uint32_t n)
{
  m_ut.Create (n);
}

} //namespace ns3
