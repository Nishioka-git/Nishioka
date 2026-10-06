#ifndef RPL_TABLES_H
#define RPL_TABLES_H

#include "rpl-header.h"

#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/simple-ref-count.h"

#include <cstdint>
#include <map>
#include <utility>

namespace ns3
{

/**
 * @ingroup rpl
 * @brief Relationship of a neighbor to this node (\RFC{6550} Section 8.2.1)
 *
 * Zigbee stores this on the Neighbor Table (NBR_PARENT / NBR_CHILD).
 * RPL uses the same idea for Neighbor Set, Parent Set, and Preferred Parent.
 */
enum class RplNeighborRelationship : uint8_t
{
    NONE = 0,             //!< Neighbor Set only
    PARENT = 1,           //!< Member of the Parent Set
    PREFERRED_PARENT = 2, //!< Preferred Parent used for upward routing
    CHILD = 3,            //!< Downward child learned from DAO (\RFC{6550} Sec. 10)
};

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
 * @brief Neighbor Set entry (\RFC{6550} Section 8.2.1)
 *
 * Holds a node discovered from DIO (and later DAO).  Parent Set and
 * Preferred Parent are views of this table, matching Zigbee NeighborTable.
 */
class RplNeighborTableEntry : public SimpleRefCount<RplNeighborTableEntry>
{
  public:
    RplNeighborTableEntry();
    /**
     * @param addr Neighbor link-local address (DIO source)
     * @param interface Incoming interface
     * @param rank Advertised Rank
     * @param dodagId DODAGID
     * @param instanceId RPLInstanceID
     */
    RplNeighborTableEntry(Ipv6Address addr,
                          uint32_t interface,
                          uint16_t rank,
                          Ipv6Address dodagId,
                          uint8_t instanceId);
    ~RplNeighborTableEntry();

    void SetAddress(Ipv6Address addr);
    Ipv6Address GetAddress() const;

    void SetInterface(uint32_t interface);
    uint32_t GetInterface() const;

    void SetRank(uint16_t rank);
    uint16_t GetRank() const;

    void SetDodagId(Ipv6Address dodagId);
    Ipv6Address GetDodagId() const;

    void SetRplInstanceId(uint8_t instanceId);
    uint8_t GetRplInstanceId() const;

    void SetVersionNumber(uint8_t version);
    uint8_t GetVersionNumber() const;

    void SetDtsn(uint8_t dtsn);
    uint8_t GetDtsn() const;

    void SetModeOfOperation(ModeOfOperation mop);
    ModeOfOperation GetModeOfOperation() const;

    void SetDodagPreference(uint8_t preference);
    uint8_t GetDodagPreference() const;

    void SetRelationship(RplNeighborRelationship relationship);
    RplNeighborRelationship GetRelationship() const;

    void SetLastHeard(Time t);
    Time GetLastHeard() const;

    /**
     * @brief Refresh last-heard time to now.
     */
    void RefreshLastHeard();

    void Print(Ptr<OutputStreamWrapper> stream) const;

  private:
    Ipv6Address m_address; //!< Neighbor IPv6 (typically link-local)
    uint32_t m_interface;  //!< Interface on which the neighbor was heard
    uint16_t m_rank;       //!< Rank advertised by the neighbor
    Ipv6Address m_dodagId;
    uint8_t m_rplInstanceId;
    uint8_t m_versionNumber;
    uint8_t m_dtsn;
    ModeOfOperation m_mop;
    uint8_t m_dodagPreference;
    RplNeighborRelationship m_relationship;
    Time m_lastHeard;
};

/**
 * @ingroup rpl
 * @brief Forwarding-table entry (\RFC{6550} Sections 9 and 10)
 *
 * Replaces the previous inline RplRouteEntry.  Upward default routes and
 * later DAO downward prefixes share this record, like Zigbee RoutingTableEntry.
 */
class RplRoutingTableEntry : public SimpleRefCount<RplRoutingTableEntry>
{
  public:
    RplRoutingTableEntry();
    /**
     * @param dest Destination prefix
     * @param prefix Prefix length
     * @param nextHop Next hop (zero for on-link)
     * @param interface Output interface
     * @param type Route origin
     */
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

    /**
     * @brief Set remaining lifetime from now (\RFC{6550} Path Lifetime).
     * @param lt Remaining lifetime; Time::Max() means no expiry
     */
    void SetLifeTime(Time lt);
    Time GetLifeTime() const;
    bool IsExpired() const;

    /**
     * @brief DAO Path Sequence (\RFC{6550} Section 6.7.7). Unused until DAO.
     */
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
 * @brief Neighbor Set / Parent Set (\RFC{6550} Section 8.2.1)
 *
 * Analogous to Zigbee NeighborTable.  Route Discovery Table is not used:
 * RPL discovers neighbors from DIO, not RREQ.
 */
class RplNeighborTable
{
  public:
    RplNeighborTable();

    /**
     * @brief Insert a neighbor. Fails if the table is full after purge.
     * @return true if inserted
     */
    bool AddEntry(Ptr<RplNeighborTableEntry> entry);

    /**
     * @brief Insert or refresh a neighbor from a received DIO.
     * @return The stored entry
     */
    Ptr<RplNeighborTableEntry> AddOrUpdate(Ipv6Address addr,
                                           uint32_t interface,
                                           uint16_t rank,
                                           Ipv6Address dodagId,
                                           uint8_t instanceId,
                                           uint8_t versionNumber,
                                           uint8_t dtsn,
                                           ModeOfOperation mop,
                                           uint8_t dodagPreference);

    bool LookUpEntry(Ipv6Address addr, Ptr<RplNeighborTableEntry>& entryFound);

    /**
     * @brief Return the Preferred Parent entry, if any.
     */
    bool GetPreferredParent(Ptr<RplNeighborTableEntry>& entryFound);

    /**
     * @brief Lowest-Rank neighbor in the same DODAG / Instance (OF0-like).
     */
    bool LookUpBestParent(Ipv6Address dodagId,
                          uint8_t instanceId,
                          Ptr<RplNeighborTableEntry>& entryFound);

    /**
     * @brief Mark @p addr as Preferred Parent and demote the previous one.
     */
    bool SetPreferredParent(Ipv6Address addr);

    void Delete(Ipv6Address addr);
    void Dispose();
    void Print(Ptr<OutputStreamWrapper> stream) const;

    uint32_t GetSize() const;
    void SetMaxTableSize(uint32_t size);
    uint32_t GetMaxTableSize() const;

  private:
    /// Neighbor address -> entry
    std::map<Ipv6Address, Ptr<RplNeighborTableEntry>> m_neighborTable;
    uint32_t m_maxTableSize;
};

/**
 * @ingroup rpl
 * @brief RPL forwarding table (\RFC{6550} Sections 9 and 10)
 *
 * Analogous to Zigbee RoutingTable.  Lookup uses longest-prefix match.
 */
class RplRoutingTable
{
  public:
    RplRoutingTable();

    bool AddEntry(Ptr<RplRoutingTableEntry> entry);

    /**
     * @brief Longest-prefix match for @p dest.
     * @param dest Destination address
     * @param entryFound Returned entry
     * @param interface If >= 0, only accept this output interface
     * @return true if a VALID matching route exists
     */
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
    using RouteKey = std::pair<Ipv6Address, uint8_t>; //!< dest + prefix length

    static RouteKey MakeRouteKey(Ipv6Address dest, Ipv6Prefix prefix);

    /// (destination, prefix length) -> entry
    std::map<RouteKey, Ptr<RplRoutingTableEntry>> m_routingTable;
    uint32_t m_maxTableSize;
};

} // namespace ns3

#endif /* RPL_TABLES_H */
