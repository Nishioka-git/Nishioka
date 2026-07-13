/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-rpl-nwk.h"

#include "ns3/log.h"

namespace ns3
{
namespace nishioka
{

NS_LOG_COMPONENT_DEFINE("NishiokaRplNwk");
NS_OBJECT_ENSURE_REGISTERED(NishiokaRplNwk);

TypeId
NishiokaRplNwk::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::NishiokaRplNwk")
                            .SetParent<NishiokaNwk>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<NishiokaRplNwk>();
    return tid;
}

NishiokaRplNwk::NishiokaRplNwk()
{
    m_rpl = CreateObject<NishiokaRplRouting>();
    m_rpl->SetNwk(this);
}

NishiokaRplNwk::~NishiokaRplNwk() = default;

Ptr<NishiokaRplRouting>
NishiokaRplNwk::GetRplRouting() const
{
    return m_rpl;
}

void
NishiokaRplNwk::SetPanId(uint16_t panId)
{
    if (m_rpl)
    {
        m_rpl->SetPanId(panId);
    }
}

void
NishiokaRplNwk::SetShortAddress(Mac16Address addr)
{
    if (m_rpl)
    {
        m_rpl->SetShortAddress(addr);
    }
}

void
NishiokaRplNwk::DoDispose()
{
    m_rpl = nullptr;
    NishiokaNwk::DoDispose();
}

void
NishiokaRplNwk::DoInitialize()
{
    AggregateObject(m_rpl);
    NishiokaNwk::DoInitialize();
}

void
NishiokaRplNwk::McpsDataIndication(lrwpan::McpsDataIndicationParams params, Ptr<Packet> msdu)
{
    if (TryDispatchRplControl(params, msdu))
    {
        return;
    }
    NishiokaNwk::McpsDataIndication(params, msdu);
}

bool
NishiokaRplNwk::TryDispatchRplControl(lrwpan::McpsDataIndicationParams params, Ptr<Packet> msdu)
{
    Ptr<Packet> copy = msdu->Copy();
    NishiokaHeader hdr;
    hdr.SetDstAddrMode(NishiokaHeader::SHORTADDR);
    hdr.SetSrcAddrMode(NishiokaHeader::SHORTADDR);

    if (copy->GetSize() < hdr.GetSerializedSize())
    {
        return false;
    }

    copy->RemoveHeader(hdr);
    if (hdr.GetFrameType() != NishiokaHeader::CUSTOM_COMMAND)
    {
        return false;
    }

    if (copy->GetSize() < 1)
    {
        return false;
    }

    uint8_t controlType = 0;
    copy->CopyData(&controlType, 1);
    switch (static_cast<RplControlType>(controlType))
    {
    case RplControlType::DIO: {
        RplDioHeader dio;
        copy->RemoveHeader(dio);
        m_rpl->HandleDio(params.m_srcAddr, dio, params.m_mpduLinkQuality);
        return true;
    }
    case RplControlType::DAO: {
        RplDaoHeader dao;
        copy->RemoveHeader(dao);
        m_rpl->HandleDao(params.m_srcAddr, dao);
        return true;
    }
    case RplControlType::DAO_ACK: {
        RplDaoAckHeader ack;
        copy->RemoveHeader(ack);
        m_rpl->HandleDaoAck(params.m_srcAddr, ack);
        return true;
    }
    default:
        break;
    }
    return false;
}

} // namespace nishioka
} // namespace ns3
