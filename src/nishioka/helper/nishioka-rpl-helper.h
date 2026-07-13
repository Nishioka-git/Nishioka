/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_RPL_HELPER_H
#define NISHIOKA_RPL_HELPER_H

#include "ns3/nishioka-helper.h"
#include "ns3/nishioka-rpl-nwk.h"
#include "ns3/nishioka-rpl-routing.h"
#include "ns3/nishioka-stack-container.h"

#include "ns3/net-device-container.h"
#include "ns3/vector.h"

#include <vector>

namespace ns3
{

/**
 * @ingroup nishioka
 *
 * @brief Helper for RPL-like routing on top of the Nishioka protocol stack.
 *
 * Relationship:
 * - **NishiokaRplHelper** (this): simulation-facing API for DODAG setup and RPL control.
 * - **NishiokaHelper** (member): packet/header utilities and MAC configuration.
 * - **NishiokaStack** + **NishiokaRplNwk**: per-node stack; NWK dispatches RPL control frames.
 * - **NishiokaRplRouting**: per-node RPL state machine (DIO/DAO/DAO-ACK).
 *
 * Install() creates NishiokaStack instances and replaces the default NishiokaNwk with
 * NishiokaRplNwk before initialization.
 */
class NishiokaRplHelper
{
  public:
    NishiokaRplHelper();

    /**
     * Install NishiokaStack with NishiokaRplNwk on each NetDevice.
     */
    nishioka::NishiokaStackContainer Install(NetDeviceContainer netDevices);

    /**
     * Install stacks and set node positions (delegates mobility to NishiokaHelper).
     */
    nishioka::NishiokaStackContainer Install(NetDeviceContainer netDevices,
                                              const std::vector<Vector>& positions);

    /**
     * Configure MAC on all stacks (delegates to NishiokaHelper::ConfigureMac).
     */
    void ConfigureMac(nishioka::NishiokaStackContainer& stacks,
                      uint8_t channel = 0xD,
                      uint16_t panId = 0xCAFE,
                      const std::vector<Mac16Address>& addresses = std::vector<Mac16Address>());

    /**
     * Bind PAN ID and short address to the RPL engine of a stack.
     */
    void BindAddresses(Ptr<nishioka::NishiokaStack> stack, uint16_t panId, Mac16Address shortAddr);

    /**
     * Configure a node as DODAG root and start periodic DIO transmission.
     */
    void ConfigureDodagRoot(Ptr<nishioka::NishiokaStack> stack,
                            Mac16Address dodagId,
                            uint8_t instanceId = 0);

    /**
     * Start DIO on root (alias for NishiokaRplRouting::StartDodag).
     */
    void StartDodag(Ptr<nishioka::NishiokaStack> stack);

    /**
     * Access underlying NishiokaHelper (packet create/extract, seq num, etc.).
     */
    NishiokaHelper& GetNishiokaHelper();
    const NishiokaHelper& GetNishiokaHelper() const;

    /**
     * Get NishiokaRplNwk from a stack (nullptr if not RPL-enabled).
     */
    static Ptr<nishioka::NishiokaRplNwk> GetRplNwk(Ptr<nishioka::NishiokaStack> stack);

    /**
     * Get NishiokaRplRouting from a stack.
     */
    static Ptr<nishioka::NishiokaRplRouting> GetRplRouting(Ptr<nishioka::NishiokaStack> stack);

    /**
     * Send application data using NishiokaHeader::CUSTOM_DATA and NWK route lookup.
     */
    void SendData(Ptr<nishioka::NishiokaStack> stack,
                  const std::string& payload,
                  Mac16Address finalDst,
                  uint8_t battery = 100,
                  uint8_t lqi = 255,
                  uint8_t hops = 0);

  private:
    void AttachRplNwk(Ptr<nishioka::NishiokaStack> stack);

    NishiokaHelper m_nishiokaHelper;
};

} // namespace ns3

#endif /* NISHIOKA_RPL_HELPER_H */
