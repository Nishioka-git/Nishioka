/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_STACK_H
#define NISHIOKA_STACK_H

#include "ns3/lr-wpan-net-device.h"
#include "ns3/node.h"
#include "ns3/net-device.h"

#include <stdint.h>
#include <string>

namespace ns3
{
class Node;

namespace nishioka
{

class NishiokaNwk;

/**
 * @ingroup nishioka
 *
 * @brief Protocol stack interface for Nishioka module
 *
 * This class is an encapsulating class representing the protocol stack for
 * the Nishioka module. It provides access to the underlying MAC layer and
 * NetDevice.
 *
 * This is a simplified version without NWK layer for basic testing.
 */
class NishiokaStack : public Object
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
    NishiokaStack();
    ~NishiokaStack() override;

    /**
     * Get the Channel object of the underlying LrWpanNetDevice
     * @return The LrWpanNetDevice Channel Object
     */
    Ptr<Channel> GetChannel() const;

    /**
     * Get the node currently using this NishiokaStack.
     * @return The reference to the node object using this NishiokaStack.
     */
    Ptr<Node> GetNode() const;

    /**
     *  Returns a smart pointer to the underlying NetDevice.
     *
     * @return A smart pointer to the underlying NetDevice.
     */
    Ptr<NetDevice> GetNetDevice() const;

    /**
     * Setup Nishioka to be the next set of higher layers for the specified NetDevice.
     * All the packets incoming and outgoing from the NetDevice will be
     * processed by NishiokaStack.
     *
     * @param netDevice A smart pointer to the NetDevice used by Nishioka.
     */
    void SetNetDevice(Ptr<NetDevice> netDevice);

    /**
     * Get the MAC layer
     *
     * @return A smart pointer to the MAC layer
     */
    Ptr<lrwpan::LrWpanMacBase> GetMac() const;

    /**
     * Get the NWK layer used by this NishiokaStack.
     *
     * @return the NWK object
     */
    Ptr<NishiokaNwk> GetNwk() const;

    /**
     * Set the NWK layer used by this NishiokaStack.
     *
     * @param nwk The NWK layer object
     */
    void SetNwk(Ptr<NishiokaNwk> nwk);

  protected:
    /**
     * Dispose of the Objects used by the NishiokaStack
     */
    void DoDispose() override;

    /**
     * Initialize of the Objects used by the NishiokaStack
     */
    void DoInitialize() override;

  private:
    Ptr<lrwpan::LrWpanMacBase> m_mac;     //!< The underlying LrWpan MAC connected to this Stack
    Ptr<NishiokaNwk> m_nwk;               //!< The Nishioka Network layer
    Ptr<Node> m_node;                     //!< The node associated with this NetDevice
    Ptr<NetDevice> m_netDevice;           //!< Smart pointer to the underlying NetDevice
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_STACK_H */
