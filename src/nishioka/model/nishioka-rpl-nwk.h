/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_RPL_NWK_H
#define NISHIOKA_RPL_NWK_H

#include "nishioka-nwk.h"
#include "nishioka-rpl-routing.h"

#include "ns3/nishioka-header.h"

namespace ns3
{
namespace nishioka
{

/**
 * @ingroup nishioka
 *
 * Nishioka NWK layer extended with RPL-like control-plane dispatch.
 *
 * CUSTOM_DATA frames are forwarded to the registered application callback.
 * CUSTOM_COMMAND frames with RPL payloads are handled internally by
 * NishiokaRplRouting; other command types may be added later.
 */
class NishiokaRplNwk : public NishiokaNwk
{
  public:
    static TypeId GetTypeId();
    NishiokaRplNwk();
    ~NishiokaRplNwk() override;

    Ptr<NishiokaRplRouting> GetRplRouting() const;

    void SetPanId(uint16_t panId);
    void SetShortAddress(Mac16Address addr);

    void McpsDataIndication(lrwpan::McpsDataIndicationParams params, Ptr<Packet> msdu) override;

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    bool TryDispatchRplControl(lrwpan::McpsDataIndicationParams params, Ptr<Packet> msdu);

    Ptr<NishiokaRplRouting> m_rpl;
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_RPL_NWK_H */
