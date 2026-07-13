/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_NWK_H
#define NISHIOKA_NWK_H

#include "ns3/lr-wpan-mac-base.h"
#include "ns3/mac16-address.h"
#include "ns3/mac64-address.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"

#include <map>

namespace ns3
{
namespace nishioka
{

/**
 * @ingroup nishioka
 *
 * @brief Network layer for Nishioka module
 *
 * This class provides network layer functionality for the Nishioka module.
 * It handles packet routing and network management.
 */
class NishiokaNwk : public Object
{
  public:
    /**
     * Get the type ID.
     *
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * Default constructor
     */
    NishiokaNwk();
    ~NishiokaNwk() override;

    /**
     * Set the MAC layer
     *
     * @param mac A smart pointer to the MAC layer
     */
    void SetMac(Ptr<lrwpan::LrWpanMacBase> mac);

    /**
     * Get the MAC layer
     *
     * @return A smart pointer to the MAC layer
     */
    Ptr<lrwpan::LrWpanMacBase> GetMac() const;

    /**
     * Set a route in the routing table
     *
     * @param dst Destination address
     * @param nextHop Next hop address to reach the destination
     * @return true if route was added/updated successfully, false otherwise
     */
    bool SetRoute(Mac16Address dst, Mac16Address nextHop);

    /**
     * Get the next hop address for a destination
     *
     * @param dst Destination address
     * @param nextHop Reference to store the next hop address
     * @return true if route was found, false otherwise
     */
    bool GetNextHop(Mac16Address dst, Mac16Address& nextHop) const;

    /**
     * Remove a route from the routing table
     *
     * @param dst Destination address
     * @return true if route was removed, false if route was not found
     */
    bool RemoveRoute(Mac16Address dst);

    /**
     * Check if a route exists for a destination
     *
     * @param dst Destination address
     * @return true if route exists, false otherwise
     */
    bool HasRoute(Mac16Address dst) const;

    /**
     * Get the number of routes in the routing table
     *
     * @return Number of routes
     */
    uint32_t GetRouteCount() const;

    /**
     * Get the extended address obtained from the MAC layer.
     *
     * @return The IEEE extended address
     */
    Mac64Address GetExtendedAddress() const;

    /**
     * MCPS-DATA.indication handler (invoked by the MAC layer).
     */
    virtual void McpsDataIndication(lrwpan::McpsDataIndicationParams params, Ptr<Packet> msdu);

    /**
     * MCPS-DATA.confirm handler (invoked by the MAC layer).
     */
    void McpsDataConfirm(lrwpan::McpsDataConfirmParams params);

    /**
     * MLME-GET.confirm handler (invoked by the MAC layer).
     */
    void MlmeGetConfirm(lrwpan::MacStatus status,
                        lrwpan::MacPibAttributeIdentifier id,
                        Ptr<lrwpan::MacPibAttributes> attribute);

    /**
     * MLME-SET.confirm handler (invoked by the MAC layer).
     */
    void MlmeSetConfirm(lrwpan::MlmeSetConfirmParams params);

    /**
     * MLME-START.confirm handler (invoked by the MAC layer).
     */
    void MlmeStartConfirm(lrwpan::MlmeStartConfirmParams params);

    /**
     * MLME-SCAN.confirm handler (invoked by the MAC layer).
     */
    void MlmeScanConfirm(lrwpan::MlmeScanConfirmParams params);

    /**
     * MLME-ASSOCIATE.indication handler (invoked by the MAC layer).
     */
    void MlmeAssociateIndication(lrwpan::MlmeAssociateIndicationParams params);

    /**
     * MLME-ASSOCIATE.confirm handler (invoked by the MAC layer).
     */
    void MlmeAssociateConfirm(lrwpan::MlmeAssociateConfirmParams params);

    /**
     * MLME orphan indication handler (invoked by the MAC layer).
     */
    void MlmeOrphanIndication(lrwpan::MlmeOrphanIndicationParams params);

    /**
     * MLME-COMM-STATUS.indication handler (invoked by the MAC layer).
     */
    void MlmeCommStatusIndication(lrwpan::MlmeCommStatusIndicationParams params);

    /**
     * MLME-BEACON-NOTIFY.indication handler (invoked by the MAC layer).
     */
    void MlmeBeaconNotifyIndication(lrwpan::MlmeBeaconNotifyIndicationParams params);

    /**
     * Register a callback for MCPS-DATA.indication.
     */
    void SetMcpsDataIndicationCallback(
        Callback<void, lrwpan::McpsDataIndicationParams, Ptr<Packet>> c);

    /**
     * Register a callback for MCPS-DATA.confirm.
     */
    void SetMcpsDataConfirmCallback(Callback<void, lrwpan::McpsDataConfirmParams> c);

  protected:
    /**
     * Dispose of the Objects used by the NishiokaNwk
     */
    void DoDispose() override;

    /**
     * Initialize of the Objects used by the NishiokaNwk
     */
    void DoInitialize() override;

  private:
    Ptr<lrwpan::LrWpanMacBase> m_mac; //!< The underlying LrWpan MAC connected to this NWK
    Mac64Address m_extendedAddress;   //!< Extended address from MAC PIB
    std::map<Mac16Address, Mac16Address> m_routingTable; //!< Routing table: dst -> nextHop
    Callback<void, lrwpan::McpsDataIndicationParams, Ptr<Packet>>
        m_mcpsDataIndicationCallback; //!< Upper-layer data indication callback
    Callback<void, lrwpan::McpsDataConfirmParams>
        m_mcpsDataConfirmCallback; //!< Upper-layer data confirm callback
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_NWK_H */

