#include "rpl-helper.h"

#include "ns3/ipv6-list-routing.h"
#include "ns3/ipv6.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/rpl.h"

#include <map>

namespace ns3
{

RPLHelper::RPLHelper()
{
    //  生成対象を ns3::RPL に固定
    // 今後必要ならここで Attribute のデフォルト上書きを行う？
    m_factory.SetTypeId("ns3::RPL");
}

RPLHelper::RPLHelper(const RPLHelper& o)
    : m_factory(o.m_factory)
{
    // Install 前設定を丸ごと複製
    //  追加メンバが増えたら同様にコピー
    m_interfaceExclusions = o.m_interfaceExclusions;
    m_interfaceMetrics = o.m_interfaceMetrics;
}

RPLHelper::~RPLHelper()
{
    m_interfaceExclusions.clear();
    m_interfaceMetrics.clear();
}

RPLHelper*
RPLHelper::Copy() const
{
    return new RPLHelper(*this);
}

Ptr<Ipv6RoutingProtocol>
RPLHelper::Create(Ptr<Node> node) const
{
    // RPL を生成し、除外 IF・メトリックを適用してノードへ関連付け
    // ノード別 Root 指定や DODAG 設定があればここで RPL へ反映
    Ptr<RPL> rpl = m_factory.Create<RPL>();

    auto it = m_interfaceExclusions.find(node);
    if (it != m_interfaceExclusions.end())
    {
        rpl->SetInterfaceExclusions(it->second);
    }

    auto iter = m_interfaceMetrics.find(node);
    if (iter != m_interfaceMetrics.end())
    {
        for (auto subiter = iter->second.begin(); subiter != iter->second.end(); subiter++)
        {
            rpl->SetInterfaceMetric(subiter->first, subiter->second);
        }
    }

    node->AggregateObject(rpl);
    return rpl;
}

void
RPLHelper::Set(std::string name, const AttributeValue& value)
{
    //  全ノード共通の RPL Attribute を factory に設定
    m_factory.Set(name, value);
}

int64_t
RPLHelper::AssignStreams(NodeContainer c, int64_t stream)
{
    //  各ノードの RPL を検索し AssignStreams を呼ぶ
    int64_t currentStream = stream;
    Ptr<Node> node;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        node = (*i);
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        NS_ASSERT_MSG(ipv6, "Ipv6 not installed on node");
        Ptr<Ipv6RoutingProtocol> proto = ipv6->GetRoutingProtocol();
        NS_ASSERT_MSG(proto, "Ipv6 routing not installed on node");
        Ptr<RPL> rpl = DynamicCast<RPL>(proto);
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
            Ptr<RPL> listRPL;
            for (uint32_t i = 0; i < list->GetNRoutingProtocols(); i++)
            {
                listProto = list->GetRoutingProtocol(i, priority);
                listRPL = DynamicCast<RPL>(listProto);
                if (listRPL)
                {
                    currentStream += listRPL->AssignStreams(currentStream);
                    break;
                }
            }
        }
    }
    return (currentStream - stream);
}

void
RPLHelper::SetDefaultRouter(Ptr<Node> node, Ipv6Address nextHop, uint32_t interface)
{
    // インストール済み RPL の AddDefaultRouteTo を呼ぶ
    // 今後DAO による自動ルートと併用する場合の優先順位は RPL シナリオ側で整理？
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    NS_ASSERT_MSG(ipv6, "Ipv6 not installed on node");
    Ptr<Ipv6RoutingProtocol> proto = ipv6->GetRoutingProtocol();
    NS_ASSERT_MSG(proto, "Ipv6 routing not installed on node");
    Ptr<RPL> rpl = DynamicCast<RPL>(proto);
    if (rpl)
    {
        rpl->AddDefaultRouteTo(nextHop, interface);
    }
    Ptr<Ipv6ListRouting> list = DynamicCast<Ipv6ListRouting>(proto);
    if (list)
    {
        int16_t priority;
        Ptr<Ipv6RoutingProtocol> listProto;
        Ptr<RPL> listRPL;
        for (uint32_t i = 0; i < list->GetNRoutingProtocols(); i++)
        {
            listProto = list->GetRoutingProtocol(i, priority);
            listRPL = DynamicCast<RPL>(listProto);
            if (listRPL)
            {
                listRPL->AddDefaultRouteTo(nextHop, interface);
                break;
            }
        }
    }
}

void
RPLHelper::ExcludeInterface(Ptr<Node> node, uint32_t interface)
{
    //  Install 前に除外 IF を map へ追加。Create 時に RPL へ渡す
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

void
RPLHelper::SetInterfaceMetric(Ptr<Node> node, uint32_t interface, uint8_t metric)
{
    //  Install 前にメトリックを map へ保存。Create 時に RPL へ渡す
    // 今後 Objective Function による親選択で使用
    m_interfaceMetrics[node][interface] = metric;
}

} // namespace ns3
