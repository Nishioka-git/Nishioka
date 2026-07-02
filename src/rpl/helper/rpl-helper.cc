#include "rpl-helper.h"

#include "ns3/ipv6-list-routing.h"
#include "ns3/ipv6.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/rpl.h"

namespace ns3
{

RplHelper::RplHelper()
{
    m_factory.SetTypeId("ns3::Rpl");
}

RplHelper::RplHelper(const RplHelper& o)
    : m_factory(o.m_factory)
{
    m_interfaceExclusions = o.m_interfaceExclusions;
}

RplHelper::~RplHelper()
{
    m_interfaceExclusions.clear();
}

RplHelper*
RplHelper::Copy() const
{
    return new RplHelper(*this);
}

Ptr<Ipv6RoutingProtocol>
RplHelper::Create(Ptr<Node> node) const
{
    Ptr<Rpl> rpl = m_factory.Create<Rpl>();

    auto it = m_interfaceExclusions.find(node);
    if (it != m_interfaceExclusions.end())
    {
        rpl->SetInterfaceExclusions(it->second);
    }

    node->AggregateObject(rpl);
    return rpl;
}

void
RplHelper::Set(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

int64_t
RplHelper::AssignStreams(NodeContainer c, int64_t stream)
{
    int64_t currentStream = stream;
    Ptr<Node> node;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        node = (*i);
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        NS_ASSERT_MSG(ipv6, "Ipv6 not installed on node");
        Ptr<Ipv6RoutingProtocol> proto = ipv6->GetRoutingProtocol();
        NS_ASSERT_MSG(proto, "Ipv6 routing not installed on node");
        Ptr<Rpl> rpl = DynamicCast<Rpl>(proto);
        if (rpl)
        {
            currentStream += rpl->AssignStreams(currentStream);
            continue;
        }
        Ptr<Ipv6ListRouting> list = DynamicCast<Ipv6ListRouting>(proto);
        if (list)
        {
            int16_t priority;
            Ptr<Ipv6RoutingProtocol> listProto;
            Ptr<Rpl> listRpl;
            for (uint32_t i = 0; i < list->GetNRoutingProtocols(); i++)
            {
                listProto = list->GetRoutingProtocol(i, priority);
                listRpl = DynamicCast<Rpl>(listProto);
                if (listRpl)
                {
                    currentStream += listRpl->AssignStreams(currentStream);
                    break;
                }
            }
        }
    }
    return (currentStream - stream);
}

void
RplHelper::SetDefaultRouter(Ptr<Node> node, Ipv6Address nextHop, uint32_t interface)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    NS_ASSERT_MSG(ipv6, "Ipv6 not installed on node");
    Ptr<Ipv6RoutingProtocol> proto = ipv6->GetRoutingProtocol();
    NS_ASSERT_MSG(proto, "Ipv6 routing not installed on node");
    Ptr<Rpl> rpl = DynamicCast<Rpl>(proto);
    if (rpl)
    {
        rpl->AddDefaultRouteTo(nextHop, interface);
    }
    Ptr<Ipv6ListRouting> list = DynamicCast<Ipv6ListRouting>(proto);
    if (list)
    {
        int16_t priority;
        Ptr<Ipv6RoutingProtocol> listProto;
        Ptr<Rpl> listRpl;
        for (uint32_t i = 0; i < list->GetNRoutingProtocols(); i++)
        {
            listProto = list->GetRoutingProtocol(i, priority);
            listRpl = DynamicCast<Rpl>(listProto);
            if (listRpl)
            {
                listRpl->AddDefaultRouteTo(nextHop, interface);
                break;
            }
        }
    }
}

void
RplHelper::ExcludeInterface(Ptr<Node> node, uint32_t interface)
{
    auto it = m_interfaceExclusions.find(node);

    if (it == m_interfaceExclusions.end())
    {
        std::set<uint32_t> interfaces;
        interfaces.insert(interface);
        m_interfaceExclusions.insert(std::make_pair(node, interfaces));
    }
    else
    {
        it->second.insert(interface);
    }
}

} // namespace ns3
