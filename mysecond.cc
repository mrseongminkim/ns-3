/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
 
// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1   n2   n3   n4
//    point-to-point  |    |    |    |
//                    ================
//                      LAN 10.1.2.0
 
using namespace ns3;
 
NS_LOG_COMPONENT_DEFINE("SecondScriptExample");
 
int
main(int argc, char* argv[])
{
    bool verbose = true;
    uint32_t nCsma = 3;
 
    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
 
    cmd.Parse(argc, argv);
 
    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }
 
    nCsma = nCsma == 0 ? 1 : nCsma;
 
    NodeContainer p2pNodes;
    p2pNodes.Create(2);
 
    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(1));
    csmaNodes.Create(nCsma);
 
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
 
    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);
 
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
 
    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);
 
    InternetStackHelper stack;
    stack.Install(p2pNodes.Get(0));
    stack.Install(csmaNodes);
 
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);
 
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);
 
    UdpEchoServerHelper echoServer(9);
 
    ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(nCsma));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));
 
    UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(nCsma), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
 
    ApplicationContainer clientApps = echoClient.Install(p2pNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));
 
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
 
    pointToPoint.EnablePcapAll("second");
    csma.EnablePcap("second", csmaDevices.Get(1), true);
 
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

/*
n0 to n3 delay: 0.0036864
arp jitter: 0.008(can be vary)
total delay: 0.00001168
CsmaNetDevice:TransmitStart(): Channel busy, backing off for +1e-06s: 0.00001; 2.0169908에 Reply를 보냄
2.011710760에 n1이 arp reply를 받음; CsmaNetDevice:TransmitStart(): Channel busy, backing off for +1e-06s: 0.00001;
2.011711760에 실제로 데이터를 전송함
2.011803920에 데이터 받음
2.017803920에 ARP req보냄; arp jitter: 0.006(can be vary)
2.0178156도착하고 백 오프 적용되서
2.0178166에 ARP response 전송
2.01782828에 ARP res받고 이제 다시 한번 비지
2.017829280에 드디어 에코 메세지 보냄
2.02160784에 받고 전부 끝남

Packet size 1
1. Ethernet II Header and Tale: 36 bytes
2. Address Resolution Protocol (request): 28 bytes
총 64 bytes
Transmission Delay: 0.00000512
Propagationn Delay: 0.00000656
Total Delayyyyyyyy: 0.00001168

Packet size 2
1. Ethernet II Header and Tale: 18 bytes
2. IP protocol Header: 20 bytes
3. UDP: 8
4. data: 1024
총 1070
Transmission Delay: 0.0000856
Total delay: 0.00009216
*/