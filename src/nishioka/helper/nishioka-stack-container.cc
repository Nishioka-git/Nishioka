/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-stack-container.h"

#include "ns3/names.h"

namespace ns3
{
namespace nishioka
{

NishiokaStackContainer::NishiokaStackContainer()
{
}

NishiokaStackContainer::NishiokaStackContainer(Ptr<NishiokaStack> stack)
{
    m_stacks.emplace_back(stack);
}

NishiokaStackContainer::NishiokaStackContainer(std::string stackName)
{
    Ptr<NishiokaStack> stack = Names::Find<NishiokaStack>(stackName);
    m_stacks.emplace_back(stack);
}

NishiokaStackContainer::Iterator
NishiokaStackContainer::Begin() const
{
    return m_stacks.begin();
}

NishiokaStackContainer::Iterator
NishiokaStackContainer::End() const
{
    return m_stacks.end();
}

uint32_t
NishiokaStackContainer::GetN() const
{
    return m_stacks.size();
}

Ptr<NishiokaStack>
NishiokaStackContainer::Get(uint32_t i) const
{
    return m_stacks[i];
}

void
NishiokaStackContainer::Add(NishiokaStackContainer other)
{
    for (auto i = other.Begin(); i != other.End(); i++)
    {
        m_stacks.emplace_back(*i);
    }
}

void
NishiokaStackContainer::Add(Ptr<NishiokaStack> stack)
{
    m_stacks.emplace_back(stack);
}

void
NishiokaStackContainer::Add(std::string stackName)
{
    Ptr<NishiokaStack> stack = Names::Find<NishiokaStack>(stackName);
    m_stacks.emplace_back(stack);
}

} // namespace nishioka
} // namespace ns3
