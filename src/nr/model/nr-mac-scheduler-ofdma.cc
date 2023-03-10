/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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

#define NS_LOG_APPEND_CONTEXT                                            \
  do                                                                     \
    {                                                                    \
      std::clog << " [ CellId " << GetCellId() << ", bwpId "             \
                << GetBwpId () << "] ";                                  \
    }                                                                    \
  while (false);

#include "nr-mac-scheduler-ofdma.h"
#include <ns3/log.h>
#include <algorithm>

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("NrMacSchedulerOfdma");
NS_OBJECT_ENSURE_REGISTERED (NrMacSchedulerOfdma);

TypeId
NrMacSchedulerOfdma::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NrMacSchedulerOfdma")
    .SetParent<NrMacSchedulerTdma> ()
    .AddTraceSource ("SymPerBeam",
                     "Number of assigned symbol per beam. Gets called every time an assignment is made",
                     MakeTraceSourceAccessor (&NrMacSchedulerOfdma::m_tracedValueSymPerBeam),
                     "ns3::TracedValueCallback::Uint32")
  ;
  return tid;
}

NrMacSchedulerOfdma::NrMacSchedulerOfdma () : NrMacSchedulerTdma ()
{
}

/**
 *
 * \brief Calculate the number of symbols to assign to each beam
 * \param symAvail Number of available symbols
 * \param activeDl Map of active DL UE and their beam
 *
 * Each beam has a different requirement in terms of byte that should be
 * transmitted with that beam. That requirement depends on the number of UE
 * that are inside such beam, and how many bytes they have to transmit.
 *
 * For the beam \f$ b \f$, the number of assigned symbols is the following:
 *
 * \f$ sym_{b} = BufSize(b) * \frac{symAvail}{BufSizeTotal} \f$
 */
NrMacSchedulerOfdma::BeamSymbolMap
NrMacSchedulerOfdma::GetSymPerBeam (uint32_t symAvail,
                                        const NrMacSchedulerNs3::ActiveUeMap &activeDl) const
{
  NS_LOG_FUNCTION (this);

  GetSecond GetUeVector;
  GetSecond GetUeBufSize;
  GetFirst GetBeamId;
  double bufTotal = 0.0;
  uint8_t symUsed = 0;
  BeamSymbolMap ret;

  // Compute buf total
  for (const auto &el : activeDl)
    {
      for (const auto & ue : GetUeVector (el))
        {
          bufTotal += GetUeBufSize (ue);
        }
    }

  for (const auto &el : activeDl)
    {
      uint32_t bufSizeBeam = 0;
      for (const auto &ue : GetUeVector (el))
        {
          bufSizeBeam += GetUeBufSize (ue);
        }

      double tmp = symAvail / bufTotal;
      uint32_t symForBeam = static_cast<uint32_t> (bufSizeBeam * tmp);
      symUsed += symForBeam;
      ret.emplace (std::make_pair (GetBeamId (el), symForBeam));
      NS_LOG_DEBUG ("Assigned to beam " << GetBeamId (el) << " symbols " << symForBeam);
    }

  NS_ASSERT (symAvail >= symUsed);
  if (symAvail - symUsed > 0)
    {
      uint8_t symToRedistribute = symAvail - symUsed;
      while (symToRedistribute > 0)
        {
          BeamSymbolMap::iterator min = ret.end ();
          for (auto it = ret.begin (); it != ret.end (); ++it)
            {
              if (min == ret.end () || it->second < min->second)
                {
                  min = it;
                }
            }
          min->second += 1;
          symToRedistribute--;
          NS_LOG_DEBUG ("Assigned to beam " << min->first <<
                        " an additional symbol, for a total of " << min->second);
        }
    }

  // Trigger the trace source firing, using const_cast as we don't change
  // the internal state of the class
  for (const auto & v : ret)
    {
      const_cast<NrMacSchedulerOfdma*> (this)->m_tracedValueSymPerBeam = v.second;
    }

  return ret;
}

/**
 * \brief Assign the available DL RBG to the UEs
 * \param symAvail Available symbols
 * \param activeDl Map of active UE and their beams
 * \return a map between beams and the symbol they need
 *
 * The algorithm redistributes the frequencies to all the UEs inside a beam.
 * The pre-requisite is to calculate the symbols for each beam, done with
 * the function GetSymPerBeam().
 * The pseudocode is the following (please note that sym_of_beam is a value
 * returned by the GetSymPerBeam() function):
 * <pre>
 * while frequencies > 0:
 *    sort (ueVector);
 *    ueVector.first().m_dlRBG += 1 * sym_of_beam;
 *    frequencies--;
 *    UpdateUeDlMetric (ueVector.first());
 * </pre>
 *
 * To sort the UEs, the method uses the function returned by GetUeCompareDlFn().
 * Two fairness helper are hard-coded in the method: the first one is avoid
 * to assign resources to UEs that already have their buffer requirement covered,
 * and the other one is avoid to assign symbols when all the UEs have their
 * requirements covered.
 */
NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignDLRBG (uint32_t symAvail, const ActiveUeMap &activeDl) const
{
  NS_LOG_FUNCTION (this);

  NS_LOG_DEBUG ("# beams active flows: " << activeDl.size () << ", # sym: " << symAvail);

  GetFirst GetBeamId;
  GetSecond GetUeVector;
  BeamSymbolMap symPerBeam = GetSymPerBeam (symAvail, activeDl);

  // Iterate through the different beams
  for (const auto &el : activeDl)
    {
      // Distribute the RBG evenly among UEs of the same beam
      uint32_t resources = GetBandwidthInRbg ();
      uint32_t beamSym = symPerBeam.at (GetBeamId (el));
      uint32_t rbgAssignable = beamSym;
      std::vector<UePtrAndBufferReq> ueVector;
      FTResources assigned (0,0);
      for (const auto &ue : GetUeVector (el))
        {
          ueVector.emplace_back (ue);
        }

      for (auto & ue : ueVector)
        {
          BeforeDlSched (ue, FTResources (rbgAssignable * beamSym, beamSym));
        }

      while (resources > 0)
        {
          GetFirst GetUe;
          std::sort (ueVector.begin (), ueVector.end (), GetUeCompareDlFn ());
          auto schedInfoIt = ueVector.begin ();

          // Ensure fairness: pass over UEs which already has enough resources to transmit
          while (schedInfoIt != ueVector.end ())
            {
              uint32_t bufQueueSize = schedInfoIt->second;
              if (GetUe (*schedInfoIt)->m_dlTbSize >= std::max (bufQueueSize, 7U))
                {
                  schedInfoIt++;
                }
              else
                {
                  break;
                }
            }

          // In the case that all the UE already have their requirements fullfilled,
          // then stop the beam processing and pass to the next
          if (schedInfoIt == ueVector.end ())
            {
              break;
            }

          // Assign 1 RBG for each available symbols for the beam,
          // and then update the count of available resources
          GetUe (*schedInfoIt)->m_dlRBG += rbgAssignable;
          assigned.m_rbg += rbgAssignable;

          GetUe (*schedInfoIt)->m_dlSym = beamSym;
          assigned.m_sym = beamSym;

          resources -= 1; // Resources are RBG, so they do not consider the beamSym

          // Update metrics
          NS_LOG_DEBUG ("Assigned " << rbgAssignable <<
                        " DL RBG, spanned over " << beamSym << " SYM, to UE " <<
                        GetUe (*schedInfoIt)->m_rnti);
          AssignedDlResources (*schedInfoIt, FTResources (rbgAssignable, beamSym),
                               assigned);

          // Update metrics for the unsuccessfull UEs (who did not get any resource in this iteration)
          for (auto & ue : ueVector)
            {
              if (GetUe (ue)->m_rnti != GetUe (*schedInfoIt)->m_rnti)
                {
                  NotAssignedDlResources (ue, FTResources (rbgAssignable, beamSym),
                                          assigned);
                }
            }
        }
    }

  return symPerBeam;
}

NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignULRBG (uint32_t symAvail, const ActiveUeMap &activeUl) const
{
  NS_LOG_FUNCTION (this);

  NS_LOG_DEBUG ("# beams active flows: " << activeUl.size () << ", # sym: " << symAvail);

  GetFirst GetBeamId;
  GetSecond GetUeVector;
  BeamSymbolMap symPerBeam = GetSymPerBeam (symAvail, activeUl);

  // Iterate through the different beams
  for (const auto &el : activeUl)
    {
      // Distribute the RBG evenly among UEs of the same beam
      uint32_t resources = GetBandwidthInRbg ();
      uint32_t beamSym = symPerBeam.at (GetBeamId (el));
      uint32_t rbgAssignable = beamSym;
      std::vector<UePtrAndBufferReq> ueVector;
      FTResources assigned (0,0);
      for (const auto &ue : GetUeVector (el))
        {
          ueVector.emplace_back (ue);
        }

      for (auto & ue : ueVector)
        {
          BeforeUlSched (ue, FTResources (rbgAssignable * beamSym, beamSym));
        }

      while (resources > 0)
        {
          GetFirst GetUe;
          std::sort (ueVector.begin (), ueVector.end (), GetUeCompareUlFn ());
          auto schedInfoIt = ueVector.begin ();

          // Ensure fairness: pass over UEs which already has enough resources to transmit
          while (schedInfoIt != ueVector.end ())
            {
              uint32_t bufQueueSize = schedInfoIt->second;
              if (GetUe (*schedInfoIt)->m_ulTbSize >= std::max (bufQueueSize, 7U))
                {
                  schedInfoIt++;
                }
              else
                {
                  break;
                }
            }

          // In the case that all the UE already have their requirements fullfilled,
          // then stop the beam processing and pass to the next
          if (schedInfoIt == ueVector.end ())
            {
              break;
            }

          // Assign 1 RBG for each available symbols for the beam,
          // and then update the count of available resources
          GetUe (*schedInfoIt)->m_ulRBG += rbgAssignable;
          assigned.m_rbg += rbgAssignable;

          GetUe (*schedInfoIt)->m_ulSym = beamSym;
          assigned.m_sym = beamSym;

          resources -= 1; // Resources are RBG, so they do not consider the beamSym

          // Update metrics
          NS_LOG_DEBUG ("Assigned " << rbgAssignable <<
                        " UL RBG, spanned over " << beamSym << " SYM, to UE " <<
                        GetUe (*schedInfoIt)->m_rnti);
          AssignedUlResources (*schedInfoIt, FTResources (rbgAssignable, beamSym),
                               assigned);

          // Update metrics for the unsuccessfull UEs (who did not get any resource in this iteration)
          for (auto & ue : ueVector)
            {
              if (GetUe (ue)->m_rnti != GetUe (*schedInfoIt)->m_rnti)
                {
                  NotAssignedUlResources (ue, FTResources (rbgAssignable, beamSym),
                                          assigned);
                }
            }
        }
    }

  return symPerBeam;
}

/**
 * \brief Create the DL DCI in OFDMA mode
 * \param spoint Starting point
 * \param ueInfo UE representation
 * \param maxSym Maximum symbols to use
 * \return a pointer to the newly created instance
 *
 * The function calculates the TBS and then call CreateDci().
 */
std::shared_ptr<DciInfoElementTdma>
NrMacSchedulerOfdma::CreateDlDci (NrMacSchedulerNs3::PointInFTPlane *spoint,
                                      const std::shared_ptr<NrMacSchedulerUeInfo> &ueInfo,
                                      uint32_t maxSym) const
{
  NS_LOG_FUNCTION (this);

  uint32_t tbs = m_dlAmc->CalculateTbSize (ueInfo->m_dlMcs,
                                           ueInfo->m_dlRBG * GetNumRbPerRbg ());
  NS_ASSERT_MSG (ueInfo->m_dlRBG % maxSym == 0, " MaxSym " << maxSym << " RBG: " << ueInfo->m_dlRBG);
  NS_ASSERT (ueInfo->m_dlRBG <= maxSym * GetBandwidthInRbg ());
  NS_ABORT_IF (maxSym > UINT8_MAX);

  // If is less than 7 (3 mac header, 2 rlc header, 2 data), then we can't
  // transmit any new data, so don't create dci.
  if (tbs < 7)
    {
      NS_LOG_DEBUG ("While creating DCI for UE " << ueInfo->m_rnti <<
                    " assigned " << ueInfo->m_dlRBG << " DL RBG, but TBS < 7");
      return nullptr;
    }

  uint32_t RBGNum = ueInfo->m_dlRBG / maxSym;
  std::vector<uint8_t> rbgBitmask;

  for (uint32_t i = 0; i < GetBandwidthInRbg (); ++i)
    {
      if (i >= spoint->m_rbg && i < spoint->m_rbg + RBGNum)
        {
          rbgBitmask.push_back (1);
        }
      else
        {
          rbgBitmask.push_back (0);
        }
    }

  NS_LOG_INFO ("UE " << ueInfo->m_rnti << " assigned RBG from " <<
               static_cast<uint32_t> (spoint->m_rbg) << " to " <<
               static_cast<uint32_t> (spoint->m_rbg + RBGNum) << " for " <<
               static_cast<uint32_t> (maxSym) << " SYM.");

  std::shared_ptr<DciInfoElementTdma> dci = std::make_shared<DciInfoElementTdma>
      (ueInfo->m_rnti, DciInfoElementTdma::DL, spoint->m_sym, maxSym, ueInfo->m_dlMcs,
       tbs, 1, 0, DciInfoElementTdma::DATA, GetBwpId ());

  dci->m_rbgBitmask = std::move (rbgBitmask);

  spoint->m_rbg += RBGNum;

  return dci;
}

std::shared_ptr<DciInfoElementTdma>
NrMacSchedulerOfdma::CreateUlDci (PointInFTPlane *spoint,
                                      const std::shared_ptr<NrMacSchedulerUeInfo> &ueInfo,
                                      uint32_t maxSym) const
{
  NS_LOG_FUNCTION (this);

  uint32_t tbs = m_ulAmc->CalculateTbSize (ueInfo->m_ulMcs,
                                           ueInfo->m_ulRBG * GetNumRbPerRbg ());

  // If is less than 7 (3 mac header, 2 rlc header, 2 data), then we can't
  // transmit any new data, so don't create dci.
  if (tbs < 7)
    {
      NS_LOG_DEBUG ("While creating DCI for UE " << ueInfo->m_rnti <<
                    " assigned " << ueInfo->m_ulRBG << " UL RBG, but TBS < 7");
      return nullptr;
    }

  uint32_t RBGNum = ueInfo->m_ulRBG / maxSym;
  std::vector<uint8_t> rbgBitmask;

  for (uint32_t i = 0; i < GetBandwidthInRbg (); ++i)
    {
      if (i >= spoint->m_rbg && i < spoint->m_rbg + RBGNum)
        {
          rbgBitmask.push_back (1);
        }
      else
        {
          rbgBitmask.push_back (0);
        }
    }

  NS_LOG_INFO ("UE " << ueInfo->m_rnti << " assigned RBG from " <<
               static_cast<uint32_t> (spoint->m_rbg) << " to " <<
               static_cast<uint32_t> (spoint->m_rbg + RBGNum) << " for " <<
               static_cast<uint32_t> (maxSym) << " SYM.");

  NS_ASSERT (spoint->m_sym >= maxSym);
  std::shared_ptr<DciInfoElementTdma> dci = std::make_shared<DciInfoElementTdma>
      (ueInfo->m_rnti, DciInfoElementTdma::UL, spoint->m_sym - maxSym, maxSym, ueInfo->m_ulMcs,
       tbs, 1, 0, DciInfoElementTdma::DATA, GetBwpId ());

  dci->m_rbgBitmask = std::move (rbgBitmask);

  spoint->m_rbg += RBGNum;

  return dci;
}

void
NrMacSchedulerOfdma::ChangeDlBeam (PointInFTPlane *spoint, uint32_t symOfBeam) const
{
  spoint->m_rbg = 0;
  spoint->m_sym += symOfBeam;
}

void
NrMacSchedulerOfdma::ChangeUlBeam (PointInFTPlane *spoint, uint32_t symOfBeam) const
{
  spoint->m_rbg = 0;
  spoint->m_sym -= symOfBeam;
}

} // namespace ns3
