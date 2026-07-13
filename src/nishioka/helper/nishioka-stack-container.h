/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_STACK_CONTAINER_H
#define NISHIOKA_STACK_CONTAINER_H

#include "ns3/nishioka-stack.h"

#include <stdint.h>
#include <vector>

namespace ns3
{
namespace nishioka
{

/**
 * @ingroup nishioka
 *
 * @brief Holds a vector of ns3::NishiokaStack pointers
 *
 * Typically each node has one LrWpanNetDevice (PHY+MAC, from LrWpanHelper) and one
 * NishiokaStack (from NishiokaHelper::Install). For each NetDevice in the container,
 * the helper instantiates a NishiokaStack, aggregates it on the device's Node, and
 * connects the MAC to NishiokaNwk inside NishiokaStack::DoInitialize.
 * For each of these NishiokaStacks, the helper also adds the NishiokaStack into a Container
 * for later use by the caller. This is that container used to hold the Ptr<NishiokaStack> which are
 * instantiated by the NishiokaHelper.
 */
class NishiokaStackContainer
{
  public:
    typedef std::vector<Ptr<NishiokaStack>>::const_iterator
        Iterator; //!< The iterator used in this Container

    /**
     * The default constructor, create an empty NishiokaStackContainer
     */
    NishiokaStackContainer();
    /**
     * Create a NishiokaStackContainer with exactly one NishiokaStack that has previously
     * been instantiated
     *
     * @param stack A NishiokaStack to add to the container
     */
    NishiokaStackContainer(Ptr<NishiokaStack> stack);
    /**
     * Create a NishiokaStackContainer with exactly one stack which has been
     * previously instantiated and assigned a name using the Object name
     * service.  This NishiokaStack is specified by its assigned name.
     *
     * @param stackName The name of the NishiokaStack to add to the container
     *
     * Create a NishiokaStackContainer with exactly one stack
     */
    NishiokaStackContainer(std::string stackName);
    /**
     * Get and iterator which refers to the first NishiokaStack in the container.
     * @return An iterator referring to the first NishiokaStack in the container.
     */
    Iterator Begin() const;
    /**
     * Get an iterator which indicates past the last NishiokaStack in the container.
     * @return An iterator referring to the past the last NishiokaStack in the container.
     */
    Iterator End() const;
    /**
     * Get the number of stacks present in the stack container.
     * @return The number of stacks in the container.
     */
    uint32_t GetN() const;
    /**
     * Get a stack element from the container.
     * @param i The element number in the container
     * @return The nishioka stack element matching the i index in the container.
     */
    Ptr<NishiokaStack> Get(uint32_t i) const;
    /**
     * Append the contents of another NishiokaStackContainer to the end of
     * this container.
     *
     * @param other The NishiokaStackContainer to append.
     */
    void Add(NishiokaStackContainer other);
    /**
     * Append a single Ptr<NishiokaStack> to this container.
     *
     * @param stack The Ptr<NishiokaStack> to append.
     */
    void Add(Ptr<NishiokaStack> stack);
    /**
     * Append to this container the single Ptr<NishiokaStack> referred to
     * via its object name service registered name.
     *
     * @param stackName The name of the NishiokaStack object to add to the container.
     */
    void Add(std::string stackName);

  private:
    std::vector<Ptr<NishiokaStack>> m_stacks; //!< NishiokaStack smart pointers
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_STACK_CONTAINER_H */
