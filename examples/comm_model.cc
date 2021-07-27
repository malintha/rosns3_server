#include "comm_model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/nstime.h"
#include "ns3/ssid.h"
#include <fstream>
#include <string>
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/on-off-helper.h"

// #include "ns3/udp-echo-helper.h"

#include <random>

NS_LOG_COMPONENT_DEFINE ("ROSNS3Model");

CoModel::CoModel(std::vector<mobile_node_t> mobile_nodes, int sim_time, bool use_real_time):
                mobile_nodes(mobile_nodes),sim_time(sim_time), use_real_time(use_real_time) {
    pcap = false;
    print_routes = true;
    n_nodes = mobile_nodes.size();

    if (use_real_time) {
        GlobalValue::Bind ("SimulatorImplementationType", StringValue (
                            "ns3::RealtimeSimulatorImpl"));
    }
    // init roi_params
    Vector2d roi_means(0,0);
    Vector2d roi_vars(5,15);

    std::vector<mobile_node_t> ue_nodes = sample_ue(roi_means, roi_vars);
    // unsigned int n_ues = ue_nodes.size();
    
    create_backbone_nodes();
    create_backbone_devices();
    create_mobility_model();
    // install_inet_stack();

    create_sta_nodes(ue_nodes);
    // NS_LOG_INFO("Created sta nodes");

    install_applications();
};

std::vector<mobile_node_t> CoModel::sample_ue(Vector2d roi_means, Vector2d roi_vars) {
    // int n_ues = 5;
    std::vector<mobile_node_t> ue_nodes;
    // std::random_device rd{};
    // std::mt19937 gen{rd()};
    // std::normal_distribution<> d1{roi_means(0),roi_vars(0)};
    // std::normal_distribution<> d2{roi_means(1),roi_vars(1)};

    Vector pose1(16.8, -12, 0);
    Vector pose2(5, 0, 0);
    Vector pose3(19.11027, 8.84741,0);
    Vector pose4(-31, 5.857286, 0);
    Vector pose5(-12.65528, 10.03626, 0);
    mobile_node_t ue_node1 = {.position=pose1, .id=1};
    mobile_node_t ue_node2 = {.position=pose2, .id=2};
    mobile_node_t ue_node3 = {.position=pose3, .id=3};
    mobile_node_t ue_node4 = {.position=pose4, .id=4};
    mobile_node_t ue_node5 = {.position=pose5, .id=5};
    ue_nodes.push_back(ue_node1);
    ue_nodes.push_back(ue_node2);
    ue_nodes.push_back(ue_node3);
    ue_nodes.push_back(ue_node4);
    ue_nodes.push_back(ue_node5);

    // for(int i=0; i<n_ues; i++) {
    //     mobile_node_t ue_node = {.position=pose, .id=i};
    //     NS_LOG_INFO("STA Locations: "<< "id: "<<i << " pose: "<< pose );
    //     ue_nodes.push_back(ue_node);   
    // }

    return ue_nodes;
}

void CoModel::run() {
    auto simulator_t = [this]() {
        Simulator::Stop (Seconds (sim_time));        
        Simulator::Run ();
        NS_LOG_DEBUG("Finished simulation.");
        report(std::cout);
        // Simulator::Destroy();
    };
    if(use_real_time) {
        NS_LOG_DEBUG("Simulation started in a new thread.");
        this->simulator = new std::thread(simulator_t);
    }
    else {
        NS_LOG_DEBUG("Simulation started.");
        simulator_t();
    }
};

void CoModel::update_mobility_model(std::vector<mobile_node_t> mobile_nodes) {
    this->mobile_nodes = mobile_nodes;
    for (uint32_t i = 0; i<n_nodes;i++) {
        Ptr<Node> node = backbone.Get (i);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
        mob->SetPosition(mobile_nodes[i].position);
    }
    NS_LOG_DEBUG("Simulating with the updated the mobility model.");
    
    if (!use_real_time) {
        run();
    }
};

void CoModel::create_mobility_model() {
    // add constant mobility model
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0;i<n_nodes;i++) {
        mobile_node_t mobile_node = mobile_nodes[i];
        positionAlloc->Add(mobile_node.position);
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (backbone);
    NS_LOG_DEBUG("Created " << n_nodes << " nodes.");
};

void CoModel::report(std::ostream &) {};

void CoModel::create_backbone_nodes() {
    backbone.Create(n_nodes);
    for (uint32_t i = 0;i<n_nodes;i++) {
        std::ostringstream os;
        os << "node-" << i+1;
        Names::Add (os.str (), backbone.Get (i));
    }
};

void CoModel::create_backbone_devices() {
    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    WifiHelper wifi;

    /**
     * https://www.nsnam.org/docs/release/3.21/doxygen/classns3_1_1_constant_rate_wifi_manager.html
     * 
     * ConstantRateWifiManager: This class uses always the same transmission rate for 
     * every packet sent. 
     * DataMode: The transmission mode to use for every data packet transmission 
     * (Initial value: OfdmRate6Mbps)
     * RtsCtsThreshold: If the size of the data packet + LLC header + MAC header + FCS 
     * trailer is bigger than this value, we use an RTS/CTS handshake before sending the 
     * data, as per IEEE Std. 802.11-2012, Section 9.3.5. This value will not have any 
     * ffect on some rate control algorithms.
     * 
     * **/
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", 
                    StringValue ("OfdmRate54Mbps"), "RtsCtsThreshold", UintegerValue (0));
    bb_devices = wifi.Install (wifiPhy, wifiMac, backbone); 
    InternetStackHelper stack;
    // AodvHelper routing;
    OlsrHelper routing;
    stack.SetRoutingHelper (routing);

    stack.Install (backbone);
    Ipv4AddressHelper address;
    address.SetBase ("172.16.0.0", "255.255.255.0");
    interfaces_bb = address.Assign (bb_devices);
    // address.NewNetwork();

    if (pcap)
    {
        NS_LOG_DEBUG("Enabled packet capturing.");
        wifiPhy.EnablePcapAll (std::string ("rosns3"));
    }
    if (print_routes)
    {
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("aodv.routes", std::ios::out);
        routing.PrintRoutingTableAllAt (Seconds (sim_time-1), routingStream);
    }

    NS_LOG_DEBUG("Installed wifi devices.");

};

// create the sta nodes and the ap nodes
void CoModel::create_sta_nodes(std::vector<mobile_node_t> ue_nodes) {
    stas.Create(ue_nodes.size());
    WifiHelper wifi_infra;

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    Ssid ssid = Ssid ("wifi-infra");
    wifi_infra.SetRemoteStationManager ("ns3::ArfWifiManager");
    WifiMacHelper mac_infra;

    // setup stas
    mac_infra.SetType("ns3::StaWifiMac", "Ssid", SsidValue (ssid));
    NetDeviceContainer sta_devices = wifi_infra.Install (wifiPhy, mac_infra, stas);

    // setup aps
    // NodeContainer ap_nodes(backbone);
    // for(uint32_t i=0;i<n_nodes; i++) {
    //     ap_nodes.Add(backbone.Get(i));
    // }

    // constant mobility for ap nodes
    NetDeviceContainer ap_devices;
    NodeContainer ap_nodes;
    for(uint32_t i=0;i<backbone.GetN(); i++) {
        NodeContainer ap_temp(backbone.Get(i));
        mac_infra.SetType("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
        NetDeviceContainer ap_device_temp = wifi_infra.Install(wifiPhy, mac_infra, ap_temp);

        Ptr<ListPositionAllocator> rel_position = CreateObject<ListPositionAllocator>();
        rel_position->Add(Vector(0,0,0));
        MobilityHelper mobility_ap;
        mobility_ap.PushReferenceMobilityModel(backbone.Get(0));
        mobility_ap.SetPositionAllocator(rel_position);
        mobility_ap.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility_ap.Install(ap_temp);

        ap_devices.Add(ap_device_temp);
        ap_nodes.Add(ap_temp);    
        }

    NetDeviceContainer infraDevices (ap_devices, sta_devices);
    InternetStackHelper stack;
    stack.Install(stas);

    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    interfaces_infra = address.Assign(infraDevices);

// position for sta nodes
    MobilityHelper mobility_sta;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (unsigned int i = 0;i<ue_nodes.size();i++) {
        mobile_node_t ue_node = ue_nodes[i];
        positionAlloc->Add(ue_node.position);
    }

    NS_LOG_INFO("Setting STA mobility model");
    mobility_sta.SetPositionAllocator(positionAlloc);
    mobility_sta.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    // mobility_sta.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
    //                              "Bounds", RectangleValue (Rectangle (-40, 40, -40, 40)),
    //                              "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1]"),
    //                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.4]"));

    mobility_sta.Install(stas);
    NS_LOG_DEBUG("Created STA and AP nodes.");

    if (pcap)
    {
        NS_LOG_DEBUG("Enabled packet capturing.");
        wifiPhy.EnablePcapAll (std::string ("ap-"));
    }



    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

}


// install applications on sta nodes
void CoModel::install_applications() {
    // address of the last sta
    NS_LOG_DEBUG("Creating UDP Applications");
    

    // Ipv4Address remoteAddr = sink->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (); 
    Ipv4Address remoteAddr("10.0.0.7");
    V4PingHelper ping (remoteAddr);
    // V4PingHelper ping(interfaces_bb.GetAddress (2));

    ping.SetAttribute ("Verbose", BooleanValue (true));
    const Time t = Seconds(1);
    ping.SetAttribute ("Interval", TimeValue(t));

    // // install ping app on source sta
    ApplicationContainer apps;
    apps.Add(ping.Install (stas.Get (0))); 

    apps.Start (Seconds (1));
    apps.Stop (Seconds (sim_time) - Seconds (0.5));

    // Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue ("1472"));
    // Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("500kb/s"));
    // uint16_t port = 9;
    // Ptr<Node> appSource = stas.Get(0); //10.0.0.5
    // Ptr<Node> appSink = stas.Get(4); //10.0.0.7
    // Ipv4Address remoteAddrs = appSink->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
    // OnOffHelper onoff ("ns3::UdpSocketFactory",
    //                  Address (InetSocketAddress (remoteAddrs, port)));  

    // ApplicationContainer app_udp = onoff.Install (appSource);
    // app_udp.Start (Seconds (1));
    // app_udp.Stop (Seconds (sim_time) - Seconds (1));

    // PacketSinkHelper sink ("ns3::UdpSocketFactory",
    //                      InetSocketAddress (Ipv4Address::GetAny (), port));
    // app_udp = sink.Install (appSink);
    // app_udp.Start (Seconds (1));


    NS_LOG_DEBUG("Installed UDP applications.");

};

void CoModel::install_inet_stack() {
    AodvHelper aodv;
    OlsrHelper olsr;
    // you can configure AODV attributes here using aodv.Set(name, value)
    InternetStackHelper stack;

    // stack.SetRoutingHelper (aodv); // has effect on the next Install ()
    // stack.Install (backbone);
    
    stack.SetRoutingHelper (olsr);
    stack.Install (backbone);

    Ipv4AddressHelper address;
    address.SetBase ("172.16.0.0", "255.255.255.0");
    interfaces_bb = address.Assign (bb_devices);

    if (print_routes)
    {
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("aodv.routes", std::ios::out);
        aodv.PrintRoutingTableAllAt (Seconds (sim_time-1), routingStream);
    }
    NS_LOG_DEBUG("Installed AODV internet stack.");

};

