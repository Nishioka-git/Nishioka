#include "rpl-tables.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include <iomanip>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RplTables");

/***********************************************************
 *                Neighbor Table Entry
 ***********************************************************/

RplNeighborTableEntry::RplNeighborTableEntry()
    : m_address(Ipv6Address::GetZero()),
      m_interface(0),
      m_rank(0),
      m_dodagId(Ipv6Address::GetZero()),
      m_rplInstanceId(0),
      m_versionNumber(0),
      m_dtsn(0),
      m_mop(ModeOfOperation::NO_DOWNWARD_ROUTES),
      m_dodagPreference(0),
      m_relationship(RplNeighborRelationship::NONE),
      m_lastHeard(Seconds(0))
{
}

RplNeighborTableEntry::RplNeighborTableEntry(Ipv6Address addr,
                                             uint32_t interface,
                                             uint16_t rank,
                                             Ipv6Address dodagId,
                                             uint8_t instanceId)
    : m_address(addr),
      m_interface(interface),
      m_rank(rank),
      m_dodagId(dodagId),
      m_rplInstanceId(instanceId),
      m_versionNumber(0),
      m_dtsn(0),
      m_mop(ModeOfOperation::NO_DOWNWARD_ROUTES),
      m_dodagPreference(0),
      m_relationship(RplNeighborRelationship::NONE),
      m_lastHeard(Simulator::Now())
{
}

RplNeighborTableEntry::~RplNeighborTableEntry()
{
}

void
RplNeighborTableEntry::SetAddress(Ipv6Address addr)
{
    m_address = addr;
}

Ipv6Address
RplNeighborTableEntry::GetAddress() const
{
    return m_address;
}

void
RplNeighborTableEntry::SetInterface(uint32_t interface)
{
    m_interface = interface;
}

uint32_t
RplNeighborTableEntry::GetInterface() const
{
    return m_interface;
}

void
RplNeighborTableEntry::SetRank(uint16_t rank)
{
    m_rank = rank;
}

uint16_t
RplNeighborTableEntry::GetRank() const
{
    return m_rank;
}

void
RplNeighborTableEntry::SetDodagId(Ipv6Address dodagId)
{
    m_dodagId = dodagId;
}

Ipv6Address
RplNeighborTableEntry::GetDodagId() const
{
    return m_dodagId;
}

void
RplNeighborTableEntry::SetRplInstanceId(uint8_t instanceId)
{
    m_rplInstanceId = instanceId;
}

uint8_t
RplNeighborTableEntry::GetRplInstanceId() const
{
    return m_rplInstanceId;
}

void
RplNeighborTableEntry::SetVersionNumber(uint8_t version)
{
    m_versionNumber = version;
}

uint8_t
RplNeighborTableEntry::GetVersionNumber() const
{
    return m_versionNumber;
}

void
RplNeighborTableEntry::SetDtsn(uint8_t dtsn)
{
    m_dtsn = dtsn;
}

uint8_t
RplNeighborTableEntry::GetDtsn() const
{
    return m_dtsn;
}

void
RplNeighborTableEntry::SetModeOfOperation(ModeOfOperation mop)
{
    m_mop = mop;
}

ModeOfOperation
RplNeighborTableEntry::GetModeOfOperation() const
{
    return m_mop;
}

void
RplNeighborTableEntry::SetDodagPreference(uint8_t preference)
{
    m_dodagPreference = preference;
}

uint8_t
RplNeighborTableEntry::GetDodagPreference() const
{
    return m_dodagPreference;
}

void
RplNeighborTableEntry::SetRelationship(RplNeighborRelationship relationship)
{
    m_relationship = relationship;
}

RplNeighborRelationship
RplNeighborTableEntry::GetRelationship() const
{
    return m_relationship;
}

void
RplNeighborTableEntry::SetLastHeard(Time t)
{
    m_lastHeard = t;
}

Time
RplNeighborTableEntry::GetLastHeard() const
{
    return m_lastHeard;
}

void
RplNeighborTableEntry::RefreshLastHeard()
{
    m_lastHeard = Simulator::Now();
}

void
RplNeighborTableEntry::Print(Ptr<OutputStreamWrapper> stream) const
{
    std::ostream* os = stream->GetStream();
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    std::ostringstream addr;
    std::ostringstream dodag;
    addr << m_address;
    dodag << m_dodagId;

    const char* rel = "NONE";
    switch (m_relationship)
    {
    case RplNeighborRelationship::PARENT:
        rel = "PARENT";
        break;
    case RplNeighborRelationship::PREFERRED_PARENT:
        rel = "PREFERRED";
        break;
    case RplNeighborRelationship::CHILD:
        rel = "CHILD";
        break;
    case RplNeighborRelationship::NONE:
    default:
        rel = "NONE";
        break;
    }

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);
    *os << std::setw(28) << addr.str();
    *os << std::setw(8) << m_interface;
    *os << std::setw(8) << m_rank;
    *os << std::setw(12) << rel;
    *os << std::setw(28) << dodag.str();
    *os << std::setw(10) << int(m_rplInstanceId);
    *os << std::setw(10) << m_lastHeard.GetSeconds();
    *os << std::endl;
    (*os).copyfmt(oldState);
}

/***********************************************************
 *                Neighbor Table
 ***********************************************************/

RplNeighborTable::RplNeighborTable()
    : m_maxTableSize(32)
{
}

bool
RplNeighborTable::AddEntry(Ptr<RplNeighborTableEntry> entry)
{
    if (!entry)
    {
        return false;
    }
    if (m_neighborTable.find(entry->GetAddress()) != m_neighborTable.end())
    {
        return false;
    }
    if (m_neighborTable.size() >= m_maxTableSize)
    {
        return false;
    }
    m_neighborTable[entry->GetAddress()] = entry;
    return true;
}

Ptr<RplNeighborTableEntry>
RplNeighborTable::AddOrUpdate(Ipv6Address addr,
                              uint32_t interface,
                              uint16_t rank,
                              Ipv6Address dodagId,
                              uint8_t instanceId,
                              uint8_t versionNumber,
                              uint8_t dtsn,
                              ModeOfOperation mop,
                              uint8_t dodagPreference)
{
    NS_LOG_FUNCTION(this << addr << interface << rank);

    Ptr<RplNeighborTableEntry> found;
    if (LookUpEntry(addr, found))
    {
        found->SetInterface(interface);
        found->SetRank(rank);
        found->SetDodagId(dodagId);
        found->SetRplInstanceId(instanceId);
        found->SetVersionNumber(versionNumber);
        found->SetDtsn(dtsn);
        found->SetModeOfOperation(mop);
        found->SetDodagPreference(dodagPreference);
        found->RefreshLastHeard();
        return found;
    }

    Ptr<RplNeighborTableEntry> entry =
        Create<RplNeighborTableEntry>(addr, interface, rank, dodagId, instanceId);
    entry->SetVersionNumber(versionNumber);
    entry->SetDtsn(dtsn);
    entry->SetModeOfOperation(mop);
    entry->SetDodagPreference(dodagPreference);
    if (!AddEntry(entry))
    {
        NS_LOG_WARN("Neighbor table full, cannot add " << addr);
        return nullptr;
    }
    return entry;
}

bool
RplNeighborTable::LookUpEntry(Ipv6Address addr, Ptr<RplNeighborTableEntry>& entryFound)
{
    NS_LOG_FUNCTION(this << addr);

    auto it = m_neighborTable.find(addr);
    if (it == m_neighborTable.end())
    {
        return false;
    }
    entryFound = it->second;
    return true;
}

bool
RplNeighborTable::GetPreferredParent(Ptr<RplNeighborTableEntry>& entryFound)
{
    NS_LOG_FUNCTION(this);

    for (const auto& [addr, entry] : m_neighborTable)
    {
        if (entry->GetRelationship() == RplNeighborRelationship::PREFERRED_PARENT)
        {
            entryFound = entry;
            return true;
        }
    }
    return false;
}

bool
RplNeighborTable::LookUpBestParent(Ipv6Address dodagId,
                                   uint8_t instanceId,
                                   Ptr<RplNeighborTableEntry>& entryFound)
{
    NS_LOG_FUNCTION(this << dodagId << int(instanceId));

    bool found = false;
    uint16_t bestRank = 0;
    for (const auto& [addr, entry] : m_neighborTable)
    {
        if (entry->GetRplInstanceId() != instanceId)
        {
            continue;
        }
        if (!dodagId.IsAny() && entry->GetDodagId() != dodagId)
        {
            continue;
        }
        if (!found || entry->GetRank() < bestRank)
        {
            entryFound = entry;
            bestRank = entry->GetRank();
            found = true;
        }
    }
    return found;
}

bool
RplNeighborTable::SetPreferredParent(Ipv6Address addr)
{
    NS_LOG_FUNCTION(this << addr);

    bool marked = false;
    for (auto& [storedAddr, entry] : m_neighborTable)
    {
        if (storedAddr == addr)
        {
            entry->SetRelationship(RplNeighborRelationship::PREFERRED_PARENT);
            marked = true;
        }
        else if (entry->GetRelationship() == RplNeighborRelationship::PREFERRED_PARENT)
        {
            entry->SetRelationship(RplNeighborRelationship::PARENT);
        }
    }
    return marked;
}

void
RplNeighborTable::Delete(Ipv6Address addr)
{
    m_neighborTable.erase(addr);
}

void
RplNeighborTable::Dispose()
{
    m_neighborTable.clear();
}

void
RplNeighborTable::Print(Ptr<OutputStreamWrapper> stream) const
{
    std::ostream* os = stream->GetStream();
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);
    *os << "RPL Neighbor table (RFC 6550 Neighbor / Parent Set)\n";
    *os << std::setw(28) << "Neighbor";
    *os << std::setw(8) << "If";
    *os << std::setw(8) << "Rank";
    *os << std::setw(12) << "Relation";
    *os << std::setw(28) << "DODAGID";
    *os << std::setw(10) << "Instance";
    *os << std::setw(10) << "Heard(s)";
    *os << std::endl;

    for (const auto& [addr, entry] : m_neighborTable)
    {
        entry->Print(stream);
    }
    *os << std::endl;
    (*os).copyfmt(oldState);
}

uint32_t
RplNeighborTable::GetSize() const
{
    return static_cast<uint32_t>(m_neighborTable.size());
}

void
RplNeighborTable::SetMaxTableSize(uint32_t size)
{
    m_maxTableSize = size;
}

uint32_t
RplNeighborTable::GetMaxTableSize() const
{
    return m_maxTableSize;
}

/***********************************************************
 *                Routing Table Entry
 ***********************************************************/

RplRoutingTableEntry::RplRoutingTableEntry()
    : m_dest(Ipv6Address::GetZero()),
      m_prefix(Ipv6Prefix::GetZero()),
      m_nextHop(Ipv6Address::GetZero()),
      m_interface(0),
      m_type(RplRouteType::LOCAL),
      m_status(RplRouteStatus::VALID),
      m_hasLifetime(false),
      m_expirationTime(Time::Max()),
      m_pathSequence(0)
{
}

RplRoutingTableEntry::RplRoutingTableEntry(Ipv6Address dest,
                                           Ipv6Prefix prefix,
                                           Ipv6Address nextHop,
                                           uint32_t interface,
                                           RplRouteType type)
    : m_dest(dest),
      m_prefix(prefix),
      m_nextHop(nextHop),
      m_interface(interface),
      m_type(type),
      m_status(RplRouteStatus::VALID),
      m_hasLifetime(false),
      m_expirationTime(Time::Max()),
      m_pathSequence(0)
{
}

RplRoutingTableEntry::~RplRoutingTableEntry()
{
}

void
RplRoutingTableEntry::SetDestination(Ipv6Address dest)
{
    m_dest = dest;
}

Ipv6Address
RplRoutingTableEntry::GetDestination() const
{
    return m_dest;
}

void
RplRoutingTableEntry::SetPrefix(Ipv6Prefix prefix)
{
    m_prefix = prefix;
}

Ipv6Prefix
RplRoutingTableEntry::GetPrefix() const
{
    return m_prefix;
}

void
RplRoutingTableEntry::SetNextHop(Ipv6Address nextHop)
{
    m_nextHop = nextHop;
}

Ipv6Address
RplRoutingTableEntry::GetNextHop() const
{
    return m_nextHop;
}

void
RplRoutingTableEntry::SetInterface(uint32_t interface)
{
    m_interface = interface;
}

uint32_t
RplRoutingTableEntry::GetInterface() const
{
    return m_interface;
}

void
RplRoutingTableEntry::SetType(RplRouteType type)
{
    m_type = type;
}

RplRouteType
RplRoutingTableEntry::GetType() const
{
    return m_type;
}

void
RplRoutingTableEntry::SetStatus(RplRouteStatus status)
{
    m_status = status;
}

RplRouteStatus
RplRoutingTableEntry::GetStatus() const
{
    return m_status;
}

void
RplRoutingTableEntry::SetLifeTime(Time lt)
{
    if (lt == Time::Max())
    {
        m_hasLifetime = false;
        m_expirationTime = Time::Max();
        return;
    }
    m_hasLifetime = true;
    m_expirationTime = Simulator::Now() + lt;
}

Time
RplRoutingTableEntry::GetLifeTime() const
{
    if (!m_hasLifetime)
    {
        return Time::Max();
    }
    if (Simulator::Now() >= m_expirationTime)
    {
        return Seconds(0);
    }
    return m_expirationTime - Simulator::Now();
}

bool
RplRoutingTableEntry::IsExpired() const
{
    return m_hasLifetime && Simulator::Now() >= m_expirationTime;
}

void
RplRoutingTableEntry::SetPathSequence(uint8_t seq)
{
    m_pathSequence = seq;
}

uint8_t
RplRoutingTableEntry::GetPathSequence() const
{
    return m_pathSequence;
}

bool
RplRoutingTableEntry::Matches(Ipv6Address dest) const
{
    return m_prefix.IsMatch(dest, m_dest);
}

void
RplRoutingTableEntry::Print(Ptr<OutputStreamWrapper> stream) const
{
    std::ostream* os = stream->GetStream();
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    std::ostringstream dest;
    std::ostringstream nextHop;
    dest << m_dest << "/" << int(m_prefix.GetPrefixLength());
    nextHop << m_nextHop;

    const char* type = "LOCAL";
    switch (m_type)
    {
    case RplRouteType::UPWARD:
        type = "UPWARD";
        break;
    case RplRouteType::DOWNWARD:
        type = "DOWNWARD";
        break;
    case RplRouteType::LOCAL:
    default:
        type = "LOCAL";
        break;
    }

    const char* status = (m_status == RplRouteStatus::VALID) ? "VALID" : "INVALID";

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);
    *os << std::setw(32) << dest.str();
    *os << std::setw(28) << nextHop.str();
    *os << std::setw(8) << m_interface;
    *os << std::setw(10) << type;
    *os << std::setw(10) << status;
    *os << std::endl;
    (*os).copyfmt(oldState);
}

/***********************************************************
 *                Routing Table
 ***********************************************************/

RplRoutingTable::RplRoutingTable()
    : m_maxTableSize(32)
{
}

RplRoutingTable::RouteKey
RplRoutingTable::MakeRouteKey(Ipv6Address dest, Ipv6Prefix prefix)
{
    return RouteKey(dest, prefix.GetPrefixLength());
}

bool
RplRoutingTable::AddEntry(Ptr<RplRoutingTableEntry> entry)
{
    Purge();
    if (!entry)
    {
        return false;
    }
    RouteKey key = MakeRouteKey(entry->GetDestination(), entry->GetPrefix());
    if (m_routingTable.find(key) != m_routingTable.end())
    {
        return false;
    }
    if (m_routingTable.size() >= m_maxTableSize)
    {
        return false;
    }
    m_routingTable[key] = entry;
    return true;
}

bool
RplRoutingTable::LookUpEntry(Ipv6Address dest,
                             Ptr<RplRoutingTableEntry>& entryFound,
                             int32_t interface)
{
    NS_LOG_FUNCTION(this << dest << interface);

    IdentifyExpiredEntries();

    bool found = false;
    uint16_t longestMask = 0;
    for (const auto& [key, entry] : m_routingTable)
    {
        if (entry->GetStatus() != RplRouteStatus::VALID)
        {
            continue;
        }
        if (interface >= 0 && static_cast<uint32_t>(interface) != entry->GetInterface())
        {
            continue;
        }
        if (!entry->Matches(dest))
        {
            continue;
        }
        uint16_t maskLen = entry->GetPrefix().GetPrefixLength();
        if (!found || maskLen >= longestMask)
        {
            longestMask = maskLen;
            entryFound = entry;
            found = true;
        }
    }
    return found;
}

bool
RplRoutingTable::LookUpExact(Ipv6Address dest,
                             Ipv6Prefix prefix,
                             Ptr<RplRoutingTableEntry>& entryFound)
{
    auto it = m_routingTable.find(MakeRouteKey(dest, prefix));
    if (it == m_routingTable.end())
    {
        return false;
    }
    entryFound = it->second;
    return true;
}

void
RplRoutingTable::IdentifyExpiredEntries()
{
    for (auto& [key, entry] : m_routingTable)
    {
        if (entry->IsExpired())
        {
            entry->SetStatus(RplRouteStatus::INVALID);
        }
    }
}

void
RplRoutingTable::Purge()
{
    IdentifyExpiredEntries();
    std::erase_if(m_routingTable, [](const auto& item) {
        return item.second->GetStatus() == RplRouteStatus::INVALID;
    });
}

void
RplRoutingTable::Delete(Ipv6Address dest, Ipv6Prefix prefix)
{
    m_routingTable.erase(MakeRouteKey(dest, prefix));
}

void
RplRoutingTable::DeleteByInterface(uint32_t interface)
{
    std::erase_if(m_routingTable, [interface](const auto& item) {
        return item.second->GetInterface() == interface;
    });
}

void
RplRoutingTable::Dispose()
{
    m_routingTable.clear();
}

void
RplRoutingTable::Print(Ptr<OutputStreamWrapper> stream) const
{
    std::ostream* os = stream->GetStream();
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);
    *os << "RPL Routing table (RFC 6550 upward / downward)\n";
    *os << std::setw(32) << "Destination";
    *os << std::setw(28) << "Next hop";
    *os << std::setw(8) << "If";
    *os << std::setw(10) << "Type";
    *os << std::setw(10) << "Status";
    *os << std::endl;

    for (const auto& [key, entry] : m_routingTable)
    {
        entry->Print(stream);
    }
    *os << std::endl;
    (*os).copyfmt(oldState);
}

uint32_t
RplRoutingTable::GetSize() const
{
    return static_cast<uint32_t>(m_routingTable.size());
}

void
RplRoutingTable::SetMaxTableSize(uint32_t size)
{
    m_maxTableSize = size;
}

uint32_t
RplRoutingTable::GetMaxTableSize() const
{
    return m_maxTableSize;
}

} // namespace ns3
