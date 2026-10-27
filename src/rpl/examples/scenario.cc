/*
 * RPL over 6LoWPAN / IEEE 802.15.4 with a real MAC bootstrap (no preset association).
 *
 * Topology (line):  Node0 (root / PAN coordinator) --80m-- Node1 --80m-- Node2
 * Radio range is 100 m, so Node0 and Node2 (160 m) can not hear each other.
 *
 * MAC bootstrap (IEEE 802.15.4, same primitives as lr-wpan-active-scan / lr-wpan-bootstrap,
 * and the "join then start router" sequence used by the Zigbee NWK layer):
 *   Node0 : MLME-START (PAN coordinator, non-beacon mode, answers beacon requests)
 *   Node1 : MLME-SCAN (active) -> Beacon Request -> Beacon from Node0
 *           -> MLME-ASSOCIATE (Association Request / Data Request / Association Response)
 *           -> MLME-START (panCoor=false: Node1 becomes a coordinator for Node2)
 *   Node2 : same sequence, but only Node1 is in range, so it associates with Node1.
 * The result is a coordinator -> child tree at the MAC level (not a MAC mesh).
 * Multi-hop reachability comes only from the radio range; RPL builds the routing topology.
 *
 * RPL on Node1/Node2 is started only after their association completed.
 *
 * Downward routes: the DODAG runs in Storing mode (MOP 2). After joining, Node2 and Node1
 * send unicast DAO to their parents so that Node0 learns  Node2 via Node1.
 */

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/rpl-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/spectrum-module.h"

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::lrwpan;

namespace
{

constexpr uint16_t kPanId = 5;
constexpr uint8_t kChannel = 11;
constexpr uint32_t kScanDuration = 3;       // 960 * (2^3 + 1) symbols = about 138 ms / channel
constexpr double kScanRetrySeconds = 1.0;   // retry interval when no coordinator was found
constexpr double kFirstScanSeconds = 1.0;   // Node1 first scan (Node2 starts 0.5 s later)

template <typename T>
std::string
ToString(const T& value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

/// ノードごとの設定と進行状況。
struct NodeInfo
{
    Ptr<LrWpanNetDevice> dev;
    Ptr<Rpl> rpl;
    uint32_t ipv6If{0};
    std::string role;       // 表示用
    bool isRouter{false};   // 関連付け後に非 PAN コーディネータとして MLME-START する
    Mac16Address shortAddr; // 事前割り当て値（IPv6 の IID 用）。コーディネータが同じ値を割り当てる
    uint32_t scanTries{0};
    bool associated{false};
    Time associatedAt;
};

std::vector<NodeInfo> g_nodes;

/// コーディネータが持つ割り当て表（拡張アドレス → 短縮アドレス）。
std::map<std::string, Mac16Address> g_shortByExt;

/// 短縮 / 拡張アドレスの文字列 → ノード番号（受信フレームの送信元特定用）。
std::map<std::string, int> g_nodeOfAddr;

/// 受信ノード → フレーム種別 → 件数（MAC フィルタの前。PHY から上がってきた全フレーム）。
std::map<uint32_t, std::map<std::string, uint32_t>> g_heard;

/// 受信ノード → 「種別 from NodeX」 → 件数（MAC フィルタを通ったフレーム）。
std::map<uint32_t, std::map<std::string, uint32_t>> g_accepted;

/// 受信ノード → MAC が受理したフレームの送信元ノード番号の集合。
std::map<uint32_t, std::set<int>> g_acceptedNeighbors;

/// 送信ノード → 「IPv6 層で送ったパケットの種別」 → 件数（ループバックを除く）。
std::map<uint32_t, std::map<std::string, uint32_t>> g_ipv6Tx;

// IPv6 層の送信（Ipv6L3Protocol の Tx トレース）。パケットは IPv6 ヘッダ付き。
void
Ipv6TxTrace(std::string context, Ptr<const Packet> packet, Ptr<Ipv6> ipv6, uint32_t interface)
{
    if (interface == 0)
    {
        return; // ループバック
    }
    const std::string prefix = "/NodeList/";
    const uint32_t node = std::stoul(context.substr(prefix.size()));

    Ptr<Packet> copy = packet->Copy();
    Ipv6Header ip;
    copy->RemoveHeader(ip);

    std::string label;
    if (ip.GetNextHeader() == Ipv6Header::IPV6_ICMPV6)
    {
        Icmpv6Header icmp;
        copy->PeekHeader(icmp);
        if (icmp.GetType() == 155) // RPL Control Message
        {
            switch (icmp.GetCode())
            {
            case 0:
                label = "ICMPv6 RPL DIS";
                break;
            case 1:
                label = "ICMPv6 RPL DIO";
                break;
            case 2:
                label = "ICMPv6 RPL DAO";
                break;
            case 3:
                label = "ICMPv6 RPL DAO-ACK";
                break;
            default:
                label = "ICMPv6 RPL code " + std::to_string(icmp.GetCode());
            }
        }
        else
        {
            label = "ICMPv6 type " + std::to_string(icmp.GetType());
        }
    }
    else
    {
        label = "IPv6 next-header " + std::to_string(ip.GetNextHeader()) + " (e.g. UDP/TCP data)";
    }
    ++g_ipv6Tx[node][label];

    if (label == "ICMPv6 type 133")
    {
        // IPv6 スタックが自動で出す Router Solicitation（RPL とは無関係）。
        std::cout << Simulator::Now().As(Time::S) << " [IPv6 RS Tx] node=" << node
                  << " src=" << ip.GetSource() << " dst=" << ip.GetDestination() << "\n";
    }
}

std::ostream&
Log(uint32_t node)
{
    std::cout << Simulator::Now().As(Time::S) << " [MAC] Node" << node << " ";
    return std::cout;
}

// ---------------------------------------------------------------------------------------------
// フレーム観測
// ---------------------------------------------------------------------------------------------

// ヘッダを取り除いた後のパケットから、フレーム種別の名前を返す。
std::string
ClassifyFrame(const LrWpanMacHeader& hdr, Ptr<Packet> rest)
{
    if (hdr.IsBeacon())
    {
        return "beacon";
    }
    if (hdr.IsAcknowledgment())
    {
        return "ack";
    }
    if (hdr.IsData())
    {
        return "data";
    }
    if (hdr.IsCommand())
    {
        CommandPayloadHeader cmd;
        rest->PeekHeader(cmd);
        switch (cmd.GetCommandFrameType())
        {
        case CommandPayloadHeader::BEACON_REQ:
            return "cmd:BEACON_REQ";
        case CommandPayloadHeader::ASSOCIATION_REQ:
            return "cmd:ASSOC_REQ";
        case CommandPayloadHeader::ASSOCIATION_RESP:
            return "cmd:ASSOC_RESP";
        case CommandPayloadHeader::DATA_REQ:
            return "cmd:DATA_REQ";
        default:
            return "cmd:other";
        }
    }
    return "other";
}

// MacPromiscRx: PHY から上がってきた全フレーム（MAC のフィルタ前）。
void
MacPromiscRxTrace(uint32_t nodeId, Ptr<const Packet> packet)
{
    Ptr<Packet> copy = packet->Copy();
    LrWpanMacHeader hdr;
    copy->RemoveHeader(hdr);
    ++g_heard[nodeId][ClassifyFrame(hdr, copy)];
}

// MacRx: MAC のフィルタ（PAN ID / 宛先アドレス）を通ったフレームだけ。
void
MacRxTrace(uint32_t nodeId, Ptr<const Packet> packet)
{
    Ptr<Packet> copy = packet->Copy();
    LrWpanMacHeader hdr;
    copy->RemoveHeader(hdr);

    std::string src;
    if (hdr.GetSrcAddrMode() == LrWpanMacHeader::SHORTADDR)
    {
        src = ToString(hdr.GetShortSrcAddr());
    }
    else if (hdr.GetSrcAddrMode() == LrWpanMacHeader::EXTADDR)
    {
        src = ToString(hdr.GetExtSrcAddr());
    }

    int srcNode = -1;
    const auto it = g_nodeOfAddr.find(src);
    if (it != g_nodeOfAddr.end())
    {
        srcNode = it->second;
        g_acceptedNeighbors[nodeId].insert(srcNode);
    }

    std::string kind = ClassifyFrame(hdr, copy);
    if (srcNode >= 0)
    {
        kind += " from Node" + std::to_string(srcNode);
    }
    ++g_accepted[nodeId][kind];
}

// ---------------------------------------------------------------------------------------------
// MLME コールバック（スキャン → 関連付け → ルータ起動 → RPL 開始）
// ---------------------------------------------------------------------------------------------

void
StartScan(uint32_t idx)
{
    NodeInfo& n = g_nodes[idx];
    ++n.scanTries;

    MlmeScanRequestParams scan;
    scan.m_chPage = 0;
    scan.m_scanChannels = 1u << kChannel;
    scan.m_scanDuration = kScanDuration;
    scan.m_scanType = MLMESCAN_ACTIVE;

    Log(idx) << "MLME-scan.request (active, ch" << static_cast<int>(kChannel) << ", try "
             << n.scanTries << ")\n";
    n.dev->GetMac()->MlmeScanRequest(scan);
}

void
ScanConfirm(uint32_t idx, MlmeScanConfirmParams params)
{
    int best = -1;
    if (params.m_status == MacStatus::SUCCESS)
    {
        for (uint32_t i = 0; i < params.m_panDescList.size(); ++i)
        {
            const PanDescriptor& d = params.m_panDescList[i];
            SuperframeField sf(d.m_superframeSpec);
            if (!sf.IsAssocPermit() || d.m_coorAddrMode != SHORT_ADDR)
            {
                continue;
            }
            if (best < 0 || d.m_linkQuality > params.m_panDescList[best].m_linkQuality)
            {
                best = static_cast<int>(i);
            }
        }
    }

    if (best < 0)
    {
        Log(idx) << "MLME-scan.confirm: "
                 << (params.m_status == MacStatus::NO_BEACON ? "NO_BEACON" : "no usable PAN")
                 << " -> retry in " << kScanRetrySeconds << " s\n";
        Simulator::Schedule(Seconds(kScanRetrySeconds), &StartScan, idx);
        return;
    }

    const PanDescriptor& d = params.m_panDescList[best];
    Log(idx) << "MLME-scan.confirm: beacon from coordinator " << d.m_coorShortAddr << " PAN "
             << d.m_coorPanId << " ch" << static_cast<int>(d.m_logCh) << " LQI "
             << static_cast<int>(d.m_linkQuality) << " (" << params.m_panDescList.size()
             << " PAN found)\n";

    MlmeAssociateRequestParams assoc;
    assoc.m_chNum = d.m_logCh;
    assoc.m_chPage = d.m_logChPage;
    assoc.m_coordPanId = d.m_coorPanId;
    assoc.m_coordAddrMode = SHORT_ADDR;
    assoc.m_coordShortAddr = d.m_coorShortAddr;
    CapabilityField capability;
    capability.SetShortAddrAllocOn(true);
    assoc.m_capabilityInfo = capability.GetCapability();

    Log(idx) << "MLME-associate.request -> coordinator " << d.m_coorShortAddr << "\n";
    Simulator::ScheduleNow(&LrWpanMac::MlmeAssociateRequest, g_nodes[idx].dev->GetMac(), assoc);
}

// コーディネータ側: 関連付け要求を受けたら、割り当て表の短縮アドレスで応答する。
void
AssociateIndication(uint32_t coordIdx, MlmeAssociateIndicationParams params)
{
    MlmeAssociateResponseParams resp;
    resp.m_extDevAddr = params.m_extDevAddr;

    const auto it = g_shortByExt.find(ToString(params.m_extDevAddr));
    if (it == g_shortByExt.end())
    {
        resp.m_status = MacStatus::ACCESS_DENIED;
        resp.m_assocShortAddr = Mac16Address("ff:ff");
        Log(coordIdx) << "MLME-associate.indication from " << params.m_extDevAddr
                      << " -> denied (unknown device)\n";
    }
    else
    {
        resp.m_status = MacStatus::SUCCESS;
        resp.m_assocShortAddr = it->second;
        Log(coordIdx) << "MLME-associate.indication from " << params.m_extDevAddr
                      << " -> assign short " << it->second << "\n";
    }
    Simulator::ScheduleNow(&LrWpanMac::MlmeAssociateResponse,
                           g_nodes[coordIdx].dev->GetMac(),
                           resp);
}

void
StartConfirm(uint32_t idx, MlmeStartConfirmParams params)
{
    Log(idx) << "MLME-start.confirm: "
             << (params.m_status == MacStatus::SUCCESS ? "SUCCESS" : "FAILED") << "\n";
}

// 関連付け完了後に RPL を開始する（除外していたインターフェースを有効化）。
void
EnableRpl(uint32_t idx)
{
    NodeInfo& n = g_nodes[idx];
    n.rpl->SetInterfaceExclusions(std::set<uint32_t>());
    n.rpl->NotifyInterfaceUp(n.ipv6If);
    Log(idx) << "RPL enabled on if " << n.ipv6If << " (association complete)\n";
}

void
AssociateConfirm(uint32_t idx, MlmeAssociateConfirmParams params)
{
    NodeInfo& n = g_nodes[idx];
    Ptr<LrWpanMac> mac = n.dev->GetMac();

    if (params.m_status != MacStatus::SUCCESS)
    {
        Log(idx) << "MLME-associate.confirm: FAILED -> rescan in " << kScanRetrySeconds << " s\n";
        Simulator::Schedule(Seconds(kScanRetrySeconds), &StartScan, idx);
        return;
    }

    n.associated = true;
    n.associatedAt = Simulator::Now();
    Log(idx) << "MLME-associate.confirm: SUCCESS short=" << params.m_assocShortAddr
             << " PAN=" << mac->GetPanId() << " coordinator=" << mac->GetCoordShortAddress()
             << "\n";

    if (n.isRouter)
    {
        // Zigbee の NLME-START-ROUTER と同じ。PAN コーディネータではないが、
        // ビーコン要求に応答して子の関連付けを受け付ける（非ビーコンモード）。
        MlmeStartRequestParams start;
        start.m_panCoor = false;
        start.m_PanId = kPanId;
        start.m_bcnOrd = 15;
        start.m_sfrmOrd = 15;
        start.m_logCh = kChannel;
        Log(idx) << "MLME-start.request (panCoor=false, router)\n";
        Simulator::ScheduleNow(&LrWpanMac::MlmeStartRequest, mac, start);
    }
    EnableRpl(idx);
}

// ---------------------------------------------------------------------------------------------
// 検証
// ---------------------------------------------------------------------------------------------

void
PrintMacTable(const NetDeviceContainer& devices)
{
    std::cout << "node  role                     panId  short  coordShort  extended\n";
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<LrWpanMac> mac = devices.Get(i)->GetObject<LrWpanNetDevice>()->GetMac();
        std::string role = g_nodes[i].role;
        role.resize(24, ' ');
        std::cout << i << "     " << role << " " << mac->GetPanId() << "      "
                  << mac->GetShortAddress() << "  " << mac->GetCoordShortAddress() << "       "
                  << mac->GetExtendedAddress() << "\n";
    }
}

// 関連付け結果（PAN ID / 短縮アドレス / コーディネータ）を検証する。
//  - 全ノードが PAN ID 5、割り当て表どおりの一意な短縮アドレスを持つ
//  - Node1 の親は Node0、Node2 の親は Node1（Node0 は 160 m 先で圏外）
bool
VerifyMacAssociation(const NetDeviceContainer& devices)
{
    bool ok = true;
    std::set<std::string> seenShort;

    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<LrWpanMac> mac = devices.Get(i)->GetObject<LrWpanNetDevice>()->GetMac();
        const Mac16Address shortAddr = mac->GetShortAddress();

        if (mac->GetPanId() != kPanId)
        {
            std::cout << "  NG: Node" << i << " PAN ID != " << kPanId << "\n";
            ok = false;
        }
        if (shortAddr != g_nodes[i].shortAddr || shortAddr == Mac16Address("ff:ff") ||
            shortAddr == Mac16Address("ff:fe") || !seenShort.insert(ToString(shortAddr)).second)
        {
            std::cout << "  NG: Node" << i << " short address " << shortAddr
                      << " is not the assigned unique one (" << g_nodes[i].shortAddr << ")\n";
            ok = false;
        }
        if (i > 0)
        {
            if (!g_nodes[i].associated)
            {
                std::cout << "  NG: Node" << i << " did not complete association\n";
                ok = false;
            }
            if (mac->GetCoordShortAddress() != g_nodes[i - 1].shortAddr)
            {
                std::cout << "  NG: Node" << i << " coordinator is " << mac->GetCoordShortAddress()
                          << ", expected " << g_nodes[i - 1].shortAddr << " (Node" << (i - 1)
                          << ")\n";
                ok = false;
            }
        }
    }
    return ok;
}

// フレームの内訳を表示し、スキャン / 関連付けの手順が実際に流れたことと、
// 隣接ノードとしか通信していないこと（0-1-2 の直線）を確認する。
bool
VerifyMacFrames(uint32_t nodeCount)
{
    bool ok = true;

    for (uint32_t i = 0; i < nodeCount; ++i)
    {
        std::cout << "Node" << i << " heard (before MAC filter):";
        for (const auto& e : g_heard[i])
        {
            std::cout << " " << e.first << "(x" << e.second << ")";
        }
        std::cout << "\n";
        std::cout << "Node" << i << " accepted (after MAC filter):\n";
        for (const auto& e : g_accepted[i])
        {
            std::cout << "    " << e.first << " x" << e.second << "\n";
        }
    }

    // 手順が実際に流れたか（受理されたフレームの種別と送信元で確認する）。
    struct Expect
    {
        uint32_t node;
        std::string kind;
        const char* why;
    };
    const std::vector<Expect> expects = {
        {0, "cmd:BEACON_REQ", "Node1 active scan reached the PAN coordinator"},
        {0, "cmd:ASSOC_REQ from Node1", "Node1 associated with Node0"},
        {1, "beacon from Node0", "Node1 received the coordinator beacon"},
        {1, "cmd:ASSOC_RESP from Node0", "Node0 answered Node1's association"},
        {1, "cmd:BEACON_REQ", "Node2 active scan reached Node1"},
        {1, "cmd:ASSOC_REQ from Node2", "Node2 associated with Node1"},
        {2, "beacon from Node1", "Node2 received Node1's beacon"},
        {2, "cmd:ASSOC_RESP from Node1", "Node1 answered Node2's association"},
    };
    for (const auto& e : expects)
    {
        if (g_accepted[e.node].find(e.kind) == g_accepted[e.node].end())
        {
            std::cout << "  NG: Node" << e.node << " never accepted \"" << e.kind << "\" ("
                      << e.why << ")\n";
            ok = false;
        }
    }

    // 隣接以外とは通信していない（Node0 と Node2 は互いに聞こえない）。
    for (uint32_t i = 0; i < nodeCount; ++i)
    {
        std::set<int> expected;
        if (i > 0)
        {
            expected.insert(static_cast<int>(i) - 1);
        }
        if (i + 1 < nodeCount)
        {
            expected.insert(static_cast<int>(i) + 1);
        }
        if (g_acceptedNeighbors[i] != expected)
        {
            std::cout << "  NG: Node" << i << " accepted frames from unexpected neighbors\n";
            ok = false;
        }
    }
    return ok;
}

Ptr<Rpl>
GetRpl(Ptr<Node> node)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    Ptr<Rpl> rpl = DynamicCast<Rpl>(ipv6->GetRoutingProtocol());
    if (rpl)
    {
        return rpl;
    }
    Ptr<Ipv6ListRouting> list = DynamicCast<Ipv6ListRouting>(ipv6->GetRoutingProtocol());
    if (!list)
    {
        return nullptr;
    }
    for (uint32_t i = 0; i < list->GetNRoutingProtocols(); ++i)
    {
        int16_t priority = 0;
        rpl = DynamicCast<Rpl>(list->GetRoutingProtocol(i, priority));
        if (rpl)
        {
            return rpl;
        }
    }
    return nullptr;
}

// 終了時の親選択結果（joined / Rank / parent）を表示する。
void
PrintNodeState(Ptr<Node> node, const std::string& label)
{
    Ptr<Rpl> rpl = GetRpl(node);
    std::cout << label << " joined=" << rpl->IsJoined() << " rank=" << rpl->GetRank()
              << " parent=" << rpl->GetPreferredParent() << "\n";
}

} // namespace

int
main(int argc, char** argv)
{
    bool verbose = false;
    double lateJoin = 3.0; // 既定: Node1 は 3 秒後に圏内へ入る（0 を指定すると最初から圏内）

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.AddValue("lateJoin",
                 "Seconds until Node1 enters radio range (default 3; 0 = present from the start). "
                 "Until then Node1's active scans find no beacon and are retried.",
                 lateJoin);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnableAll(LogLevel(LOG_PREFIX_TIME | LOG_PREFIX_FUNC | LOG_PREFIX_NODE));
        LogComponentEnable("Rpl", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(3); // root(PAN coordinator) -- router -- end device

    // 直線配置。N2 が root と直接通信できない距離にする。
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0)); // Node0 root
    // lateJoin > 0 のとき Node1 は最初は圏外（どのノードからも 100 m 超）に置く。
    const Vector node1Home(80.0, 0.0, 0.0);
    const Vector node1Away(80.0, 500.0, 0.0);
    positions->Add(lateJoin > 0.0 ? node1Away : node1Home); // Node1
    positions->Add(Vector(160.0, 0.0, 0.0));                // Node2
    mobility.SetPositionAllocator(positions);
    mobility.Install(nodes);

    if (lateJoin > 0.0)
    {
        // 指定時刻に Node1 を圏内へ移す（後から現れたノードの再現）。
        Ptr<MobilityModel> mm1 = nodes.Get(1)->GetObject<MobilityModel>();
        Simulator::Schedule(Seconds(lateJoin), [mm1, node1Home]() {
            mm1->SetPosition(node1Home);
            std::cout << Simulator::Now().As(Time::S)
                      << " [Scenario] Node1 moved into radio range\n";
        });
    }

    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    // 到達距離 100 m → 隣接のみ通信可（160 m 先の N0–N2 は非接続）。
    lrWpanHelper.AddPropagationLossModel("ns3::RangePropagationLossModel",
                                         "MaxRange",
                                         DoubleValue(100.0));
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // アソシエーション結果の直接設定（CreateAssociatedPan）は使わない。
    // PAN ID とコーディネータ情報は、MLME-START / MLME-ASSOCIATE の結果として設定される。
    // 拡張アドレスは機器固有値として設定する。
    // 短縮アドレスだけは事前に決めておく。6LoWPAN / IPv6 のアドレス（IID）が MAC の
    // GetAddress()（短縮アドレスから生成）で決まり、ネットワーク層は t=0 で設定するため。
    // コーディネータは関連付け応答で、この表と同じ値を割り当てる。
    lrWpanHelper.SetExtendedAddresses(lrwpanDevices);

    const std::vector<std::string> roles = {"PAN coordinator (root)",
                                            "router (non-PAN coordinator)",
                                            "end device"};
    g_nodes.resize(nodes.GetN());
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NodeInfo& n = g_nodes[i];
        n.dev = lrwpanDevices.Get(i)->GetObject<LrWpanNetDevice>();
        n.role = roles[i];
        n.isRouter = (i == 1);

        uint8_t buf[2] = {static_cast<uint8_t>((i + 1) >> 8), static_cast<uint8_t>((i + 1) & 0xff)};
        n.shortAddr.CopyFrom(buf);
        n.dev->GetMac()->SetShortAddress(n.shortAddr);

        g_shortByExt[ToString(n.dev->GetMac()->GetExtendedAddress())] = n.shortAddr;
        g_nodeOfAddr[ToString(n.shortAddr)] = static_cast<int>(i);
        g_nodeOfAddr[ToString(n.dev->GetMac()->GetExtendedAddress())] = static_cast<int>(i);

        n.dev->GetMac()->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxTrace, i));
        n.dev->GetMac()->TraceConnectWithoutContext("MacPromiscRx",
                                                    MakeBoundCallback(&MacPromiscRxTrace, i));
        n.dev->GetMac()->SetMlmeScanConfirmCallback(MakeBoundCallback(&ScanConfirm, i));
        n.dev->GetMac()->SetMlmeAssociateConfirmCallback(MakeBoundCallback(&AssociateConfirm, i));
        n.dev->GetMac()->SetMlmeStartConfirmCallback(MakeBoundCallback(&StartConfirm, i));
        // コーディネータになりうるノード（Node0 と Node1）は関連付け要求を処理する。
        if (i < 2)
        {
            n.dev->GetMac()->SetMlmeAssociateIndicationCallback(
                MakeBoundCallback(&AssociateIndication, i));
        }
    }

    // Node0: PAN コーディネータ。非ビーコンモード（bcnOrd = 15）で、ビーコン要求に応答する。
    MlmeStartRequestParams params;
    params.m_panCoor = true;
    params.m_PanId = kPanId;
    params.m_bcnOrd = 15;
    params.m_sfrmOrd = 15;
    params.m_logCh = kChannel;
    Simulator::ScheduleWithContext(g_nodes[0].dev->GetNode()->GetId(),
                                   Seconds(0),
                                   &LrWpanMac::MlmeStartRequest,
                                   g_nodes[0].dev->GetMac(),
                                   params);

    // Node1 / Node2: アクティブスキャンから開始する（見つからなければ再試行）。
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        Simulator::ScheduleWithContext(nodes.Get(i)->GetId(),
                                       Seconds(kFirstScanSeconds + 0.5 * (i - 1)),
                                       &StartScan,
                                       i);
    }

    RplHelper rplHelper;
    // DIO は Trickle（RFC 6206）で送る。RFC 6550 既定値:
    // Imin = 2^3 ms = 8 ms、Imax = Imin * 2^20、冗長定数 k = 10。
    rplHelper.Set("DioIntervalMin", UintegerValue(3));
    rplHelper.Set("DioIntervalDoublings", UintegerValue(20));
    rplHelper.Set("DioRedundancyConstant", UintegerValue(10));
    rplHelper.Set("DisInterval", TimeValue(Seconds(0.5)));
    // DIS の試行は RPL 開始（= 関連付け完了）後にだけ消費される。
    rplHelper.Set("DisMaxAttempts", UintegerValue(10));
    // DAO は DIO を受けて参加した後、DelayDAO（既定 1 秒）後に親へ送る（Storing mode のみ）。
    rplHelper.Set("DaoDelay", TimeValue(Seconds(1)));
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(rplHelper);
    internetv6.Install(nodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        g_nodes[i].rpl = GetRpl(nodes.Get(i));
    }
    NS_ABORT_MSG_UNLESS(g_nodes[0].rpl && g_nodes[1].rpl && g_nodes[2].rpl,
                        "RPL routing protocol was not installed");
    g_nodes[0].rpl->SetRoot(true);
    // MOP は root だけが決める。Node1/Node2 は DIO を受けて同じ MOP（2 = Storing）になる。
    g_nodes[0].rpl->SetAttribute(
        "ModeOfOperation",
        EnumValue<ModeOfOperation>(ModeOfOperation::STORING_NO_MULTICAST));

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    // 2001:2::/64 は「同一リンク」ではない（route-over で複数ホップ）。on-link を付けると
    // Node2 宛が直接到達できる扱いになるため、on-link なしで割り当てる。
    // 他ノードへは RPL（Upward: ::/0、Downward: DAO で学習した /128）で到達する。
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces = ipv6.Assign(devices,
                                                          std::vector<bool>(nodes.GetN(), true),
                                                          std::vector<bool>(nodes.GetN(), false));

    // Node1 / Node2 の RPL は、関連付けが終わるまで起動しない（スキャン中に DIS を出さない）。
    // アドレスは保持したまま、RPL だけ対象インターフェースから外しておく。
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        g_nodes[i].ipv6If = deviceInterfaces.GetInterfaceIndex(i);
        if (i > 0)
        {
            g_nodes[i].rpl->SetInterfaceExclusions(std::set<uint32_t>{g_nodes[i].ipv6If});
        }
    }

    std::cout << "Topology: Node0(root) --80m-- Node1 --80m-- Node2 (MaxRange=100m)\n"
              << "Root=" << deviceInterfaces.GetAddress(0, 1)
              << "  N1=" << deviceInterfaces.GetAddress(1, 1)
              << "  N2=" << deviceInterfaces.GetAddress(2, 1) << "\n"
              << "MAC : Node0 MLME-START(PAN coordinator, non-beacon); Node1/Node2 "
                 "active scan -> beacon -> associate (no preset association).\n"
              << "      Node1 becomes a router (MLME-START panCoor=false) after joining.\n"
              << "RPL : DIO by Trickle (Imin=8ms, doublings=20, k=10); started on Node1/Node2 "
                 "after association; multicast DIS resets Trickle.\n"
              << "      MOP=2 (Storing): after join, DAO advertises addresses Upward so parents "
                 "install Downward /128 routes.\n"
              << "Expect: MAC tree Node0 -> Node1 -> Node2; RPL N1 parent=N0, N2 parent=N1; "
                 "Node0 DOWNWARD to N2 via N1.\n\n";

    Config::Connect("/NodeList/*/$ns3::Ipv6L3Protocol/Tx", MakeCallback(&Ipv6TxTrace));

    std::cout << "=== MAC state (before run: nothing associated yet) ===\n";
    PrintMacTable(lrwpanDevices);
    std::cout << "\n";

    Simulator::Stop(Seconds(10 + lateJoin));
    Simulator::Run();

    std::cout << "\n=== MAC state (after run) ===\n";
    PrintMacTable(lrwpanDevices);
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        std::cout << "Node" << i << ": scan tries=" << g_nodes[i].scanTries
                  << ", associated at "
                  << (g_nodes[i].associated ? ToString(g_nodes[i].associatedAt.As(Time::S))
                                            : std::string("(never)"))
                  << "\n";
    }
    bool macOk = VerifyMacAssociation(lrwpanDevices);

    std::cout << "\n=== MAC frames ===\n";
    macOk = VerifyMacFrames(nodes.GetN()) && macOk;
    std::cout << (macOk ? "MAC: OK" : "MAC: NG") << "\n";

    // IPv6 層で送ったパケットの内訳（RPL 制御と ND。アプリケーションデータは無い）。
    std::cout << "\n=== IPv6 packets sent (L3 Tx, excluding loopback) ===\n";
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        std::cout << "Node" << i << ":";
        if (g_ipv6Tx[i].empty())
        {
            std::cout << " (none)";
        }
        for (const auto& e : g_ipv6Tx[i])
        {
            std::cout << " " << e.first << " x" << e.second << ";";
        }
        std::cout << "\n";
    }

    std::cout << "\n=== Final RPL state ===\n";
    PrintNodeState(nodes.Get(0), "Node0");
    PrintNodeState(nodes.Get(1), "Node1");
    PrintNodeState(nodes.Get(2), "Node2");

    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    std::cout << "\n";
    g_nodes[0].rpl->PrintRoutingTable(routingStream);
    g_nodes[1].rpl->PrintRoutingTable(routingStream);
    g_nodes[2].rpl->PrintRoutingTable(routingStream);

    Ptr<Rpl> rpl0 = g_nodes[0].rpl;
    Ptr<Rpl> rpl1 = g_nodes[1].rpl;
    Ptr<Rpl> rpl2 = g_nodes[2].rpl;

    const Ipv6Address n1Addr = deviceInterfaces.GetAddress(1, 1);
    const Ipv6Address n2Addr = deviceInterfaces.GetAddress(2, 1);
    const bool daoOk = rpl0->HasDownwardRoute(n1Addr) && rpl0->HasDownwardRoute(n2Addr) &&
                       rpl1->HasDownwardRoute(n2Addr);
    std::cout << "\n=== DAO downward routes ===\n"
              << "  Node0 has DOWNWARD to N1: " << rpl0->HasDownwardRoute(n1Addr) << "\n"
              << "  Node0 has DOWNWARD to N2: " << rpl0->HasDownwardRoute(n2Addr) << "\n"
              << "  Node1 has DOWNWARD to N2: " << rpl1->HasDownwardRoute(n2Addr) << "\n";

    // 期待: N0 Rank1、N1→N0 Rank2、N2→N1 Rank3。DAO で N0 が N2 への下りを持つ。
    const bool pass =
        macOk && daoOk && rpl0->IsJoined() && rpl0->IsRoot() && rpl0->GetRank() == 1 &&
        rpl1->IsJoined() && rpl1->GetRank() == 2 && !rpl1->GetPreferredParent().IsAny() &&
        rpl2->IsJoined() && rpl2->GetRank() == 3 && !rpl2->GetPreferredParent().IsAny() &&
        rpl2->GetPreferredParent() != rpl1->GetPreferredParent();

    std::cout << "\n";
    if (pass)
    {
        std::cout << "PASS: MAC bootstrap (scan/associate) + Trickle-driven multi-hop join "
                     "(N1->root Rank2, N2->N1 Rank3) + DAO downward routes.\n";
    }
    else
    {
        std::cout << "UNEXPECTED: MAC association, parent/rank chain or DAO downward routes "
                     "not as expected.\n";
    }

    Simulator::Destroy();
    return pass ? 0 : 1;
}
