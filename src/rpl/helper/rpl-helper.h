#ifndef RPL_HELPER_H
#define RPL_HELPER_H

#include "ns3/ipv6-routing-helper.h"

#include "ns3/node-container.h"
#include "ns3/node.h"
#include "ns3/object-factory.h"

#include <map>
#include <set>

namespace ns3
{

/**
 * @ingroup ipv6Helpers
 *
 * @brief Helper class that adds RPL routing to nodes.
 *
 * This class is expected to be used in conjunction with
 * ns3::InternetStackHelper::SetRoutingHelper
 */
class RPLHelper : public Ipv6RoutingHelper
{
  public:
    /**
     * 現状: ObjectFactory に ns3::RPL を登録する。
     * 今後: デフォルト属性（MOP, Trickle 間隔等）の初期値をここで設定？
     */
    RPLHelper();

    /**
     * factory・除外 IF・メトリック設定をコピー
     */
    RPLHelper(const RPLHelper& o);

    ~RPLHelper() override;

    RPLHelper& operator=(const RPLHelper&) = delete;

    /**
     *新しい RPLHelper インスタンスをヒープ上に複製して返す
     */
    RPLHelper* Copy() const override;

    /**
     * ノードに RPL を生成し、事前設定した除外 IF・メトリックを適用
     */
    Ptr<Ipv6RoutingProtocol> Create(Ptr<Node> node) const override;

    /**
     *  ObjectFactory 経由で ns3::RPL の Attribute を設定
     */
    void Set(std::string name, const AttributeValue& value);

    /**
     * 各ノードの RPL（または Ipv6ListRouting 内の RPL）に乱数ストリームを割り当て
     */
    int64_t AssignStreams(NodeContainer c, int64_t stream);

    /**
     *  インストール済みノードの RPL に ::/0 デフォルトルートを追加
     */
    void SetDefaultRouter(Ptr<Node> node, Ipv6Address nextHop, uint32_t interface);

    /**
     *  Install 前に呼び、当該 IF を RPL 対象外として Helper 内に記録す
     */
    void ExcludeInterface(Ptr<Node> node, uint32_t interface);

    /**
     *  Install 前に呼び、IF のリンクメトリックを Helper 内に記録
     */
    void SetInterfaceMetric(Ptr<Node> node, uint32_t interface, uint8_t metric);

  private:
    ObjectFactory m_factory; //!< ns3::RPL 生成用

    //!< ノードごとの RPL 除外 IF（Install 前に蓄積）
    std::map<Ptr<Node>, std::set<uint32_t>> m_interfaceExclusions;

    //!< ノードごとの IF メトリック（Install 前に蓄積）
    std::map<Ptr<Node>, std::map<uint32_t, uint8_t>> m_interfaceMetrics;
};

} // namespace ns3

#endif /* RPL_HELPER_H */
