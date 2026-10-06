#include "rpl-tables.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RplTables");

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

bool
RplRoutingTable::AddEntry(Ptr<RplRoutingTableEntry> entry)
{
    Purge();
    if (!entry)
    {
        return false;
    }
    Ptr<RplRoutingTableEntry> existing;
    if (LookUpExact(entry->GetDestination(), entry->GetPrefix(), existing))
    {
        return false;
    }
    if (m_routingTable.size() >= m_maxTableSize)
    {
        return false;
    }
    m_routingTable.push_back(entry);
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
    for (const auto& entry : m_routingTable)
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
    for (const auto& entry : m_routingTable)
    {
        if (entry->GetDestination() == dest && entry->GetPrefix() == prefix)
        {
            entryFound = entry;
            return true;
        }
    }
    return false;
}

void
RplRoutingTable::IdentifyExpiredEntries()
{
    for (const auto& entry : m_routingTable)
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
    std::erase_if(m_routingTable, [](const Ptr<RplRoutingTableEntry>& entry) {
        return entry->GetStatus() == RplRouteStatus::INVALID;
    });
}

void
RplRoutingTable::Delete(Ipv6Address dest, Ipv6Prefix prefix)
{
    std::erase_if(m_routingTable, [&dest, &prefix](const Ptr<RplRoutingTableEntry>& entry) {
        return entry->GetDestination() == dest && entry->GetPrefix() == prefix;
    });
}

void
RplRoutingTable::DeleteByInterface(uint32_t interface)
{
    std::erase_if(m_routingTable, [interface](const Ptr<RplRoutingTableEntry>& entry) {
        return entry->GetInterface() == interface;
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
    *os << "RPL Routing table\n";
    *os << std::setw(32) << "Destination";
    *os << std::setw(28) << "Next hop";
    *os << std::setw(8) << "If";
    *os << std::setw(10) << "Type";
    *os << std::setw(10) << "Status";
    *os << std::endl;

    for (const auto& entry : m_routingTable)
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
