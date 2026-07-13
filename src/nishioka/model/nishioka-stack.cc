/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-stack.h"

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
{
    NS_LOG_FUNCTION(this);
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
    m_nwk = nullptr;
    m_mac = nullptr;
    Object::DoDispose();
}

void
NishiokaStack::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_UNLESS(m_nwk, "NishiokaNwk not found when attempting to install NishiokaStack");

    AggregateObject(m_nwk);

    NS_ABORT_MSG_UNLESS(m_netDevice,
                        "Invalid NetDevice found when attempting to install NishiokaStack");

    // Make sure the NetDevice is previously initialized before using NishiokaStack
    m_netDevice->Initialize();

    m_mac = m_netDevice->GetObject<LrWpanMacBase>();
    NS_ABORT_MSG_UNLESS(m_mac,
                        "No valid LrWpanMacBase found in this NetDevice, cannot use NishiokaStack");

    // Set NWK callback hooks with the MAC (same pattern as ZigbeeStack)
    m_nwk->SetMac(m_mac);
    m_mac->SetMcpsDataIndicationCallback(MakeCallback(&NishiokaNwk::McpsDataIndication, m_nwk));
    m_mac->SetMlmeOrphanIndicationCallback(MakeCallback(&NishiokaNwk::MlmeOrphanIndication, m_nwk));
    m_mac->SetMlmeCommStatusIndicationCallback(
        MakeCallback(&NishiokaNwk::MlmeCommStatusIndication, m_nwk));
    m_mac->SetMlmeBeaconNotifyIndicationCallback(
        MakeCallback(&NishiokaNwk::MlmeBeaconNotifyIndication, m_nwk));
    m_mac->SetMlmeAssociateIndicationCallback(
        MakeCallback(&NishiokaNwk::MlmeAssociateIndication, m_nwk));
    m_mac->SetMcpsDataConfirmCallback(MakeCallback(&NishiokaNwk::McpsDataConfirm, m_nwk));
    m_mac->SetMlmeScanConfirmCallback(MakeCallback(&NishiokaNwk::MlmeScanConfirm, m_nwk));
    m_mac->SetMlmeStartConfirmCallback(MakeCallback(&NishiokaNwk::MlmeStartConfirm, m_nwk));
    m_mac->SetMlmeSetConfirmCallback(MakeCallback(&NishiokaNwk::MlmeSetConfirm, m_nwk));
    m_mac->SetMlmeGetConfirmCallback(MakeCallback(&NishiokaNwk::MlmeGetConfirm, m_nwk));
    m_mac->SetMlmeAssociateConfirmCallback(MakeCallback(&NishiokaNwk::MlmeAssociateConfirm, m_nwk));

    // Obtain extended address as soon as NWK is set to begin operations
    m_mac->MlmeGetRequest(MacPibAttributeIdentifier::macExtendedAddress);

    NS_LOG_INFO("NishiokaStack initialized: Node=" << m_node->GetId() << " NetDevice=" << m_netDevice);

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
    NS_ABORT_MSG_UNLESS(netDevice, "Invalid NetDevice passed to NishiokaStack::SetNetDevice");
    m_netDevice = netDevice;
    m_node = m_netDevice->GetNode();
    NS_ABORT_MSG_UNLESS(m_node,
                        "NetDevice must be attached to a Node before NishiokaStack::SetNetDevice");
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
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(NishiokaStack::IsInitialized(), "NWK layer cannot be set after initialization");
    m_nwk = nwk;
}

} // namespace nishioka
} // namespace ns3
