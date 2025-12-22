
/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Simple Data Payload Header implementation
 */

#ifndef DATA_PAYLOAD_HEADER_H
#define DATA_PAYLOAD_HEADER_H

#include "ns3/header.h"

namespace ns3
{
namespace nishioka
{

/**
 * @ingroup nishioka
 * Simple Data Payload Header with ID and Energy fields
 */
class DataPayloadHeader : public Header
{
  public:
    DataPayloadHeader();

    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * Set the ID
     * @param id the ID value
     */
    void SetId(uint32_t id);

    /**
     * Get the ID
     * @return the ID value
     */
    uint32_t GetId() const;

    /**
     * Set the Energy
     * @param energy the energy value
     */
    void SetEnergy(uint32_t energy);

    /**
     * Get the Energy
     * @return the energy value
     */
    uint32_t GetEnergy() const;

  private:
    uint32_t m_id;     //!< ID field
    uint32_t m_energy; //!< Energy field
};

} // namespace nishioka
} // namespace ns3

#endif /* DATA_PAYLOAD_HEADER_H */

