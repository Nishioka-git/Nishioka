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
#include "ns3/object.h"
#include "ns3/packet.h"

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
    std::map<Mac16Address, Mac16Address> m_routingTable; //!< Routing table: dst -> nextHop
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_NWK_H */

