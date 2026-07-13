/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-rpl-routing.h"

#include "nishioka-nwk.h"
#include "nishioka-header.h"

#include "ns3/log.h"
#include "ns3/lr-wpan-mac.h"
#include "ns3/simulator.h"

using namespace ns3::lrwpan;

namespace ns3
{
namespace nishioka
{

NS_LOG_COMPONENT_DEFINE("NishiokaRplRouting");
NS_OBJECT_ENSURE_REGISTERED(NishiokaRplRouting);

TypeId
NishiokaRplRouting::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::NishiokaRplRouting")
                            .SetParent<Object>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<NishiokaRplRouting>()
                            .AddTraceSource("ParentSelected",
                                            "Emitted when a parent is selected",
                                            MakeTraceSourceAccessor(&NishiokaRplRouting::m_traceParentSelected),
                                            "ns3::TracedCallback<ns3::Mac16Address,uint16_t>")
                            .AddTraceSource("RouteInstalled",
                                            "Emitted when a downward route is installed",
                                            MakeTraceSourceAccessor(&NishiokaRplRouting::m_traceRouteInstalled),
                                            "ns3::TracedCallback<ns3::Mac16Address,ns3::Mac16Address>");
    return tid;
}

NishiokaRplRouting::NishiokaRplRouting() = default;

NishiokaRplRouting::~NishiokaRplRouting()
{
    CancelDio();
}

void
NishiokaRplRouting::SetNwk(Ptr<NishiokaNwk> nwk)
{
    m_nwk = nwk;
}

Ptr<NishiokaNwk>
NishiokaRplRouting::GetNwk() const
{
    return m_nwk;
}

void
NishiokaRplRouting::SetPanId(uint16_t panId)
{
    m_panId = panId;
}

uint16_t
NishiokaRplRouting::GetPanId() const
{
    return m_panId;
}

void
NishiokaRplRouting::SetShortAddress(Mac16Address addr)
{
    m_shortAddr = addr;
}

Mac16Address
NishiokaRplRouting::GetShortAddress() const
{
    return m_shortAddr;
}

void
NishiokaRplRouting::ConfigureAsRoot(Mac16Address dodagId, uint8_t instanceId)
{
    m_role = RplNodeRole::ROOT;
    m_dodag.dodagId = dodagId;
    m_dodag.instanceId = instanceId;
    m_dodag.version = 1;
    m_dodag.rank = RPL_ROOT_RANK;
    NS_LOG_INFO("Configured as DODAG root id=" << dodagId);
}

void
NishiokaRplRouting::StartDodag()
{
    if (m_role != RplNodeRole::ROOT)
    {
        NS_LOG_WARN("StartDodag called on non-root node");
        return;
    }
    ScheduleDio();
}

void
NishiokaRplRouting::StopDodag()
{
    CancelDio();
}

RplNodeRole
NishiokaRplRouting::GetRole() const
{
    return m_role;
}

uint16_t
NishiokaRplRouting::GetRank() const
{
    if (m_role == RplNodeRole::ROOT)
    {
        return RPL_ROOT_RANK;
    }
    auto parent = m_table.GetParent();
    if (parent)
    {
        return static_cast<uint16_t>(parent->parentRank + 256);
    }
    return RPL_INFINITE_RANK;
}

RplDodagInfo
NishiokaRplRouting::GetDodagInfo() const
{
    RplDodagInfo info = m_dodag;
    info.rank = GetRank();
    return info;
}

const RplRoutingTable&
NishiokaRplRouting::GetRoutingTable() const
{
    return m_table;
}

void
NishiokaRplRouting::HandleDio(Mac16Address sender, const RplDioHeader& dio, uint8_t lqi)
{
    NS_LOG_FUNCTION(this << sender << dio.GetSenderRank() << static_cast<uint32_t>(lqi));

    if (m_role == RplNodeRole::ROOT)
    {
        // Root ignores DIO from others in this scaffold.
        return;
    }

    const uint16_t advertisedRank = dio.GetSenderRank();
    if (advertisedRank >= RPL_INFINITE_RANK)
    {
        return;
    }

    const uint16_t candidateRank = static_cast<uint16_t>(advertisedRank + 256);
    const uint16_t currentRank = GetRank();

    if (currentRank == RPL_INFINITE_RANK || candidateRank < currentRank)
    {
        RplParentInfo parent;
        parent.parentAddr = sender;
        parent.parentRank = advertisedRank;
        parent.parentLqi = lqi;
        parent.valid = true;
        m_table.SetParent(parent);

        m_dodag = dio.GetDodagInfo();
        m_dodag.rank = candidateRank;
        m_role = RplNodeRole::ROUTER;

        InstallDefaultUpwardRoute();
        m_traceParentSelected(sender, candidateRank);

        NS_LOG_INFO("Parent selected: " << sender << " rank=" << candidateRank);

        // TODO: implement Trickle timer (RFC 6206) instead of immediate DAO.
        SendDao(m_shortAddr);
    }
}

void
NishiokaRplRouting::HandleDao(Mac16Address sender, const RplDaoHeader& dao)
{
    NS_LOG_FUNCTION(this << sender << dao.GetTarget());

    InstallDownwardRoute(dao.GetTarget(), sender);

    if (m_role == RplNodeRole::ROOT)
    {
        SendDaoAck(dao.GetTarget(), 0);
        return;
    }

    // Non-root: propagate DAO toward root via parent.
    auto parent = m_table.GetParent();
    if (!parent)
    {
        NS_LOG_WARN("DAO received but no parent configured");
        return;
    }

    Ptr<Packet> packet = BuildDaoPacket(dao.GetTarget());
    if (!packet || !m_nwk || !m_nwk->GetMac())
    {
        return;
    }

    McpsDataRequestParams params;
    params.m_dstPanId = m_panId;
    params.m_srcAddrMode = SHORT_ADDR;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_dstAddr = parent->parentAddr;
    params.m_msduHandle = 10;
    params.m_txOptions = TX_OPTION_NONE;
    m_nwk->GetMac()->McpsDataRequest(params, packet);
}

void
NishiokaRplRouting::HandleDaoAck(Mac16Address sender, const RplDaoAckHeader& ack)
{
    NS_LOG_FUNCTION(this << sender << ack.GetTarget() << static_cast<uint32_t>(ack.GetStatus()));
    // TODO: handle DAO-ACK status, retransmit DAO if needed.
}

Ptr<Packet>
NishiokaRplRouting::BuildDioPacket() const
{
    RplDioHeader dio;
    dio.SetDodagInfo(m_dodag);
    dio.SetSenderRank(GetRank());

    NishiokaHeader hdr;
    hdr.SetFrameType(NishiokaHeader::CUSTOM_COMMAND);
    hdr.SetSeqNum(0);
    hdr.SetSrcAddrFields(m_panId, m_shortAddr);
    hdr.SetDstAddrFields(m_panId, Mac16Address::GetBroadcast());

    Ptr<Packet> pkt = Create<Packet>(0);
    pkt->AddHeader(dio);
    pkt->AddHeader(hdr);
    return pkt;
}

Ptr<Packet>
NishiokaRplRouting::BuildDaoPacket(Mac16Address target) const
{
    RplDaoHeader dao;
    dao.SetTarget(target);
    dao.SetPathSequence(m_daoPathSequence);

    NishiokaHeader hdr;
    hdr.SetFrameType(NishiokaHeader::CUSTOM_COMMAND);
    hdr.SetSeqNum(0);
    hdr.SetSrcAddrFields(m_panId, m_shortAddr);

    Mac16Address dst = m_dodag.dodagId;
    if (m_role != RplNodeRole::ROOT)
    {
        auto parent = m_table.GetParent();
        if (parent)
        {
            dst = parent->parentAddr;
        }
    }
    hdr.SetDstAddrFields(m_panId, dst);

    Ptr<Packet> pkt = Create<Packet>(0);
    pkt->AddHeader(dao);
    pkt->AddHeader(hdr);
    return pkt;
}

Ptr<Packet>
NishiokaRplRouting::BuildDaoAckPacket(Mac16Address target, uint8_t status) const
{
    RplDaoAckHeader ack;
    ack.SetTarget(target);
    ack.SetStatus(status);

    NishiokaHeader hdr;
    hdr.SetFrameType(NishiokaHeader::CUSTOM_COMMAND);
    hdr.SetSeqNum(0);
    hdr.SetSrcAddrFields(m_panId, m_shortAddr);
    hdr.SetDstAddrFields(m_panId, Mac16Address::GetBroadcast());

    Ptr<Packet> pkt = Create<Packet>(0);
    pkt->AddHeader(ack);
    pkt->AddHeader(hdr);
    return pkt;
}

void
NishiokaRplRouting::SendDio()
{
    if (!m_nwk || !m_nwk->GetMac())
    {
        return;
    }

    Ptr<Packet> packet = BuildDioPacket();
    McpsDataRequestParams params;
    params.m_dstPanId = m_panId;
    params.m_srcAddrMode = SHORT_ADDR;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_dstAddr = Mac16Address::GetBroadcast();
    params.m_msduHandle = 11;
    params.m_txOptions = TX_OPTION_NONE;
    m_nwk->GetMac()->McpsDataRequest(params, packet);

    if (m_role == RplNodeRole::ROOT)
    {
        ScheduleDio();
    }
}

void
NishiokaRplRouting::SendDao(Mac16Address target)
{
  m_daoPathSequence++;
    if (!m_nwk || !m_nwk->GetMac())
    {
        return;
    }
    Ptr<Packet> packet = BuildDaoPacket(target);
    McpsDataRequestParams params;
    params.m_dstPanId = m_panId;
    params.m_srcAddrMode = SHORT_ADDR;
    params.m_dstAddrMode = SHORT_ADDR;

    auto parent = m_table.GetParent();
    params.m_dstAddr = parent ? parent->parentAddr : m_dodag.dodagId;
    params.m_msduHandle = 12;
    params.m_txOptions = TX_OPTION_NONE;
    m_nwk->GetMac()->McpsDataRequest(params, packet);
}

void
NishiokaRplRouting::SendDaoAck(Mac16Address target, uint8_t status)
{
    if (!m_nwk || !m_nwk->GetMac())
    {
        return;
    }
    Ptr<Packet> packet = BuildDaoAckPacket(target, status);
    McpsDataRequestParams params;
    params.m_dstPanId = m_panId;
    params.m_srcAddrMode = SHORT_ADDR;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_dstAddr = Mac16Address::GetBroadcast();
    params.m_msduHandle = 13;
    params.m_txOptions = TX_OPTION_NONE;
    m_nwk->GetMac()->McpsDataRequest(params, packet);
}

void
NishiokaRplRouting::InstallDefaultUpwardRoute()
{
    auto parent = m_table.GetParent();
    if (!parent || !m_nwk)
    {
        return;
    }
    // Default route to DODAG root via selected parent.
    m_nwk->SetRoute(m_dodag.dodagId, parent->parentAddr);
}

void
NishiokaRplRouting::InstallDownwardRoute(Mac16Address target, Mac16Address nextHop)
{
    RplRouteEntry entry;
    entry.target = target;
    entry.nextHop = nextHop;
    entry.valid = true;
    m_table.AddDownwardRoute(entry);

    if (m_nwk)
    {
        m_nwk->SetRoute(target, nextHop);
    }
    m_traceRouteInstalled(target, nextHop);
    NS_LOG_INFO("Downward route: target=" << target << " nextHop=" << nextHop);
}

void
NishiokaRplRouting::ScheduleDio()
{
    CancelDio();
    m_dioEvent = Simulator::Schedule(Seconds(RPL_DEFAULT_DIO_INTERVAL_SEC),
                                     &NishiokaRplRouting::SendDio,
                                     this);
}

void
NishiokaRplRouting::CancelDio()
{
    if (m_dioEvent.IsPending())
    {
        m_dioEvent.Cancel();
    }
}

} // namespace nishioka
} // namespace ns3
