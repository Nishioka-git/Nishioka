/*
 * Copyright (c) 2023 Tokushima University, Japan.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#include "uart-net-device.h"

#include <ns3/abort.h>
#include <ns3/boolean.h>
#include <ns3/log.h>
#include <ns3/node.h>
#include <ns3/packet.h>
#include <ns3/pointer.h>
#include <ns3/spectrum-channel.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UartNetDevice");

NS_OBJECT_ENSURE_REGISTERED(UartNetDevice);

TypeId
UartNetDevice::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UartNetDevice")
                            .SetParent<NetDevice>()
                            .SetGroupName("UartNetDevice")
                            .AddConstructor<UartNetDevice>();
    return tid;
}

UartNetDevice::UartNetDevice()
{
}

UartNetDevice::UartNetDevice(std::string port)
{
    NS_LOG_FUNCTION(this);
    m_mac = CreateObject<UartLrWpanMac>(port);
}

UartNetDevice::~UartNetDevice()
{
    NS_LOG_FUNCTION(this);
}

void
UartNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_mac->Dispose();
    m_mac = nullptr;
    m_node = nullptr;
    // chain up.
    NetDevice::DoDispose();
}

void
UartNetDevice::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    m_mac->Initialize();
    NetDevice::DoInitialize();
}

void
UartNetDevice::SetMac(Ptr<UartLrWpanMac> mac)
{
    m_mac = mac;
}

Ptr<UartLrWpanMac>
UartNetDevice::GetMac() const
{
    return m_mac;
}

void
UartNetDevice::SetIfIndex(const uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    m_ifIndex = index;
}

uint32_t
UartNetDevice::GetIfIndex() const
{
    NS_LOG_FUNCTION(this);
    return m_ifIndex;
}

Ptr<Channel>
UartNetDevice::GetChannel() const
{
    NS_ABORT_MSG("Unsupported");
    return nullptr;
}

void
UartNetDevice::LinkUp()
{
    NS_LOG_FUNCTION(this);
    m_linkUp = true;
}

void
UartNetDevice::LinkDown()
{
    NS_LOG_FUNCTION(this);
    m_linkUp = false;
}

void
UartNetDevice::SetAddress(Address address)
{
    NS_LOG_FUNCTION(this);
    if (Mac16Address::IsMatchingType(address))
    {
        //  m_mac->SetShortAddress(Mac16Address::ConvertFrom(address));
    }
    else
    {
        NS_ABORT_MSG("UartNetDevice::SetAddress - address is not of a compatible type");
    }
}

Address
UartNetDevice::GetAddress() const
{
    NS_ABORT_MSG("Unsupported");
    return Mac16Address::GetBroadcast();
}

bool
UartNetDevice::SetMtu(const uint16_t mtu)
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

uint16_t
UartNetDevice::GetMtu() const
{
    NS_ABORT_MSG("Unsupported");
    return 0;
}

bool
UartNetDevice::IsLinkUp() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

void
UartNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    NS_LOG_FUNCTION(this);
    m_linkChanges.ConnectWithoutContext(callback);
}

bool
UartNetDevice::IsBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

Address
UartNetDevice::GetBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return Mac16Address::GetBroadcast();
}

bool
UartNetDevice::IsMulticast() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

Address
UartNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    NS_ABORT_MSG("Unsupported");
    return Address();
}

Address
UartNetDevice::GetMulticast(Ipv6Address addr) const
{
    NS_ABORT_MSG("Unsupported");
    return Mac16Address::GetBroadcast();
}

bool
UartNetDevice::IsBridge() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

bool
UartNetDevice::IsPointToPoint() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

bool
UartNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    return true;
}

bool
UartNetDevice::SendFrom(Ptr<Packet> packet,
                        const Address& source,
                        const Address& dest,
                        uint16_t protocolNumber)
{
    NS_ABORT_MSG("Unsupported");
    // TODO: To support SendFrom, the MACs McpsDataRequest has to use the provided source address,
    // instead of to local one.
    return false;
}

Ptr<Node>
UartNetDevice::GetNode() const
{
    NS_LOG_FUNCTION(this);
    return m_node;
}

void
UartNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this);
    m_node = node;
}

bool
UartNetDevice::NeedsArp() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

void
UartNetDevice::SetReceiveCallback(ReceiveCallback cb)
{
    NS_LOG_FUNCTION(this);
    m_receiveCallback = cb;
}

void
UartNetDevice::SetPromiscReceiveCallback(PromiscReceiveCallback cb)
{
    // This method basically assumes an 802.3-compliant device, but a raw
    // 802.15.4 device does not have an ethertype, and requires specific
    // McpsDataIndication parameters.
    // For further study:  how to support these methods somehow, such as
    // inventing a fake ethertype and packet tag for McpsDataRequest
    NS_LOG_WARN("Unsupported; use LrWpan MAC APIs instead");
}

bool
UartNetDevice::SupportsSendFrom() const
{
    NS_LOG_FUNCTION_NOARGS();
    return false;
}

} // namespace ns3
