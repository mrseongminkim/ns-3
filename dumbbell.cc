#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/config-store-module.h"

#include <iostream>

/*
   Network Topology
   N0 ---+            +--- N10
         |            |
   .. ---R1 -- R2 -- R3--- ..
         |            |
   N9 ---+            +--- N19
*/

using namespace ns3;
 
NS_LOG_COMPONENT_DEFINE("Dumbbell");

AsciiTraceHelper asciiTraceHelper;

int operationTime = 30;
bool enableRED = false;

Ptr<OutputStreamWrapper> stream[10];
Ptr<Queue<Packet>> R1Queue;
Ptr<PacketSink> sink[10];
uint64_t lastTotalRX[10] = {0, };

void
StreamMaker(void)
{
    for (int i = 0; i < 10; i++)
    {
        std::string fileName = "N1" + std::to_string(i);
        fileName += enableRED ? "_RED" : "";
        fileName += ".dat";

        stream[i] = asciiTraceHelper.CreateFileStream(fileName);
    }
}

void
CalculateThroughput(void)
{
    Time now = Simulator::Now();
    for (int i = 0; i < 10; i++)
    {
        uint64_t currentTotalRx = sink[i]->GetTotalRx();
        double currentThroughput = ((currentTotalRx - lastTotalRX[i]) * 8) / 1e6;
        lastTotalRX[i] = currentTotalRx;
        *(stream[i]->GetStream()) << now.GetSeconds() << " " << currentThroughput << std::endl;
    }
    Simulator::Schedule(MilliSeconds(100), &CalculateThroughput);
}

void
PrintAverageThroughput(void)
{
    for (int i = 0; i < 10; i++)
    {
        uint64_t currentTotalRx = sink[i]->GetTotalRx();
        double currentThroughput = (currentTotalRx * 8) / (operationTime * 1e6);
        std::cout << i + 10 << " " << currentThroughput << " Mbits" << std::endl;
    }
}

void
CongStateChange(TcpSocketState::TcpCongState_t oldState, TcpSocketState::TcpCongState_t newState)
{
    std::cout << Simulator::Now().GetSeconds() << " congestion state change from: " << oldState << " to: " << newState << std::endl;
}

void
CwndChange(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
    int i = context[10] - '0';

    //std::cout << "CWND " << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
    *(stream[i]->GetStream()) << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

void
RwndChange(uint32_t oldRwnd, uint32_t newRwnd)
{
    std::cout << "RWND " << Simulator::Now().GetSeconds() << " " << newRwnd << std::endl;
}

void
RtoChange(Time oldValue, Time newValue)
{
    std::cout << "RTO " << Simulator::Now().GetSeconds() << " " << newValue.GetSeconds() << std::endl;
}

void
RttChange(Time oldValue, Time newValue)
{
    std::cout << "RTT " << Simulator::Now().GetSeconds() << " " << newValue.GetSeconds() << std::endl;
}

void
AddTracer(void)
{
    //Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongState", MakeCallback(&CongStateChange));
    for (int i = 0; i < 10; i++)
    {
        std::string path = "/NodeList/" + std::to_string(i) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";
        Config::Connect(path, MakeCallback(&CwndChange));
    }
    //Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RWND", MakeCallback(&RwndChange));
    //Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTO", MakeCallback(&RtoChange));
    //Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback(&RttChange));
}

uint64_t drop = 0, dropBeforeEnqueue = 0, dropAfterDequeue = 0;

void
Drop(Ptr<const QueueDiscItem> item)
{
    drop++;
}

void
DropBeforeEnqueue(Ptr<const QueueDiscItem> item, const char* str)
{
    dropBeforeEnqueue++;
}

void
DropAfterDequeue(Ptr<const QueueDiscItem> item, const char* str)
{
    dropAfterDequeue++;
}

int
main(int argc, char* argv[])
{
    //Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(0.096)));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(42949672));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(42949672));

/*
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType", TypeIdValue(TypeId::LookupByName("ns3::TcpClassicRecovery")));
*/

    CommandLine cmd(__FILE__);
    cmd.AddValue("operationTime", "time value where application sends packet in second", operationTime);
    cmd.AddValue("RED", "Enable RED policy on R1", enableRED);
    cmd.Parse(argc, argv);

    StreamMaker();

    Time::SetResolution(Time::NS);

/*
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("DropTailQueue", LOG_LEVEL_INFO);
*/

    NodeContainer leftNodes, rightNodes, routers[2];
    leftNodes.Create(10);
    rightNodes.Create(10);
    routers[0].Create(2);
    routers[1].Add(routers[0].Get(1));
    routers[1].Create(1);

    PointToPointHelper gateway;
    gateway.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    gateway.SetChannelAttribute("Delay", StringValue("2ms"));
    gateway.DisableFlowControl();
    gateway.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("300p"));

    NetDeviceContainer leftNodeDevices, leftRouterDevices, rightNodeDevices, rightRouterDevices;
    for (int i = 0; i < 10; i++)
    {
        NetDeviceContainer left = gateway.Install(leftNodes.Get(i), routers[0].Get(0));
        leftNodeDevices.Add(left.Get(0));
        leftRouterDevices.Add(left.Get(1));
        NetDeviceContainer right = gateway.Install(rightNodes.Get(i), routers[1].Get(1));
        rightNodeDevices.Add(right.Get(0));
        rightRouterDevices.Add(right.Get(1));
    }

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("300Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.DisableFlowControl();
    if (enableRED)
        bottleneck.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("300p"));
    else
        bottleneck.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));
 
    NetDeviceContainer routerDevices[2];
    routerDevices[0] = bottleneck.Install(routers[0]);
    routerDevices[1] = bottleneck.Install(routers[1]);

    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers[0]);
    stack.Install(routers[1].Get(1));

    if (enableRED)
    {
        Ptr<PointToPointNetDevice> devA = StaticCast<PointToPointNetDevice>(routerDevices[0].Get(0));
        Ptr<Queue<Packet>> queueA = devA->GetQueue();

        Ptr<NetDeviceQueueInterface> ndqiA = CreateObject<NetDeviceQueueInterface>();
        ndqiA->GetTxQueue(0)->ConnectQueueTraces(queueA);
        devA->AggregateObject(ndqiA);

        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::RedQueueDisc", "MaxSize", StringValue("700p"), "LinkBandwidth", StringValue("300Mbps"), "LinkDelay", StringValue("10ms"));
        tch.Install(routerDevices[0].Get(0));

        Config::ConnectWithoutContext("/NodeList/20/$ns3::Node/$ns3::TrafficControlLayer/RootQueueDiscList/10/$ns3::RedQueueDisc/Drop", MakeCallback(&Drop));
        Config::ConnectWithoutContext("/NodeList/20/$ns3::Node/$ns3::TrafficControlLayer/RootQueueDiscList/10/$ns3::RedQueueDisc/DropBeforeEnqueue", MakeCallback(&DropBeforeEnqueue));
        Config::ConnectWithoutContext("/NodeList/20/$ns3::Node/$ns3::TrafficControlLayer/RootQueueDiscList/10/$ns3::RedQueueDisc/DropAfterDequeue", MakeCallback(&DropAfterDequeue));
    }

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftNodeInterfaces, leftRouterInterfaces;
    for (int i = 0; i < 10; i++)
    {
        NetDeviceContainer ndc;
        ndc.Add(leftNodeDevices.Get(i));
        ndc.Add(leftRouterDevices.Get(i));
        Ipv4InterfaceContainer ifc = address.Assign(ndc);
        leftNodeInterfaces.Add(ifc.Get(0));
        leftRouterInterfaces.Add(ifc.Get(1));
        address.NewNetwork();
    }

    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer rightNodeIterfaces, rightRouterInterfaces;
    for (int i = 0; i < 10; i++)
    {
        NetDeviceContainer ndc;
        ndc.Add(rightNodeDevices.Get(i));
        ndc.Add(rightRouterDevices.Get(i));
        Ipv4InterfaceContainer ifc = address.Assign(ndc);
        rightNodeIterfaces.Add(ifc.Get(0));
        rightRouterInterfaces.Add(ifc.Get(1));
        address.NewNetwork();
    }

    Ipv4InterfaceContainer routerInterfaces[2];
    address.SetBase("10.3.1.0", "255.255.255.0");
    routerInterfaces[0] = address.Assign(routerDevices[0]);
    address.SetBase("10.4.1.0", "255.255.255.0");
    routerInterfaces[1] = address.Assign(routerDevices[1]);

    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    for (int i = 0; i < 10; i++)
    {
        ApplicationContainer sinkApps = packetSinkHelper.Install(rightNodes.Get(i));
        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(operationTime + 1.0));
        sink[i] = StaticCast<PacketSink>(sinkApps.Get(0));

        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(rightNodeIterfaces.GetAddress(i), sinkPort));
        ApplicationContainer sourceApps = source.Install(leftNodes.Get(i));
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(operationTime + 1.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

/*
    Ptr<Node> R1 = routers[0].Get(0);
    Ptr<ConstantPositionMobilityModel> loc = R1->GetObject<ConstantPositionMobilityModel>();
    if (!loc)
    {
        loc = CreateObject<ConstantPositionMobilityModel>();
        R1->AggregateObject(loc);
    }
    Vector R1_location(25, 50, 0);
    loc->SetPosition(R1_location);

    Ptr<Node> R2 = routers[1].Get(0);
    loc = R2->GetObject<ConstantPositionMobilityModel>();
    if (!loc)
    {
        loc = CreateObject<ConstantPositionMobilityModel>();
        R2->AggregateObject(loc);
    }
    Vector R2_location(50, 50, 0);
    loc->SetPosition(R2_location);

    Ptr<Node> R3 = routers[1].Get(1);
    loc = R3->GetObject<ConstantPositionMobilityModel>();
    if (!loc)
    {
        loc = CreateObject<ConstantPositionMobilityModel>();
        R3->AggregateObject(loc);
    }
    Vector R3_location(75, 50, 0);
    loc->SetPosition(R3_location);

    for (int i = 0; i < 10; i++)
    {
        Ptr<Node> left = leftNodes.Get(i);
        loc = left->GetObject<ConstantPositionMobilityModel>();
        if (!loc)
        {
            loc = CreateObject<ConstantPositionMobilityModel>();
            left->AggregateObject(loc);
        }
        Vector left_location(0, 5 + 10 * i, 0);
        loc->SetPosition(left_location);

        Ptr<Node> right = rightNodes.Get(i);
        loc = right->GetObject<ConstantPositionMobilityModel>();
        if (!loc)
        {
            loc = CreateObject<ConstantPositionMobilityModel>();
            right->AggregateObject(loc);
        }
        Vector right_location(100, 5 + 10 * i, 0);
        loc->SetPosition(right_location);
    }

    AnimationInterface anim("dumbbell.xml");
*/

    R1Queue = StaticCast<Queue<Packet>>(StaticCast<PointToPointNetDevice>(routerDevices[0].Get(0))->GetQueue());

/*
    gateway.EnableAsciiAll(asciiTraceHelper.CreateFileStream("dumbbell.tr"));
    gateway.EnablePcap("dumbbell", leftNodeDevices.Get(0), false);
*/

    Simulator::Schedule(Seconds(1.000000001), &AddTracer);
    //Simulator::Schedule(Seconds(1.1), &CalculateThroughput);
    Simulator::Stop(Seconds(operationTime + 1.0));

/*
    Config::SetDefault("ns3::ConfigStore::Filename", StringValue("output-attributes.txt"));
    Config::SetDefault("ns3::ConfigStore::FileFormat", StringValue("RawText"));
    Config::SetDefault("ns3::ConfigStore::Mode", StringValue("Save"));
    ConfigStore outputConfig2;
    outputConfig2.ConfigureAttributes();
*/

    Simulator::Run();
    Simulator::Destroy();

    if (enableRED) {
        std::cout << "Drop: " << drop << std::endl;
        std::cout << "DropBeforeEnqueue: " << dropBeforeEnqueue << std::endl;
        std::cout << "DropAfterDequeue: " << dropAfterDequeue << std::endl;
    }
    else {
        std::cout << "Drop: " << R1Queue->GetTotalDroppedPackets() << std::endl;
    }

    PrintAverageThroughput();

    return 0;
}