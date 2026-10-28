#ifndef RPL_HELPER_H
#define RPL_HELPER_H

#include "ns3/ipv6-routing-helper.h"

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
class RplHelper : public Ipv6RoutingHelper
{
  public:
    RplHelper();
    RplHelper(const RplHelper& o);
    ~RplHelper() override;

    RplHelper& operator=(const RplHelper&) = delete;

    RplHelper* Copy() const override;
    Ptr<Ipv6RoutingProtocol> Create(Ptr<Node> node) const override;
    void Set(std::string name, const AttributeValue& value);

    void SetDefaultRouter(Ptr<Node> node, Ipv6Address nextHop, uint32_t interface);
    void ExcludeInterface(Ptr<Node> node, uint32_t interface);

  private:
    ObjectFactory m_factory;
    std::map<Ptr<Node>, std::set<uint32_t>> m_interfaceExclusions;
};

} // namespace ns3

#endif /* RPL_HELPER_H */
