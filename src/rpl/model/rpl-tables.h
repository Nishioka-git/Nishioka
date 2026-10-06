#ifndef RPL_TABLES_H
#define RPL_TABLES_H

#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/simple-ref-count.h"

#include <cstdint>
#include <vector>

namespace ns3
{

/**
 * @ingroup rpl
 * @brief Origin of a forwarding entry (\RFC{6550} Sections 9 and 10)
 */
enum class RplRouteType : uint8_t
{
    LOCAL = 0,    //!< Connected prefix on this node
    UPWARD = 1,   //!< Default / upward route via preferred parent (Sec. 9)
    DOWNWARD = 2, //!< Destination installed from DAO in storing mode (Sec. 10)
};

/**
 * @ingroup rpl
 * @brief Validity of a forwarding entry
 */
enum class RplRouteStatus : uint8_t
{
    VALID = 0,
    INVALID = 1,
};

/**
 * @ingroup rpl
 * @brief Forwarding-table entry (\RFC{6550} Sections 9 and 10)
 *
 * One table holds local prefixes, upward default routes, and later DAO
 * downward routes. Neighbor / Parent Set state lives in Rpl members
 * (preferred parent, rank), not in a separate table.
 */
class RplRoutingTableEntry : public SimpleRefCount<RplRoutingTableEntry>
{
  public:
    RplRoutingTableEntry();
    RplRoutingTableEntry(Ipv6Address dest,
                         Ipv6Prefix prefix,
                         Ipv6Address nextHop,
                         uint32_t interface,
                         RplRouteType type);
    ~RplRoutingTableEntry();

    void SetDestination(Ipv6Address dest);
    Ipv6Address GetDestination() const;

    void SetPrefix(Ipv6Prefix prefix);
    Ipv6Prefix GetPrefix() const;

    void SetNextHop(Ipv6Address nextHop);
    Ipv6Address GetNextHop() const;

    void SetInterface(uint32_t interface);
    uint32_t GetInterface() const;

    void SetType(RplRouteType type);
    RplRouteType GetType() const;

    void SetStatus(RplRouteStatus status);
    RplRouteStatus GetStatus() const;

    void SetLifeTime(Time lt);
    Time GetLifeTime() const;
    bool IsExpired() const;

    void SetPathSequence(uint8_t seq);
    uint8_t GetPathSequence() const;

    bool Matches(Ipv6Address dest) const;
    void Print(Ptr<OutputStreamWrapper> stream) const;

  private:
    Ipv6Address m_dest;
    Ipv6Prefix m_prefix;
    Ipv6Address m_nextHop;
    uint32_t m_interface;
    RplRouteType m_type;
    RplRouteStatus m_status;
    bool m_hasLifetime;
    Time m_expirationTime;
    uint8_t m_pathSequence;
};

/**
 * @ingroup rpl
 * @brief RPL forwarding table (\RFC{6550} Sections 9 and 10)
 *
 * Stored as a vector. Lookup walks entries for longest-prefix match.
 */
class RplRoutingTable
{
  public:
    RplRoutingTable();

    bool AddEntry(Ptr<RplRoutingTableEntry> entry);

    bool LookUpEntry(Ipv6Address dest,
                     Ptr<RplRoutingTableEntry>& entryFound,
                     int32_t interface = -1);

    bool LookUpExact(Ipv6Address dest,
                     Ipv6Prefix prefix,
                     Ptr<RplRoutingTableEntry>& entryFound);

    void IdentifyExpiredEntries();
    void Purge();
    void Delete(Ipv6Address dest, Ipv6Prefix prefix);
    void DeleteByInterface(uint32_t interface);
    void Dispose();
    void Print(Ptr<OutputStreamWrapper> stream) const;

    uint32_t GetSize() const;
    void SetMaxTableSize(uint32_t size);
    uint32_t GetMaxTableSize() const;

  private:
    std::vector<Ptr<RplRoutingTableEntry>> m_routingTable;
    uint32_t m_maxTableSize;
};

} // namespace ns3

#endif /* RPL_TABLES_H */
