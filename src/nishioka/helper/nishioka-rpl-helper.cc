/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-rpl-helper.h"

#include "ns3/log.h"
#include "ns3/lr-wpan-mac.h"
#include "ns3/nishioka-stack.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NishiokaRplHelper");

NishiokaRplHelper::NishiokaRplHelper() = default;

void
NishiokaRplHelper::AttachRplNwk(Ptr<nishioka::NishiokaStack> stack)
{
    NS_ASSERT_MSG(stack, "Null stack");
    NS_ABORT_MSG_IF(stack->IsInitialized(),
                    "NishiokaRplNwk must be attached before stack initialization");

    Ptr<nishioka::NishiokaRplNwk> rplNwk = CreateObject<nishioka::NishiokaRplNwk>();
    stack->SetNwk(rplNwk);
}

nishioka::NishiokaStackContainer
NishiokaRplHelper::Install(NetDeviceContainer netDevices)
{
    nishioka::NishiokaStackContainer stacks = m_nishiokaHelper.Install(netDevices);
    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        AttachRplNwk(stacks.Get(i));
    }
    return stacks;
}

nishioka::NishiokaStackContainer
NishiokaRplHelper::Install(NetDeviceContainer netDevices, const std::vector<Vector>& positions)
{
    nishioka::NishiokaStackContainer stacks = m_nishiokaHelper.Install(netDevices, positions);
    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        AttachRplNwk(stacks.Get(i));
    }
    return stacks;
}

void
NishiokaRplHelper::ConfigureMac(nishioka::NishiokaStackContainer& stacks,
                                 uint8_t channel,
                                 uint16_t panId,
                                 const std::vector<Mac16Address>& addresses)
{
    m_nishiokaHelper.ConfigureMac(stacks, channel, panId, addresses);

    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        Mac16Address addr;
        if (!addresses.empty() && i < addresses.size())
        {
            addr = addresses[i];
        }
        else if (stacks.Get(i)->GetMac())
        {
            addr = stacks.Get(i)->GetMac()->GetShortAddress();
        }
        BindAddresses(stacks.Get(i), panId, addr);
    }
}

void
NishiokaRplHelper::BindAddresses(Ptr<nishioka::NishiokaStack> stack,
                                    uint16_t panId,
                                    Mac16Address shortAddr)
{
    Ptr<nishioka::NishiokaRplNwk> rplNwk = GetRplNwk(stack);
    if (!rplNwk)
    {
        NS_LOG_WARN("BindAddresses: stack has no NishiokaRplNwk");
        return;
    }
    rplNwk->SetPanId(panId);
    rplNwk->SetShortAddress(shortAddr);
}

void
NishiokaRplHelper::ConfigureDodagRoot(Ptr<nishioka::NishiokaStack> stack,
                                       Mac16Address dodagId,
                                       uint8_t instanceId)
{
    Ptr<nishioka::NishiokaRplRouting> rpl = GetRplRouting(stack);
    NS_ABORT_MSG_IF(!rpl, "ConfigureDodagRoot requires NishiokaRplRouting");
    rpl->ConfigureAsRoot(dodagId, instanceId);
}

void
NishiokaRplHelper::StartDodag(Ptr<nishioka::NishiokaStack> stack)
{
    Ptr<nishioka::NishiokaRplRouting> rpl = GetRplRouting(stack);
    NS_ABORT_MSG_IF(!rpl, "StartDodag requires NishiokaRplRouting");
    rpl->StartDodag();
}

NishiokaHelper&
NishiokaRplHelper::GetNishiokaHelper()
{
    return m_nishiokaHelper;
}

const NishiokaHelper&
NishiokaRplHelper::GetNishiokaHelper() const
{
    return m_nishiokaHelper;
}

Ptr<nishioka::NishiokaRplNwk>
NishiokaRplHelper::GetRplNwk(Ptr<nishioka::NishiokaStack> stack)
{
    if (!stack)
    {
        return nullptr;
    }
    return DynamicCast<nishioka::NishiokaRplNwk>(stack->GetNwk());
}

Ptr<nishioka::NishiokaRplRouting>
NishiokaRplHelper::GetRplRouting(Ptr<nishioka::NishiokaStack> stack)
{
    Ptr<nishioka::NishiokaRplNwk> rplNwk = GetRplNwk(stack);
    if (!rplNwk)
    {
        return nullptr;
    }
    return rplNwk->GetRplRouting();
}

void
NishiokaRplHelper::SendData(Ptr<nishioka::NishiokaStack> stack,
                             const std::string& payload,
                             Mac16Address finalDst,
                             uint8_t battery,
                             uint8_t lqi,
                             uint8_t hops)
{
    NS_ABORT_MSG_IF(!stack || !stack->GetNwk() || !stack->GetMac(), "Invalid stack for SendData");

    Mac16Address src = stack->GetMac()->GetShortAddress();
    Mac16Address nextHop;
    if (!stack->GetNwk()->GetNextHop(finalDst, nextHop))
    {
        NS_LOG_ERROR("No route to " << finalDst);
        return;
    }

    Ptr<Packet> packet =
        m_nishiokaHelper.CreatePacket(payload, src, finalDst, 0xCAFE, battery, lqi, hops, 0);

    lrwpan::McpsDataRequestParams params;
    params.m_dstPanId = 0xCAFE;
    params.m_srcAddrMode = lrwpan::SHORT_ADDR;
    params.m_dstAddrMode = lrwpan::SHORT_ADDR;
    params.m_dstAddr = nextHop;
    params.m_msduHandle = 20;
    params.m_txOptions = lrwpan::TX_OPTION_NONE;
    stack->GetMac()->McpsDataRequest(params, packet);
}

} // namespace ns3
