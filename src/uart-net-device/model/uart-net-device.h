/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *    Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */
#ifndef UART_NET_DEVICE_H
#define UART_NET_DEVICE_H

#include "uart-lr-wpan-mac.h"

#include <ns3/net-device.h>
#include <ns3/traced-callback.h>

namespace ns3
{

class Node;

namespace uartnetdevice
{

class UartNetDevice : public NetDevice
{
  public:
    /**
     * Get the type ID.
     *
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    UartNetDevice();
    UartNetDevice(std::string port);
    ~UartNetDevice() override;

    /**
     * Set the MAC to be used by this NetDevice.
     *
     * \param mac the MAC to be used
     */
    void SetMac(Ptr<UartLrWpanMac> mac);

    /**
     * Get the MAC used by this NetDevice.
     *
     * \return the MAC object
     */
    Ptr<UartLrWpanMac> GetMac() const;

    // From class NetDevice
    void SetIfIndex(const uint32_t index) override;
    uint32_t GetIfIndex() const override;
    Ptr<Channel> GetChannel() const override;

    /**
     * This method indirects to LrWpanMac::SetShortAddress ()
     * \param address The short address.
     */
    void SetAddress(Address address) override;

    /**
     * This method indirects to LrWpanMac::SetShortAddress ()
     * \returns The short address.
     */
    Address GetAddress() const override;

    bool SetMtu(const uint16_t mtu) override;
    uint16_t GetMtu() const override;
    bool IsLinkUp() const override;
    void AddLinkChangeCallback(Callback<void> callback) override;
    bool IsBroadcast() const override;
    Address GetBroadcast() const override;
    bool IsMulticast() const override;
    Address GetMulticast(Ipv4Address multicastGroup) const override;
    Address GetMulticast(Ipv6Address addr) const override;
    bool IsBridge() const override;
    bool IsPointToPoint() const override;
    bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber) override;
    bool SendFrom(Ptr<Packet> packet,
                  const Address& source,
                  const Address& dest,
                  uint16_t protocolNumber) override;
    Ptr<Node> GetNode() const override;
    void SetNode(Ptr<Node> node) override;
    bool NeedsArp() const override;

    void SetReceiveCallback(NetDevice::ReceiveCallback cb) override;
    void SetPromiscReceiveCallback(PromiscReceiveCallback cb) override;
    bool SupportsSendFrom() const override;

  private:
    // Inherited from NetDevice/Object
    void DoDispose() override;
    void DoInitialize() override;

    /**
     * Mark NetDevice link as up.
     */
    void LinkUp();

    /**
     * Mark NetDevice link as down.
     */
    void LinkDown();

    /**
     * The MAC for this NetDevice.
     */
    Ptr<UartLrWpanMac> m_mac;

    /**
     * The node associated with this NetDevice.
     */
    Ptr<Node> m_node;

    /**
     * Is the link/device currently up and running?
     */
    bool m_linkUp;

    /**
     * The interface index of this NetDevice.
     */
    uint32_t m_ifIndex;

    /**
     * Trace source for link up/down changes.
     */
    TracedCallback<> m_linkChanges;

    /**
     * Upper layer callback used for notification of new data packet arrivals.
     */
    ReceiveCallback m_receiveCallback;
};

} // namespace uartnetdevice
} // namespace ns3

#endif /* UART_NET_DEVICE_H */
