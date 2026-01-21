/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-stack.h"
#include "nishioka-nwk.h"

#include "ns3/channel.h"
#include "ns3/log.h"
#include "ns3/lr-wpan-mac-base.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

using namespace ns3::lrwpan;

namespace ns3
{
namespace nishioka
{

NS_LOG_COMPONENT_DEFINE("NishiokaStack");
NS_OBJECT_ENSURE_REGISTERED(NishiokaStack);

TypeId
NishiokaStack::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::NishiokaStack")
                            .SetParent<Object>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<NishiokaStack>();
    return tid;
}

NishiokaStack::NishiokaStack()
    : m_nwk(nullptr)
{
    NS_LOG_FUNCTION(this);
    // Create NWK layer by default
    m_nwk = CreateObject<NishiokaNwk>();
}

NishiokaStack::~NishiokaStack()
{
    NS_LOG_FUNCTION(this);
}

void
NishiokaStack::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_netDevice = nullptr;
    m_node = nullptr;
    m_mac = nullptr;
    m_nwk = nullptr;
    Object::DoDispose();
}

void
NishiokaStack::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_UNLESS(m_netDevice,
                        "Invalid NetDevice found when attempting to install NishiokaStack");

    // Make sure the NetDevice is previously initialized
    // before using NishiokaStack
    m_netDevice->Initialize();

    // Get MAC layer from NetDevice
    m_mac = m_netDevice->GetObject<LrWpanMacBase>();
    NS_ABORT_MSG_UNLESS(m_mac,
                        "No valid LrWpanMacBase found in this NetDevice, cannot use NishiokaStack");

    // Aggregate NWK layer to the node
    if (m_nwk)
    {
        m_node->AggregateObject(m_nwk);
        m_nwk->SetMac(m_mac);
        NS_LOG_INFO("NishiokaNwk aggregated to node " << m_node->GetId());
    }

    NS_LOG_INFO("NishiokaStack initialized: Node=" << m_node->GetId()
                                                    << " NetDevice=" << m_netDevice
                                                    << " NWK=" << (m_nwk ? "enabled" : "disabled"));

    Object::DoInitialize();
}

Ptr<Channel>
NishiokaStack::GetChannel() const
{
    return m_netDevice->GetChannel();
}

Ptr<Node>
NishiokaStack::GetNode() const
{
    return m_node;
}

Ptr<NetDevice>
NishiokaStack::GetNetDevice() const
{
    return m_netDevice;
}

void
NishiokaStack::SetNetDevice(Ptr<NetDevice> netDevice)
{
    NS_LOG_FUNCTION(this << netDevice);
    m_netDevice = netDevice;
    m_node = m_netDevice->GetNode();
}

Ptr<LrWpanMacBase>
NishiokaStack::GetMac() const
{
    return m_mac;
}

Ptr<NishiokaNwk>
NishiokaStack::GetNwk() const
{
    return m_nwk;
}

void
NishiokaStack::SetNwk(Ptr<NishiokaNwk> nwk)
{
    NS_LOG_FUNCTION(this << nwk);
    m_nwk = nwk;
}

} // namespace nishioka
} // namespace ns3
