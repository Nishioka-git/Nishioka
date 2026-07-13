/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_RPL_ROUTING_H
#define NISHIOKA_RPL_ROUTING_H

#include "nishioka-rpl-fields.h"
#include "nishioka-rpl-payload-header.h"
#include "nishioka-rpl-tables.h"

#include "ns3/event-id.h"
#include "ns3/mac16-address.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"

namespace ns3
{
namespace nishioka
{

class NishiokaNwk;

/**
 * @ingroup nishioka
 *
 * RPL-like routing engine for the Nishioka protocol (scaffold).
 *
 * Responsibilities (to be filled in incrementally):
 * - DODAG formation via DIO / parent selection
 * - Downward route installation via DAO / DAO-ACK
 * - Synchronization with NishiokaNwk::m_routingTable for data forwarding
 *
 * This class is owned by NishiokaRplNwk and invoked from its MCPS handlers.
 */
class NishiokaRplRouting : public Object
{
  public:
    static TypeId GetTypeId();
    NishiokaRplRouting();
    ~NishiokaRplRouting() override;

    void SetNwk(Ptr<NishiokaNwk> nwk);
    Ptr<NishiokaNwk> GetNwk() const;

    void SetPanId(uint16_t panId);
    uint16_t GetPanId() const;

    void SetShortAddress(Mac16Address addr);
    Mac16Address GetShortAddress() const;

    // --- DODAG lifecycle (scaffold API) ---

    void ConfigureAsRoot(Mac16Address dodagId, uint8_t instanceId = 0);
    void StartDodag();
    void StopDodag();

    RplNodeRole GetRole() const;
    uint16_t GetRank() const;
    RplDodagInfo GetDodagInfo() const;
    const RplRoutingTable& GetRoutingTable() const;

    // --- Control-plane ingress (called from NishiokaRplNwk) ---

    void HandleDio(Mac16Address sender, const RplDioHeader& dio, uint8_t lqi);
    void HandleDao(Mac16Address sender, const RplDaoHeader& dao);
    void HandleDaoAck(Mac16Address sender, const RplDaoAckHeader& ack);

    // --- Control-plane egress (scaffold; extend for full RPL) ---

    Ptr<Packet> BuildDioPacket() const;
    Ptr<Packet> BuildDaoPacket(Mac16Address target) const;
    Ptr<Packet> BuildDaoAckPacket(Mac16Address target, uint8_t status = 0) const;

    void SendDio();
    void SendDao(Mac16Address target);
    void SendDaoAck(Mac16Address target, uint8_t status = 0);

    /**
     * After parent selection, push default route toward DODAG via parent
     * into NishiokaNwk routing table (dst = dodagId or coordinator).
     */
    void InstallDefaultUpwardRoute();

    /**
     * Install downward route for @p target using DAO sender as next hop.
     */
    void InstallDownwardRoute(Mac16Address target, Mac16Address nextHop);

    TracedCallback<Mac16Address, uint16_t> m_traceParentSelected;
    TracedCallback<Mac16Address, Mac16Address> m_traceRouteInstalled;

  private:
    void ScheduleDio();
    void CancelDio();

    Ptr<NishiokaNwk> m_nwk;
    uint16_t m_panId{0xCAFE};
    Mac16Address m_shortAddr;
    RplNodeRole m_role{RplNodeRole::UNASSIGNED};
    RplDodagInfo m_dodag;
    RplRoutingTable m_table;
    EventId m_dioEvent;
    uint8_t m_daoPathSequence{0};
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_RPL_ROUTING_H */
