#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

#include <iostream>

/*
   Network Topology
                                     +--- N10
                                     |
   N0   N1   ...   N9   R1 -- R2 -- R3---..
   |    |          |    |            |
   ======================            +--- N19
*/

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Csma");

int nLeftNode = 10;
double operationTime = 20;

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeftNode", "number of left side node", nLeftNode);
    cmd.AddValue("operationTime", "time value where application sends packet in second", operationTime);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer leftNodes, rightNodes, routers[2];
    leftNodes.Create(nLeftNode);
    rightNodes.Create(10);
    routers[0].Create(2);
    routers[1].Add(routers[0].Get(1));
    routers[1].Create(1);

    //여기까지 함

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