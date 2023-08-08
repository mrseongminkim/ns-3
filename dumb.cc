#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/point-to-point-module.h"

#include <iostream>

using namespace ns3;

Ptr<PacketSink> sink[10];
uint64_t lastTotalRX[10] = {0, };
void
calculateThroughput()
{
    //Time now = Simulator::Now();
    for (int i = 0; i < 10; i++)
    {
        uint64_t curr_total_rx = sink[i]->GetTotalRx();
        //double cur = (curr_total_rx - lastTotalRX[i]) * (double)8 / 1e5; /* Convert Application RX Packets to MBits. */
        //lastTotalRX[i] = curr_total_rx;
        std::cout << "N" << i + 10 << "번 노드: " << curr_total_rx << std::endl;
    }
    //std::cout << std::endl;
    //Simulator::Schedule(MilliSeconds(1000), &calculateThroughput);
}

int count = 0;
void
DropTracer(Ptr<const Packet> p)
{
    count++;
    Time now = Simulator::Now();
    Ptr<Packet> copy = p->Copy();
    Ipv4Header iph;
    copy->RemoveHeader(iph);
    std::cout << now.GetSeconds() << " ";
    iph.GetSource().Print(std::cout);
    std::cout << " " << count << std::endl;
}

void
CwndChange(uint32_t oldCwnd, uint32_t newCwnd)
{
    std::cout << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

static void
TraceCwnd()
{
    Config::ConnectWithoutContext("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndChange));
}

int
main(int argc, char* argv[])
{
    //Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    //LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    
    int operationTime = 100;

    PointToPointHelper gateway;
    gateway.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    gateway.SetChannelAttribute("Delay", StringValue("2ms"));
    gateway.DisableFlowControl();

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("300Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.DisableFlowControl();

    PointToPointDumbbellHelper d(10, gateway, 10, gateway, bottleneck);

    InternetStackHelper stack;
    d.InstallStack(stack);

    d.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),
                          Ipv4AddressHelper("10.2.1.0", "255.255.255.0"),
                          Ipv4AddressHelper("10.3.1.0", "255.255.255.0"));
    
    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    for (int i = 0; i < 10; i++)
    {
        ApplicationContainer sinkApps = packetSinkHelper.Install(d.GetRight(i));
        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(operationTime + 2.0));
        sink[i] = StaticCast<PacketSink>(sinkApps.Get(0));

        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(d.GetRightIpv4Address(i), sinkPort));
        ApplicationContainer sourceApps = source.Install(d.GetLeft(i));
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(operationTime + 1.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    //Config::ConnectWithoutContext("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Drop", MakeCallback(&DropTracer));

    Simulator::Schedule(Seconds(1.01), &TraceCwnd);
    Simulator::Stop(Seconds(operationTime + 2.0));
    Simulator::Run();
    Simulator::Destroy();

    calculateThroughput();
}